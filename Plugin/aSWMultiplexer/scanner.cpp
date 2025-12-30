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

namespace fs = std::filesystem;

#pragma pack(push, 1)
struct TES4RecordHeader
{
    uint32_t type;
    uint32_t dataSize;
    uint32_t flags;
    uint32_t formID;
    uint32_t timestamp;
    uint16_t version;
    uint16_t unknown;
};

struct GenericRecordHeader
{
    uint32_t type;
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
    s[0] = char(v & 0xFF);
    s[1] = char((v >> 8) & 0xFF);
    s[2] = char((v >> 16) & 0xFF);
    s[3] = char((v >> 24) & 0xFF);
    s[4] = '\0';
    return std::string(s);
}

static uint32_t string_to_fourcc(const char* s)
{
    return
        (uint32_t(s[0])) |
        (uint32_t(s[1]) << 8) |
        (uint32_t(s[2]) << 16) |
        (uint32_t(s[3]) << 24);
}

static bool read_exact(std::ifstream& in, void* buf, std::size_t len)
{
    in.read(static_cast<char*>(buf), static_cast<std::streamsize>(len));
    return in.gcount() == std::streamsize(len);
}

static bool inflate_payload(const std::vector<uint8_t>& src, std::vector<uint8_t>& dst)
{
    if (src.size() < 4)
        return false;

    uint32_t uncompressedSize = 0;
    std::memcpy(&uncompressedSize, src.data(), 4);

    if (uncompressedSize == 0)
        return false;

    const Bytef* compData = reinterpret_cast<const Bytef*>(src.data() + 4);
    uLong compSize = uLong(src.size() - 4);

    dst.resize(uncompressedSize);
    uLong destLen = uncompressedSize;

    int zres = uncompress(reinterpret_cast<Bytef*>(dst.data()), &destLen, compData, compSize);
    if (zres != Z_OK)
        return false;

    dst.resize(destLen);
    return true;
}

static void parse_lvli_subrecord(uint32_t subType, const uint8_t* data, uint16_t size, RecordPayload& out)
{
    const uint32_t kLVLO = string_to_fourcc("LVLO");
    if (subType != kLVLO || size < 12)
        return;

    RecordPayload::LvliEntry entry;
    std::memset(&entry, 0, sizeof(entry));

    std::memcpy(&entry.formID, data, 4);
    std::memcpy(&entry.level, data + 4, 2);
    std::memcpy(&entry.count, data + 8, 2);

    out.lvliEntries.push_back(entry);
}

static void parse_keyword_subrecord(uint32_t subType, const uint8_t* data, uint16_t size, RecordPayload& out)
{
    const uint32_t kKWDA = string_to_fourcc("KWDA");
    if (subType != kKWDA || (size % 4) != 0)
        return;

    uint32_t count = size / 4;
    for (uint32_t i = 0; i < count; ++i)
    {
        uint32_t id = 0;
        std::memcpy(&id, data + i * 4, 4);
        out.keywordFormIDs.push_back(id);
    }
}

static void parse_subrecords_buffer(const uint8_t* buffer, std::size_t bufferSize, uint32_t recordType, RecordPayload& out)
{
    std::size_t offset = 0;

    while (offset + sizeof(SubrecordHeader) <= bufferSize)
    {
        const SubrecordHeader* sh = reinterpret_cast<const SubrecordHeader*>(buffer + offset);
        offset += sizeof(SubrecordHeader);

        if (offset + sh->dataSize > bufferSize)
            break;

        const uint8_t* data = buffer + offset;
        offset += sh->dataSize;

        switch (recordType)
        {
        case 0x494C564Cu: // 'LVLI'
            parse_lvli_subrecord(sh->type, data, sh->dataSize, out);
            break;

        default:
            parse_keyword_subrecord(sh->type, data, sh->dataSize, out);
            break;
        }
    }
}

static std::string find_plugin_path(const std::string& moduleName)
{
    char gamePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, gamePath, MAX_PATH) == 0)
        return {};

    fs::path p(gamePath);
    p = p.parent_path();
    p /= "Data";
    p /= moduleName;

    if (!fs::exists(p) || !fs::is_regular_file(p))
        return {};

    return p.string();
}

// Discover BA2 archives for a module
std::vector<std::string> discover_ba2s(const std::string& moduleName)
{
    std::vector<std::string> result;

    char gamePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, gamePath, MAX_PATH) == 0)
        return result;

    fs::path root(gamePath);
    root = root.parent_path();
    fs::path data = root / "Data";

    if (!fs::exists(data) || !fs::is_directory(data))
        return result;

    std::string baseName = moduleName;
    std::size_t dot = baseName.find_last_of('.');
    if (dot != std::string::npos)
        baseName = baseName.substr(0, dot);

    std::string baseLower = baseName;
    std::transform(baseLower.begin(), baseLower.end(), baseLower.begin(), ::tolower);

    for (fs::directory_iterator it(data); it != fs::directory_iterator(); ++it)
    {
        if (!it->is_regular_file())
            continue;

        fs::path path = it->path();
        if (!path.has_extension())
            continue;

        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".ba2")
            continue;

        std::string stem = path.stem().string();
        std::string stemLower = stem;
        std::transform(stemLower.begin(), stemLower.end(), stemLower.begin(), ::tolower);

        if (stemLower.rfind(baseLower, 0) == 0)
            result.push_back(path.string());
    }

    return result;
}

// Scan plugin metadata
bool scan_plugin_metadata(const std::string& moduleName, ModuleDescriptor& out)
{
    out.name = moduleName;
    out.isESL = false;
    out.eslSlot = 0;
    out.ba2Paths.clear();
    out.containsWorldspace = false;

    std::string path = find_plugin_path(moduleName);
    if (path.empty())
        return false;

    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;

    TES4RecordHeader header;
    std::memset(&header, 0, sizeof(header));

    if (!read_exact(in, &header, sizeof(header)))
        return false;

    const uint32_t kTES4 = string_to_fourcc("TES4");
    if (header.type != kTES4)
        return false;

    // ESL detection
    const uint32_t kESLFlag = 0x00000002u;
    out.isESL = (header.flags & kESLFlag) != 0;

    // FE pseudo-slot (stable hash)
    std::uint32_t h = 2166136261u;
    for (std::size_t i = 0; i < moduleName.size(); ++i)
    {
        h ^= static_cast<unsigned char>(moduleName[i]);
        h *= 16777619u;
    }
    out.eslSlot = static_cast<uint16_t>(h & 0x0FFFu);

    // BA2 discovery
    out.ba2Paths = discover_ba2s(moduleName);

    return true;
}

// Overload 1: legacy form
std::vector<RawRecord> scan_plugin_records(const std::string& moduleName)
{
    ModuleDescriptor dummy;
    dummy.name = moduleName;
    return scan_plugin_records(moduleName, dummy);
}

// Overload 2: preferred form
std::vector<RawRecord> scan_plugin_records(const std::string& moduleName, ModuleDescriptor& module)
{
    std::vector<RawRecord> out;

    std::string path = find_plugin_path(moduleName);
    if (path.empty())
        return out;

    std::ifstream in(path, std::ios::binary);
    if (!in)
        return out;

    TES4RecordHeader tes4;
    std::memset(&tes4, 0, sizeof(tes4));

    if (!read_exact(in, &tes4, sizeof(tes4)))
        return out;

    const uint32_t kTES4 = string_to_fourcc("TES4");
    if (tes4.type != kTES4)
        return out;

    in.seekg(tes4.dataSize, std::ios::cur);

    const uint32_t kKYWD = string_to_fourcc("KYWD");
    const uint32_t kWEAP = string_to_fourcc("WEAP");
    const uint32_t kARMO = string_to_fourcc("ARMO");
    const uint32_t kLVLI = string_to_fourcc("LVLI");

    const uint32_t kWRLD = string_to_fourcc("WRLD");
    const uint32_t kCELL = string_to_fourcc("CELL");
    const uint32_t kLAND = string_to_fourcc("LAND");
    const uint32_t kNAVM = string_to_fourcc("NAVM");
    const uint32_t kREFR = string_to_fourcc("REFR");
    const uint32_t kACHR = string_to_fourcc("ACHR");

    const uint32_t kCompressedFlag = 0x00040000u;

    while (true)
    {
        GenericRecordHeader rh;
        std::memset(&rh, 0, sizeof(rh));

        if (!read_exact(in, &rh, sizeof(rh)))
            break;

        if (rh.dataSize == 0)
            continue;

        uint32_t sig = rh.type;

        bool isWorldspace =
            sig == kWRLD || sig == kCELL || sig == kLAND ||
            sig == kNAVM || sig == kREFR || sig == kACHR;

        if (isWorldspace)
        {
            module.containsWorldspace = true;
            in.seekg(rh.dataSize, std::ios::cur);
            continue;
        }

        if (sig != kKYWD && sig != kWEAP && sig != kARMO && sig != kLVLI)
        {
            in.seekg(rh.dataSize, std::ios::cur);
            continue;
        }

        std::vector<uint8_t> payload(rh.dataSize);
        if (!read_exact(in, payload.data(), payload.size()))
            break;

        RawRecord rec;
        std::memset(&rec, 0, sizeof(rec));

        rec.localFormID = rh.formID & 0x00FFFFFFu;
        rec.type = sig;

        if (rh.flags & kCompressedFlag)
        {
            std::vector<uint8_t> decompressed;
            if (!inflate_payload(payload, decompressed))
                continue;

            parse_subrecords_buffer(decompressed.data(), decompressed.size(), sig, rec.payload);
        }
        else
        {
            parse_subrecords_buffer(payload.data(), payload.size(), sig, rec.payload);
        }

        out.push_back(rec);
    }

    return out;
}
