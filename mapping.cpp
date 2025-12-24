#include "pch.h"
#include "mapping.hpp"
#include "log.hpp"

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
        logf("Config not found: %s. Using default slot with no modules.", cfgPath.string().c_str());

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

    std::uint8_t fileIndex = 0xF0;
    std::vector<std::string> moduleNames;

    std::string line;
    while (std::getline(in, line)) {
        line = trim_copy(line);
        if (line.empty() || line[0] == '#')
            continue;

        std::string key, value;
        if (!parse_config_line(line, key, value))
            continue;

        if (_stricmp(key.c_str(), "fileIndex") == 0) {
            // Accept decimal or hex (0x..)
            std::uint32_t idx = 0;

            if (value.rfind("0x", 0) == 0 || value.rfind("0X", 0) == 0) {
                std::stringstream hx(value);
                hx >> std::hex >> idx;
            }
            else {
                std::stringstream ds(value);
                ds >> idx;
            }

            if (idx > 0xFF) {
                logf("WARNING: fileIndex out of range (%s). Clamping to 0xFE.", value.c_str());
                idx = 0xFE;
            }

            fileIndex = static_cast<std::uint8_t>(idx);
        }
        else if (_stricmp(key.c_str(), "modules") == 0) {
            moduleNames = split_modules(value);
        }
    }

    // Populate slot descriptor
    outSlot.fileIndex = fileIndex;
    outSlot.modules.clear();
    outSlot.modules.reserve(moduleNames.size());

    for (const auto& name : moduleNames) {
        ModuleDescriptor md;
        md.name = name;  // full plugin filename (e.g., "MyMod.esl")
        // ESL fields (isESL, eslSlot) remain at defaults; scanner.cpp fills them.
        outSlot.modules.push_back(std::move(md));
    }

    logf("Loaded slot.cfg: fileIndex=0x%02X, modules=%zu",
        outSlot.fileIndex,
        outSlot.modules.size());

    return true;
}
