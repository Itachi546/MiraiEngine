#pragma once

#include "Graphics/RenderingDevice.hpp"
#include "Common/ThreadSafeQueue.hpp"
#include <atomic>
#include <thread>

namespace mirai
{
    struct BufferCopyTask
    {
        BufferID dst;
        void *data;
        uint32_t offset_in_bytes;
        uint32_t size_in_bytes;
    };

    class AsyncLoader
    {
      public:
        AsyncLoader() = default;

        void start();

        void wait();

        void add_buffer_copy_task(BufferCopyTask &&copy_task)
        {
            buffer_copy_tasks.push(copy_task);
        }

      private:
        ThreadSafeQueue<BufferCopyTask> buffer_copy_tasks;
        BufferID staging_buffer;
        std::thread task_thread;
    };
} // namespace mirai