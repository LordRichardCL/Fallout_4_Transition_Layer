#pragma once

#include <string>
#include <vector>
#include <unordered_map>

// Simple CSV row type: column name -> value
using CsvRow = std::unordered_map<std::string, std::string>;

// Read a CSV file into a vector of rows (map column -> value).
// Returns an empty vector on failure.
std::vector<CsvRow> read_csv(const std::string& path);

// Trim leading and trailing whitespace from a string.
std::string trim(const std::string& s);
