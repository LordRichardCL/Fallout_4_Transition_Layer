#include "pch.h"
#include "config.hpp"
#include "log.hpp"

#include <windows.h>
#include <string>
#include <unordered_set>
#include <fstream>
#include <sstream>
#include <vector>

// ------------------------------------------------------------
// Global configuration values
// ------------------------------------------------------------
bool g_debugLogging = false;
bool g_scanOnStartup = true;
bool g_eslDebug = false;
bool g_showConsole = false;
bool g_enableRuntimeRewrite = true;
bool g_writeSkippedModules = true;

std::string g_targetModule;
std::string g_csvPath;

// ------------------------------------------------------------
// Protected plugin whitelist storage
// ------------------------------------------------------------
static std::unordered_set<std::string> g_protectedPlugins;

// Forward declaration
void LoadProtectedPluginWhitelist();

// ------------------------------------------------------------
// LoadConfig() - reads multiplexer.ini
// ------------------------------------------------------------
void LoadConfig()
{
    const std::string iniPath = "Data\\F4SE\\Plugins\\Multiplexer\\multiplexer.ini";

    // Read booleans
    g_debugLogging = GetPrivateProfileIntA("General", "bEnableDebugLogging", 0, iniPath.c_str()) != 0;
    g_scanOnStartup = GetPrivateProfileIntA("General", "bScanOnStartup", 1, iniPath.c_str()) != 0;
    g_eslDebug = GetPrivateProfileIntA("General", "bEnableESLDebug", 0, iniPath.c_str()) != 0;
    g_showConsole = GetPrivateProfileIntA("General", "bShowConsole", 0, iniPath.c_str()) != 0;

    g_enableRuntimeRewrite =
        GetPrivateProfileIntA("General", "bEnableRuntimeRewrite", 1, iniPath.c_str()) != 0;

    g_writeSkippedModules =
        GetPrivateProfileIntA("General", "bWriteSkippedModules", 1, iniPath.c_str()) != 0;

    // Read strings
    char buf[512] = {};

    GetPrivateProfileStringA("General", "sTargetModule", "", buf, sizeof(buf), iniPath.c_str());
    g_targetModule = buf;

    GetPrivateProfileStringA("General", "sCSVPath", "", buf, sizeof(buf), iniPath.c_str());
    g_csvPath = buf;

    // Idiot-proofing: If CSV path is empty, auto-fill default
    if (g_csvPath.empty()) {
        g_csvPath = "Data\\F4SE\\Plugins\\Multiplexer\\loadorder_mapped_filtered_clean.csv";
        logf("No CSV path specified in INI - using default: %s", g_csvPath.c_str());
    }

    // Log final configuration
    logf("Config loaded: Debug=%s, ScanOnStartup=%s, ESLDebug=%s, ShowConsole=%s, RuntimeRewrite=%s",
        g_debugLogging ? "YES" : "NO",
        g_scanOnStartup ? "YES" : "NO",
        g_eslDebug ? "YES" : "NO",
        g_showConsole ? "YES" : "NO",
        g_enableRuntimeRewrite ? "YES" : "NO");

    logf("Configuration loaded:");
    logf("  Debug Logging: %s", g_debugLogging ? "ENABLED" : "DISABLED");
    logf("  ESL Debug: %s", g_eslDebug ? "ENABLED" : "DISABLED");
    logf("  Scan On Startup: %s", g_scanOnStartup ? "YES" : "NO");
    logf("  Target Module: '%s'", g_targetModule.empty() ? "<none>" : g_targetModule.c_str());
    logf("  CSV Path: '%s'", g_csvPath.c_str());
    logf("  Runtime Rewrite: %s", g_enableRuntimeRewrite ? "ENABLED" : "DISABLED");
    logf("  Write Skipped Modules: %s", g_writeSkippedModules ? "ENABLED" : "DISABLED");

    // Load protected plugin whitelist
    LoadProtectedPluginWhitelist();
}

// ------------------------------------------------------------
// Minimal JSON parser for protected_plugins.json
// ------------------------------------------------------------

static std::string ReadFileToString(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
        return "";

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

static std::vector<std::string> ExtractJsonKeys(const std::string& json)
{
    std::vector<std::string> keys;

    size_t pos = 0;
    while (true)
    {
        size_t start = json.find('"', pos);
        if (start == std::string::npos)
            break;

        size_t end = json.find('"', start + 1);
        if (end == std::string::npos)
            break;

        std::string key = json.substr(start + 1, end - start - 1);
        pos = end + 1;

        if (key == "_comment")
            continue;

        if (key.empty())
            continue;

        if (key == "protected" || key == "injectable" || key == "reason")
            continue;

        if (key.find('.') != std::string::npos)
            keys.push_back(key);
    }

    return keys;
}

// ------------------------------------------------------------
// LoadProtectedPluginWhitelist()
// ------------------------------------------------------------
void LoadProtectedPluginWhitelist()
{
    const std::string path =
        "Data\\F4SE\\Plugins\\Multiplexer\\extern\\Whitelist\\protected_plugins.json";

    logf("Loading protected plugin whitelist from: %s", path.c_str());

    std::string json = ReadFileToString(path);
    if (json.empty())
    {
        logf("WARNING: Could not read protected_plugins.json or file is empty.");
        return;
    }

    std::vector<std::string> keys = ExtractJsonKeys(json);

    g_protectedPlugins.clear();
    for (std::size_t i = 0; i < keys.size(); ++i)
        g_protectedPlugins.insert(keys[i]);

    logf("Protected plugin whitelist loaded: %zu entries", g_protectedPlugins.size());
}

// ------------------------------------------------------------
// IsPluginProtected()
// ------------------------------------------------------------
bool IsPluginProtected(const std::string& pluginName)
{
    return g_protectedPlugins.find(pluginName) != g_protectedPlugins.end();
}
