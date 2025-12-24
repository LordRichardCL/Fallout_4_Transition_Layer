#include "pch.h"
#include "csv_loader.hpp"
#include "log.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

// Trim helper
static inline void trim(std::string& s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
        [](unsigned char c) { return !std::isspace(c); }));

    s.erase(std::find_if(s.rbegin(), s.rend(),
        [](unsigned char c) { return !std::isspace(c); }).base(), s.end());
}

// Split a comma-separated list into vector<string>
static std::vector<std::string> split_mod_list(const std::string& mods)
{
    std::vector<std::string> result;
    std::stringstream ss(mods);
    std::string item;

    while (std::getline(ss, item, ',')) {
        trim(item);
        if (!item.empty())
            result.push_back(item);
    }

    return result;
}

bool load_csv_slots(const std::string& path, std::vector<CSVSlot>& out)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        logf("CSV ERROR: Could not open file '%s'", path.c_str());
        return false;
    }

    std::string line;
    size_t lineNum = 0;

    while (std::getline(file, line)) {
        lineNum++;

        // Skip header row
        if (lineNum == 1)
            continue;

        // Handle BOM on first data line
        if (lineNum == 2 && line.size() >= 3 &&
            (unsigned char)line[0] == 0xEF &&
            (unsigned char)line[1] == 0xBB &&
            (unsigned char)line[2] == 0xBF)
        {
            line = line.substr(3);
        }

        if (line.empty())
            continue;

        std::stringstream ss(line);
        std::string dummy, virt, mods;

        // Extract 3 CSV columns
        if (!std::getline(ss, dummy, ',')) continue;
        if (!std::getline(ss, virt, ',')) continue;
        if (!std::getline(ss, mods, ',')) continue;

        // Remove quotes
        auto strip_quotes = [](std::string& s) {
            trim(s);
            if (!s.empty() && s.front() == '"') s.erase(0, 1);
            if (!s.empty() && s.back() == '"') s.pop_back();
            trim(s);
            };

        strip_quotes(dummy);
        strip_quotes(virt);
        strip_quotes(mods);

        if (dummy.empty() || virt.empty() || mods.empty()) {
            logf("CSV WARNING: Empty field on line %zu", lineNum);
            continue;
        }

        // Parse virtual ID
        uint32_t virtualID = 0;
        try {
            virtualID = std::stoul(virt, nullptr, 10);
        }
        catch (...) {
            logf("CSV ERROR: Invalid Virtual_ID '%s' on line %zu", virt.c_str(), lineNum);
            continue;
        }

        CSVSlot slot;
        slot.dummyPlugin = dummy;
        slot.virtualID = virtualID;
        slot.sourceMods = split_mod_list(mods);

        out.push_back(slot);
    }

    logf("CSV: Loaded %zu dummy slot entries from '%s'", out.size(), path.c_str());
    return true;
}
