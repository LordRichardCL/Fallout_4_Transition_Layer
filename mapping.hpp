#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

// Represents one original module (plugin) that will be multiplexed into a dummy slot.
struct ModuleDescriptor
{
    std::string name;                                  // Module name (e.g., "MyWeapons.esp")
    std::vector<std::string> ba2Paths;                 // Paths to BA2 archives belonging to this module

    // Maps local form IDs (from the source module) to composed target FormIDs in the dummy slot.
    std::unordered_map<std::uint32_t, std::uint32_t> formIdMap;

    // ============================================================
    // ESL SUPPORT (NEW)
    // ============================================================

    bool isESL = false;            // True if .esl or ESL-flagged ESP
    std::uint16_t eslSlot = 0;     // FE slot index (0–4095) if isESL

    // For ESL plugins, localFormID is compact (0x000–0xFFF)
    // For normal plugins, localFormID is 0x000000–0xFFFFFF
};

// Represents one dummy file index where multiple modules are multiplexed.
struct SlotDescriptor
{
    std::uint8_t fileIndex = 0;                        // Target dummy file index (0x00..0xFE)
    std::vector<ModuleDescriptor> modules;             // Modules multiplexed into this slot
};

// Load slot configuration from disk (optional config file).
// Returns true on success, false on failure.
bool load_slot_config(SlotDescriptor& outSlot);

// Utility: trim whitespace from a string and return a new copy.
std::string trim_copy(const std::string& s);
