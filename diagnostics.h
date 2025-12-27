#pragma once
#include <string>
#include <vector>
#include <cstdint>

// ------------------------------------------------------------
// Diagnostics Event Types
// ------------------------------------------------------------

enum class DiagnosticsEventType
{
    Info,
    Warning,
    Error,
    Remap,
    Injection,
    Scan,
    MappingIssue,
    SlotConfigIssue,
    FormIDTrace
};

// ------------------------------------------------------------
// Public Data Structures
// ------------------------------------------------------------

// Summary of a plugin's activity during scanning/injection
struct PluginDiagnosticsSummary
{
    std::string pluginName;
    uint32_t recordsScanned = 0;
    uint32_t recordsInjected = 0;
    uint32_t recordsSkipped = 0;
    uint32_t lvliRemaps = 0;
};

// A single diagnostics event (for "why" explanations, errors, etc.)
struct DiagnosticsEvent
{
    DiagnosticsEventType type;
    std::string message;
};

// FormID remap trace result
struct FormIDTraceResult
{
    bool found = false;
    std::string pluginName;
    uint32_t originalFormID = 0;
    uint32_t localKey = 0;
    uint32_t virtualFormID = 0;
    bool isESL = false;
    std::string dummySlot;
    std::string reason;
};

// ------------------------------------------------------------
// Diagnostics API (public interface)
// ------------------------------------------------------------

// Initialize diagnostics system (called at plugin startup)
void Diagnostics_Initialize();

// Called by plugin.cpp at the end of initialization
void Diagnostics_Finalize();

// Called by plugin.cpp at startup to validate slot.cfg, CSV, etc.
void Diagnostics_RunValidator();

// Called by plugin.cpp to handle console commands
void Diagnostics_HandleCommand(const std::string& cmdLine);

// ------------------------------------------------------------
// Event Recording API (called by mapping/scanner/injector)
// ------------------------------------------------------------

// Record a generic diagnostics event
void Diagnostics_RecordEvent(DiagnosticsEventType type, const std::string& message);

// Record plugin-level summary data
void Diagnostics_RecordPluginScan(const std::string& pluginName);
void Diagnostics_RecordPluginInjection(const std::string& pluginName);
void Diagnostics_RecordPluginSkip(const std::string& pluginName);
void Diagnostics_RecordPluginLVLIRemap(const std::string& pluginName);

// Record slot.cfg issues
void Diagnostics_RecordSlotConfigIssue(const std::string& message);

// Record mapping issues (missing FormIDs, collisions, etc.)
void Diagnostics_RecordMappingIssue(const std::string& message);

// Record a FormID remap explanation
void Diagnostics_RecordFormIDTrace(
    const std::string& pluginName,
    uint32_t originalFormID,
    uint32_t localKey,
    uint32_t virtualFormID,
    bool isESL,
    const std::string& dummySlot,
    const std::string& reason
);

// ------------------------------------------------------------
// Query API (used by console commands)
// ------------------------------------------------------------

// Query a FormID remap trace
FormIDTraceResult Diagnostics_QueryFormID(uint32_t formID);

// Dump all diagnostics to file (called at finalize)
void Diagnostics_DumpToFile();
