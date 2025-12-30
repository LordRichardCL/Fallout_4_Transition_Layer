#pragma once

#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <unordered_set>
#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>

// Represents one original module (plugin) that will be multiplexed into a dummy slot.
struct ModuleDescriptor
{
    std::string name;                                  // Module name (e.g., "MyWeapons.esp")
    std::vector<std::string> ba2Paths;                 // Paths to BA2 archives belonging to this module

    // Maps local form IDs (from the source module) to composed target FormIDs in the dummy slot.
    std::unordered_map<std::uint32_t, std::uint32_t> formIdMap;

    // ESL support
    bool isESL = false;
    std::uint16_t eslSlot = 0;     // FE slot index (0–4095) if isESL

    // Original plugin index (for runtime rewrite)
    uint8_t originalFileIndex = 0;

    // Worldspace content flag: set if any worldspace-like records are detected.
    bool containsWorldspace = false;
};

// Represents one dummy file index where multiple modules are multiplexed.
struct SlotDescriptor
{
    std::uint8_t fileIndex = 0;                        // Target dummy file index (0x00..0xFE)
    std::vector<ModuleDescriptor> modules;             // Modules multiplexed into this slot
};

// Load slot configuration from disk.
bool load_slot_config(SlotDescriptor& outSlot);

// Utility: trim whitespace from a string and return a new copy.
std::string trim_copy(const std::string& s);
