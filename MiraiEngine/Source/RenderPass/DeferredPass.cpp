#include "DeferredPass.hpp"
#include "Scene/Scene.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Scene/Camera.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"

namespace mirai {
    DeferredPass::DeferredPass() : FrameGraphRenderPass("deferred_pass"), shader(nullptr), uniform_set(K_INVALID_ID) {
    }

    void DeferredPass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node) {

        shader = std::make_shared<ShaderMaterial>("DeferredPassMaterial");
        shader->create_from_file({
            "SPIRV/fullscreen.vert.spv",
            "SPIRV/deferred_shading.frag.spv",
        });
        shader->set_depth_write(false);
        shader->set_depth_test(false);

        // Deferred Shading Textures
        uint32_t binding_count = static_cast<uint32_t>(node->inputs.size());
        std::vector<UniformBinding> bindings;
        std::vector<UniformLayout> binding_layout;

        for (uint32_t i = 0; i < binding_count; ++i) {
            FrameGraphResource *resource = frame_graph->get_resource(node->inputs[i]);
            ASSERT(resource->resource_type == FRAMEGRAPH_RESOURCE_TYPE_TEXTURE);
            binding_layout.push_back(UniformLayout{.binding = i, .binding_type = BINDING_TYPE_COMBINED_IMAGE_SAMPLER, .shader_stage = SHADER_STAGE_FRAGMENT});
            bindings.push_back(UniformBinding{.resource_id = resource->resource_info.texture});
        }

        uniform_set = RenderingDevice::get()->create_uniform_set(binding_layout.data(), binding_count, 0, "deferred_binding_set");
        RenderingDevice::get()->update_uniform_set(uniform_set, bindings.data(), static_cast<uint32_t>(bindings.size()));
    }

    void DeferredPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) {
        ASSERT(node != nullptr);

        glm::mat4 inv_VP = scene->get_camera()->get_inv_view_projection_transform();

        ScopedGpuProfiling(command_buffer, "Deferred Shading");

        RenderingDevice::get()->begin_debug_utils_label(command_buffer, "DeferredPass", nullptr);

        command_buffer->begin_render_pass(node, frame_graph);

        shader->set_uniform_sets(&uniform_set, 1);

        PushConstant push_constant = {.data = &inv_VP[0][0], .shader_stage = SHADER_STAGE_FRAGMENT, .size = sizeof(glm::mat4), .offset = 0};
        shader->set_push_constant(&push_constant, 1);

        shader->bind(command_buffer, node, frame_graph);

        command_buffer->draw(3, 1, 0, 0);

        command_buffer->end_render_pass();

        RenderingDevice::get()->end_debug_utils_label(command_buffer);
    }

    DeferredPass::~DeferredPass() {
    }
} // namespace mirai