#pragma once

#include <string>

// Global configuration flags
extern bool g_debugLogging;
extern bool g_scanOnStartup;
extern std::string g_targetModule;
extern std::string g_csvPath;

// NEW: ESL-specific debug logging
extern bool g_eslDebug;

// NEW: Console visibility toggle
extern bool g_showConsole;

// Load configuration from INI
void LoadConfig();
