#pragma once

#include <cassert>
#include <iostream>
#include <mutex>
#include <string_view>
#include <queue>
#include <sstream>
#include <fstream>

namespace mirai {

#define ENABLE_LOG_FILE 0

#define MI_EXIT_FAIL(code) (exit(code))
    class Log {
      public:
        template <typename... Args>
        static void Debug(Args &&...args) {
            entries.push(LogEntry{LogLevel::Debug, FormatLog(args...)});
            Write(Color_Blue, "[DEBUG] ", args...);
        }

        template <typename... Args>
        static void Info(Args &&...args) {
            entries.push(LogEntry{LogLevel::Info, FormatLog(args...)});
            Write(Color_Default, "[INFO] ", args...);
        }

        template <typename... Args>
        static void Warn(Args &&...args) {
            entries.push(LogEntry{LogLevel::Warning, FormatLog(args...)});
            Write(Color_Yellow, "[WARN] ", args...);
        }

        template <typename... Args>
        static void Error(Args &&...args) {
            entries.push(LogEntry{LogLevel::Error, FormatLog(args...)});
            Write(Color_Red, "[ERROR] ", args...);
            assert(0);
        }

        template <typename... Args>
        static void Fatal(Args &&...args) {
            entries.push(LogEntry{LogLevel::Error, FormatLog(args...)});
            Write(Color_Red, "[ERROR] ", args...);
#ifdef _DEBUG
            assert(0);
#else
            MI_EXIT_FAIL(-1);
#endif
        }

      private:
        static std::mutex WriteMutex;

        enum class LogLevel {
            Info,
            Debug,
            Warning,
            Error
        };

        struct LogEntry {
            LogLevel level;
            std::string message;
        };
        static std::queue<LogEntry> entries;
#if ENABLE_LOG_FILE
        static std::ofstream log_file;
#endif

        template <typename... Args>
        static void Write(Args &&...args) {
            std::unique_lock<std::mutex> lock(WriteMutex);
#if ENABLE_LOG_FILE
            if (!log_file)
                log_file.open("log.txt", std::ios::out | std::ios::trunc);
#endif
#ifdef _DEBUG
            ((void)args, ...);
            const LogEntry &entry = entries.back();
            std::cout << entry.message << std::endl;
#if ENABLE_LOG_FILE
            log_file << entry.message << '\n';
#endif
#else
            (std::cout << ... << args) << '\n';
#if ENABLE_LOG_FILE
            (log_file << ... << args) << '\n';
#endif
#endif
        }

        template <typename... Args>
        static std::string FormatLog(Args &&...args) {
            std::stringstream ss;
            (ss << ... << args);
            return ss.str();
        }

        constexpr static std::string_view Color_Default = "\033[39m";
        constexpr static std::string_view Color_Red = "\033[31m";
        constexpr static std::string_view Color_Yellow = "\033[33m";
        constexpr static std::string_view Color_Blue = "\033[34m";
    };
} // namespace mirai