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

        UniformLayout uniform_layouts[] = {
            {.binding = 0, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_VERTEX},
            {.binding = 1, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_VERTEX},
        };
        uniform_set = RenderingDevice::get()->create_uniform_set(uniform_layouts, 2, 0, "vertex_binding");
    }

    void ForwardPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, const FrameGraphNode *node, Scene *scene)
    {
        ASSERT(node != nullptr);

        command_buffer->begin_render_pass(node, frame_graph);

        std::vector<DrawData> &draw_infos = scene->draw_infos;
        if (draw_infos.size() > 0)
        {
            UniformSetID uniform_sets[] = {scene->per_frame_uniform_set, uniform_set};
            shader->set_uniform_sets(uniform_sets, (uint32_t)std::size(uniform_sets));
            shader->bind(command_buffer, node, frame_graph);
            
            UniformBinding bindings[] = {
                {.resource_id = draw_infos[0].vertex_buffer, .offset = 0},
                {.resource_id = scene->transform_buffer, .offset = 0},
            };
            RenderingDevice::get()->update_uniform_set(uniform_set, bindings, (uint32_t)std::size(bindings));

            command_buffer->set_index_buffer(draw_infos[0].index_buffer);

            for (uint32_t i = 0; i < draw_infos.size(); ++i)
            {
                draw_indirect_array[i].first_index = draw_infos[i].index_offset;
                draw_indirect_array[i].instance_count = 1;
                draw_indirect_array[i].index_count = draw_infos[i].index_count;
                draw_indirect_array[i].vertex_offset = draw_infos[i].vertex_offset;
                draw_indirect_array[i].first_instance = 0;
            }

            command_buffer->draw_indexed_indirect(draw_indirect_buffer, 0, (uint32_t)draw_infos.size(), sizeof(DrawIndirectCommand));
        }
        command_buffer->end_render_pass();
    }

    ForwardPass::~ForwardPass()
    {
        RenderingDevice::get()->destroy_buffers(&draw_indirect_buffer, 1);
    }
} // namespace mirai