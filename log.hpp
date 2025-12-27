#pragma once

#include <string>
#include <cstdarg>

// Clears the log file at plugin startup (truncate + header)
void clear_log();

// Simple formatted logging function.
// Uses printf‑style formatting.
void logf(const char* fmt, ...);

// Report progress for long operations (e.g., scanning, mapping, injecting).
// 'stage' is a label, 'current' and 'total' define progress.
void log_progress(const std::string& stage, int current, int total);
