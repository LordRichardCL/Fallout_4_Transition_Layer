#pragma once
#include <string>

void Diagnostics_Initialize();
void Diagnostics_HandleCommand(const std::string& cmdLine);

// Exposed so plugin.cpp can call validator at startup
void Diagnostics_RunValidator();
#pragma once
