#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct SlotDescriptor;
struct ModuleDescriptor;

// ============================================================================
// Visibility data structures
// ============================================================================

struct ModuleScanSummary
{
    std::string name;

    // Mapping
    std::uint8_t slotFileIndex = 0xFF;
    bool inSlotConfig = false;

    // Metadata
    bool metadataScanned = false;
    bool metadataScanSuccess = false;
    bool isESL = false;
    uint16_t pseudoEslSlot = 0;
    std::vector<std::string> ba2Paths;

    // Records
    bool recordsScanned = false;
    std::size_t recordCount = 0;
    std::size_t compressedCount = 0;
    std::size_t uncompressedCount = 0;

    // Aggregated issues
    bool hadErrors = false;
    bool hadWarnings = false;
};

struct VisibilitySnapshot
{
    std::uint8_t slotFileIndex = 0xFF;
    std::vector<ModuleScanSummary> modules;
};

// ============================================================================
// API
// ============================================================================

// Build a visibility snapshot from the slot descriptor and module descriptors.
VisibilitySnapshot BuildVisibilitySnapshot(
    const SlotDescriptor& slot,
    const std::vector<ModuleDescriptor>& modules);

// Dump a human-readable summary to logf and diagnostics.
void DumpVisibilitySnapshotToLog(const VisibilitySnapshot& snapshot);
