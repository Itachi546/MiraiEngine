#include "AsyncLoader.hpp"
#include "Math/MathUtils.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Common/dds.hpp"
#include "Common/FileUtils.hpp"
#include <chrono>

namespace mirai {
    using namespace std::chrono_literals;

    AsyncLoader::AsyncLoader() : stopped(false) {
        BufferDescription buffer_desc = {
            .size = staging_buffer_size,
            .usage_flags = BUFFER_USAGE_TRANSFER_SRC_BIT,
            .allocation_type = MEMORY_ALLOCATION_TYPE_CPU,
        };
        staging_buffer = RenderingDevice::get()->create_buffer(&buffer_desc, "async_staging_buffer");
        task_thread = std::thread(&AsyncLoader::worker_loop, this);
    }

    void AsyncLoader::worker_loop() {
        CommandBuffer *command_buffer = RenderingDevice::get()->get_command_buffer(1);
        void *staging_buffer_ptr = RenderingDevice::get()->map_buffer(staging_buffer);

        while (true) {
            Task task;
            {
                std::unique_lock<std::mutex> lk(mu);
                cv.wait(lk, [this]() {
                    return !task_queue.empty() || stopped;
                });

                if (stopped && task_queue.empty())
                    return;
            }

            task_queue.try_pop(task);

            switch (task.task_type) {
            case TaskType::UploadBuffer: {
                copy_buffer(command_buffer, std::get<BufferCopyTask>(task.data), staging_buffer_ptr);
                total_buffer_loaded++;
                break;
            }
            case TaskType::LoadTexture: {
                TextureLoadTask texture_load_task = std::get<TextureLoadTask>(task.data);
                if (texture_load_task.is_dds_texture)
                    load_dds_texture(command_buffer, texture_load_task, staging_buffer_ptr);
                else
                    load_texture(command_buffer, texture_load_task, staging_buffer_ptr);
                Log::Info("Texture Loaded: ", texture_load_task.filename, " id: ", texture_load_task.texture.id);
                break;
            }
            default:
                Log::Fatal("Unknown tasktype");
            }
        }
    }

    void AsyncLoader::load_dds_texture(CommandBuffer *command_buffer, const TextureLoadTask &load_task, void *&staging_buffer_ptr) {
        FILE *file = fopen(load_task.filename.c_str(), "rb");
        if (!file)
            return;
        std::unique_ptr<FILE, int (*)(FILE *)> file_ptr(file, fclose);
        dds::Header header;
        fread(&header, sizeof(header), 1, file);

        size_t image_size = header.data_size();
        if (image_size > staging_buffer_size) {
            resize_staging_buffer(staging_buffer_ptr, cast_u32(image_size));
        }

        uint32_t mip_count = header.header.dwMipMapCount - load_task.skip_first_n_levels;
        uint32_t buffer_offset = cast_u32(header.mip_offset(load_task.skip_first_n_levels)) - sizeof(header);

        if (buffer_offset > 0)
            fseek(file, buffer_offset, SEEK_CUR);

        size_t read_size = fread(staging_buffer_ptr, 1, image_size, file);
        ASSERT(read_size == image_size);

        size_t ret = fgetc(file);
        if (ret != -1) {
            Log::Warn("Incomplete file read: ", load_task.filename);
        }

        file_ptr.reset();
        file_ptr = nullptr;

        command_buffer->begin();

        command_buffer->copy_texture(load_task.texture, staging_buffer, buffer_offset, mip_count, header.block_size(), header.bits_per_element());
        command_buffer->prepare_image_for_shader_read(load_task.texture);

        RenderingDevice::get()->submit_command_buffer_immediate(command_buffer);
        command_buffer->wait();
    }

    void AsyncLoader::load_texture(CommandBuffer *command_buffer, const TextureLoadTask &load_task, void *&staging_buffer_ptr) {
        int width, height, n_channel;
        auto data_ptr = utils::load_image(load_task.filename.c_str(), &width, &height, &n_channel, load_task.force_rgba ? 4 : 0);
        if (!data_ptr) {
            Log::Warn("Failed to load texture", load_task.filename);
            return;
        }
        if (load_task.force_rgba)
            n_channel = 4;

        size_t image_size = width * height * n_channel;

        if (image_size > staging_buffer_size) {
            resize_staging_buffer(staging_buffer_ptr, cast_u32(image_size));
        }

        std::memcpy(staging_buffer_ptr, data_ptr.get(), image_size);
        data_ptr.reset();
        data_ptr = nullptr;

        command_buffer->begin();
        command_buffer->copy_texture(load_task.texture, staging_buffer, 0, 1, 1, 32);
        RenderingDevice::get()->generate_mipmap(command_buffer, load_task.texture, PIPELINE_STAGE_TRANSFER_BIT);
        command_buffer->prepare_image_for_shader_read(load_task.texture);
        RenderingDevice::get()->submit_command_buffer_immediate(command_buffer);
        command_buffer->wait();
    }

    void AsyncLoader::copy_buffer(CommandBuffer *command_buffer, const BufferCopyTask &copy_task, void *staging_buffer_ptr) {
        uint32_t copy_data_size = std::min(staging_buffer_size, copy_task.size_in_bytes);

        memcpy(staging_buffer_ptr, copy_task.data, copy_data_size);

        // Immediate Copy
        command_buffer->begin();

        BufferCopyRegion copy_region = {
            .src_offset = 0,
            .dst_offset = copy_task.offset_in_bytes,
            .size = copy_data_size,
        };
        command_buffer->copy_buffer(copy_task.dst, staging_buffer, &copy_region, 1);
        RenderingDevice::get()->submit_command_buffer_immediate(command_buffer);

        command_buffer->wait();

        // Check if the copy size is greater than the staging buffer
        if (copy_task.size_in_bytes > staging_buffer_size) {
            uint32_t remaining_data_size = copy_task.size_in_bytes - staging_buffer_size;
            copy_buffer(
                command_buffer,
                BufferCopyTask{
                    .dst = copy_task.dst,
                    .data = ((uint8_t *)copy_task.data + staging_buffer_size),
                    .offset_in_bytes = staging_buffer_size + copy_task.offset_in_bytes,
                    .size_in_bytes = remaining_data_size,

                },
                staging_buffer_ptr);
            Log::Warn("Splitting data, total: ", copy_task.size_in_bytes, " remaining: ", remaining_data_size);
        }
    }

    void AsyncLoader::resize_staging_buffer(void *&staging_buffer_ptr, uint32_t req_size) {
        RenderingDevice *device = RenderingDevice::get();
        Log::Info("Resizing async loader staging buffer from ", utils::bytes_to_mb(staging_buffer_size), "mb to ", utils::bytes_to_mb(req_size), "mb");

        BufferDescription buffer_desc = {
            .size = req_size,
            .usage_flags = BUFFER_USAGE_TRANSFER_SRC_BIT,
            .allocation_type = MEMORY_ALLOCATION_TYPE_CPU,
        };

        device->resize_buffer(&buffer_desc, staging_buffer, false, "async_staging_buffer");
        staging_buffer_ptr = device->map_buffer(staging_buffer);
        staging_buffer_size = req_size;
    }

    void AsyncLoader::wait() {
        {
            std::lock_guard<std::mutex> lk(mu);
            stopped = true;
        }
        cv.notify_one();

        if (task_thread.joinable())
            task_thread.join();

        Log::Info("buffer copy: ", total_buffer_loaded, " texture copy: ", total_texture_loaded);
        RenderingDevice::get()->destroy_buffers(&staging_buffer, 1);
        Log::Info("Destroying async loader staging buffer ", staging_buffer.id);
    }

    AsyncLoader::~AsyncLoader() {
        if (task_thread.joinable())
            wait();
    }

} // namespace mirai