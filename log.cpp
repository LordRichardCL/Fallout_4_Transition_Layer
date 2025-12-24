#include "pch.h"
#include "log.hpp"

#include <fstream>
#include <mutex>
#include <cstdarg>
#include <cstdio>

namespace
{
    std::mutex gLogMutex;
    std::ofstream gLogFile;

    void ensure_log_open()
    {
        if (!gLogFile.is_open()) {
            // F4SE plugins write logs under Data\F4SE by convention
            gLogFile.open("Data\\F4SE\\Multiplexer.log",
                std::ios::out | std::ios::app);
        }
    }
}

void logf(const char* fmt, ...)
{
    std::lock_guard<std::mutex> lock(gLogMutex);
    ensure_log_open();

    char buf[1024];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    gLogFile << buf << std::endl;
    gLogFile.flush();
}

void log_progress(const std::string& stage, int current, int total)
{
    std::lock_guard<std::mutex> lock(gLogMutex);
    ensure_log_open();

    constexpr int barWidth = 20;
    int filled = (total > 0) ? (current * barWidth / total) : 0;

    gLogFile << stage << " [";
    for (int i = 0; i < barWidth; ++i) {
        gLogFile << (i < filled ? '#' : '.');
    }
    gLogFile << "] " << current << "/" << total << std::endl;

    gLogFile.flush();
}
