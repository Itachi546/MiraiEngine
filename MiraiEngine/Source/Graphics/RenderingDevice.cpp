#include "RenderingDevice.hpp"
#include "Vulkan/CommandBuffer.hpp"
#include "Common/FileUtils.hpp"
#include "Engine/Log.hpp"

#include <cstring>

namespace mirai {
    RenderingDevice *RenderingDevice::Instance = nullptr;

    ShaderID rendering_utils::create_shader_module_from_file(const std::string &filename) {
        std::optional<std::string> result = utils::read_file_binary(filename);
        if (result.has_value()) {
            std::string content = result.value();
            return RenderingDevice::get()->create_shader((uint32_t *)(content.c_str()), static_cast<uint32_t>(content.length()), filename);
        } else {
            Log::Error("Error loading file: ", filename);
            return ShaderID{K_INVALID_ID};
        }
    }

    void rendering_utils::copy_texture_immediate(TextureID dst, unsigned char *data, uint32_t size) {
        BufferDescription buffer_desc = {
            .size = size,
            .usage_flags = BUFFER_USAGE_TRANSFER_SRC_BIT,
            .allocation_type = MEMORY_ALLOCATION_TYPE_CPU,
        };

        RenderingDevice *device = RenderingDevice::get();
        BufferID staging_buffer = device->create_buffer(&buffer_desc, "staging_buffer");
        uint8_t *ptr = device->map_buffer(staging_buffer);
        std::memcpy(ptr, data, size);

        CommandBuffer *command_buffer = device->get_command_buffer(0);
        command_buffer->begin();
        // Block size is ignored for single mip
        command_buffer->copy_texture(dst, staging_buffer, 0, 1, 32);
        device->submit_command_buffer_immediate(command_buffer);
        command_buffer->wait();
        device->destroy_buffers(&staging_buffer, 1);
    }

} // namespace mirai