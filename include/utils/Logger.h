#pragma once

#include <cstdio>
#include <cstdarg>
#include <string>

// ─── Logger ───────────────────────────────────────────────────
// Simple printf-style logger. Writes to console and optionally
// to a log file. Good enough for development — swap for spdlog
// or similar if you want fancier output later.
class Logger {
public:
    static void Init() {
        Info("Logger initialized");
    }

    static void Info(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        Log("[INFO]  ", fmt, args);
        va_end(args);
    }

    static void Warning(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        Log("[WARN]  ", fmt, args);
        va_end(args);
    }

    static void Error(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        Log("[ERROR] ", fmt, args);
        va_end(args);
    }

private:
    static void Log(const char* prefix, const char* fmt, va_list args) {
        printf("%s", prefix);
        vprintf(fmt, args);
        printf("\n");
    }
};
