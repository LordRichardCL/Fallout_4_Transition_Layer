#include "pch.h"
#include "csv.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>

// Read a CSV file into a vector of rows (map column -> value)
std::vector<std::unordered_map<std::string, std::string>> read_csv(const std::string& path)
{
    std::ifstream file(path);
    std::vector<std::unordered_map<std::string, std::string>> rows;

    if (!file.is_open()) {
        return rows; // return empty if file can't be opened
    }

    std::string headerLine;
    if (!std::getline(file, headerLine)) {
        return rows; // no header line
    }

    // Parse header columns
    std::vector<std::string> headers;
    {
        std::stringstream ss(headerLine);
        std::string col;
        while (std::getline(ss, col, ',')) {
            headers.push_back(trim(col));   // trim comes from utils.cpp
        }
    }

    // Parse each subsequent line
    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string value;
        std::unordered_map<std::string, std::string> row;
        size_t colIndex = 0;

        while (std::getline(ss, value, ',')) {
            if (colIndex < headers.size()) {
                row[headers[colIndex]] = trim(value);   // trim comes from utils.cpp
            }
            ++colIndex;
        }

        if (!row.empty()) {
            rows.push_back(std::move(row));
        }
    }

    return rows;
}
