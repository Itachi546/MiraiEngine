#include "RenderingDevice.hpp"
#include "Vulkan/CommandBuffer.hpp"
#include "Common/FileUtils.hpp"

#include <cstring>

namespace mirai {
    enum SamplerTypeID {
        SAMPLER_LINEAR_REPEAT = 0,
        SAMPLER_LINEAR_CLAMP,
        SAMPLER_LINEAR_REPEAT_ANISO16,

        SAMPLER_POINT_REPEAT,
        SAMPLER_POINT_CLAMP,
        SAMPLER_POINT_REPEAT_ANISO16,
    };

    RenderingDevice *RenderingDevice::Instance = nullptr;

    void rendering_utils::upload_default_samplers(RenderingDevice *device, void *ptr) {
        // Linear Sampler
        {
            SamplerDescription sampler_info = SamplerDescription::create();
            sampler_info.address_mode_u = sampler_info.address_mode_v = SAMPLER_ADDRESS_MODE_REPEAT;
            device->write_sampler_descriptors(&sampler_info, 1, static_cast<uint8_t *>(ptr) + SAMPLER_LINEAR_REPEAT);

            sampler_info.enable_anisotropy = true;
            device->write_sampler_descriptors(&sampler_info, 1, static_cast<uint8_t *>(ptr) + SAMPLER_LINEAR_REPEAT_ANISO16);
        }
        {
            SamplerDescription sampler_info = SamplerDescription::create();
            device->write_sampler_descriptors(&sampler_info, 1, static_cast<uint8_t *>(ptr) + SAMPLER_LINEAR_CLAMP);
        }

        // Point Sampler
        {
            SamplerDescription sampler_info = SamplerDescription::create();
            sampler_info.address_mode_u = sampler_info.address_mode_v = SAMPLER_ADDRESS_MODE_REPEAT;
            sampler_info.min_filter = sampler_info.mag_filter = FILTER_NEAREST;
            sampler_info.mipmap_mode = SAMPLER_MIPMAP_NEAREST;
            device->write_sampler_descriptors(&sampler_info, 1, static_cast<uint8_t *>(ptr) + SAMPLER_POINT_REPEAT);

            sampler_info.enable_anisotropy = true;
            device->write_sampler_descriptors(&sampler_info, 1, static_cast<uint8_t *>(ptr) + SAMPLER_POINT_REPEAT_ANISO16);
        }
        {
            SamplerDescription sampler_info = SamplerDescription::create();
            sampler_info.min_filter = sampler_info.mag_filter = FILTER_NEAREST;
            sampler_info.mipmap_mode = SAMPLER_MIPMAP_NEAREST;
            device->write_sampler_descriptors(&sampler_info, 1, static_cast<uint8_t *>(ptr) + SAMPLER_POINT_CLAMP);
        }
    }

    void rendering_utils::copy_texture_immediate(TextureID dst, void *data, uint32_t size) {
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
        command_buffer->copy_texture(dst, staging_buffer, 0, 1, 1, 32);
        command_buffer->prepare_image_for_shader_read(dst);
        device->submit_command_buffer_immediate(command_buffer);
        command_buffer->wait();
        device->destroy_buffers(&staging_buffer, 1);
    }

    TextureID rendering_utils::load_texture2d_from_path(const std::string &path) {
        int n_channel, width, height;
        auto data = utils::load_image(path.c_str(), &width, &height, &n_channel, 4);
        ASSERT(data != nullptr);
        if (data == nullptr)
            return TextureID{K_INVALID_ID};

        TextureDescription texture_desc = {
            .create_flags = 0,
            .width = (uint32_t)width,
            .height = (uint32_t)height,
            .depth = 1,
            .mip_levels = 1,
            .array_layers = 1,
            .texture_type = TEXTURE_TYPE_2D,
            .format = FORMAT_B8G8R8A8_UNORM,
            .usage_flags = TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_TRANSFER_DST_BIT,
        };

        TextureID texture_id = RenderingDevice::get()->create_texture(&texture_desc, "hdri_texture");
        rendering_utils::copy_texture_immediate(texture_id, data.get(), width * height * sizeof(uint8_t) * 4);

        data.reset();
        data = nullptr;
        return texture_id;
    }
} // namespace mirai