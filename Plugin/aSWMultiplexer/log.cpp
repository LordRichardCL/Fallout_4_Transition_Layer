#include "pch.h"
#include "log.hpp"

#include <fstream>
#include <mutex>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <windows.h>

namespace
{
    std::mutex gLogMutex;
    std::ofstream gLogFile;

    // ------------------------------------------------------------
    // Timestamp helper
    // ------------------------------------------------------------
    std::string CurrentTimestamp()
    {
        SYSTEMTIME st;
        GetLocalTime(&st);

        char buffer[64];
        sprintf_s(buffer, "[%04d-%02d-%02d %02d:%02d:%02d] ",
            st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond);

        return std::string(buffer);
    }

    // ------------------------------------------------------------
    // Ensure log file is open (append mode)
    // ------------------------------------------------------------
    void ensure_log_open()
    {
        if (!gLogFile.is_open()) {
            gLogFile.open("Data\\F4SE\\Plugins\\Multiplexer\\Multiplexer.log",
                std::ios::out | std::ios::app);
        }
    }
}

// ------------------------------------------------------------
// Clear log at startup (truncate + header)
// ------------------------------------------------------------
void clear_log()
{
    std::lock_guard<std::mutex> lock(gLogMutex);

    gLogFile.open("Data\\F4SE\\Plugins\\Multiplexer\\Multiplexer.log",
        std::ios::out | std::ios::trunc);

    if (gLogFile.is_open()) {
        gLogFile << "=== aSWMultiplexer Log Started ===" << std::endl;
        gLogFile.flush();
        gLogFile.close();
    }
}

// ------------------------------------------------------------
// Standard formatted log with timestamp
// ------------------------------------------------------------
void logf(const char* fmt, ...)
{
    std::lock_guard<std::mutex> lock(gLogMutex);
    ensure_log_open();

    char buf[1024];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    gLogFile << CurrentTimestamp() << buf << std::endl;
    gLogFile.flush();
}

// ------------------------------------------------------------
// Progress bar logging with timestamp
// ------------------------------------------------------------
void log_progress(const std::string& stage, int current, int total)
{
    std::lock_guard<std::mutex> lock(gLogMutex);
    ensure_log_open();

    constexpr int barWidth = 20;
    int filled = (total > 0) ? (current * barWidth / total) : 0;

    gLogFile << CurrentTimestamp() << stage << " [";
    for (int i = 0; i < barWidth; ++i) {
        gLogFile << (i < filled ? '#' : '.');
    }
    gLogFile << "] " << current << "/" << total << std::endl;

    gLogFile.flush();
}
