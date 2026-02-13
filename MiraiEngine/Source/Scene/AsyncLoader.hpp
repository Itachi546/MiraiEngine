#pragma once

#include "Graphics/RenderingDevice.hpp"
#include "Common/ThreadSafeQueue.hpp"
#include "Engine/Log.hpp"
#include <atomic>
#include <thread>
#include <condition_variable>
#include <variant>
#include <memory>
namespace mirai {
    enum class TaskType {
        UploadBuffer,
        LoadTexture
    };
    struct BufferCopyTask {
        BufferID dst;
        void *data;
        uint32_t offset_in_bytes;
        uint32_t size_in_bytes;
    };

    struct TextureLoadTask {
        TextureID texture;
        std::string filename;
        bool is_dds_texture;
        uint32_t skip_first_n_levels;
        bool force_rgba;
    };

    struct Task {
        TaskType task_type;
        std::variant<BufferCopyTask, TextureLoadTask> data;
    };

    class AsyncLoader {
      public:
        AsyncLoader();
        AsyncLoader(const AsyncLoader &) = delete;
        AsyncLoader &operator=(const AsyncLoader &) = delete;
        AsyncLoader(AsyncLoader &&) = delete;
        AsyncLoader &operator=(AsyncLoader &&) = delete;

        void wait();

        void push(Task task) {
            {
                std::lock_guard<std::mutex> lk(mu);
                if (stopped)
                    Log::Fatal("Cannot push task anymore");
            }
            task_queue.push(std::move(task));
            cv.notify_one();
        }

        ~AsyncLoader();

      private:
        bool stopped;
        std::thread task_thread;
        std::mutex mu;
        ThreadSafeQueue<Task> task_queue;
        std::condition_variable cv;

        BufferID staging_buffer;

        uint32_t total_buffer_loaded = 0;
        uint32_t total_texture_loaded = 0;
        uint32_t staging_buffer_size = 32 * 1024 * 1024; // 32MB

        void load_dds_texture(CommandBuffer *command_buffer, const TextureLoadTask &load_task, void *&staging_buffer_ptr);
        void load_texture(CommandBuffer *command_buffer, const TextureLoadTask &load_task, void *&staging_buffer_ptr);
        void copy_buffer(CommandBuffer *command_buffer, const BufferCopyTask &load_task, void *staging_buffer_ptr);
        void resize_staging_buffer(void *&staging_buffer_ptr, uint32_t req_size);
        void worker_loop();
    };
} // namespace mirai