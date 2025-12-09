#include "EnvironmentMap.hpp"
#include "Common/FileUtils.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include <cmath>

namespace mirai {

    void EnvironmentMap::initialize_textures() {
        TextureDescription texture_desc = {
            .create_flags = 0,
            .width = (uint32_t)cubemap_size,
            .height = (uint32_t)cubemap_size,
            .depth = 1,
            .mip_levels = cast_u32(std::floor(log2(cubemap_size))),
            .array_layers = 6,
            .texture_type = TEXTURE_TYPE_CUBE,
            .format = FORMAT_R16G16B16A16_SFLOAT,
            .usage_flags = TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_STORAGE_BIT | TEXTURE_USAGE_TRANSFER_DST_BIT | TEXTURE_USAGE_TRANSFER_SRC_BIT,
        };

        RenderingDevice *device = RenderingDevice::get();
        SamplerDescription sampler_desc = SamplerDescription::create();
        default_sampler = device->create_sampler(&sampler_desc);

        cubemap_texture = device->create_texture(&texture_desc, "cubemap");

        texture_desc.width = irradiance_map_size;
        texture_desc.height = irradiance_map_size;
        texture_desc.mip_levels = 1;
        texture_desc.usage_flags = TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_STORAGE_BIT;
        irradiance_texture = device->create_texture(&texture_desc, "cubemap_irradiance");

        // Create Prefilter Environment map texture
        texture_desc.create_flags = TEXTURE_CREATION_FLAG_IMAGE_VIEW_PER_MIP;
        texture_desc.mip_levels = prefilter_num_mip_levels;
        texture_desc.width = prefilter_map_size;
        texture_desc.height = prefilter_map_size;
        prefilter_texture = device->create_texture(&texture_desc, "prefilter_envmap");
        sampler_desc.mipmap_mode = SAMPLER_MIPMAP_LINEAR;
        SamplerID prefilter_sampler = device->create_sampler(&sampler_desc);

        // Create 2D BRDF Texture
        texture_desc.create_flags = 0;
        texture_desc.array_layers = 1;
        texture_desc.mip_levels = 1;
        texture_desc.width = brdf_texture_size;
        texture_desc.height = brdf_texture_size;
        texture_desc.texture_type = TEXTURE_TYPE_2D;
        texture_desc.format = FORMAT_R16G16_SFLOAT;
        brdf_texture = device->create_texture(&texture_desc, "brdf_texture");
    }

    EnvironmentMap::EnvironmentMap(const std::string &hdri_path) : hdri_path(hdri_path) {
        int width, height, n_channel;
        float *data = utils::load_image_float(hdri_path.c_str(), &width, &height, &n_channel, 4);
        if (data == nullptr) {
            Log::Error("Failed to load hdri: ", hdri_path);
        }

        RenderingDevice *device = RenderingDevice::get();

        SamplerDescription sampler_desc = SamplerDescription::create();
        // @NOTE this is done to prevent the brdf texture wrap around when dot(N, V) = 1
        sampler_desc.address_mode_u = sampler_desc.address_mode_v = sampler_desc.address_mode_w = SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_desc.enable_anisotropy = false;
        SamplerID hdri_sampler = device->create_sampler(&sampler_desc);

        TextureDescription texture_desc = {
            .create_flags = 0,
            .width = (uint32_t)width,
            .height = (uint32_t)height,
            .depth = 1,
            .mip_levels = 1,
            .array_layers = 1,
            .texture_type = TEXTURE_TYPE_2D,
            .format = FORMAT_R32G32B32A32_SFLOAT,
            .usage_flags = TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_TRANSFER_DST_BIT,
        };

        TextureID hdri_texture = device->create_texture(&texture_desc, "hdri_texture");
        rendering_utils::copy_texture_immediate(hdri_texture, data, width * height * sizeof(float) * 4);
        utils::free_image(data);

        initialize_textures();

        UniformLayout layout[] = {
            {0, BINDING_TYPE_COMBINED_IMAGE_SAMPLER, SHADER_STAGE_COMPUTE},
            {1, BINDING_TYPE_STORAGE_IMAGE, SHADER_STAGE_COMPUTE},
        };

        UniformSetID uniform_set = device->create_uniform_set(layout, (uint32_t)std::size(layout), 0, "hdri_to_cubemap_set");
        UniformBinding bindings[] = {
            {.resource_id = hdri_texture, .texture_info = {.sampler = hdri_sampler}},
            {.resource_id = cubemap_texture, .texture_info = {.sampler = default_sampler}},
        };
        device->update_uniform_set(uniform_set, bindings, (uint32_t)std::size(bindings));

        ComputeShader cubemap_shader("hdri_cubemap");
        cubemap_shader.create_from_file({"SPIRV/hdri-to-cubemap.comp.spv"});
        cubemap_shader.set_custom_bindings(&uniform_set, 1);

        CommandBuffer *command_buffer = device->get_command_buffer(0);
        command_buffer->begin();
        device->begin_debug_utils_label(command_buffer, "HDRI Conversion Pass", nullptr);

        generate_cubemap(command_buffer, cubemap_shader);

        device->end_debug_utils_label(command_buffer);

        device->submit_command_buffer_immediate(command_buffer);
        command_buffer->wait();

        device->destroy_uniform_sets(&uniform_set, 1);
        device->destroy_textures(&hdri_texture, 1);

        create_pbr_env_map();
    }

    EnvironmentMap::EnvironmentMap() {
        initialize_textures();

        SamplerDescription sampler_desc = SamplerDescription::create();

        RenderingDevice *device = RenderingDevice::get();

        ComputeShader cubemap_shader("hdri_cubemap");
        cubemap_shader.create_from_file({"SPIRV/procedural_sky.comp.spv"});

        CommandBuffer *command_buffer = device->get_command_buffer(0);
        command_buffer->begin();
        device->begin_debug_utils_label(command_buffer, "Procedural Sky Pass", nullptr);

        generate_cubemap(command_buffer, cubemap_shader);

        device->end_debug_utils_label(command_buffer);
        device->submit_command_buffer_immediate(command_buffer);
        command_buffer->wait();

        create_pbr_env_map();
    }

    void EnvironmentMap::create_pbr_env_map() {
        RenderingDevice *device = RenderingDevice::get();

        ComputeShader convolute_shader("convolute_cubemap_shader");
        convolute_shader.create_from_file("SPIRV/convolute_cubemap.comp.spv");

        ComputeShader prefilter_shader("prefilter_shader");
        prefilter_shader.create_from_file("SPIRV/prefilter-envmap.comp.spv");

        ComputeShader integrate_brdf_shader("integrate_brdf_shader");
        integrate_brdf_shader.create_from_file("SPIRV/integrate-brdf.comp.spv");

        CommandBuffer *command_buffer = device->get_command_buffer(0);
        command_buffer->begin();
        device->begin_debug_utils_label(command_buffer, "Environment Map Pass", nullptr);

        convolute_diffuse_cubemap(command_buffer, convolute_shader);
        convolute_specular_cubemap(command_buffer, prefilter_shader);
        integrate_brdf_texture(command_buffer, integrate_brdf_shader);

        device->end_debug_utils_label(command_buffer);
        device->submit_command_buffer_immediate(command_buffer);
        command_buffer->wait();

        SamplerDescription sampler_desc = SamplerDescription::create();
        sampler_desc.mipmap_mode = SAMPLER_MIPMAP_LINEAR;
        SamplerID prefilter_sampler = device->create_sampler(&sampler_desc);

        BindlessTextureEntry textures[] = {
            {.texture = irradiance_texture, .sampler = default_sampler},
            {.texture = brdf_texture, .sampler = default_sampler},
            {.texture = prefilter_texture, .sampler = prefilter_sampler},
        };
        device->add_bindless_texture(textures, cast_u32(std::size(textures)));
    }

    void EnvironmentMap::generate_cubemap(CommandBuffer *command_buffer, ComputeShader &cubemap_shader) {

        RenderingDevice *device = RenderingDevice::get();

        float push_constant_data[] = {(float)cubemap_size, (float)cubemap_size, 0.0f, 0.0f};
        PushConstant push_constant = {
            .data = push_constant_data,
            .shader_stage = SHADER_STAGE_COMPUTE,
            .size = sizeof(uint32_t) * 4,
            .offset = 0,
        };

        TextureBarrierInfo barrier_info = {
            .texture_id = cubemap_texture,
            .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            .access_mask = ACCESS_FLAG_SHADER_WRITE,
            .layout = IMAGE_LAYOUT_GENERAL,
        };

        command_buffer->prepare_image(&barrier_info, 1);

        cubemap_shader.bind(command_buffer);

        PipelineID pipeline_id = cubemap_shader.get_pipeline_id();
        command_buffer->set_push_constants(pipeline_id, &push_constant, 1);

        uint32_t work_size_x = rendering_utils::get_workgroup_size(cubemap_size, 32);
        uint32_t work_size_y = rendering_utils::get_workgroup_size(cubemap_size, 32);

        command_buffer->dispatch(work_size_x, work_size_y, 6);

        device->generate_mipmap(command_buffer, cubemap_texture, PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    }

    void EnvironmentMap::convolute_diffuse_cubemap(CommandBuffer *command_buffer, ComputeShader &convolute_shader) {
        // Layout transition
        TextureBarrierInfo barrier_infos[] = {
            {
                .texture_id = cubemap_texture, // CUBEMAP TEXTURE
                .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .access_mask = ACCESS_FLAG_SHADER_READ,
                .layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
            {
                .texture_id = irradiance_texture, // IRRADIANCE TEXTURE
                .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .access_mask = ACCESS_FLAG_SHADER_WRITE,
                .layout = IMAGE_LAYOUT_GENERAL,
            },
        };

        command_buffer->prepare_image(barrier_infos, cast_u32(std::size(barrier_infos)));

        RenderingDevice *device = RenderingDevice::get();
        UniformLayout layouts[] = {
            {0, BINDING_TYPE_COMBINED_IMAGE_SAMPLER, SHADER_STAGE_COMPUTE},
            {1, BINDING_TYPE_STORAGE_IMAGE, SHADER_STAGE_COMPUTE},
        };
        UniformSetID uniform_set = device->create_uniform_set(layouts, cast_u32(std::size(layouts)), 0, "temp_convolute_cubemap_set");
        UniformBinding bindings[] = {
            {.resource_id = cubemap_texture, .texture_info = {.sampler = default_sampler}},
            {.resource_id = irradiance_texture},
        };
        device->update_uniform_set(uniform_set, bindings, cast_u32(std::size(bindings)));

        float map_dims[] = {cast_float(irradiance_map_size), cast_float(irradiance_map_size),
                            cast_float(cubemap_size), cast_float(cubemap_size)};

        PushConstant push_constant = {
            .data = &map_dims,
            .shader_stage = SHADER_STAGE_COMPUTE,
            .size = sizeof(float) * 4,
            .offset = 0,
        };

        PipelineID pipeline_id = convolute_shader.get_pipeline_id();

        convolute_shader.bind(command_buffer);
        command_buffer->set_uniform_sets(pipeline_id, &uniform_set, 1);
        command_buffer->set_push_constants(pipeline_id, &push_constant, 1);

        uint32_t work_group_size = rendering_utils::get_workgroup_size(irradiance_map_size, 32);
        command_buffer->dispatch(work_group_size, work_group_size, 6);

        barrier_infos[1].stage_mask = PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        barrier_infos[1].access_mask = ACCESS_FLAG_SHADER_READ;
        barrier_infos[1].layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        command_buffer->prepare_image(&barrier_infos[1], 1);

        device->destroy_uniform_sets(&uniform_set, 1);
    }

    void EnvironmentMap::convolute_specular_cubemap(CommandBuffer *command_buffer, ComputeShader &prefilter_shader) {
        // Layout transition
        TextureBarrierInfo barrier_infos[] = {
            {
                .texture_id = cubemap_texture, // CUBEMAP TEXTURE
                .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .access_mask = ACCESS_FLAG_SHADER_READ,
                .layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
            {
                .texture_id = prefilter_texture, // PREFILTER TEXTURE
                .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .access_mask = ACCESS_FLAG_SHADER_WRITE,
                .layout = IMAGE_LAYOUT_GENERAL,
            },
        };

        command_buffer->prepare_image(barrier_infos, cast_u32(std::size(barrier_infos)));

        RenderingDevice *device = RenderingDevice::get();

        UniformLayout layouts[] = {
            {0, BINDING_TYPE_COMBINED_IMAGE_SAMPLER, SHADER_STAGE_COMPUTE},
            {1, BINDING_TYPE_STORAGE_IMAGE, SHADER_STAGE_COMPUTE},
        };
        std::vector<UniformSetID> uniform_sets(prefilter_num_mip_levels);

        UniformBinding bindings[] = {
            {.resource_id = cubemap_texture, .texture_info = {.sampler = default_sampler}},
            {.resource_id = prefilter_texture},
        };

        float map_dims[] = {cast_float(prefilter_map_size), cast_float(prefilter_map_size),
                            cast_float(cubemap_size), cast_float(cubemap_size)};

        PushConstant push_constant = {
            .data = &map_dims,
            .shader_stage = SHADER_STAGE_COMPUTE,
            .size = sizeof(float) * 4,
            .offset = 0,
        };

        for (uint32_t i = 0; i < prefilter_num_mip_levels; ++i) {

            uniform_sets[i] = device->create_uniform_set(layouts, cast_u32(std::size(layouts)), 0, "temp_uniform_set");
            bindings[1].texture_info.mip_levels = i;
            device->update_uniform_set(uniform_sets[i], bindings, cast_u32(std::size(bindings)));
        }

        prefilter_shader.bind(command_buffer);
        PipelineID pipeline_id = prefilter_shader.get_pipeline_id();
        uint32_t dims = prefilter_map_size;

        for (uint32_t i = 0; i < prefilter_num_mip_levels; ++i) {
            map_dims[3] = cast_float(i) / cast_float(prefilter_num_mip_levels - 1);
            map_dims[0] = cast_float(dims);
            map_dims[1] = cast_float(dims);

            command_buffer->set_push_constants(pipeline_id, &push_constant, 1);
            command_buffer->set_uniform_sets(pipeline_id, &uniform_sets[i], 1);
            uint32_t work_group_size = rendering_utils::get_workgroup_size(dims, 16);
            command_buffer->dispatch(work_group_size, work_group_size, 6);
            dims = dims / 2;
        }

        for (int i = 0; i < 2; ++i) {
            barrier_infos[i].stage_mask = PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            barrier_infos[i].access_mask = ACCESS_FLAG_SHADER_READ;
            barrier_infos[i].layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        command_buffer->prepare_image(barrier_infos, cast_u32(std::size(barrier_infos)));

        device->destroy_uniform_sets(uniform_sets.data(), cast_u32(uniform_sets.size()));
    }

    void EnvironmentMap::integrate_brdf_texture(CommandBuffer *command_buffer, ComputeShader &integrate_brdf_shader) {
        // Layout transition
        TextureBarrierInfo barrier_infos[] = {
            {
                .texture_id = brdf_texture, // BRDF TEXTURE
                .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .access_mask = ACCESS_FLAG_SHADER_READ,
                .layout = IMAGE_LAYOUT_GENERAL,
            },
        };

        command_buffer->prepare_image(barrier_infos, cast_u32(std::size(barrier_infos)));

        RenderingDevice *device = RenderingDevice::get();
        UniformLayout layouts[] = {
            {0, BINDING_TYPE_STORAGE_IMAGE, SHADER_STAGE_COMPUTE},
        };
        UniformSetID uniform_set = device->create_uniform_set(layouts, cast_u32(std::size(layouts)), 0, "temp_brdf_set");
        UniformBinding bindings[] = {
            {.resource_id = brdf_texture},
        };
        device->update_uniform_set(uniform_set, bindings, cast_u32(std::size(bindings)));

        float inv_brdf_texture_size = 1.0f / cast_float(brdf_texture_size);
        float map_dims[] = {inv_brdf_texture_size, inv_brdf_texture_size};

        PushConstant push_constant = {
            .data = &map_dims,
            .shader_stage = SHADER_STAGE_COMPUTE,
            .size = sizeof(float) * 2,
            .offset = 0,
        };

        integrate_brdf_shader.bind(command_buffer);

        PipelineID pipeline_id = integrate_brdf_shader.get_pipeline_id();
        command_buffer->set_uniform_sets(pipeline_id, &uniform_set, 1);
        command_buffer->set_push_constants(pipeline_id, &push_constant, 1);

        uint32_t work_group_size = rendering_utils::get_workgroup_size(brdf_texture_size, 32);
        command_buffer->dispatch(work_group_size, work_group_size, 1);

        barrier_infos[0].stage_mask = PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        barrier_infos[0].access_mask = ACCESS_FLAG_SHADER_READ;
        barrier_infos[0].layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        command_buffer->prepare_image(barrier_infos, 1);
    }

    EnvironmentMap::~EnvironmentMap() {
        RenderingDevice *device = RenderingDevice::get();
        TextureID textures[] = {cubemap_texture, irradiance_texture, prefilter_texture, brdf_texture};
        device->destroy_textures(textures, cast_u32(std::size(textures)));
    }

} // namespace mirai