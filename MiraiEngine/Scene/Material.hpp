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

            calculate_hash();
            this->cull_mode = cull_mode;
        }

        void set_front_face(FrontFace front_face)
        {
            if (this->front_face == front_face)
                return;

            calculate_hash();
            this->front_face = front_face;
        }

        void set_depth_test(bool depth_test)
        {
            if (this->enable_depth_test == depth_test)
                return;
            calculate_hash();
            this->enable_depth_test = depth_test;
        }

        void set_depth_write(bool depth_write)
        {
            if (this->enable_depth_write == depth_write)
                return;

            calculate_hash();
            this->enable_depth_write = depth_write;
        }

        void bind(CommandBuffer *command_buffer, FrameGraphNode *node, FrameGraph *frame_graph);

        void set_resource(const std::string &name, TextureID texture)
        {
            resources.push_back(Resource{name, texture});
        }

        uint64_t get_hash()
        {
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

        struct Resource
        {
            std::string name;
            ID resource_id;
        };
        std::vector<Resource> resources;

        PipelineID create_pipeline(FrameGraphNode *render_pass, FrameGraph *frame_graph);
    };
} // namespace mirai