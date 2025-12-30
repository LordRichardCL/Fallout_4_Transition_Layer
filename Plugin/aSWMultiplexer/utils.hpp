#pragma once

#include <string>
#include <vector>

// Read a CSV file into a vector of raw lines.
// Returns an empty vector on failure.
std::vector<std::string> read_csv_lines(const std::string& path);

// Trim leading and trailing whitespace from a string.
std::string trim(const std::string& s);
