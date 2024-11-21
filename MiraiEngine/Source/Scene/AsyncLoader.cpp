#include "AsyncLoader.hpp"
#include "Common/MathUtils.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Common/dds.hpp"
#include <chrono>

namespace mirai {
    using namespace std::chrono_literals;
    const uint32_t K_STAGING_BUFFER_SIZE = static_cast<uint32_t>(utils::mb_to_bytes(32));

    static size_t get_image_size_bc(uint32_t width, uint32_t height, uint32_t mip_levels, uint32_t block_size) {
        size_t size = 0;
        for (uint32_t i = 0; i < mip_levels; ++i) {
            size += ((width + 3) / 4) * ((height + 3) / 4) * block_size;
            width = width > 1 ? width / 2 : 1;
            height = height > 1 ? height / 2 : 1;
        }
        return size;
    }

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
                while (!buffer_copy_tasks.empty() || !texture_load_tasks.empty()) {
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

                        total_buffer_loaded++;
                    }

                    std::shared_ptr<TextureLoadTask> texture_load_task = texture_load_tasks.try_pop();
                    if (texture_load_task != nullptr) {
                        FILE *file = fopen(texture_load_task->filename.c_str(), "rb");
                        if (!file)
                            continue;
                        std::unique_ptr<FILE, int (*)(FILE *)> file_ptr(file, fclose);
                        dds::Header header;
                        fread(&header, sizeof(header), 1, file);

                        uint32_t block_size = texture_load_task->block_size;
                        size_t image_size = get_image_size_bc(header.header.dwWidth, header.header.dwHeight, header.header.dwMipMapCount, block_size);
                        ASSERT(image_size <= K_STAGING_BUFFER_SIZE);
                        size_t read_size = fread(staging_buffer_ptr, 1, image_size, file);

                        uint32_t buffer_offset = 0;
                        if (texture_load_task->skip_n_levels > 0) {
                            uint32_t w = header.header.dwWidth;
                            uint32_t h = header.header.dwHeight;
                            for (uint32_t i = 0; i < texture_load_task->skip_n_levels; ++i) {
                                buffer_offset += ((w + 3) / 4) * ((h + 3) / 4) * block_size;
                                w = w > 1 ? w / 2 : 1;
                                h = h > 1 ? h / 2 : 1;
                            }
                        }

                        ASSERT(read_size == image_size);
                        ASSERT(fgetc(file) == -1);

                        file_ptr.reset();
                        file_ptr = nullptr;

                        command_buffer->begin();

                        command_buffer->copy_texture(texture_load_task->texture, staging_buffer, buffer_offset, header.header.dwMipMapCount - texture_load_task->skip_n_levels, block_size);

                        RenderingDevice::get()->submit_command_buffer_immediate(command_buffer);
                        command_buffer->wait();

                        total_texture_loaded++;
                        Log::Info("Texture Loaded: ", texture_load_task->filename, " texture_id: ", texture_load_task->texture.id);
                    }
                }
            });
    }

    void AsyncLoader::wait() {
        task_thread.join();
        Log::Info("buffer copy: ", total_buffer_loaded, " texture copy: ", total_texture_loaded);
        RenderingDevice::get()->destroy_buffers(&staging_buffer, 1);
    }

} // namespace mirai