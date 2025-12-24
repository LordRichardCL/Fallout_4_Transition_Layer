#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "mapping.hpp"


// ============================================================================
// Payload describing a plugin record extracted from an ESP/ESM/ESL.
// This is a lightweight, safe representation that does NOT depend on F4SE types.
// ============================================================================
struct RecordPayload
{
    std::string editorName;

    // Keywords referenced by the record (raw FormIDs from disk)
    std::vector<std::uint32_t> keywordFormIDs;

    // Minimal weapon data
    struct WeaponData
    {
        std::uint32_t damage = 0;
        float         weight = 0.0f;
        std::uint32_t value = 0;
        bool          present = false;
    } weapon;

    // Minimal armor data
    struct ArmorData
    {
        std::uint32_t armorRating = 0;
        float         weight = 0.0f;
        std::uint32_t value = 0;
        bool          present = false;
    } armor;

    // Leveled list entry (form reference + level + count)
    struct LvliEntry
    {
        std::uint32_t formID = 0;   // Full FormID from disk
        std::uint16_t level = 1;
        std::uint16_t count = 1;
    };

    std::vector<LvliEntry> lvliEntries;
};

// ============================================================================
// Raw record container representing a single ESP/ESM/ESL record.
// ============================================================================
struct RawRecord
{
    std::uint32_t localFormID = 0;  // 0x00FFFFFF space
    std::uint32_t type = 0;         // FourCC: 'KYWD','WEAP','ARMO','LVLI'
    RecordPayload payload;
};

// ============================================================================
// Discover BA2 archives for a module (optional if you mount assets).
// ============================================================================
std::vector<std::string> discover_ba2s(const std::string& moduleName);

// ============================================================================
// Scan plugin records for a module by reading ESP/ESM/ESL from disk.
// ============================================================================
std::vector<RawRecord> scan_plugin_records(const std::string& moduleName);

// ============================================================================
// Scan plugin metadata (ESL detection, FE slot extraction)
// ============================================================================
bool scan_plugin_metadata(const std::string& moduleName, ModuleDescriptor& out);
