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

// Forward declarations
static void Cmd_Identity();
static void Cmd_Aliases();
static void Cmd_Modules();
static void Cmd_VirtualOrder();
static void Cmd_Why(const std::string& plugin);

// ============================================================================
// Initialization
// ============================================================================

void Diagnostics_Initialize()
{
    DX("[Diagnostics] Initialized.");
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
        else {
            DX("mx commands:");
            DX("  mx identity");
            DX("  mx aliases");
            DX("  mx modules");
            DX("  mx virtualorder");
            DX("  mx why <plugin>");
        }
    }
}

// ============================================================================
// Command implementations
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

    DX("FileIndex: 0x" + std::to_string(slot.fileIndex));
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
// Safety Validator
// ============================================================================

void Diagnostics_RunValidator()
{
    DX("[Validator] Running safety checks...");

    for (auto& kv : g_pluginAliasMap)
    {
        const std::string& original = kv.first;
        const std::string& dummy = kv.second;

        if (IsSystemDependentCall(original.c_str())) {
            DX("[WARNING] System plugin '" + original +
                "' is multiplexed! This may cause breakage.");
        }
    }

    DX("[Validator] Completed.");
}
