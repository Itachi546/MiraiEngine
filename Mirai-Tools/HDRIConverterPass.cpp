#include "HDRIConverterPass.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"

void HDRIConverterPass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node) {
    shader = std::make_unique<ComputeShader>("hdri_converter");
    shader->create_from_file({"SPIRV/hdri-to-cubemap.comp.spv"});

    SamplerDescription sampler_desc = SamplerDescription::create();
    sampler_desc.enable_anisotropy = false;

    TextureDescription texture_desc = {
        .width = (uint32_t)width,
        .height = (uint32_t)height,
        .depth = 1,
        .mip_levels = 1,
        .array_layers = 6,
        .texture_type = TEXTURE_TYPE_CUBE,
        .format = FORMAT_R16G16B16A16_SFLOAT,
        .usage_flags = TEXTURE_USAGE_STORAGE_BIT | TEXTURE_USAGE_SAMPLED_BIT,
        .sampler_desc = &sampler_desc,
    };
    cubemap = device->create_texture(&texture_desc, "cubemap");

    UniformLayout layout[] = {
        {0, BINDING_TYPE_COMBINED_IMAGE_SAMPLER, SHADER_STAGE_COMPUTE},
        {1, BINDING_TYPE_STORAGE_IMAGE, SHADER_STAGE_COMPUTE},
    };
    uniform_set = device->create_uniform_set(layout, (uint32_t)std::size(layout), 0, "hdri_to_cubemap_set");
}

void HDRIConverterPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) {
    if (!dirty)
        return;

    UniformBinding bindings[] = {
        {.resource_id = hdri},
        {.resource_id = cubemap},
    };
    device->update_uniform_set(uniform_set, bindings, (uint32_t)std::size(bindings));
    shader->set_uniform_sets(&uniform_set, 1);

    float push_constant_data[] = {(float)width, (float)height, 0.0f, 0.0f};
    PushConstant push_constant = {
        .data = push_constant_data,
        .shader_stage = SHADER_STAGE_COMPUTE,
        .size = sizeof(uint32_t) * 4,
        .offset = 0,
    };
    shader->set_push_constant(&push_constant, 1);

    TextureBarrierInfo barrier_info = {
        .texture_id = cubemap,
        .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .access_mask = ACCESS_FLAG_SHADER_WRITE,
        .layout = IMAGE_LAYOUT_GENERAL,
    };

    command_buffer->prepare_image(&barrier_info, 1);

    shader->bind(command_buffer);

    uint32_t work_size_x = rendering_utils::get_workgroup_size(width, 32);
    uint32_t work_size_y = rendering_utils::get_workgroup_size(height, 32);

    command_buffer->dispatch(work_size_x, work_size_y, 6);

    barrier_info.stage_mask = PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    barrier_info.access_mask = ACCESS_FLAG_SHADER_READ;
    barrier_info.layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    command_buffer->prepare_image(&barrier_info, 1);
    dirty = false;
}

HDRIConverterPass::~HDRIConverterPass() {
    device->destroy_textures(&cubemap, 1);
}
