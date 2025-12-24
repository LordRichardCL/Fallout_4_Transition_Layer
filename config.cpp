#include "pch.h"
#include "config.hpp"
#include "log.hpp"

#include <windows.h>
#include <string>

// Global configuration values
bool g_debugLogging = false;
bool g_scanOnStartup = true;
std::string g_targetModule;
std::string g_csvPath;

// ESL debug flag
bool g_eslDebug = false;

// Console toggle
bool g_showConsole = false;

void LoadConfig()
{
    const std::string iniPath = "Data\\F4SE\\Plugins\\Multiplexer\\multiplexer.ini";

    // ------------------------------------------------------------
    // Read booleans
    // ------------------------------------------------------------
    g_debugLogging = GetPrivateProfileIntA("General", "bEnableDebugLogging", 0, iniPath.c_str()) != 0;
    g_scanOnStartup = GetPrivateProfileIntA("General", "bScanOnStartup", 1, iniPath.c_str()) != 0;
    g_eslDebug = GetPrivateProfileIntA("General", "bEnableESLDebug", 0, iniPath.c_str()) != 0;
    g_showConsole = GetPrivateProfileIntA("General", "bShowConsole", 0, iniPath.c_str()) != 0;

    // ------------------------------------------------------------
    // Read strings
    // ------------------------------------------------------------
    char buf[512] = {};

    // Target module
    GetPrivateProfileStringA("General", "sTargetModule", "", buf, sizeof(buf), iniPath.c_str());
    g_targetModule = buf;

    // CSV path
    GetPrivateProfileStringA("General", "sCSVPath", "", buf, sizeof(buf), iniPath.c_str());
    g_csvPath = buf;

    // ------------------------------------------------------------
    // Idiot-proofing: If CSV path is empty, auto-fill default
    // ------------------------------------------------------------
    if (g_csvPath.empty()) {
        g_csvPath = "Data\\F4SE\\Plugins\\Multiplexer\\loadorder_mapped_filtered_clean.csv";
        logf("No CSV path specified in INI - using default: %s", g_csvPath.c_str());
    }

    // ------------------------------------------------------------
    // Log final configuration
    // ------------------------------------------------------------
    logf("Config loaded: Debug=%s, ScanOnStartup=%s, ESLDebug=%s, ShowConsole=%s",
        g_debugLogging ? "YES" : "NO",
        g_scanOnStartup ? "YES" : "NO",
        g_eslDebug ? "YES" : "NO",
        g_showConsole ? "YES" : "NO");

    logf("Configuration loaded:");
    logf("  Debug Logging: %s", g_debugLogging ? "ENABLED" : "DISABLED");
    logf("  ESL Debug: %s", g_eslDebug ? "ENABLED" : "DISABLED");
    logf("  Scan On Startup: %s", g_scanOnStartup ? "YES" : "NO");
    logf("  Target Module: '%s'", g_targetModule.empty() ? "<none>" : g_targetModule.c_str());
    logf("  CSV Path: '%s'", g_csvPath.c_str());
}
