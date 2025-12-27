#include "pch.h"
#include "diagnostics.h"
#include "identity.h"
#include "log.hpp"
#include "config.hpp"
#include "scanner.hpp"
#include "mapping.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <algorithm>

// External globals from plugin.cpp
extern std::unordered_map<std::string, std::string> g_pluginAliasMap;
extern bool g_consoleActive;

// Console helper
#define DX(msg) do { if (g_consoleActive) std::cout << msg << std::endl; } while (0)

// ============================================================================
// Internal Diagnostics State
// ============================================================================

static std::vector<DiagnosticsEvent> g_events;
static std::unordered_map<std::string, PluginDiagnosticsSummary> g_pluginSummaries;
static std::vector<std::string> g_slotConfigIssues;
static std::vector<std::string> g_mappingIssues;
static std::unordered_map<uint32_t, FormIDTraceResult> g_formIDTraces;

// Simple helper to get or create a plugin summary
static PluginDiagnosticsSummary& GetOrCreatePluginSummary(const std::string& pluginName)
{
    auto it = g_pluginSummaries.find(pluginName);
    if (it != g_pluginSummaries.end())
        return it->second;

    PluginDiagnosticsSummary summary;
    summary.pluginName = pluginName;
    auto inserted = g_pluginSummaries.emplace(pluginName, std::move(summary));
    return inserted.first->second;
}

// ============================================================================
// Forward declarations for console commands
// ============================================================================

static void Cmd_Identity();
static void Cmd_Aliases();
static void Cmd_Modules();
static void Cmd_VirtualOrder();
static void Cmd_Why(const std::string& plugin);

// New diagnostics-related console helpers
static void Cmd_DiagSummary();
static void Cmd_DiagSlots();
static void Cmd_DiagMappings();
static void Cmd_DiagEvents();
static void Cmd_DiagTrace(uint32_t formID);

// ============================================================================
// Initialization / Finalization
// ============================================================================

void Diagnostics_Initialize()
{
    g_events.clear();
    g_pluginSummaries.clear();
    g_slotConfigIssues.clear();
    g_mappingIssues.clear();
    g_formIDTraces.clear();

    DX("[Diagnostics] Initialized.");
    logf("[Diagnostics] Initialized.");
}

void Diagnostics_Finalize()
{
    // Dump diagnostics to file at the end of initialization
    Diagnostics_DumpToFile();
    DX("[Diagnostics] Finalized (diagnostics written).");
    logf("[Diagnostics] Finalized (diagnostics written).");
}

// ============================================================================
// Command dispatcher
// ============================================================================

void Diagnostics_HandleCommand(const std::string& cmdLine)
{
    std::stringstream ss(cmdLine);
    std::string cmd;
    ss >> cmd;

    if (cmd == "mx")
    {
        std::string sub;
        ss >> sub;

        if (sub == "identity") {
            Cmd_Identity();
        }
        else if (sub == "aliases") {
            Cmd_Aliases();
        }
        else if (sub == "modules") {
            Cmd_Modules();
        }
        else if (sub == "virtualorder") {
            Cmd_VirtualOrder();
        }
        else if (sub == "why") {
            std::string plugin;
            ss >> plugin;
            if (!plugin.empty())
                Cmd_Why(plugin);
            else
                DX("Usage: mx why <plugin>");
        }
        else if (sub == "diag") {
            std::string diagSub;
            ss >> diagSub;

            if (diagSub == "summary") {
                Cmd_DiagSummary();
            }
            else if (diagSub == "slots") {
                Cmd_DiagSlots();
            }
            else if (diagSub == "mappings") {
                Cmd_DiagMappings();
            }
            else if (diagSub == "events") {
                Cmd_DiagEvents();
            }
            else if (diagSub == "trace") {
                std::string formStr;
                ss >> formStr;
                if (formStr.empty()) {
                    DX("Usage: mx diag trace <hexFormID>");
                }
                else {
                    uint32_t formID = 0;
                    std::stringstream fs;
                    fs << std::hex << formStr;
                    fs >> formID;
                    Cmd_DiagTrace(formID);
                }
            }
            else {
                DX("mx diag commands:");
                DX("  mx diag summary");
                DX("  mx diag slots");
                DX("  mx diag mappings");
                DX("  mx diag events");
                DX("  mx diag trace <hexFormID>");
            }
        }
        else {
            DX("mx commands:");
            DX("  mx identity");
            DX("  mx aliases");
            DX("  mx modules");
            DX("  mx virtualorder");
            DX("  mx why <plugin>");
            DX("  mx diag summary");
            DX("  mx diag slots");
            DX("  mx diag mappings");
            DX("  mx diag events");
            DX("  mx diag trace <hexFormID>");
        }
    }
}
// ============================================================================
// Command implementations (existing behaviour)
// ============================================================================

static void Cmd_Identity()
{
    DX("=== Identity Map ===");

    for (auto& kv : g_pluginAliasMap)
    {
        const std::string& original = kv.first;
        const std::string& dummy = kv.second;

        if (IsSystemDependentCall(original.c_str())) {
            DX(original + " → SYSTEM (DLL reference)");
        }
        else {
            DX(original + " → " + dummy);
        }
    }
}

static void Cmd_Aliases()
{
    DX("=== Alias Mappings ===");

    for (auto& kv : g_pluginAliasMap)
    {
        DX(kv.first + " → " + kv.second);
    }
}

static void Cmd_Modules()
{
    DX("=== Slot Modules ===");

    SlotDescriptor slot{};
    if (!load_slot_config(slot)) {
        DX("ERROR: Could not load slot.cfg");
        return;
    }

    {
        std::stringstream ss;
        ss << std::hex << std::uppercase << static_cast<uint32_t>(slot.fileIndex);
        DX("FileIndex: 0x" + ss.str());
    }

    DX("Modules:");
    for (auto& m : slot.modules)
    {
        DX("  " + m.name +
            " (ESL=" + std::string(m.isESL ? "YES" : "NO") +
            ", eslSlot=" + std::to_string(m.eslSlot) + ")");
    }
}

static void Cmd_VirtualOrder()
{
    DX("=== Writing virtual_loadorder.txt ===");

    std::ofstream out("Data\\F4SE\\Plugins\\Multiplexer\\virtual_loadorder.txt");
    if (!out.is_open()) {
        DX("ERROR: Could not write virtual_loadorder.txt");
        return;
    }

    for (auto& kv : g_pluginAliasMap)
    {
        const std::string& original = kv.first;
        const std::string& dummy = kv.second;

        if (IsSystemDependentCall(original.c_str())) {
            out << original << " → SYSTEM\n";
        }
        else {
            out << original << " → " << dummy << "\n";
        }
    }

    DX("virtual_loadorder.txt written.");
}

static void Cmd_Why(const std::string& plugin)
{
    DX("=== Why: " + plugin + " ===");

    bool isSystem = IsSystemDependentCall(plugin.c_str());
    auto it = g_pluginAliasMap.find(plugin);

    if (isSystem) {
        DX(plugin + " → SYSTEM");
        DX("Reason:");
        DX("  - DLL reference detected");
        return;
    }

    if (it != g_pluginAliasMap.end()) {
        DX(plugin + " → MULTIPLEXED");
        DX("Reason:");
        DX("  - Alias mapping found in slot.cfg");
        DX("  - No DLL references detected");
        return;
    }

    DX(plugin + " → UNKNOWN");
    DX("Reason:");
    DX("  - Not in alias map");
    DX("  - Not system-dependent");
}

// ============================================================================
// Command implementations (new diagnostics tooling)
// ============================================================================

static void Cmd_DiagSummary()
{
    DX("=== Diagnostics: Plugin Summary ===");

    if (g_pluginSummaries.empty()) {
        DX("No plugin diagnostics recorded.");
        return;
    }

    for (auto& kv : g_pluginSummaries)
    {
        const auto& s = kv.second;
        std::stringstream line;
        line << s.pluginName
            << ": scanned=" << s.recordsScanned
            << " injected=" << s.recordsInjected
            << " skipped=" << s.recordsSkipped
            << " lvliRemaps=" << s.lvliRemaps;
        DX(line.str());
    }
}

static void Cmd_DiagSlots()
{
    DX("=== Diagnostics: Slot Config Issues ===");
    if (g_slotConfigIssues.empty()) {
        DX("No slot.cfg issues detected.");
        return;
    }
    for (auto& msg : g_slotConfigIssues)
        DX("  " + msg);
}

static void Cmd_DiagMappings()
{
    DX("=== Diagnostics: Mapping Issues ===");
    if (g_mappingIssues.empty()) {
        DX("No mapping issues detected.");
        return;
    }
    for (auto& msg : g_mappingIssues)
        DX("  " + msg);
}

static void Cmd_DiagEvents()
{
    DX("=== Diagnostics: Events ===");
    if (g_events.empty()) {
        DX("No diagnostics events recorded.");
        return;
    }

    for (auto& ev : g_events)
    {
        std::string typeStr;
        switch (ev.type)
        {
        case DiagnosticsEventType::Info:            typeStr = "INFO"; break;
        case DiagnosticsEventType::Warning:         typeStr = "WARN"; break;
        case DiagnosticsEventType::Error:           typeStr = "ERROR"; break;
        case DiagnosticsEventType::Remap:           typeStr = "REMAP"; break;
        case DiagnosticsEventType::Injection:       typeStr = "INJECT"; break;
        case DiagnosticsEventType::Scan:            typeStr = "SCAN"; break;
        case DiagnosticsEventType::MappingIssue:    typeStr = "MAPISSUE"; break;
        case DiagnosticsEventType::SlotConfigIssue: typeStr = "SLOTISSUE"; break;
        case DiagnosticsEventType::FormIDTrace:     typeStr = "FORMID"; break;
        }

        DX("[" + typeStr + "] " + ev.message);
    }
}

static void Cmd_DiagTrace(uint32_t formID)
{
    std::stringstream hex;
    hex << std::hex << std::uppercase << formID;

    DX("=== Diagnostics: FormID Trace 0x" + hex.str() + " ===");
    FormIDTraceResult res = Diagnostics_QueryFormID(formID);
    if (!res.found) {
        DX("No trace found for this FormID.");
        return;
    }

    std::stringstream orig, virt, local;
    orig << std::hex << std::uppercase << res.originalFormID;
    virt << std::hex << std::uppercase << res.virtualFormID;
    local << std::hex << std::uppercase << res.localKey;

    DX("Plugin: " + res.pluginName);
    DX("Original: 0x" + orig.str());
    DX("LocalKey: 0x" + local.str());
    DX("Virtual: 0x" + virt.str());
    DX("ESL: " + std::string(res.isESL ? "YES" : "NO"));
    DX("DummySlot: " + res.dummySlot);
    DX("Reason: " + res.reason);
}

// ============================================================================
// Safety Validator
// ============================================================================

void Diagnostics_RunValidator()
{
    DX("[Validator] Running safety checks...");
    logf("[Validator] Running safety checks...");

    // Example: warn if a system-dependent plugin is multiplexed
    for (auto& kv : g_pluginAliasMap)
    {
        const std::string& original = kv.first;
        const std::string& dummy = kv.second;

        if (IsSystemDependentCall(original.c_str())) {
            std::string msg = "[WARNING] System plugin '" + original +
                "' is multiplexed! This may cause breakage.";
            DX(msg);
            logf("%s", msg.c_str());
            Diagnostics_RecordSlotConfigIssue(msg);
        }
    }

    DX("[Validator] Completed.");
    logf("[Validator] Completed.");
}

// ============================================================================
// Event Recording API
// ============================================================================

void Diagnostics_RecordEvent(DiagnosticsEventType type, const std::string& message)
{
    DiagnosticsEvent ev;
    ev.type = type;
    ev.message = message;
    g_events.push_back(std::move(ev));
}

void Diagnostics_RecordPluginScan(const std::string& pluginName)
{
    auto& s = GetOrCreatePluginSummary(pluginName);
    ++s.recordsScanned;
}

void Diagnostics_RecordPluginInjection(const std::string& pluginName)
{
    auto& s = GetOrCreatePluginSummary(pluginName);
    ++s.recordsInjected;
}

void Diagnostics_RecordPluginSkip(const std::string& pluginName)
{
    auto& s = GetOrCreatePluginSummary(pluginName);
    ++s.recordsSkipped;
}

void Diagnostics_RecordPluginLVLIRemap(const std::string& pluginName)
{
    auto& s = GetOrCreatePluginSummary(pluginName);
    ++s.lvliRemaps;
}

void Diagnostics_RecordSlotConfigIssue(const std::string& message)
{
    g_slotConfigIssues.push_back(message);
    Diagnostics_RecordEvent(DiagnosticsEventType::SlotConfigIssue, message);
}

void Diagnostics_RecordMappingIssue(const std::string& message)
{
    g_mappingIssues.push_back(message);
    Diagnostics_RecordEvent(DiagnosticsEventType::MappingIssue, message);
}

void Diagnostics_RecordFormIDTrace(
    const std::string& pluginName,
    uint32_t originalFormID,
    uint32_t localKey,
    uint32_t virtualFormID,
    bool isESL,
    const std::string& dummySlot,
    const std::string& reason
)
{
    FormIDTraceResult res;
    res.found = true;
    res.pluginName = pluginName;
    res.originalFormID = originalFormID;
    res.localKey = localKey;
    res.virtualFormID = virtualFormID;
    res.isESL = isESL;
    res.dummySlot = dummySlot;
    res.reason = reason;

    g_formIDTraces[originalFormID] = res;

    std::stringstream msg;
    msg << pluginName << ": 0x" << std::hex << std::uppercase << originalFormID
        << " -> 0x" << virtualFormID << " (" << reason << ")";
    Diagnostics_RecordEvent(DiagnosticsEventType::FormIDTrace, msg.str());
}

// ============================================================================
// Query API
// ============================================================================

FormIDTraceResult Diagnostics_QueryFormID(uint32_t formID)
{
    auto it = g_formIDTraces.find(formID);
    if (it != g_formIDTraces.end())
        return it->second;

    FormIDTraceResult res;
    res.found = false;
    return res;
}

// ============================================================================
// Dump API
// ============================================================================

void Diagnostics_DumpToFile()
{
    const char* path = "Data\\F4SE\\Plugins\\Multiplexer\\diagnostics.txt";
    std::ofstream out(path);
    if (!out.is_open()) {
        logf("Diagnostics_DumpToFile: Could not open diagnostics.txt for writing.");
        return;
    }

    out << "=== Multiplexer Diagnostics ===\n\n";

    // Plugin summaries
    out << "[Plugin Summaries]\n";
    if (g_pluginSummaries.empty()) {
        out << "  (none)\n";
    }
    else {
        for (auto& kv : g_pluginSummaries)
        {
            const auto& s = kv.second;
            out << "  " << s.pluginName
                << ": scanned=" << s.recordsScanned
                << " injected=" << s.recordsInjected
                << " skipped=" << s.recordsSkipped
                << " lvliRemaps=" << s.lvliRemaps
                << "\n";
        }
    }
    out << "\n";

    // Slot config issues
    out << "[Slot Config Issues]\n";
    if (g_slotConfigIssues.empty()) {
        out << "  (none)\n";
    }
    else {
        for (auto& msg : g_slotConfigIssues)
            out << "  " << msg << "\n";
    }
    out << "\n";

    // Mapping issues
    out << "[Mapping Issues]\n";
    if (g_mappingIssues.empty()) {
        out << "  (none)\n";
    }
    else {
        for (auto& msg : g_mappingIssues)
            out << "  " << msg << "\n";
    }
    out << "\n";

    // Events
    out << "[Events]\n";
    if (g_events.empty()) {
        out << "  (none)\n";
    }
    else {
        for (auto& ev : g_events)
        {
            const char* typeStr = "UNKNOWN";
            switch (ev.type)
            {
            case DiagnosticsEventType::Info:            typeStr = "INFO"; break;
            case DiagnosticsEventType::Warning:         typeStr = "WARN"; break;
            case DiagnosticsEventType::Error:           typeStr = "ERROR"; break;
            case DiagnosticsEventType::Remap:           typeStr = "REMAP"; break;
            case DiagnosticsEventType::Injection:       typeStr = "INJECT"; break;
            case DiagnosticsEventType::Scan:            typeStr = "SCAN"; break;
            case DiagnosticsEventType::MappingIssue:    typeStr = "MAPISSUE"; break;
            case DiagnosticsEventType::SlotConfigIssue: typeStr = "SLOTISSUE"; break;
            case DiagnosticsEventType::FormIDTrace:     typeStr = "FORMID"; break;
            }
            out << "  [" << typeStr << "] " << ev.message << "\n";
        }
    }
    out << "\n";

    // FormID traces
    out << "[FormID Traces]\n";
    if (g_formIDTraces.empty()) {
        out << "  (none)\n";
    }
    else {
        for (auto& kv : g_formIDTraces)
        {
            const auto& t = kv.second;
            std::stringstream orig, virt, local;
            orig << std::hex << std::uppercase << t.originalFormID;
            virt << std::hex << std::uppercase << t.virtualFormID;
            local << std::hex << std::uppercase << t.localKey;

            out << "  Plugin: " << t.pluginName << "\n";
            out << "    Original: 0x" << orig.str() << "\n";
            out << "    LocalKey: 0x" << local.str() << "\n";
            out << "    Virtual:  0x" << virt.str() << "\n";
            out << "    ESL:      " << (t.isESL ? "YES" : "NO") << "\n";
            out << "    DummySlot:" << t.dummySlot << "\n";
            out << "    Reason:   " << t.reason << "\n";
        }
    }
    out << "\n";

    out.close();
    logf("Diagnostics_DumpToFile: diagnostics.txt written.");
}
