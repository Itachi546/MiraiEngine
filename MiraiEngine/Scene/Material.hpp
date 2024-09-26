#pragma once

#include "Graphics/RenderingDevice.hpp"

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

            hash = 0;
            this->cull_mode = cull_mode;
        }

        void set_front_face(FrontFace front_face)
        {
            if (this->front_face == front_face)
                return;

            hash = 0;
            this->front_face = front_face;
        }

        void set_depth_test(bool depth_test)
        {
            if (this->enable_depth_test == depth_test)
                return;
            hash = 0;
            this->enable_depth_test = depth_test;
        }

        void set_depth_write(bool depth_write)
        {
            if (this->enable_depth_write == depth_write)
                return;

            hash = 0;
            this->enable_depth_write = depth_write;
        }

        void bind(CommandBuffer *command_buffer, FrameGraphNode *node, FrameGraph *frame_graph);

        uint64_t get_hash()
        {
            if (hash == 0)
                calculate_hash();
            return hash;
        }

      protected:
        std::string name;
        uint64_t hash;

        CullMode cull_mode;
        FrontFace front_face;

        bool enable_depth_test;
        bool enable_depth_write;

        void calculate_hash();

        PipelineID create_pipeline(FrameGraphNode *render_pass, FrameGraph *frame_graph);
    };
} // namespace mirai