#pragma once

#include "Graphics/RenderingDevice.hpp"
#include <unordered_map>

namespace mirai {
    struct FrameGraphRenderpassInfo;
    struct ShaderMaterialProperties {
        CullMode cull_mode = CULL_MODE_BACK;
        FrontFace front_face = FRONT_FACE_COUNTER_CLOCKWISE;
        bool depth_test = false;
        bool depth_write = false;
        bool depth_clamp = false;
        bool blend = false;
        CompareOp depth_op = COMPARE_OP_LESS_OR_EQUAL;
        Topology topology = TOPOLOGY_TRIANGLE_LIST;
        PolygonMode polygon_mode = POLYGON_MODE_FILL;
    };

    class ShaderMaterial {
      public:
        ShaderMaterial(const std::string &name);

        virtual ~ShaderMaterial();

        std::string &get_name() {
            return name;
        }

        void create_from_file(const std::vector<std::string> &shader_files, const ShaderMaterialProperties &properties);

        void set_custom_bindings(UniformSetID *uniform_sets, uint32_t uniform_set_count) {
            custom_uniform_sets.insert(custom_uniform_sets.end(), uniform_sets, uniform_sets + uniform_set_count);
        }

        virtual void bind(CommandBuffer *command_buffer, const FrameGraphRenderpassInfo *renderpass);

        PipelineID get_pipeline_id() const {
            return pipeline;
        }

        // Used for sorting/batching
        uint64_t get_id() {
            return shader_material_id;
        }

      protected:
        std::string name;
        PipelineID pipeline;

        std::vector<UniformSetID> custom_uniform_sets;
        std::vector<std::string> shader_files;
        ShaderMaterialProperties properties;

        PipelineID create_pipeline(const FrameGraphRenderpassInfo *renderpass);
        uint64_t shader_material_id;

        void update_shader_material_id();
    };

    class ComputeShader {
      public:
        ComputeShader(const std::string &name);

        void create_from_file(const std::string &file);

        virtual void bind(CommandBuffer *command_buffer);

        void set_custom_bindings(UniformSetID *uniform_sets, uint32_t uniform_set_count) {
            custom_uniform_sets.insert(custom_uniform_sets.end(), uniform_sets, uniform_sets + uniform_set_count);
        }

        PipelineID get_pipeline_id() const {
            return pipeline;
        }
        ~ComputeShader();

        std::string name;

      private:
        std::vector<UniformSetID> custom_uniform_sets;
        PipelineID pipeline;
        void create_pipeline(ShaderID shader);
    };

} // namespace mirai