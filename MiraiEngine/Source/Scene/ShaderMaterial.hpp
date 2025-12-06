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

        virtual void bind(CommandBuffer *command_buffer, const FrameGraphRenderpassInfo *renderpass);

        void set_uniform_sets(UniformSetID *uniform_sets, uint32_t count) {
            this->uniform_sets.clear();
            this->uniform_sets.insert(this->uniform_sets.end(), uniform_sets, uniform_sets + count);
        }

        void set_push_constant(PushConstant *push_constants, uint32_t count) {
            this->push_constants.clear();
            this->push_constants.insert(this->push_constants.end(), push_constants, push_constants + count);
        }

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

        std::vector<std::string> shader_files;
        std::vector<UniformSetID> uniform_sets;
        std::vector<PushConstant> push_constants;
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

        void set_uniform_sets(UniformSetID *uniform_sets, uint32_t count) {
            this->uniform_sets.clear();
            this->uniform_sets.insert(this->uniform_sets.end(), uniform_sets, uniform_sets + count);
        }

        void set_push_constant(PushConstant *push_constants, uint32_t count) {
            this->push_constants.clear();
            this->push_constants.insert(this->push_constants.end(), push_constants, push_constants + count);
        }

        PipelineID get_pipeline_id() const {
            return pipeline;
        }
        ~ComputeShader();

        std::string name;

      private:
        PipelineID pipeline;
        std::vector<UniformSetID> uniform_sets;
        std::vector<PushConstant> push_constants;
        void create_pipeline(ShaderID shader);
    };

} // namespace mirai