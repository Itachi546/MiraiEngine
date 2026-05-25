#include "Log.hpp"

namespace mirai {
    std::mutex Log::WriteMutex;
    std::queue<Log::LogEntry> Log::entries;
#if ENABLE_LOG_FILE
    std::ofstream Log::log_file;
#endif

} // namespace mirai