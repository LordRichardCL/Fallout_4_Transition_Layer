#include "pch.h"
#include "utils.hpp"

#include <fstream>

// Read a CSV file into a vector of raw lines.
std::vector<std::string> read_csv_lines(const std::string& path)
{
    std::ifstream file(path);
    std::vector<std::string> lines;

    if (!file.is_open())
        return lines;

    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }

    return lines;
}

// Trim leading and trailing whitespace.
std::string trim(const std::string& s)
{
    const char* whitespace = " \t\n\r\f\v";

    size_t start = s.find_first_not_of(whitespace);
    if (start == std::string::npos)
        return "";

    size_t end = s.find_last_not_of(whitespace);
    return s.substr(start, end - start + 1);
}
