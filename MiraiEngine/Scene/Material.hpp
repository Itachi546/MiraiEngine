#pragma once

#include "Graphics/RenderingDevice.hpp"
#include <unordered_map>

namespace mirai
{
    struct FrameGraphNode;
    class FrameGraph;

    class Material
    {
      public:
        Material(const std::string &name);

        void create_from_file(const std::vector<std::string> &shader_files);

        void set_cull_mode(CullMode cull_mode)
        {
            if (this->cull_mode == cull_mode)
                return;

            clear_pipeline_state();
            this->cull_mode = cull_mode;
        }

        void set_front_face(FrontFace front_face)
        {
            if (this->front_face == front_face)
                return;

            clear_pipeline_state();
            this->front_face = front_face;
        }

        void set_depth_test(bool depth_test)
        {
            if (this->enable_depth_test == depth_test)
                return;

            clear_pipeline_state();
            this->enable_depth_test = depth_test;
        }

        void set_depth_write(bool depth_write)
        {
            if (this->enable_depth_write == depth_write)
                return;

            clear_pipeline_state();
            this->enable_depth_write = depth_write;
        }

        void bind(CommandBuffer *command_buffer, const FrameGraphNode *node, FrameGraph *frame_graph);

        void set_resource(const std::string &name, ID resource);

        void set_push_constant(CommandBuffer *command_buffer, ShaderStage shader_stage, uint32_t offset, uint32_t size, void *data);

        uint64_t get_hash()
        {
            return hash;
        }

      protected:
        std::string name;
        uint64_t hash;
        PipelineID pipeline;

        CullMode cull_mode;
        FrontFace front_face;

        bool enable_depth_test;
        bool enable_depth_write;

        void calculate_hash();

        void clear_pipeline_state()
        {
            pipeline.id = K_INVALID_ID;
            calculate_hash();
        }

        struct Resource
        {
            std::string name;
            ID resource_id;
            bool dirty;
        };

        bool is_resource_updated;
        std::unordered_map<uint32_t, Resource> resources;

        PipelineID create_pipeline(const FrameGraphNode *render_pass, FrameGraph *frame_graph);
    };
} // namespace mirai