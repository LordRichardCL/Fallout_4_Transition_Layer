#include "pch.h"
#include "identity.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_set>
#include <algorithm>

static std::unordered_set<std::string> g_systemDependentPlugins;

// Convert to lowercase
static std::string ToLower(const std::string& s)
{
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return out;
}

// Efficient streaming search (no full file load)
static bool StreamSearchFor(const std::string& filePath, const std::string& needle)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open())
        return false;

    const size_t chunkSize = 4096;
    std::string buffer(chunkSize + needle.size(), '\0');

    while (file.read(&buffer[0], chunkSize) || file.gcount() > 0)
    {
        size_t bytesRead = file.gcount();
        size_t searchSize = bytesRead + needle.size();

        if (searchSize > buffer.size())
            searchSize = buffer.size();

        if (std::search(buffer.begin(),
            buffer.begin() + searchSize,
            needle.begin(),
            needle.end(),
            [](char a, char b) { return std::tolower(a) == std::tolower(b); })
            != buffer.begin() + searchSize)
        {
            return true;
        }

        // Move last few bytes to front for overlap
        std::memmove(&buffer[0], &buffer[bytesRead], needle.size());
    }

    return false;
}

static void ScanDLLsForPluginReferences()
{
    std::string dllFolder = "Data\\F4SE\\Plugins";

    // Collect plugin names from Data folder (not recursive)
    std::vector<std::string> pluginNames;
    for (auto& entry : std::filesystem::directory_iterator("Data"))
    {
        if (!entry.is_regular_file())
            continue;

        auto path = entry.path().string();
        if (path.size() < 4)
            continue;

        std::string ext = path.substr(path.size() - 4);
        if (ext == ".esp" || ext == ".esl")
        {
            pluginNames.push_back(entry.path().filename().string());
        }
    }

    // Scan DLLs for references to plugin names
    for (auto& entry : std::filesystem::directory_iterator(dllFolder))
    {
        if (!entry.is_regular_file())
            continue;

        auto dllPath = entry.path().string();
        if (dllPath.size() < 4 || dllPath.substr(dllPath.size() - 4) != ".dll")
            continue;

        for (auto& plugin : pluginNames)
        {
            if (StreamSearchFor(dllPath, plugin))
            {
                g_systemDependentPlugins.insert(ToLower(plugin));
            }
        }
    }
}

void Identity_Initialize()
{
    g_systemDependentPlugins.clear();

    // Hardcoded known system mods
    g_systemDependentPlugins.insert(ToLower("Rusty Face Fix.esp"));
    g_systemDependentPlugins.insert(ToLower("LooksMenu.esp"));

    // Auto-detect DLL references
    ScanDLLsForPluginReferences();
}

bool IsSystemDependentCall(const char* pluginName)
{
    if (!pluginName)
        return false;

    std::string lower = ToLower(pluginName);
    return g_systemDependentPlugins.count(lower) > 0;
}
