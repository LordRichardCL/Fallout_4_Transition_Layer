#include "pch.h"

#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <algorithm>
#include <filesystem>
#include <cstring>

#include <zlib.h>

#include "log.hpp"
#include "scanner.hpp"
#include "mapping.hpp"
#include "diagnostics.h"
#include "records.hpp"
// ============================================================================
// Helpers
// ============================================================================

namespace fs = std::filesystem;

#pragma pack(push, 1)
struct TES4RecordHeader
{
    uint32_t type;       // 'TES4'
    uint32_t dataSize;
    uint32_t flags;
    uint32_t formID;
    uint32_t timestamp;
    uint16_t version;
    uint16_t unknown;
};

struct GenericRecordHeader
{
    uint32_t type;       // 'KYWD','WEAP','ARMO','LVLI', etc.
    uint32_t dataSize;
    uint32_t flags;
    uint32_t formID;
    uint32_t timestamp;
    uint16_t version;
    uint16_t unknown;
};

struct SubrecordHeader
{
    uint32_t type;
    uint16_t dataSize;
};
#pragma pack(pop)

static std::string fourcc_to_string(uint32_t v)
{
    char s[5];
    s[0] = static_cast<char>(v & 0xFF);
    s[1] = static_cast<char>((v >> 8) & 0xFF);
    s[2] = static_cast<char>((v >> 16) & 0xFF);
    s[3] = static_cast<char>((v >> 24) & 0xFF);
    s[4] = '\0';
    return std::string(s);
}

static uint32_t string_to_fourcc(const char* s)
{
    return
        (static_cast<uint32_t>(s[0])) |
        (static_cast<uint32_t>(s[1]) << 8) |
        (static_cast<uint32_t>(s[2]) << 16) |
        (static_cast<uint32_t>(s[3]) << 24);
}

static bool read_exact(std::ifstream& in, void* buf, std::size_t len)
{
    in.read(static_cast<char*>(buf), static_cast<std::streamsize>(len));
    return in.gcount() == static_cast<std::streamsize>(len);
}

// ============================================================================
// Zlib inflate for compressed records
// Layout:
//   [0..3]  = uint32 uncompressedSize (little-endian)
//   [4..N]  = deflate-compressed payload
// ============================================================================

static bool inflate_payload(const std::vector<std::uint8_t>& src, std::vector<std::uint8_t>& dst)
{
    if (src.size() < 4) {
        Diagnostics_RecordEvent(
            DiagnosticsEventType::Error,
            "Compressed record too small to contain uncompressed size header."
        );
        logf("ERROR: Compressed record too small to contain uncompressed size header.");
        return false;
    }

    uint32_t uncompressedSize = 0;
    std::memcpy(&uncompressedSize, src.data(), sizeof(uint32_t));

    if (uncompressedSize == 0 || uncompressedSize > (128u * 1024u * 1024u)) {
        Diagnostics_RecordEvent(
            DiagnosticsEventType::Error,
            "Implausible uncompressed size in compressed record: " + std::to_string(uncompressedSize)
        );
        logf("ERROR: Implausible uncompressed size in compressed record: %u", uncompressedSize);
        return false;
    }

    const Bytef* compData = reinterpret_cast<const Bytef*>(src.data() + 4);
    uLong compSize = static_cast<uLong>(src.size() - 4);

    dst.clear();
    dst.resize(uncompressedSize);

    uLong destLen = static_cast<uLong>(uncompressedSize);
    int zres = uncompress(reinterpret_cast<Bytef*>(dst.data()), &destLen, compData, compSize);

    if (zres != Z_OK) {
        Diagnostics_RecordEvent(
            DiagnosticsEventType::Error,
            "zlib uncompress failed with code " + std::to_string(zres)
        );
        logf("ERROR: zlib uncompress failed (code %d). Expected %u bytes, got %lu.",
            zres, uncompressedSize, static_cast<unsigned long>(destLen));
        dst.clear();
        return false;
    }

    if (destLen != uncompressedSize) {
        Diagnostics_RecordEvent(
            DiagnosticsEventType::Warning,
            "zlib uncompress size mismatch. Expected " +
            std::to_string(uncompressedSize) + ", got " + std::to_string(destLen)
        );
        logf("WARNING: zlib uncompress size mismatch. Expected %u, got %lu.",
            uncompressedSize, static_cast<unsigned long>(destLen));
        dst.resize(destLen);
    }

    return true;
}
// ============================================================================
// Subrecord parsing
// ============================================================================

static void parse_weapon_subrecord(uint32_t subType, const uint8_t* data, uint16_t size, RecordPayload& out)
{
    // Stub: user-defined parsing can go here.
    (void)subType;
    (void)data;
    (void)size;
    (void)out;
}

static void parse_armor_subrecord(uint32_t subType, const uint8_t* data, uint16_t size, RecordPayload& out)
{
    (void)subType;
    (void)data;
    (void)size;
    (void)out;
}

static void parse_lvli_subrecord(uint32_t subType, const uint8_t* data, uint16_t size, RecordPayload& out)
{
    const uint32_t kLVLO = string_to_fourcc("LVLO");
    if (subType != kLVLO || size < 12) {
        return;
    }

    RecordPayload::LvliEntry entry{};
    std::memcpy(&entry.formID, data + 0, sizeof(uint32_t));
    std::memcpy(&entry.level, data + 4, sizeof(uint16_t));
    std::memcpy(&entry.count, data + 8, sizeof(uint16_t));

    out.lvliEntries.push_back(entry);
}

static void parse_keyword_subrecord(uint32_t subType, const uint8_t* data, uint16_t size, RecordPayload& out)
{
    const uint32_t kKWDA = string_to_fourcc("KWDA");
    if (subType != kKWDA || (size % 4) != 0) {
        return;
    }

    const uint32_t count = size / 4;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t kFormID = 0;
        std::memcpy(&kFormID, data + i * 4, sizeof(uint32_t));
        out.keywordFormIDs.push_back(kFormID);
    }
}

// ============================================================================
// Parse subrecords buffer
// ============================================================================

static void parse_subrecords_buffer(const uint8_t* buffer, std::size_t bufferSize, uint32_t recordType, RecordPayload& out)
{
    std::size_t offset = 0;

    while (offset + sizeof(SubrecordHeader) <= bufferSize) {

        auto* sh = reinterpret_cast<const SubrecordHeader*>(buffer + offset);
        offset += sizeof(SubrecordHeader);

        // Bounds check
        if (offset + sh->dataSize > bufferSize) {
            Diagnostics_RecordEvent(
                DiagnosticsEventType::Warning,
                "Subrecord overruns record bounds for record type " +
                fourcc_to_string(recordType)
            );
            logf("WARNING: Subrecord overruns record bounds.");
            break;
        }

        const uint8_t* data = buffer + offset;
        offset += sh->dataSize;

        // Dispatch based on record type
        switch (recordType) {

        case /* 'WEAP' */ 0x50414557u:
            parse_weapon_subrecord(sh->type, data, sh->dataSize, out);
            parse_keyword_subrecord(sh->type, data, sh->dataSize, out);
            break;

        case /* 'ARMO' */ 0x4F4D5241u:
            parse_armor_subrecord(sh->type, data, sh->dataSize, out);
            parse_keyword_subrecord(sh->type, data, sh->dataSize, out);
            break;

        case /* 'LVLI' */ 0x494C564Cu:
            parse_lvli_subrecord(sh->type, data, sh->dataSize, out);
            break;

        default:
            // KYWD and others: mostly keyword data
            parse_keyword_subrecord(sh->type, data, sh->dataSize, out);
            break;
        }
    }

    // If offset != bufferSize, we may have trailing bytes
    if (offset != bufferSize) {
        Diagnostics_RecordEvent(
            DiagnosticsEventType::Warning,
            "Trailing bytes detected after subrecord parsing for record type " +
            fourcc_to_string(recordType)
        );
        logf("WARNING: Trailing bytes detected after subrecord parsing.");
    }
}
// ============================================================================
// Plugin path helpers
// ============================================================================

static std::string find_plugin_path(const std::string& moduleName)
{
    char gamePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, gamePath, MAX_PATH) == 0) {
        Diagnostics_RecordEvent(
            DiagnosticsEventType::Error,
            "GetModuleFileNameA failed while resolving plugin path for " + moduleName
        );
        logf("ERROR: GetModuleFileNameA failed.");
        return {};
    }

    fs::path p(gamePath);
    p = p.parent_path();  // Fallout 4 root
    p /= "Data";
    p /= moduleName;

    if (!fs::exists(p)) {
        Diagnostics_RecordEvent(
            DiagnosticsEventType::Warning,
            "Plugin file not found: " + p.string()
        );
        logf("WARNING: Plugin file not found: %s", p.string().c_str());
        return {};
    }

    if (!fs::is_regular_file(p)) {
        Diagnostics_RecordEvent(
            DiagnosticsEventType::Warning,
            "Plugin path exists but is not a file: " + p.string()
        );
        logf("WARNING: Plugin path exists but is not a file: %s", p.string().c_str());
        return {};
    }

    return p.string();
}

// ============================================================================
// Discover BA2 archives for a module
// ============================================================================

std::vector<std::string> discover_ba2s(const std::string& moduleName)
{
    std::vector<std::string> result;

    char gamePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, gamePath, MAX_PATH) == 0) {
        Diagnostics_RecordEvent(
            DiagnosticsEventType::Error,
            "GetModuleFileNameA failed while discovering BA2s for " + moduleName
        );
        logf("ERROR: GetModuleFileNameA failed.");
        return result;
    }

    fs::path root(gamePath);
    root = root.parent_path(); // Fallout 4 root
    fs::path data = root / "Data";

    if (!fs::exists(data) || !fs::is_directory(data)) {
        Diagnostics_RecordEvent(
            DiagnosticsEventType::Warning,
            "Data directory not found while discovering BA2s for " + moduleName
        );
        logf("WARNING: Data directory not found.");
        return result;
    }

    // Bethesda naming convention: <ModuleName> - Main.ba2, <ModuleName> - Textures.ba2, etc.
    std::string baseName = moduleName;
    auto dot = baseName.find_last_of('.');
    if (dot != std::string::npos) {
        baseName = baseName.substr(0, dot);
    }

    std::string baseLower = baseName;
    std::transform(baseLower.begin(), baseLower.end(), baseLower.begin(), ::tolower);

    for (auto& entry : fs::directory_iterator(data)) {
        if (!entry.is_regular_file())
            continue;

        auto path = entry.path();
        if (!path.has_extension())
            continue;

        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".ba2")
            continue;

        std::string stem = path.stem().string();
        std::string stemLower = stem;
        std::transform(stemLower.begin(), stemLower.end(), stemLower.begin(), ::tolower);

        if (stemLower.rfind(baseLower, 0) == 0) {
            result.push_back(path.string());
        }
    }

    // Diagnostics summary
    Diagnostics_RecordEvent(
        DiagnosticsEventType::Info,
        "Discovered " + std::to_string(result.size()) +
        " BA2 archives for " + moduleName
    );

    return result;
}
// ============================================================================
// Scan plugin metadata (ESL detection, FE slot)
// ============================================================================

bool scan_plugin_metadata(const std::string& moduleName, ModuleDescriptor& out)
{
    Diagnostics_RecordPluginScan(moduleName);
    Diagnostics_RecordEvent(
        DiagnosticsEventType::Info,
        "Scanning plugin metadata: " + moduleName
    );

    out.name = moduleName;
    out.isESL = false;
    out.eslSlot = 0;
    out.ba2Paths.clear();

    std::string path = find_plugin_path(moduleName);
    if (path.empty()) {
        Diagnostics_RecordEvent(
            DiagnosticsEventType::Warning,
            "scan_plugin_metadata: could not find plugin file for " + moduleName
        );
        logf("WARNING: scan_plugin_metadata: could not find path for module '%s'", moduleName.c_str());
        return false;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        Diagnostics_RecordEvent(
            DiagnosticsEventType::Warning,
            "scan_plugin_metadata: failed to open plugin file " + path
        );
        logf("WARNING: scan_plugin_metadata: failed to open '%s'", path.c_str());
        return false;
    }

    TES4RecordHeader header{};
    if (!read_exact(in, &header, sizeof(header))) {
        Diagnostics_RecordEvent(
            DiagnosticsEventType::Error,
            "scan_plugin_metadata: failed to read TES4 header for " + moduleName
        );
        logf("WARNING: scan_plugin_metadata: failed to read TES4 header for '%s'", moduleName.c_str());
        return false;
    }

    const uint32_t kTES4 = string_to_fourcc("TES4");
    if (header.type != kTES4) {
        Diagnostics_RecordEvent(
            DiagnosticsEventType::Error,
            "scan_plugin_metadata: plugin does not start with TES4: " + moduleName
        );
        logf("WARNING: scan_plugin_metadata: '%s' does not start with TES4.", moduleName.c_str());
        return false;
    }

    // ESL detection
    constexpr uint32_t kESLFlag = 0x00000002u; // Compact FormIDs flag
    out.isESL = (header.flags & kESLFlag) != 0;

    Diagnostics_RecordEvent(
        DiagnosticsEventType::Info,
        std::string("ESL flag for ") + moduleName + ": " + (out.isESL ? "true" : "false")
    );

    // FE pseudo-slot (stable hash)
    {
        std::uint32_t h = 2166136261u;
        for (char c : moduleName) {
            h ^= static_cast<std::uint8_t>(c);
            h *= 16777619u;
        }
        out.eslSlot = static_cast<uint16_t>(h & 0x0FFFu);
    }

    Diagnostics_RecordEvent(
        DiagnosticsEventType::Info,
        "Assigned pseudo FE slot " + std::to_string(out.eslSlot) +
        " for plugin " + moduleName
    );

    // BA2 discovery
    out.ba2Paths = discover_ba2s(moduleName);

    Diagnostics_RecordEvent(
        DiagnosticsEventType::Info,
        "Plugin " + moduleName + " has " +
        std::to_string(out.ba2Paths.size()) + " BA2 archives"
    );

    return true;
}
// ============================================================================
// Scan plugin records (WEAP/ARMO/KYWD/LVLI) from ESP/ESM/ESL
// ============================================================================

std::vector<RawRecord> scan_plugin_records(const std::string& moduleName)
{
    Diagnostics_RecordEvent(
        DiagnosticsEventType::Info,
        "Scanning plugin records: " + moduleName
    );

    std::vector<RawRecord> out;

    std::string path = find_plugin_path(moduleName);
    if (path.empty()) {
        Diagnostics_RecordEvent(
            DiagnosticsEventType::Warning,
            "scan_plugin_records: could not find plugin file for " + moduleName
        );
        logf("WARNING: scan_plugin_records: could not find path for module '%s'", moduleName.c_str());
        return out;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        Diagnostics_RecordEvent(
            DiagnosticsEventType::Warning,
            "scan_plugin_records: failed to open plugin file " + path
        );
        logf("WARNING: scan_plugin_records: failed to open '%s'", path.c_str());
        return out;
    }

    TES4RecordHeader tes4{};
    if (!read_exact(in, &tes4, sizeof(tes4))) {
        Diagnostics_RecordEvent(
            DiagnosticsEventType::Error,
            "scan_plugin_records: failed to read TES4 header for " + moduleName
        );
        logf("WARNING: scan_plugin_records: failed to read TES4 header for '%s'", moduleName.c_str());
        return out;
    }

    const uint32_t kTES4 = string_to_fourcc("TES4");
    if (tes4.type != kTES4) {
        Diagnostics_RecordEvent(
            DiagnosticsEventType::Error,
            "scan_plugin_records: plugin does not start with TES4: " + moduleName
        );
        logf("WARNING: scan_plugin_records: '%s' does not start with TES4.", moduleName.c_str());
        return out;
    }

    // Skip TES4 payload
    in.seekg(tes4.dataSize, std::ios::cur);

    const uint32_t kKYWD = string_to_fourcc("KYWD");
    const uint32_t kWEAP = string_to_fourcc("WEAP");
    const uint32_t kARMO = string_to_fourcc("ARMO");
    const uint32_t kLVLI = string_to_fourcc("LVLI");

    constexpr uint32_t kCompressedFlag = 0x00040000u;

    std::size_t compressedCount = 0;
    std::size_t uncompressedCount = 0;

    while (true) {
        GenericRecordHeader rh{};
        if (!read_exact(in, &rh, sizeof(rh))) {
            break; // end of file
        }

        if (rh.dataSize == 0) {
            continue;
        }

        const uint32_t sig = rh.type;
        if (sig != kKYWD && sig != kWEAP && sig != kARMO && sig != kLVLI) {
            // Skip uninteresting record types
            in.seekg(rh.dataSize, std::ios::cur);
            continue;
        }

        std::vector<uint8_t> payload(rh.dataSize);
        if (!read_exact(in, payload.data(), payload.size())) {
            Diagnostics_RecordEvent(
                DiagnosticsEventType::Warning,
                "Failed to read payload for record " +
                fourcc_to_string(sig) + ":" + std::to_string(rh.formID)
            );
            logf("WARNING: Failed to read payload for record %s:%08X",
                fourcc_to_string(sig).c_str(), rh.formID);
            break;
        }

        RawRecord rec{};
        rec.localFormID = rh.formID & 0x00FFFFFFu;
        rec.type = sig;

        if ((rh.flags & kCompressedFlag) != 0) {
            compressedCount++;

            std::vector<uint8_t> decompressed;
            if (!inflate_payload(payload, decompressed)) {
                Diagnostics_RecordEvent(
                    DiagnosticsEventType::Error,
                    "Failed to inflate compressed record " +
                    fourcc_to_string(sig) + ":" + std::to_string(rh.formID)
                );
                logf("ERROR: Failed to inflate compressed record %s:%08X in '%s'",
                    fourcc_to_string(sig).c_str(), rh.formID, moduleName.c_str());
                continue;
            }

            parse_subrecords_buffer(decompressed.data(), decompressed.size(), sig, rec.payload);
        }
        else {
            uncompressedCount++;
            parse_subrecords_buffer(payload.data(), payload.size(), sig, rec.payload);
        }

        out.push_back(std::move(rec));
    }

    // Summary diagnostics
    Diagnostics_RecordEvent(
        DiagnosticsEventType::Info,
        "scan_plugin_records: " + moduleName +
        " -> " + std::to_string(out.size()) +
        " records (" +
        std::to_string(uncompressedCount) + " uncompressed, " +
        std::to_string(compressedCount) + " compressed)"
    );

    logf("scan_plugin_records: '%s' -> %zu records (KYWD/WEAP/ARMO/LVLI)",
        moduleName.c_str(), out.size());

    return out;
}
