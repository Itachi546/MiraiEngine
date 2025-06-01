#include "DirectionalShadowPassRT.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Camera.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Engine/Profiler.hpp"

namespace mirai {
    void DirectionalShadowPassRT::initialize(FrameGraph *frame_graph, const FrameGraphNode *node) {
        shader = std::make_unique<ComputeShader>("rt_directional_light");
        shader->create_from_file("SPIRV/rt_directional_shadow.comp.spv");

        UniformLayout layouts[] = {
            {0, BINDING_TYPE_STORAGE_IMAGE, SHADER_STAGE_COMPUTE},
            {1, BINDING_TYPE_COMBINED_IMAGE_SAMPLER, SHADER_STAGE_COMPUTE},
            {2, BINDING_TYPE_ACCELERATION_STRUCTURE, SHADER_STAGE_COMPUTE},
        };

        ASSERT(node->outputs.size() == 1);
        ASSERT(node->inputs.size() == 2);

        TextureID rt_shadow_texture = frame_graph->get_resource(node->outputs[0])->handle;
        TextureID depth_texture = frame_graph->get_resource(node->inputs[0])->handle;
        TextureID normal_texture = frame_graph->get_resource(node->inputs[1])->handle;

        SamplerDescription sampler_desc = SamplerDescription::create();
        SamplerID depth_sampler = device->create_sampler(&sampler_desc);

        UniformBinding bindings[] = {
            {.resource_id = rt_shadow_texture},
            {.resource_id = depth_texture, .texture_info = {.sampler = depth_sampler}},
            // Acceleration structure is global and populated by the vulkan device
            {.resource_id = K_INVALID_ID},
        };

        // SSAO Uniform Set
        rt_set = device->create_uniform_set(layouts, cast_u32(std::size(layouts)), 0, "ssao_uniform_set");
        device->update_uniform_set(rt_set, bindings, cast_u32(std::size(bindings)));
        shader->set_uniform_sets(&rt_set, 1);
    }

    void DirectionalShadowPassRT::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) {
        ScopedCpuProfiling("RT Shadow Pass");
        ScopedGpuProfiling(command_buffer, "RT Shadow Pass");
        device->begin_debug_utils_label(command_buffer, "RT Shadow Pass", nullptr);

        struct ShaderData {
            glm::mat4 invVP;
            glm::vec3 light_direction;
            float width;
            float height;
        } shader_data;

        Camera *camera = scene->get_camera();
        shader_data.invVP = camera->get_inv_view_projection_transform();
        shader_data.light_direction = scene->get_sun()->get_direction();
        shader_data.width = cast_float(node->width);
        shader_data.height = cast_float(node->height);

        command_buffer->begin_compute_pass(node, frame_graph);
        PushConstant push_constant = {
            .data = &shader_data,
            .shader_stage = SHADER_STAGE_COMPUTE,
            .size = sizeof(shader_data),
            .offset = 0,
        };

        shader->set_push_constant(&push_constant, 1);
        shader->bind(command_buffer);

        uint32_t work_size_x = rendering_utils::get_workgroup_size(node->width, 32);
        uint32_t work_size_y = rendering_utils::get_workgroup_size(node->height, 32);

        command_buffer->dispatch(work_size_x, work_size_y, 1);

        device->end_debug_utils_label(command_buffer);
    } // namespace mirai

    DirectionalShadowPassRT::~DirectionalShadowPassRT() {
    }
} // namespace mirai