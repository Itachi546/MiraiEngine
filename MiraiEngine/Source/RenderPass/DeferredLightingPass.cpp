#include "DeferredLightingPass.hpp"
#include "Scene/Scene.hpp"
#include "Scene/EnvironmentMap.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Scene/Camera.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"
#include "Math/Math.hpp"

namespace mirai {
    DeferredLightingPass::DeferredLightingPass() : FrameGraphRenderer("deferred_lighting_pass"), shader(nullptr), uniform_set(K_INVALID_ID) {
    }

    void DeferredLightingPass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node) {

        shader = std::make_shared<ShaderMaterial>("DeferredLightingPasMaterial");
        shader->create_from_file({
            "SPIRV/fullscreen.vert.spv",
            "SPIRV/deferred_shading.frag.spv",
        });
        shader->set_depth_write(false);
        shader->set_depth_test(false);

        // Deferred Shading Textures
        uint32_t binding_count = static_cast<uint32_t>(node->inputs.size());

        UniformLayout layouts[] = {
            {.binding = 0, .binding_type = BINDING_TYPE_COMBINED_IMAGE_SAMPLER, .shader_stage = SHADER_STAGE_FRAGMENT},
            {.binding = 1, .binding_type = BINDING_TYPE_COMBINED_IMAGE_SAMPLER, .shader_stage = SHADER_STAGE_FRAGMENT},
            {.binding = 2, .binding_type = BINDING_TYPE_COMBINED_IMAGE_SAMPLER, .shader_stage = SHADER_STAGE_FRAGMENT},
            {.binding = 3, .binding_type = BINDING_TYPE_COMBINED_IMAGE_SAMPLER, .shader_stage = SHADER_STAGE_FRAGMENT},
            {.binding = 4, .binding_type = BINDING_TYPE_COMBINED_IMAGE_SAMPLER, .shader_stage = SHADER_STAGE_FRAGMENT},
            {.binding = 5, .binding_type = BINDING_TYPE_COMBINED_IMAGE_SAMPLER, .shader_stage = SHADER_STAGE_FRAGMENT},
        };

        SamplerDescription desc = SamplerDescription::create();
        SamplerID default_sampler = device->create_sampler(&desc);

        desc.min_filter = desc.mag_filter = FILTER_NEAREST;
        SamplerID depth_sampler = device->create_sampler(&desc);

        UniformBinding bindings[] = {
            {.resource_id = frame_graph->get_resource("gbuffer_color")->handle, .texture_info = {.sampler = default_sampler}},
            {.resource_id = frame_graph->get_resource("gbuffer_depth")->handle, .texture_info = {.sampler = depth_sampler}},
            {.resource_id = frame_graph->get_resource("gbuffer_normal")->handle, .texture_info = {.sampler = default_sampler}},
            {.resource_id = frame_graph->get_resource("gbuffer_emissive")->handle, .texture_info = {.sampler = default_sampler}},
            {.resource_id = frame_graph->get_resource("cascaded_shadow_map")->handle, .texture_info = {.sampler = depth_sampler}},
            {.resource_id = frame_graph->get_resource("ssao_texture")->handle, .texture_info = {.sampler = default_sampler}},
        };

        /*
        for (uint32_t i = 0; i < binding_count; ++i) {
            FrameGraphResource *resource = frame_graph->get_resource(node->inputs[i]);
            binding_layout[i] = UniformLayout{.binding = i, .binding_type = BINDING_TYPE_COMBINED_IMAGE_SAMPLER, .shader_stage = SHADER_STAGE_FRAGMENT};
            bindings[i] = UniformBinding{.resource_id = resource->handle};
        }
        */
        uniform_set = device->create_uniform_set(layouts, cast_u32(std::size(layouts)), 0, "deferred_binding_set");
        device->update_uniform_set(uniform_set, bindings, cast_u32(std::size(bindings)));

        UniformLayout cascade_data = {
            .binding = 0,
            .binding_type = BINDING_TYPE_UNIFORM_BUFFER,
            .shader_stage = SHADER_STAGE_FRAGMENT,
        };
        cascade_uniform_set = device->create_uniform_set(&cascade_data, 1, 2, "cascade_info_set");
    }

    void DeferredLightingPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) {
        ASSERT(node != nullptr);

        Camera *camera = scene->get_camera();
        struct {
            glm::mat4 inv_VP;
            glm::vec4 camera_position;
            glm::vec4 light_direction;
            glm::vec4 light_color;
            uint32_t irradiance_map;
            uint32_t prefilter_map;
            uint32_t brdf_texture;
        } push_constant_data;

        push_constant_data.inv_VP = camera->get_inv_view_projection_transform();
        push_constant_data.camera_position = glm::vec4(camera->position, 0.0f);

        Light *sun = scene->get_sun();
        glm::vec3 light_direction = sun->get_direction(); 
        push_constant_data.light_direction = glm::vec4(light_direction, (float)sun->cast_shadow);
        push_constant_data.light_color = glm::vec4(sun->color, sun->intensity);

        EnvironmentMap *env_map = scene->get_environment_map();
        push_constant_data.irradiance_map = env_map->get_irradiance_map().id;
        push_constant_data.prefilter_map = env_map->get_prefilter_map().id;
        push_constant_data.brdf_texture = env_map->get_brdf_texture().id;

        ScopedGpuProfiling(command_buffer, "Deferred Lighting");

        device->begin_debug_utils_label(command_buffer, "DeferredLightingPass", nullptr);

        command_buffer->begin_render_pass(node, frame_graph);

        UniformBinding cascade_binding = {.resource_id = scene->directional_light_info.cascade_uniform_buffer};
        device->update_uniform_set(cascade_uniform_set, &cascade_binding, 1);

        UniformSetID uniform_sets[] = {uniform_set, cascade_uniform_set};
        shader->set_uniform_sets(uniform_sets, static_cast<uint32_t>(std::size(uniform_sets)));

        PushConstant push_constant = {.data = &push_constant_data, .shader_stage = SHADER_STAGE_FRAGMENT, .size = sizeof(push_constant_data), .offset = 0};
        shader->set_push_constant(&push_constant, 1);

        shader->bind(command_buffer, &node->renderpass_info);

        command_buffer->draw(3, 1, 0, 0);

        command_buffer->end_render_pass();

        RenderingDevice::get()->end_debug_utils_label(command_buffer);
    }

    DeferredLightingPass::~DeferredLightingPass() {
    }
} // namespace mirai