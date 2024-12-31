#include "EnvironmentMap.hpp"
#include "Common/FileUtils.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"

namespace mirai {
    EnvironmentMap::EnvironmentMap(const std::string &hdri_path) : hdri_path(hdri_path) {
        int width, height, n_channel;
        float *data = utils::load_image_float(hdri_path.c_str(), &width, &height, &n_channel, 4);

        SamplerDescription sampler_desc = SamplerDescription::create();
        sampler_desc.enable_anisotropy = false;

        TextureDescription texture_desc = {
            .width = (uint32_t)width,
            .height = (uint32_t)height,
            .depth = 1,
            .mip_levels = 1,
            .array_layers = 1,
            .texture_type = TEXTURE_TYPE_2D,
            .format = FORMAT_R32G32B32A32_SFLOAT,
            .usage_flags = TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_TRANSFER_DST_BIT,
            .sampler_desc = &sampler_desc,
        };

        RenderingDevice *device = RenderingDevice::get();
        TextureID hdri_texture = device->create_texture(&texture_desc, "hdri_texture");
        rendering_utils::copy_texture_immediate(hdri_texture, data, width * height * sizeof(float) * 4);
        utils::free_image(data);

        texture_desc.width = cubemap_size;
        texture_desc.height = cubemap_size;
        texture_desc.array_layers = 6;
        texture_desc.texture_type = TEXTURE_TYPE_CUBE;
        texture_desc.format = FORMAT_R16G16B16A16_SFLOAT;
        texture_desc.usage_flags = TEXTURE_USAGE_STORAGE_BIT | TEXTURE_USAGE_SAMPLED_BIT,
        cubemap_texture = device->create_texture(&texture_desc, "cubemap");

        generate(hdri_texture);
        device->destroy_textures(&hdri_texture, 1);
    }

    void EnvironmentMap::generate(TextureID hdri_texture) {
        RenderingDevice *device = RenderingDevice::get();
        UniformLayout layout[] = {
            {0, BINDING_TYPE_COMBINED_IMAGE_SAMPLER, SHADER_STAGE_COMPUTE},
            {1, BINDING_TYPE_STORAGE_IMAGE, SHADER_STAGE_COMPUTE},
        };

        UniformSetID uniform_set = device->create_uniform_set(layout, (uint32_t)std::size(layout), 0, "hdri_to_cubemap_set");
        UniformBinding bindings[] = {
            {.resource_id = hdri_texture},
            {.resource_id = cubemap_texture},
        };
        device->update_uniform_set(uniform_set, bindings, (uint32_t)std::size(bindings));

        ComputeShader cubemap_shader("hdri_cubemap");
        cubemap_shader.create_from_file({"SPIRV/hdri-to-cubemap.comp.spv"});
        cubemap_shader.set_uniform_sets(&uniform_set, 1);

        float push_constant_data[] = {(float)cubemap_size, (float)cubemap_size, 0.0f, 0.0f};
        PushConstant push_constant = {
            .data = push_constant_data,
            .shader_stage = SHADER_STAGE_COMPUTE,
            .size = sizeof(uint32_t) * 4,
            .offset = 0,
        };
        cubemap_shader.set_push_constant(&push_constant, 1);

        CommandBuffer *command_buffer = device->get_command_buffer(0);
        command_buffer->begin();

        TextureBarrierInfo barrier_info = {
            .texture_id = cubemap_texture,
            .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            .access_mask = ACCESS_FLAG_SHADER_WRITE,
            .layout = IMAGE_LAYOUT_GENERAL,
        };

        command_buffer->prepare_image(&barrier_info, 1);

        cubemap_shader.bind(command_buffer);

        uint32_t work_size_x = rendering_utils::get_workgroup_size(cubemap_size, 32);
        uint32_t work_size_y = rendering_utils::get_workgroup_size(cubemap_size, 32);

        command_buffer->dispatch(work_size_x, work_size_y, 6);

        barrier_info.stage_mask = PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        barrier_info.access_mask = ACCESS_FLAG_SHADER_READ;
        barrier_info.layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        command_buffer->prepare_image(&barrier_info, 1);

        device->submit_command_buffer_immediate(command_buffer);
        command_buffer->wait();
    }

    EnvironmentMap::~EnvironmentMap() {
        RenderingDevice *device = RenderingDevice::get();
        TextureID textures[] = {cubemap_texture};
        device->destroy_textures(textures, 1);
    }

} // namespace mirai