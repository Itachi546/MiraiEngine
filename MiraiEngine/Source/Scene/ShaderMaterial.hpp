#pragma once

#include "Graphics/RenderingDevice.hpp"
#include <unordered_map>

namespace mirai {
    struct FrameGraphRenderpassInfo;

    class ShaderMaterial {
      public:
        ShaderMaterial(const std::string &name);

        virtual ~ShaderMaterial() = default;

        std::string &get_name() {
            return name;
        }
        void create_from_file(const std::vector<std::string> &shader_files);

        void set_cull_mode(CullMode cull_mode) {
            if (this->cull_mode == cull_mode)
                return;

            this->cull_mode = cull_mode;
            clear_pipeline_state();
        }

        void set_depth_compare_op(CompareOp compare_op) {
            if (this->depth_compare_op == compare_op)
                return;
            depth_compare_op = compare_op;
            clear_pipeline_state();
        }

        void set_front_face(FrontFace front_face) {
            if (this->front_face == front_face)
                return;

            this->front_face = front_face;
            clear_pipeline_state();
        }

        void set_depth_test(bool depth_test) {
            if (this->enable_depth_test == depth_test)
                return;

            this->enable_depth_test = depth_test;
            clear_pipeline_state();
        }

        void set_depth_clamp(bool depth_clamp) {
            if (this->enable_depth_clamp == depth_clamp)
                return;
            this->enable_depth_clamp = depth_clamp;
            clear_pipeline_state();
        }

        void set_enable_blend(bool blend) {
            if (this->enable_blend == blend)
                return;
            this->enable_blend = blend;
            clear_pipeline_state();
        }

        void set_depth_write(bool depth_write) {
            if (this->enable_depth_write == depth_write)
                return;

            this->enable_depth_write = depth_write;
            clear_pipeline_state();
        }

        void set_topology(Topology topology) {
            if (this->topology == topology)
                return;
            this->topology = topology;
            clear_pipeline_state();
        }

        virtual void bind(CommandBuffer *command_buffer, const FrameGraphRenderpassInfo *renderpass);

        void set_uniform_sets(UniformSetID *uniform_sets, uint32_t count) {
            this->uniform_sets.clear();
            this->uniform_sets.insert(this->uniform_sets.end(), uniform_sets, uniform_sets + count);
        }

        void set_push_constant(PushConstant *push_constants, uint32_t count) {
            this->push_constants.clear();
            this->push_constants.insert(this->push_constants.end(), push_constants, push_constants + count);
        }

        uint64_t get_hash() {
            return hash;
        }

        PipelineID get_pipeline_id() const {
            return pipeline;
        }

      protected:
        std::vector<ShaderID> shaders;
        std::string name;

        uint64_t shader_hash;
        uint64_t hash;
        PipelineID pipeline;

        CullMode cull_mode;
        FrontFace front_face;

        bool dirty = true;
        bool enable_depth_test;
        bool enable_depth_write;
        bool enable_depth_clamp;
        bool enable_blend;
        CompareOp depth_compare_op;
        Topology topology;

        void calculate_hash();

        void clear_pipeline_state() {
            pipeline.id = K_INVALID_ID;
            dirty = true;
        }

        struct Resource {
            std::string name;
            ID resource_id;
            bool dirty;
        };

        bool is_resource_updated;
        std::vector<UniformSetID> uniform_sets;
        std::vector<PushConstant> push_constants;

        PipelineID create_pipeline(const FrameGraphRenderpassInfo *renderpass);
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