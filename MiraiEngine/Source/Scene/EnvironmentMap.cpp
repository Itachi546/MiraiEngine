#include "EnvironmentMap.hpp"
#include "Common/FileUtils.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Graphics/Renderer.hpp"
#include "Engine/AppSettings.hpp"
#include <cmath>

namespace mirai {

    void EnvironmentMap::initialize_textures() {
        uint32_t cubemap_size = EnvironmentSettings::K_CUBEMAP_SIZE;
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

        uint32_t irradiance_map_size = EnvironmentSettings::K_IRRADIANCE_MAP_SIZE;
        cubemap_texture = device->create_texture(&texture_desc, "cubemap");
        texture_desc.width = irradiance_map_size;
        texture_desc.height = irradiance_map_size;
        texture_desc.mip_levels = 1;
        texture_desc.usage_flags = TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_STORAGE_BIT;
        irradiance_texture = device->create_texture(&texture_desc, "cubemap_irradiance");

        // Create Prefilter Environment map texture
        uint32_t prefilter_map_size = EnvironmentSettings::K_PREFILTER_MAP_SIZE;
        texture_desc.create_flags = TEXTURE_CREATION_FLAG_IMAGE_VIEW_PER_MIP;
        texture_desc.mip_levels = EnvironmentSettings::K_PREFILTER_MAP_MAX_MIP_LEVELS;
        texture_desc.width = prefilter_map_size;
        texture_desc.height = prefilter_map_size;
        prefilter_texture = device->create_texture(&texture_desc, "prefilter_envmap");

        // Create 2D BRDF Texture
        uint32_t brdf_texture_size = EnvironmentSettings::K_BRDF_MAP_SIZE;
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
        auto data = utils::load_image_float(hdri_path.c_str(), &width, &height, &n_channel, 4);
        if (data == nullptr) {
            Log::Error("Failed to load hdri: ", hdri_path);
        }

        RenderingDevice *device = RenderingDevice::get();

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
        rendering_utils::copy_texture_immediate(hdri_texture, data.get(), width * height * sizeof(float) * 4);

        data.reset();
        data = nullptr;

        Log::Debug("Loading Environment map");

        initialize_textures();

        ComputeShader cubemap_shader{"hdri-cubemap", "SPIRV/hdri-to-cubemap.comp.spv"};
        ComputeShader convolute_shader{"ConvoluteEnvMap", "SPIRV/convolute-cubemap.comp.spv"};
        ComputeShader prefilter_shader{"PrefilterEnvMap", "SPIRV/prefilter-envmap.comp.spv"};
        ComputeShader integrate_brdf_shader{"IntegrateBRDF", "SPIRV/integrate-brdf.comp.spv"};

        DescriptorInfo descriptor_infos[] = {
            {.type = DescriptorType::SampledImage, .resource = hdri_texture, .image_info = {0, ~0u, 0, ~0u}},
            {.type = DescriptorType::StorageImage, .resource = cubemap_texture, .image_info = {0, ~0u, 0, ~0u}},
        };

        Renderer *renderer = Renderer::get();
        DescriptorOffset descriptor_index = renderer->resource_heap.push_descriptors_per_frame(device, descriptor_infos, cast_u32(std::size(descriptor_infos)));

        CommandBuffer *command_buffer = device->get_command_buffer(0);
        command_buffer->begin();

        command_buffer->bind_resource_heap(renderer->resource_heap.buffer);
        command_buffer->bind_sampler_heap(renderer->sampler_heap.buffer);

        command_buffer->begin_gpu_debug_label("EnvironmentMap");

        uint32_t descriptors[] = {descriptor_index, descriptor_index + 1};

        generate_cubemap(command_buffer, &cubemap_shader, descriptors, 2);

        convolute_diffuse_cubemap(command_buffer, &convolute_shader);

        convolute_specular_cubemap(command_buffer, &prefilter_shader);

        integrate_brdf_texture(command_buffer, &integrate_brdf_shader);

        command_buffer->end_gpu_debug_label();

        device->submit_command_buffer_immediate(command_buffer);

        command_buffer->wait();

        device->destroy_textures(&hdri_texture, 1);

        renderer->add_bindless_texture(cubemap_texture);
        renderer->add_bindless_texture(irradiance_texture);
        renderer->add_bindless_texture(prefilter_texture);
        renderer->add_bindless_texture(brdf_texture);
    }

    EnvironmentMap::EnvironmentMap() {
        initialize_textures();

        DescriptorInfo descriptor_infos[] = {
            {.type = DescriptorType::StorageImage, .resource = cubemap_texture, .image_info = {0, ~0u, 0, ~0u}},
        };
        Renderer *renderer = Renderer::get();
        DescriptorOffset descriptor_index = renderer->resource_heap.push_descriptors_per_frame(RenderingDevice::get(), descriptor_infos, cast_u32(std::size(descriptor_infos)));

        RenderingDevice *device = RenderingDevice::get();
        ComputeShader generate_cubemap_shader{"generate-cubemap", "SPIRV/procedural-sky.comp.spv"};
        ComputeShader convolute_shader{"ConvoluteEnvMap", "SPIRV/convolute-cubemap.comp.spv"};
        ComputeShader prefilter_shader{"PrefilterEnvMap", "SPIRV/prefilter-envmap.comp.spv"};
        ComputeShader integrate_brdf_shader{"IntegrateBRDF", "SPIRV/integrate-brdf.comp.spv"};

        CommandBuffer *command_buffer = device->get_command_buffer(0);
        command_buffer->begin();
        command_buffer->begin_gpu_debug_label("EnvironmentMap");

        command_buffer->bind_resource_heap(renderer->resource_heap.buffer);
        command_buffer->bind_sampler_heap(renderer->sampler_heap.buffer);

        generate_cubemap(command_buffer, &generate_cubemap_shader, &descriptor_index, 1);

        convolute_diffuse_cubemap(command_buffer, &convolute_shader);

        convolute_specular_cubemap(command_buffer, &prefilter_shader);

        integrate_brdf_texture(command_buffer, &integrate_brdf_shader);

        command_buffer->end_gpu_debug_label();

        device->submit_command_buffer_immediate(command_buffer);

        command_buffer->wait();
    }

    void EnvironmentMap::generate_cubemap(CommandBuffer *command_buffer, ComputeShader *cubemap_shader, uint32_t *descriptors, uint32_t descriptor_count) {

        RenderingDevice *device = RenderingDevice::get();

        uint32_t cubemap_size = EnvironmentSettings::K_CUBEMAP_SIZE;
        float push_data[] = {cast_float(cubemap_size), cast_float(cubemap_size), 0.0f, 0.0f};

        TextureBarrierInfo barrier_info = {
            .texture_id = cubemap_texture,
            .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            .access_mask = ACCESS_FLAG_SHADER_WRITE,
            .layout = IMAGE_LAYOUT_GENERAL,
        };

        command_buffer->prepare_image(&barrier_info, 1);

        cubemap_shader->bind(command_buffer);

        uint32_t push_data_size = sizeof(float) * 4;
        command_buffer->set_push_data(0, push_data, push_data_size);
        command_buffer->set_push_data(push_data_size, descriptors, descriptor_count * sizeof(uint32_t));

        uint32_t work_size_x = rendering_utils::get_workgroup_size(cubemap_size, 32);
        uint32_t work_size_y = rendering_utils::get_workgroup_size(cubemap_size, 32);

        command_buffer->dispatch(work_size_x, work_size_y, 6);

        device->generate_mipmap(command_buffer, cubemap_texture, PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    }
    void EnvironmentMap::convolute_diffuse_cubemap(CommandBuffer *command_buffer, ComputeShader *convolute_shader) {
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
        Renderer *renderer = Renderer::get();

        DescriptorInfo descriptor_infos[] = {
            {.type = DescriptorType::SampledImage, .resource = cubemap_texture, .image_info = {0, ~0u, 0, ~0u}},
            {.type = DescriptorType::StorageImage, .resource = irradiance_texture, .image_info = {0, ~0u, 0, ~0u}},
        };
        DescriptorOffset descriptor_index = renderer->resource_heap.push_descriptors_per_frame(device, descriptor_infos, cast_u32(std::size(descriptor_infos)));
        uint32_t descriptors[] = {descriptor_index, descriptor_index + 1};

        uint32_t irradiance_map_size = EnvironmentSettings::K_IRRADIANCE_MAP_SIZE;
        uint32_t cubemap_size = EnvironmentSettings::K_CUBEMAP_SIZE;
        float map_dims[] = {cast_float(irradiance_map_size), cast_float(irradiance_map_size),
                            cast_float(cubemap_size), cast_float(cubemap_size)};

        convolute_shader->bind(command_buffer);
        command_buffer->set_push_data(0, map_dims, sizeof(float) * 4);
        command_buffer->set_push_data(sizeof(float) * 4, &descriptors, cast_u32(sizeof(uint32_t) * 2));

        uint32_t work_group_size = rendering_utils::get_workgroup_size(irradiance_map_size, 32);
        command_buffer->dispatch(work_group_size, work_group_size, 6);

        barrier_infos[1].stage_mask = PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        barrier_infos[1].access_mask = ACCESS_FLAG_SHADER_READ;
        barrier_infos[1].layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        command_buffer->prepare_image(&barrier_infos[1], 1);
    }

    void EnvironmentMap::convolute_specular_cubemap(CommandBuffer *command_buffer, ComputeShader *prefilter_shader) {
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
        Renderer *renderer = Renderer::get();

        DescriptorInfo cubemap_descriptor_info = {.type = DescriptorType::SampledImage, .resource = cubemap_texture, .image_info = {0, ~0u, 0, ~0u}};
        DescriptorOffset cubemap_descriptor = renderer->resource_heap.push_descriptors_per_frame(device, &cubemap_descriptor_info, 1);

        DescriptorInfo prefilter_map_descriptor_info = {.type = DescriptorType::StorageImage, .resource = prefilter_texture, .image_info = {0, 1, 0, ~0u}};

        uint32_t num_mip_levels = EnvironmentSettings::K_PREFILTER_MAP_MAX_MIP_LEVELS;
        std::vector<DescriptorOffset> output_image_descriptors(num_mip_levels);
        for (uint32_t i = 0; i < num_mip_levels; ++i) {
            prefilter_map_descriptor_info.image_info.base_mip_level = i;
            output_image_descriptors[i] = renderer->resource_heap.push_descriptors_per_frame(device, &prefilter_map_descriptor_info, 1);
        }

        uint32_t cubemap_size = EnvironmentSettings::K_CUBEMAP_SIZE;
        float push_data[] = {0.0f, 0.0f, cast_float(cubemap_size), 0.0f};
        uint32_t push_data_size = cast_u32(sizeof(push_data));

        prefilter_shader->bind(command_buffer);
        command_buffer->set_push_data(push_data_size, &cubemap_descriptor, cast_u32(sizeof(uint32_t)));

        uint32_t dims = EnvironmentSettings::K_PREFILTER_MAP_SIZE;
        for (uint32_t i = 0; i < num_mip_levels; ++i) {
            push_data[0] = cast_float(dims);
            push_data[1] = cast_float(dims);
            push_data[3] = cast_float(i) / cast_float(num_mip_levels - 1);

            command_buffer->set_push_data(0, push_data, push_data_size);
            command_buffer->set_push_data(push_data_size + sizeof(uint32_t), output_image_descriptors.data() + i, cast_u32(sizeof(uint32_t)));

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
    }

    void EnvironmentMap::integrate_brdf_texture(CommandBuffer *command_buffer, ComputeShader *integrate_brdf_shader) {
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

        uint32_t brdf_texture_size = EnvironmentSettings::K_BRDF_MAP_SIZE;

        DescriptorInfo descriptor_info = {.type = DescriptorType::StorageImage, .resource = brdf_texture, .image_info = {0, ~0u, 0, ~0u}};
        DescriptorOffset descriptor = Renderer::get()->resource_heap.push_descriptors_per_frame(device, &descriptor_info, 1);

        float inv_brdf_texture_size = 1.0f / cast_float(brdf_texture_size);
        float map_dims[] = {inv_brdf_texture_size, inv_brdf_texture_size, 0.0f, 0.0f};

        integrate_brdf_shader->bind(command_buffer);

        command_buffer->set_push_data(0, map_dims, sizeof(float) * 4);
        command_buffer->set_push_data(sizeof(float) * 4, &descriptor, cast_u32(sizeof(uint32_t)));

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