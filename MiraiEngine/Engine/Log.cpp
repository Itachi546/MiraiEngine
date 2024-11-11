#include "Log.hpp"

namespace mirai {
    std::mutex Log::WriteMutex;
    std::queue<Log::LogEntry> Log::entries;
} // namespace mirai