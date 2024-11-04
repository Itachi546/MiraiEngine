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

        BufferDescription buffer_desc = {
            .size = 1000 * sizeof(DrawIndirectCommand),
            .usage_flags = BUFFER_USAGE_INDIRECT_BUFFER_BIT,
            .allocation_type = MEMORY_ALLOCATION_TYPE_CPU,
        };

        draw_indirect_buffer = RenderingDevice::get()->create_buffer(&buffer_desc, "draw_indirect_buffer");
        draw_indirect_array = (DrawIndirectCommand *)RenderingDevice::get()->map_buffer(draw_indirect_buffer);

        UniformLayout mesh_data_layout = {.binding = 0, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_VERTEX};
        UniformLayout mesh_instance_layout = {.binding = 0, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_VERTEX};

        mesh_instance_set = RenderingDevice::get()->create_uniform_set(&mesh_instance_layout, 1, 2, "mesh_instance_set");
    }

    void ForwardPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, const FrameGraphNode *node, Scene *scene)
    {
        ASSERT(node != nullptr);

        command_buffer->begin_render_pass(node, frame_graph);

        std::vector<DrawData> &draw_infos = scene->draw_infos;
        if (draw_infos.size() > 0)
        {
            UniformSetID uniform_sets[] = {scene->per_frame_uniform_set, mesh_instance_set};
            shader->set_uniform_sets(uniform_sets, (uint32_t)std::size(uniform_sets));
            shader->bind(command_buffer, node, frame_graph);

            UniformBinding per_shader_bindings[] = {
                {.resource_id = scene->transform_buffer, .offset = 0},
            };
            RenderingDevice::get()->update_uniform_set(mesh_instance_set, per_shader_bindings, (uint32_t)std::size(per_shader_bindings));

            UniformBinding per_draw_bindings = {.resource_id = BufferID{K_INVALID_ID}, .offset = 0};
            PushConstant push_constant = {.data = nullptr, .shader_stage = SHADER_STAGE_VERTEX, .size = sizeof(uint32_t), .offset = 0};

            PipelineID pipeline_id = shader->get_pipeline_id();
            for (uint32_t i = 0; i < draw_infos.size(); ++i)
            {
                BufferID current_buffer = draw_infos[i].vertex_buffer;
                if (current_buffer.id != per_draw_bindings.resource_id.id)
                {
                    per_draw_bindings.resource_id = current_buffer;
                    command_buffer->set_uniform_sets(pipeline_id, &draw_infos[i].uniform_set, 1);
                    command_buffer->set_index_buffer(draw_infos[i].index_buffer);
                }
                push_constant.data = &draw_infos[i].transform_index;
                command_buffer->set_push_constants(pipeline_id, &push_constant, 1);
                command_buffer->draw_indexed(draw_infos[i].index_count,
                                             1,
                                             draw_infos[i].index_offset,
                                             draw_infos[i].vertex_offset,
                                             0);
            }
            /*
            for (uint32_t i = 0; i < draw_infos.size(); ++i)
            {
                draw_indirect_array[i].first_index = draw_infos[i].index_offset;
                draw_indirect_array[i].instance_count = 1;
                draw_indirect_array[i].index_count = draw_infos[i].index_count;
                draw_indirect_array[i].vertex_offset = draw_infos[i].vertex_offset;
                draw_indirect_array[i].first_instance = 0;
            }
            command_buffer->draw_indexed_indirect(draw_indirect_buffer, 0, (uint32_t)draw_infos.size(), sizeof(DrawIndirectCommand));
            */
        }
        command_buffer->end_render_pass();
    }

    ForwardPass::~ForwardPass()
    {
        RenderingDevice::get()->destroy_buffers(&draw_indirect_buffer, 1);
    }
} // namespace mirai