#pragma once

#include <cassert>
#include <iostream>
#include <mutex>
#include <string_view>
#include <queue>
#include <sstream>

namespace mirai
{
    class Log
    {
      public:
        template <typename... Args>
        static void Debug(Args &&...args)
        {
            entries.push(LogEntry{LogLevel::Debug, FormatLog(args...)});
            Write(Color_Blue, "[DEBUG] ", args...);
        }

        template <typename... Args>
        static void Info(Args &&...args)
        {
            entries.push(LogEntry{LogLevel::Info, FormatLog(args...)});
            Write(Color_Default, "[INFO] ", args...);
        }

        template <typename... Args>
        static void Warn(Args &&...args)
        {
            entries.push(LogEntry{LogLevel::Warning, FormatLog(args...)});
            Write(Color_Yellow, "[WARN] ", args...);
        }

        template <typename... Args>
        static void Error(Args &&...args)
        {
            entries.push(LogEntry{LogLevel::Error, FormatLog(args...)});
            Write(Color_Red, "[ERROR] ", args...);
            assert(0);
        }

        template <typename... Args>
        static void Fatal(Args &&...args)
        {
            entries.push(LogEntry{LogLevel::Error, FormatLog(args...)});
            Write(Color_Red, "[ERROR] ", args...);
            exit(-1);
        }

      private:
        static std::mutex WriteMutex;

        enum class LogLevel
        {
            Info,
            Debug,
            Warning,
            Error
        };

        struct LogEntry
        {
            LogLevel level;
            std::string message;
        };
        static std::queue<LogEntry> entries;

        template <typename... Args>
        static void Write(Args &&...args)
        {
            std::unique_lock<std::mutex> lock(WriteMutex);
            (std::cout << ... << args) << '\n';
        }

        template <typename... Args>
        static std::string FormatLog(Args &&...args)
        {
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