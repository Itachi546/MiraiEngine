#pragma once

#include "Graphics/RenderingDevice.hpp"
#include "Common/ThreadSafeQueue.hpp"
#include <atomic>
#include <thread>

namespace mirai {
    struct BufferCopyTask {
        BufferID dst;
        void *data;
        uint32_t offset_in_bytes;
        uint32_t size_in_bytes;
    };

    struct TextureLoadTask {
        TextureID texture;
        std::string filename;
        uint32_t block_size;
        uint32_t skip_n_levels;
    };

    class AsyncLoader {
      public:
        AsyncLoader() = default;

        void start();

        void wait();

        void add_buffer_copy_task(const BufferCopyTask &copy_task) {
            buffer_copy_tasks.push(copy_task);
        }

        void add_texture_load_task(const TextureLoadTask &texture_load_task) {
            texture_load_tasks.push(texture_load_task);
        }

      private:
        ThreadSafeQueue<BufferCopyTask> buffer_copy_tasks;
        ThreadSafeQueue<TextureLoadTask> texture_load_tasks;
        BufferID staging_buffer;
        std::thread task_thread;

        uint32_t total_buffer_loaded = 0;
        uint32_t total_texture_loaded = 0;
    };
} // namespace mirai