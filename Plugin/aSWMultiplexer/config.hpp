#pragma once

#include <string>

// ------------------------------------------------------------
// Global configuration flags
// ------------------------------------------------------------
extern bool g_debugLogging;
extern bool g_scanOnStartup;
extern std::string g_targetModule;
extern std::string g_csvPath;

// ESL-specific debug logging
extern bool g_eslDebug;

// Console visibility toggle
extern bool g_showConsole;

// Runtime FormID rewrite toggle (Safety Layer)
extern bool g_enableRuntimeRewrite;

// Write SkippedModules.txt toggle
extern bool g_writeSkippedModules;

// ------------------------------------------------------------
// Load configuration from INI
// ------------------------------------------------------------
void LoadConfig();

// ------------------------------------------------------------
// Protected plugin whitelist API
// ------------------------------------------------------------
void LoadProtectedPluginWhitelist();
bool IsPluginProtected(const std::string& pluginName);
