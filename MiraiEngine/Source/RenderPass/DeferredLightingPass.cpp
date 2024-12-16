#include "DeferredLightingPass.hpp"
#include "Scene/Scene.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Scene/Camera.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"

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
        std::vector<UniformBinding> bindings(binding_count);
        std::vector<UniformLayout> binding_layout(binding_count);

        for (uint32_t i = 0; i < binding_count; ++i) {
            FrameGraphResource *resource = frame_graph->get_resource(node->inputs[i]);
            binding_layout[i] = UniformLayout{.binding = i, .binding_type = BINDING_TYPE_COMBINED_IMAGE_SAMPLER, .shader_stage = SHADER_STAGE_FRAGMENT};
            bindings[i] = UniformBinding{.resource_id = resource->handle};
        }

        uniform_set = device->create_uniform_set(binding_layout.data(), binding_count, 0, "deferred_binding_set");
        device->update_uniform_set(uniform_set, bindings.data(), static_cast<uint32_t>(bindings.size()));

        UniformLayout cascade_data = {
            .binding = 0,
            .binding_type = BINDING_TYPE_UNIFORM_BUFFER,
            .shader_stage = SHADER_STAGE_FRAGMENT,
        };
        cascade_uniform_set = device->create_uniform_set(&cascade_data, 1, 1, "cascade_info_set");
    }

    void DeferredLightingPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) {
        ASSERT(node != nullptr);

        Camera *camera = scene->get_camera();
        struct {
            glm::mat4 inv_VP;
            glm::vec4 camera_position;
            glm::vec4 light_direction;
        } push_constant_data;
        push_constant_data.inv_VP = camera->get_inv_view_projection_transform();
        push_constant_data.camera_position = glm::vec4(camera->position, 0.0f);
        push_constant_data.light_direction = glm::vec4(scene->get_sun()->direction, scene->get_sun()->intensity);

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