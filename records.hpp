#pragma once

#include <cstdint>
#include <string>
#include <vector>

// ============================================================================
// Record payload: parsed subrecord data (LVLI, keywords, etc.)
// ============================================================================

struct RecordPayload
{
    struct LvliEntry
    {
        std::uint32_t formID = 0;
        std::uint16_t level = 0;
        std::uint16_t count = 0;
    };

    // Optional editor name (not currently populated by scanner, but used in logs)
    std::string editorName;

    // LVLI entries parsed from LVLO subrecords
    std::vector<LvliEntry> lvliEntries;

    // Keyword FormIDs parsed from KWDA subrecords
    std::vector<std::uint32_t> keywordFormIDs;
};

// ============================================================================
// RawRecord: one record from a plugin (KYWD/WEAP/ARMO/LVLI)
// ============================================================================

struct RawRecord
{
    // Local FormID (0x00FFFFFF portion only)
    std::uint32_t localFormID = 0;

    // Record type (fourCC: 'KYWD','WEAP','ARMO','LVLI', etc.)
    std::uint32_t type = 0;

    // Parsed payload (subrecords)
    RecordPayload payload;
};
