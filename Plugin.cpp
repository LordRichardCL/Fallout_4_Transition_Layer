#include "pch.h"
#include "log.hpp"
#include "mapping.hpp"
#include "injector.hpp"
#include "config.hpp"
#include "csv_loader.hpp"
#include "scanner.hpp"
#include "relocations.hpp"
#include "identity.h"
#include "diagnostics.h"

#include <f4se/PluginAPI.h>
#include "F4SE_Types.h"
#include <f4se_common/Relocation.h>
#include <f4se_common/SafeWrite.h>

#include <windows.h>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <cctype>

// ============================================================================
// Globals
// ============================================================================

// From config.cpp
extern bool g_debugLogging;
extern bool g_scanOnStartup;
extern std::string g_targetModule;
extern std::string g_csvPath;
extern bool g_eslDebug;
extern bool g_showConsole;

F4SEInterface* g_f4se = nullptr;

// Alias map: original plugin name -> DummySlotXXX.esp
// (non-static so diagnostics/identity can see it)
std::unordered_map<std::string, std::string> g_pluginAliasMap;

// Console state (non-static so diagnostics can see it)
bool g_consoleActive = false;

// Simple trampoline wrapper (using SKSE/F4SE-style branch write)
class SimpleTrampoline
{
public:
    explicit SimpleTrampoline(std::size_t size)
    {
        m_buffer = (std::uint8_t*)VirtualAlloc(
            nullptr,
            size,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_EXECUTE_READWRITE
        );
        m_capacity = size;
        m_writePtr = m_buffer;
    }

    ~SimpleTrampoline()
    {
        // Typically never freed in plugins.
        // if (m_buffer) VirtualFree(m_buffer, 0, MEM_RELEASE);
    }

    // Writes a 5-byte CALL at src, redirecting to dst.
    // Returns the original function stub pointer.
    void* Write5CallEx(std::uintptr_t src, std::uintptr_t dst)
    {
        if (!src || !dst)
            return nullptr;

        // Backup original bytes
        std::uint8_t original[5];
        std::memcpy(original, (void*)src, 5);

        // Allocate space in trampoline for original bytes + jump back
        if (!m_buffer || (m_writePtr + 5 + 5 > m_buffer + m_capacity)) {
            return nullptr;
        }

        std::uint8_t* stub = m_writePtr;
        m_writePtr += 5 + 5;

        // Copy original bytes into stub
        std::memcpy(stub, original, 5);

        // Add jump back to src+5
        std::uintptr_t returnAddr = src + 5;
        std::uint8_t* jmpBack = stub + 5;
        jmpBack[0] = 0xE9;
        *(std::int32_t*)(jmpBack + 1) = (std::int32_t)(returnAddr - (std::uintptr_t)(jmpBack + 5));

        // Now patch src with CALL dst
        SafeWrite8(src, 0xE8);
        std::int32_t rel = (std::int32_t)(dst - (src + 5));
        SafeWrite32(src + 1, rel);

        return stub;
    }

private:
    std::uint8_t* m_buffer = nullptr;
    std::uint8_t* m_writePtr = nullptr;
    std::size_t   m_capacity = 0;
};

static SimpleTrampoline* g_trampoline = nullptr;

// ============================================================================
// Console integration
// ============================================================================

static void InitializeConsoleIfEnabled()
{
    if (!g_showConsole)
        return;

    if (g_consoleActive)
        return;

    if (!AllocConsole()) {
        logf("Console: AllocConsole failed, falling back to file logging only.");
        return;
    }

    FILE* fpOut = nullptr;
    FILE* fpErr = nullptr;

    freopen_s(&fpOut, "CONOUT$", "w", stdout);
    freopen_s(&fpErr, "CONOUT$", "w", stderr);

    g_consoleActive = true;

    std::cout << "=== aSWMultiplexer Console Initialized ===" << std::endl;
    std::cout << "[Multiplexer] Console logging ENABLED (bShowConsole=1)." << std::endl;
    std::cout << std::endl;
}

#define CONSOLEF(msg)                       \
    do {                                    \
        if (g_consoleActive) {              \
            std::cout << msg << std::endl;  \
        }                                   \
    } while (0)

// ============================================================================
// Alias loader
// ============================================================================

static void LoadAliasesFromSlotCfg()
{
    g_pluginAliasMap.clear();

    const std::string cfgPath = "Data\\F4SE\\Plugins\\Multiplexer\\slot.cfg";

    std::ifstream in(cfgPath);
    if (!in) {
        logf("Alias loader: slot.cfg not found at '%s' — no aliases loaded.", cfgPath.c_str());
        return;
    }

    logf("Alias loader: Reading aliases from '%s'.", cfgPath.c_str());

    std::string line;
    bool inAliasesSection = false;
    std::size_t aliasCount = 0;

    auto trim = [](std::string& s) {
        auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };

        // left
        s.erase(s.begin(),
            std::find_if(s.begin(), s.end(), [&](unsigned char c) { return !is_space(c); }));
        // right
        s.erase(
            std::find_if(s.rbegin(), s.rend(), [&](unsigned char c) { return !is_space(c); }).base(),
            s.end());
        };

    while (std::getline(in, line)) {
        trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#')
            continue;

        if (line[0] == '[') {
            if (_stricmp(line.c_str(), "[Aliases]") == 0) {
                inAliasesSection = true;
            }
            else {
                inAliasesSection = false;
            }
            continue;
        }

        if (!inAliasesSection)
            continue;

        std::size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;

        std::string original = line.substr(0, eq);
        std::string dummy = line.substr(eq + 1);

        trim(original);
        trim(dummy);

        if (original.empty() || dummy.empty())
            continue;

        g_pluginAliasMap[original] = dummy;
        aliasCount++;
    }

    logf("Alias loader: Loaded %zu alias mappings from slot.cfg.", aliasCount);

    if (g_consoleActive && aliasCount > 0) {
        CONSOLEF("[Aliases] Loaded " + std::to_string(aliasCount) + " alias mappings from slot.cfg:");
        for (const auto& kv : g_pluginAliasMap) {
            CONSOLEF("  " + kv.first + " -> " + kv.second);
        }
    }
}

// ============================================================================
// Redirection helpers
// ============================================================================

static const char* ResolvePluginAlias(const char* name)
{
    if (!name)
        return nullptr;

    auto it = g_pluginAliasMap.find(name);
    if (it == g_pluginAliasMap.end())
        return name; // no alias, return original

    const std::string& mapped = it->second;

    if (g_debugLogging) {
        logf("Alias: '%s' -> '%s'", name, mapped.c_str());
    }

    return mapped.c_str();
}

// ============================================================================
// Hooks
// ============================================================================

// Function pointer types are defined in relocations.hpp:
//   using LookupModByName_t   = void* (*)(const char*);
//   using GetLoadedModIndex_t = UInt8 (*)(const char*);

static LookupModByName_t   s_originalLookupModByName = nullptr;
static GetLoadedModIndex_t s_originalGetLoadedModIndex = nullptr;

static void* Hook_LookupModByName(const char* name)
{
    // System-dependent plugins must retain their original identity
    if (IsSystemDependentCall(name)) {
        if (g_debugLogging) {
            logf("Identity: System-dependent plugin '%s' bypassed alias redirection in LookupModByName.", name ? name : "<null>");
        }
        return s_originalLookupModByName(name);
    }

    const char* aliased = ResolvePluginAlias(name);
    return s_originalLookupModByName(aliased);
}

static UInt8 Hook_GetLoadedModIndex(const char* name)
{
    if (IsSystemDependentCall(name)) {
        if (g_debugLogging) {
            logf("Identity: System-dependent plugin '%s' bypassed alias redirection in GetLoadedModIndex.", name ? name : "<null>");
        }
        return s_originalGetLoadedModIndex(name);
    }

    const char* aliased = ResolvePluginAlias(name);
    return s_originalGetLoadedModIndex(aliased);
}

// Install hooks using RelocAddr offsets
static bool InstallRedirectionHooks()
{
    if (!g_trampoline) {
        // Reserve 4 KB for stubs — more than enough for a couple of hooks.
        g_trampoline = new SimpleTrampoline(4096);
    }

    // These addresses come from RelocAddr<> in relocations.hpp
    std::uintptr_t addrLookup = Reloc::LookupModByName.GetUIntPtr();
    std::uintptr_t addrIndex = Reloc::GetLoadedModIndex.GetUIntPtr();

    if (addrLookup == 0 || addrIndex == 0) {
        logf("ERROR: RelocAddr returned 0 — redirection hooks not installed. Check your offsets.");
        return false;
    }

    s_originalLookupModByName = (LookupModByName_t)g_trampoline->Write5CallEx(
        addrLookup,
        (std::uintptr_t)&Hook_LookupModByName
    );

    s_originalGetLoadedModIndex = (GetLoadedModIndex_t)g_trampoline->Write5CallEx(
        addrIndex,
        (std::uintptr_t)&Hook_GetLoadedModIndex
    );

    if (!s_originalLookupModByName || !s_originalGetLoadedModIndex) {
        logf("ERROR: Failed to install redirection hooks.");
        return false;
    }

    logf("Redirection hooks installed successfully.");
    return true;
}

// ============================================================================
// F4SE Plugin Query
// ============================================================================

extern "C" __declspec(dllexport)
bool F4SEPlugin_Query(const F4SEInterface* f4se, PluginInfo* info)
{
    info->infoVersion = PluginInfo::kInfoVersion;
    info->name = "aSWMultiplexer";
    info->version = 1;

    if (f4se->isEditor) {
        logf("Loaded in Creation Kit — aborting.");
        return false;
    }

    g_f4se = const_cast<F4SEInterface*>(f4se);

    logf("F4SEPlugin_Query successful.");
    return true;
}

// ============================================================================
// F4SE Plugin Load
// ============================================================================

extern "C" __declspec(dllexport)
bool F4SEPlugin_Load(const F4SEInterface* f4se)
{
    g_f4se = const_cast<F4SEInterface*>(f4se);

    logf("aSWMultiplexer plugin loaded.");

    //
    // ------------------------------------------------------------
    // FORCE CONSOLE EARLY SO USERS SEE STARTUP BANNER
    // ------------------------------------------------------------
    InitializeConsoleIfEnabled();

    if (g_consoleActive) {
        std::cout << "[aSWMultiplexer] Virtualization layer active" << std::endl;
        std::cout << "[aSWMultiplexer] Alias redirection enabled" << std::endl;
        std::cout << "[aSWMultiplexer] System plugin protection enabled" << std::endl;
        std::cout << std::endl;
    }

    // Also print to loader logs (MO2, Vortex, DebugView)
    OutputDebugStringA("[aSWMultiplexer] Virtualization layer active\n");
    OutputDebugStringA("[aSWMultiplexer] Alias redirection enabled\n");
    OutputDebugStringA("[aSWMultiplexer] System plugin protection enabled\n");

    // ------------------------------------------------------------
    // Load configuration from INI
    // ------------------------------------------------------------
    logf("Loading configuration...");
    LoadConfig();

    logf("Config loaded: Debug=%s, ScanOnStartup=%s, ESLDebug=%s, ShowConsole=%s",
        g_debugLogging ? "YES" : "NO",
        g_scanOnStartup ? "YES" : "NO",
        g_eslDebug ? "YES" : "NO",
        g_showConsole ? "YES" : "NO");

    // Initialize identity layer (system-dependent detection)
    Identity_Initialize();

    // Initialize diagnostics
    Diagnostics_Initialize();

    CONSOLEF("=== aSWMultiplexer Initialization ===");
    CONSOLEF("Configuration loaded:");
    CONSOLEF(std::string("  Debug Logging: ") + (g_debugLogging ? "ENABLED" : "DISABLED"));
    CONSOLEF(std::string("  ESL Debug: ") + (g_eslDebug ? "YES" : "NO"));
    CONSOLEF(std::string("  Scan On Startup: ") + (g_scanOnStartup ? "YES" : "NO"));
    CONSOLEF(std::string("  Target Module: '") + (g_targetModule.empty() ? "<none>" : g_targetModule.c_str()) + "'");
    CONSOLEF(std::string("  CSV Path: '") + g_csvPath + "'");

    // ------------------------------------------------------------
    // Install redirection hooks (before anything asks for mods)
    // ------------------------------------------------------------
    if (!InstallRedirectionHooks()) {
        CONSOLEF("ERROR: Failed to install redirection hooks. Plugin will continue without alias redirection.");
        logf("WARNING: Continuing without alias redirection.");
    }
    else {
        CONSOLEF("Redirection hooks installed successfully.");
    }

    // Run validator once we have identity + aliases (loaded later), but we can at
    // least warn about identity-only issues now.
    Diagnostics_RunValidator(); // first-pass (will warn about system + alias once aliases are loaded too)

    if (!g_scanOnStartup) {
        logf("ScanOnStartup=0 — skipping record scanning.");
        CONSOLEF("ScanOnStartup=0 — skipping record scanning. Initialization complete.");
        return true;
    }

    // ------------------------------------------------------------
    // Load CSV dummy-slot mapping
    // ------------------------------------------------------------
    CONSOLEF("");
    CONSOLEF("[Step 1/4] Loading CSV dummy-slot mapping...");

    std::vector<CSVSlot> csvSlots;

    if (!g_csvPath.empty()) {
        if (!load_csv_slots(g_csvPath, csvSlots)) {
            logf("ERROR: Failed to load CSV slots from '%s'", g_csvPath.c_str());
            CONSOLEF(std::string("ERROR: Failed to load CSV slots from '") + g_csvPath + "'.");
            return false;
        }
        logf("Loaded %zu CSV dummy slot entries.", csvSlots.size());
        CONSOLEF("Loaded " + std::to_string(csvSlots.size()) + " CSV dummy slot entries.");
    }
    else {
        logf("WARNING: No CSV path specified — skipping CSV slot mapping.");
        CONSOLEF("WARNING: No CSV path specified — skipping CSV slot mapping.");
    }

    // ------------------------------------------------------------
    // Load slot configuration
    // ------------------------------------------------------------
    CONSOLEF("");
    CONSOLEF("[Step 2/4] Loading slot configuration...");

    SlotDescriptor slot{};
    if (!load_slot_config(slot)) {
        logf("ERROR: Failed to load slot configuration.");
        CONSOLEF("ERROR: Failed to load slot configuration.");
        return false;
    }

    logf("Loaded slot.cfg: fileIndex=0x%02X, modules=%zu", slot.fileIndex, slot.modules.size());
    CONSOLEF("Slot configuration loaded. Modules in slot: " + std::to_string(slot.modules.size()));

    // ------------------------------------------------------------
    // Load aliases from slot.cfg
    // ------------------------------------------------------------
    CONSOLEF("");
    CONSOLEF("[Aliases] Loading plugin alias mappings from slot.cfg...");
    LoadAliasesFromSlotCfg();

    // Now that aliases are loaded, re-run validator to catch system+alias conflicts
    Diagnostics_RunValidator();

    // ------------------------------------------------------------
    // Scan metadata for each module (ESL detection, FE slot, etc.)
    // ------------------------------------------------------------
    CONSOLEF("");
    CONSOLEF("[Step 3/4] Scanning module metadata (ESL, FE slots, etc.)...");

    for (auto& m : slot.modules) {
        if (!scan_plugin_metadata(m.name, m)) {
            logf("WARNING: Failed to scan metadata for module '%s'", m.name.c_str());
            CONSOLEF(std::string("WARNING: Failed to scan metadata for module '") + m.name + "'.");
        }
        else if (g_eslDebug) {
            logf("Module '%s': ESL=%s, eslSlot=%u",
                m.name.c_str(),
                m.isESL ? "YES" : "NO",
                m.eslSlot);

            CONSOLEF(std::string("Module '") + m.name +
                "': ESL=" + (m.isESL ? "YES" : "NO") +
                ", eslSlot=" + std::to_string(m.eslSlot));
        }
    }

    // ------------------------------------------------------------
    // Build form ID maps
    // ------------------------------------------------------------
    CONSOLEF("");
    CONSOLEF("[Step 4/4] Building form ID maps...");

    if (!build_form_maps(slot)) {
        logf("ERROR: Failed to build form maps.");
        CONSOLEF("ERROR: Failed to build form maps.");
        return false;
    }

    CONSOLEF("Form ID maps built successfully.");

    // ------------------------------------------------------------
    // Inject records using CSV slot mapping
    // ------------------------------------------------------------
    CONSOLEF("");
    CONSOLEF("[Final] Injecting records using CSV slot mapping...");

    if (!inject_records(slot, csvSlots)) {
        logf("ERROR: Record injection failed.");
        CONSOLEF("ERROR: Record injection failed.");
        return false;
    }

    logf("aSWMultiplexer initialization complete.");
    CONSOLEF("Record injection completed successfully.");
    CONSOLEF("");
    CONSOLEF("=== aSWMultiplexer initialization complete. ===");
    CONSOLEF("You can now close this console window if desired.");

    return true;
}
