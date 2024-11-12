#include "AsyncLoader.hpp"
#include "Common/MathUtils.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include <chrono>

namespace mirai {
    using namespace std::chrono_literals;
    const uint32_t K_STAGING_BUFFER_SIZE = utils::mb_to_bytes(32);
    void AsyncLoader::start() {
        BufferDescription buffer_desc = {
            .size = K_STAGING_BUFFER_SIZE,
            .usage_flags = BUFFER_USAGE_TRANSFER_SRC_BIT,
            .allocation_type = MEMORY_ALLOCATION_TYPE_CPU,
        };
        staging_buffer = RenderingDevice::get()->create_buffer(&buffer_desc, "async_staging_buffer");
        task_thread = std::thread(
            [this]() {
                CommandBuffer *command_buffer = RenderingDevice::get()->get_command_buffer(1);
                void *staging_buffer_ptr = RenderingDevice::get()->map_buffer(staging_buffer);
                while (!buffer_copy_tasks.empty()) {
                    std::shared_ptr<BufferCopyTask> copy_task = buffer_copy_tasks.try_pop();
                    if (copy_task != nullptr) {
                        uint32_t copy_data_size = copy_task->size_in_bytes;

                        // Check if the copy size is greater than the staging buffer
                        if (copy_task->size_in_bytes > K_STAGING_BUFFER_SIZE) {
                            uint32_t remaining_data_size = copy_task->size_in_bytes - K_STAGING_BUFFER_SIZE;
                            add_buffer_copy_task({
                                .dst = copy_task->dst,
                                .data = ((uint8_t *)copy_task->data + K_STAGING_BUFFER_SIZE),
                                .offset_in_bytes = K_STAGING_BUFFER_SIZE + copy_task->offset_in_bytes,
                                .size_in_bytes = remaining_data_size,
                            });
                            copy_data_size = K_STAGING_BUFFER_SIZE;
                            Log::Warn("Splitting data, total: ", copy_task->size_in_bytes, " remaining: ", remaining_data_size);
                        }

                        memcpy(staging_buffer_ptr, copy_task->data, copy_data_size);

                        // Immediate Copy
                        command_buffer->begin();

                        command_buffer->copy_buffer(copy_task->dst, staging_buffer, {
                                                                                        .src_offset = 0,
                                                                                        .dst_offset = copy_task->offset_in_bytes,
                                                                                        .size = copy_data_size,
                                                                                    });
                        RenderingDevice::get()->submit_command_buffer_immediate(command_buffer);

                        command_buffer->wait();
                    } else
                        std::this_thread::sleep_for(10ms);
                }
            });
    }

    void AsyncLoader::wait() {
        task_thread.join();
        RenderingDevice::get()->destroy_buffers(&staging_buffer, 1);
    }

} // namespace mirai