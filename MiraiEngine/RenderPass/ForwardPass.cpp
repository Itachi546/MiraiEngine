#include "ForwardPass.hpp"
#include "Scene/Scene.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"

namespace mirai
{
    ForwardPass::ForwardPass(const std::string &name) : FrameGraphRenderPass(name)
    {
        shader = std::make_shared<ShaderMaterial>("TriangleMaterial");
        shader->create_from_file({
            "SPIRV/main.vert.spv",
            "SPIRV/main.frag.spv",
        });
        shader->set_depth_write(true);
        shader->set_depth_test(true);

        UniformLayout mesh_data_layout[] = {
            {.binding = 0, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_VERTEX},
        };

        UniformLayout mesh_instance_layout[] = {
            {.binding = 0, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_VERTEX},
        };

        mesh_instance_set = RenderingDevice::get()->create_uniform_set(mesh_instance_layout, (uint32_t)std::size(mesh_instance_layout), 2, "mesh_instance_set");
    }

    void ForwardPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, const FrameGraphNode *node, Scene *scene)
    {
        ASSERT(node != nullptr);
        command_buffer->begin_render_pass(node, frame_graph);

        std::vector<DrawData> &draw_infos = scene->draw_infos;
        if (draw_infos.size() > 0)
        {
            UniformBinding per_shader_bindings[] = {
                {.resource_id = scene->transform_buffer, .offset = 0},
            };
            RenderingDevice::get()->update_uniform_set(mesh_instance_set, per_shader_bindings, (uint32_t)std::size(per_shader_bindings));

            UniformSetID uniform_sets[] = {scene->per_frame_uniform_set, mesh_instance_set};
            shader->set_uniform_sets(uniform_sets, (uint32_t)std::size(uniform_sets));
            shader->bind(command_buffer, node, frame_graph);

            PushConstant push_constant = {.data = nullptr, .shader_stage = SHADER_STAGE_VERTEX, .size = sizeof(uint32_t) * 4, .offset = 0};
            PipelineID pipeline_id = shader->get_pipeline_id();
            uint32_t last_buffer_id = K_INVALID_ID;
            for (uint32_t i = 0; i < draw_infos.size(); ++i)
            {
                BufferID current_buffer = draw_infos[i].vertex_buffer;
                if (current_buffer.id != last_buffer_id)
                {
                    command_buffer->set_index_buffer(draw_infos[i].index_buffer);
                    last_buffer_id = current_buffer.id;
                    command_buffer->set_uniform_sets(pipeline_id, &draw_infos[i].vertex_binding_set, 1);
                }

                uint32_t instance_data[] = {draw_infos[i].transform_index, 0, 0, 0};
                push_constant.data = instance_data;
                command_buffer->set_push_constants(pipeline_id, &push_constant, 1);
                command_buffer->draw_indexed(draw_infos[i].index_count,
                                             1,
                                             draw_infos[i].index_offset,
                                             draw_infos[i].vertex_offset,
                                             0);
            }
        }
        command_buffer->end_render_pass();
    }

    ForwardPass::~ForwardPass()
    {
    }
} // namespace mirai