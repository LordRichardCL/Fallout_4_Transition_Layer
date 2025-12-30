#include "pch.h"
#include "visibility.hpp"
#include "mapping.hpp"
#include "scanner.hpp"
#include "log.hpp"
#include "diagnostics.h"

// ============================================================================
// BuildVisibilitySnapshot (initial skeleton, updated to new architecture)
// ============================================================================

VisibilitySnapshot BuildVisibilitySnapshot(
    const SlotDescriptor& slot,
    const std::vector<ModuleDescriptor>& modules)
{
    VisibilitySnapshot snapshot;
    snapshot.slotFileIndex = slot.fileIndex;

    snapshot.modules.reserve(modules.size());

    for (const auto& mod : modules)
    {
        ModuleScanSummary summary;
        summary.name = mod.name;

        //
        // Mapping info
        //
        summary.slotFileIndex = slot.fileIndex;
        summary.inSlotConfig = true;   // All modules in SlotDescriptor come from slot.cfg

        //
        // Metadata info
        //
        summary.metadataScanned = true;        // In your pipeline, metadata scan always runs
        summary.metadataScanSuccess = true;    // scanner.cpp logs failures

        summary.isESL = mod.isESL;
        summary.pseudoEslSlot = mod.eslSlot;
        summary.ba2Paths = mod.ba2Paths;

        if (mod.name.empty()) {
            summary.metadataScanSuccess = false;
            summary.hadErrors = true;
        }

        //
        // Record scan info (updated)
        // Old world: module.records was pre-filled.
        // New world: we query scan_plugin_records(mod.name) on demand.
        //
        std::vector<RawRecord> recs = scan_plugin_records(mod.name);

        summary.recordsScanned = true;
        summary.recordCount = recs.size();

        // We no longer track per-record compression state in RawRecord.
        // For now, treat them all as "uncompressed" in the summary.
        summary.uncompressedCount = recs.size();
        summary.compressedCount = 0;

        // Error/warning heuristic:
        // If the module has a name but produced zero records, flag a warning.
        if (!mod.name.empty() && summary.recordCount == 0) {
            summary.hadWarnings = true;
        }

        snapshot.modules.push_back(std::move(summary));
    }

    return snapshot;
}
// ============================================================================
// DumpVisibilitySnapshotToLog (final version)
// ============================================================================

void DumpVisibilitySnapshotToLog(const VisibilitySnapshot& snapshot)
{
    Diagnostics_RecordEvent(
        DiagnosticsEventType::Info,
        "Building visibility snapshot dump"
    );

    logf("============================================================");
    logf("=== Multiplexer Visibility Snapshot =========================");
    logf("============================================================");

    logf("Slot fileIndex: 0x%02X", snapshot.slotFileIndex);
    logf("Module count: %zu", snapshot.modules.size());
    logf("");

    for (const auto& m : snapshot.modules)
    {
        logf("------------------------------------------------------------");
        logf("Module: %s", m.name.c_str());
        logf("  In slot.cfg: %s", m.inSlotConfig ? "yes" : "no");
        logf("  Slot fileIndex: 0x%02X", m.slotFileIndex);

        //
        // Metadata
        //
        logf("  Metadata scanned: %s", m.metadataScanned ? "yes" : "no");
        logf("  Metadata success: %s", m.metadataScanSuccess ? "yes" : "no");
        logf("  ESL flag: %s", m.isESL ? "true" : "false");
        logf("  Pseudo FE slot: 0x%03X", m.pseudoEslSlot);

        if (!m.ba2Paths.empty()) {
            logf("  BA2 archives:");
            for (const auto& p : m.ba2Paths) {
                logf("    - %s", p.c_str());
            }
        }
        else {
            logf("  BA2 archives: none");
        }

        //
        // Record scan
        //
        logf("  Records scanned: %s", m.recordsScanned ? "yes" : "no");
        logf("  Total records: %zu", m.recordCount);
        logf("    Uncompressed: %zu", m.uncompressedCount);
        logf("    Compressed:   %zu", m.compressedCount);

        //
        // Issues
        //
        if (m.hadErrors || m.hadWarnings) {
            logf("  Issues:");
            if (m.hadErrors)   logf("    - Errors detected");
            if (m.hadWarnings) logf("    - Warnings detected");
        }
        else {
            logf("  Issues: none");
        }

        logf("");
    }

    logf("============================================================");
    logf("=== End Visibility Snapshot ================================");
    logf("============================================================");

    Diagnostics_RecordEvent(
        DiagnosticsEventType::Info,
        "Visibility snapshot dump complete"
    );
}
