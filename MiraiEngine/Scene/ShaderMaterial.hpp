#pragma once

#include "Graphics/RenderingDevice.hpp"
#include <unordered_map>

namespace mirai
{
    struct FrameGraphNode;
    class FrameGraph;

    class ShaderMaterial
    {
      public:
        ShaderMaterial(const std::string &name);

        virtual ~ShaderMaterial() = default;

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

        void set_uniform_sets(UniformSetID *uniform_sets, uint32_t count)
        {
            this->uniform_sets.clear();
            this->uniform_sets.insert(this->uniform_sets.end(), uniform_sets, uniform_sets + count);
        }

        void set_push_constant(PushConstant *push_constants, uint32_t count)
        {
            this->push_constants.clear();
            this->push_constants.insert(this->push_constants.end(), push_constants, push_constants + count);
        }

        uint64_t get_hash()
        {
            return hash;
        }

        PipelineID get_pipeline_id() const {
            return pipeline;
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
        std::vector<UniformSetID> uniform_sets;
        std::vector<PushConstant> push_constants;

        PipelineID create_pipeline(const FrameGraphNode *render_pass, FrameGraph *frame_graph);
    };
} // namespace mirai