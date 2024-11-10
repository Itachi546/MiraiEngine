#pragma once

#include <chrono>

namespace mirai
{
    class Timer
    {
      public:
        Timer()
        {
            timestamp = std::chrono::high_resolution_clock::now();
        }

        void record()
        {
            timestamp = std::chrono::high_resolution_clock::now();
        }

        double elapsed_seconds()
        {
            auto timestamp2 = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> timespan = std::chrono::duration_cast<std::chrono::duration<double>>(timestamp2 - timestamp);
            return timespan.count();
        }

        double elapsed_milliseconds()
        {
            return elapsed_seconds() * 1000.0f;
        }

        double elapsed()
        {
            return elapsed_milliseconds();
        }

      private:
        std::chrono::high_resolution_clock::time_point timestamp;
    };
} // namespace mirai