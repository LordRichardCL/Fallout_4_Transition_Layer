#include "pch.h"
#include "mapping.hpp"
#include "log.hpp"
#include "diagnostics.h"
#include "records.hpp"


#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>   // for _stricmp

// ============================================================================
// Config file layout (simple key = value pairs + modules list):
//
// Data\F4SE\Plugins\Multiplexer\slot.cfg
// fileIndex = 0xF0
// modules = ModA.esp,ModB.esm,ModC.esl
//
// Module names are expected to be full plugin filenames
// (e.g., "MyMod.esp", "MyDLC.esm", "MyLight.esl").
// scanner.cpp is responsible for resolving paths and headers.
// ============================================================================

static std::filesystem::path config_dir()
{
    return std::filesystem::path("Data") / "F4SE" / "Plugins" / "Multiplexer";
}

// Trim whitespace from a string and return a new copy.
std::string trim_copy(const std::string& s)
{
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";

    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Parse "key = value" lines.
static bool parse_config_line(const std::string& line, std::string& key, std::string& value)
{
    auto eq = line.find('=');
    if (eq == std::string::npos)
        return false;

    key = trim_copy(line.substr(0, eq));
    value = trim_copy(line.substr(eq + 1));

    return !key.empty();
}

// Split comma-separated module names.
static std::vector<std::string> split_modules(const std::string& value)
{
    std::vector<std::string> out;
    std::stringstream ss(value);
    std::string item;

    while (std::getline(ss, item, ',')) {
        item = trim_copy(item);
        if (!item.empty())
            out.push_back(item);
    }

    return out;
}
// ============================================================================
// Load slot configuration from disk.
// ============================================================================
bool load_slot_config(SlotDescriptor& outSlot)
{
    const auto cfgPath = config_dir() / "slot.cfg";

    if (!std::filesystem::exists(cfgPath)) {
        logf("Config not found: %s. Using default slot with no modules.",
            cfgPath.string().c_str());

        // Sensible default: fileIndex 0xF0, empty module list.
        outSlot.fileIndex = 0xF0;
        outSlot.modules.clear();
        return true;
    }

    std::ifstream in(cfgPath);
    if (!in) {
        logf("ERROR: Could not open config: %s", cfgPath.string().c_str());
        return false;
    }

    // Working values
    std::uint8_t fileIndex = 0xF0;
    std::vector<std::string> moduleNames;

    // Diagnostics flags
    bool fileIndexSeen = false;
    bool modulesSeen = false;

    std::string line;

    // ========================================================================
    // Begin parsing loop
    // ========================================================================
    while (std::getline(in, line)) {

        // --- BOM detection BEFORE trimming ---
        if (line.size() >= 3 &&
            (unsigned char)line[0] == 0xEF &&
            (unsigned char)line[1] == 0xBB &&
            (unsigned char)line[2] == 0xBF)
        {
            Diagnostics_RecordSlotConfigIssue("UTF-8 BOM detected in slot.cfg");
            line = line.substr(3); // strip BOM
        }

        // Trim whitespace
        line = trim_copy(line);

        // Skip empty or comment lines
        if (line.empty() || line[0] == '#')
            continue;

        // Parsing continues in Chunk 3...
   // --------------------------------------------------------------------
        // Parse "key = value"
        // --------------------------------------------------------------------
        std::string key, value;
        if (!parse_config_line(line, key, value)) {
            Diagnostics_RecordSlotConfigIssue("Malformed line in slot.cfg: " + line);
            Diagnostics_RecordEvent(
                DiagnosticsEventType::SlotConfigIssue,
                "Malformed line in slot.cfg: " + line
            );
            continue;
        }

        // --------------------------------------------------------------------
        // fileIndex = ...
        // --------------------------------------------------------------------
        if (_stricmp(key.c_str(), "fileIndex") == 0) {

            fileIndexSeen = true;

            // Accept decimal or hex (0x..)
            std::uint32_t idx = 0;
            bool parseOK = true;

            if (value.rfind("0x", 0) == 0 || value.rfind("0X", 0) == 0) {
                std::stringstream hx(value);
                hx >> std::hex >> idx;
                if (!hx)
                    parseOK = false;
            }
            else {
                std::stringstream ds(value);
                ds >> idx;
                if (!ds)
                    parseOK = false;
            }

            // Non-numeric or failed parse
            if (!parseOK) {
                Diagnostics_RecordSlotConfigIssue("Invalid fileIndex value: " + value);
                Diagnostics_RecordEvent(
                    DiagnosticsEventType::SlotConfigIssue,
                    "Invalid fileIndex value: " + value
                );
                continue;
            }

            // Out-of-range
            if (idx > 0xFF) {
                Diagnostics_RecordSlotConfigIssue(
                    "fileIndex out of range (" + value + "), clamped to 0xFE"
                );
                Diagnostics_RecordEvent(
                    DiagnosticsEventType::Warning,
                    "fileIndex out of range: " + value
                );
                idx = 0xFE;
            }

            fileIndex = static_cast<std::uint8_t>(idx);
            continue;
        }

        // --------------------------------------------------------------------
        // modules = ...
        // --------------------------------------------------------------------
        if (_stricmp(key.c_str(), "modules") == 0) {
            modulesSeen = true;
            moduleNames = split_modules(value);
            continue;
        }

        // --------------------------------------------------------------------
        // Unknown key
        // --------------------------------------------------------------------
        Diagnostics_RecordSlotConfigIssue("Unknown key in slot.cfg: " + key);
        Diagnostics_RecordEvent(
            DiagnosticsEventType::SlotConfigIssue,
            "Unknown key in slot.cfg: " + key
        );
    } // end while getline
     // ========================================================================
    // Missing-key diagnostics
    // ========================================================================
    if (!fileIndexSeen) {
        Diagnostics_RecordSlotConfigIssue("Missing key: fileIndex");
        Diagnostics_RecordEvent(
            DiagnosticsEventType::Warning,
            "slot.cfg missing fileIndex"
        );
    }

    if (!modulesSeen) {
        Diagnostics_RecordSlotConfigIssue("Missing key: modules");
        Diagnostics_RecordEvent(
            DiagnosticsEventType::Warning,
            "slot.cfg missing modules"
        );
    }

    // ========================================================================
    // Populate slot descriptor
    // ========================================================================
    outSlot.fileIndex = fileIndex;
    outSlot.modules.clear();
    outSlot.modules.reserve(moduleNames.size());
    // ========================================================================
    // Validate modules and populate outSlot.modules
    // ========================================================================
    {
        std::unordered_set<std::string> seenModules;

        for (const auto& name : moduleNames) {

            // --- Duplicate detection ---
            if (seenModules.count(name)) {
                Diagnostics_RecordSlotConfigIssue("Duplicate module in slot.cfg: " + name);
                Diagnostics_RecordEvent(
                    DiagnosticsEventType::SlotConfigIssue,
                    "Duplicate module in slot.cfg: " + name
                );
                continue; // skip duplicate
            }
            seenModules.insert(name);

            // --- Validate plugin extension ---
            bool validExt =
                name.size() > 4 &&
                (
                    _stricmp(name.c_str() + name.size() - 4, ".esp") == 0 ||
                    _stricmp(name.c_str() + name.size() - 4, ".esm") == 0 ||
                    _stricmp(name.c_str() + name.size() - 4, ".esl") == 0
                    );

            if (!validExt) {
                Diagnostics_RecordSlotConfigIssue("Invalid plugin filename in slot.cfg: " + name);
                Diagnostics_RecordEvent(
                    DiagnosticsEventType::SlotConfigIssue,
                    "Invalid plugin filename in slot.cfg: " + name
                );
                // Still add it so downstream logic can report more details
            }

            // --- Record plugin scan event ---
            Diagnostics_RecordPluginScan(name);
            Diagnostics_RecordEvent(
                DiagnosticsEventType::Info,
                "Loaded module from slot.cfg: " + name
            );

            // --- Add module descriptor ---
            ModuleDescriptor md;
            md.name = name;  // full plugin filename (e.g., "MyMod.esl")
            // ESL fields remain defaults; scanner.cpp fills them.
            outSlot.modules.push_back(std::move(md));
        }
    }

    // ========================================================================
    // Summary event
    // ========================================================================
    {
        std::stringstream ss;
        ss << "slot.cfg loaded: fileIndex=0x"
            << std::hex << std::uppercase << (int)outSlot.fileIndex
            << ", modules=" << outSlot.modules.size();

        Diagnostics_RecordEvent(DiagnosticsEventType::Info, ss.str());
    }

    logf("Loaded slot.cfg: fileIndex=0x%02X, modules=%zu",
        outSlot.fileIndex,
        outSlot.modules.size());

    return true;
}
