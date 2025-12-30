#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Represents one dummy slot row from the CSV:
//   dummyPlugin = "DummySlot001.esp"
//   virtualID   = 1001
//   sourceMods  = list of plugin names
struct CSVSlot {
    std::string dummyPlugin;
    uint32_t    virtualID;
    std::vector<std::string> sourceMods;
};

// Load all dummy slot mappings from the CSV file.
// Returns true on success, false on failure.
bool load_csv_slots(const std::string& path, std::vector<CSVSlot>& out);
