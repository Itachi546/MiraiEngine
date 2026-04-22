#pragma once

#include <string>
#include "Math/Math.hpp"
#include "Common/Hash.hpp"
#include "Engine/Log.hpp"
#include "Shader.hpp"
#include "Engine/AppSettings.hpp"

namespace mirai {

    struct Material {
      public:
        Material(const std::string_view name);

        void set_cull_mode(CullMode m) {
            material_state.cull_mode = m;
        }

        CullMode get_cull_mode() const {
            return material_state.cull_mode;
        }

        void set_front_face(FrontFace f) {
            material_state.front_face = f;
        }

        FrontFace get_front_face() const {
            return material_state.front_face;
        }

        void set_polygon_mode(PolygonMode p) {
            material_state.polygon_mode = p;
        }

        PolygonMode get_polygon_mode() const {
            return material_state.polygon_mode;
        }

        void set_alpha_mode(AlphaMode a) {
            material_state.alpha_mode = a;
        }

        AlphaMode get_alpha_mode() const {
            return material_state.alpha_mode;
        }

        // Render flags — behavioral, NOT part of the sort key.
        void set_render_flags(uint32_t flags) {
            material_state.render_flags = flags;
        }

        void add_render_flag(RenderFlags flag) {
            material_state.render_flags |= flag;
        }

        void remove_render_flag(RenderFlags flag) {
            material_state.render_flags &= ~static_cast<uint32_t>(flag);
        }

        bool has_render_flag(RenderFlags flag) const {
            return material_state.has_flag(flag);
        }

        // Hash is computed live — get_hash() is a trivial bitfield pack.
        uint32_t get_hash() const {
            return material_state.get_hash();
        }

        void set_dirty(bool state) {
            this->dirty = state;
        }

        bool is_dirty() const {
            return this->dirty;
        }

        std::string name;
        bool dirty = false;

        virtual ~Material() = default;

      protected:
        MaterialState material_state;
    };

    struct Material3D : public Material {
        Material3D(const std::string_view name) : Material(name) {
            properties.albedo = glm::vec4(1.0f);
            properties.emissive_factor = glm::vec3(0.0f);
            properties.metallic_factor = 0.01f;
            properties.roughness_factor = 0.5f;
            properties.alpha_cutoff = 0.9f;
            properties.flags = 0;
            properties.emissive_texture = K_INVALID_ID;

            properties.albedo_texture = K_INVALID_ID;
            properties.normal_texture = K_INVALID_ID;
            properties.metallic_roughness_texture = K_INVALID_ID;
            properties.occlusion_texture = K_INVALID_ID;

            properties.transmission = 1.0f;
            properties.texture_scale_x = 1.0f;
            properties.texture_scale_y = 1.0f;
            properties._reserved = 0.0f;
        }

        bool is_transparent() const {
            return material_state.alpha_mode == ALPHA_MODE_BLEND;
        }

        bool is_alpha_mask() const {
            return material_state.alpha_mode == ALPHA_MODE_MASK;
        }

        // Returns true for ShaderMaterial3D; false for standard PBR Material3D.
        virtual bool is_custom_shader() const { return false; }

        // Returns the embedded Shader* when is_custom_shader() is true; nullptr otherwise.
        virtual Shader *get_custom_shader() const { return nullptr; }

        virtual ~Material3D() = default;
        struct Properties {
            glm::vec4 albedo;

            glm::vec3 emissive_factor;
            float metallic_factor;

            float roughness_factor;
            float alpha_cutoff;
            uint32_t flags = 0;
            uint32_t emissive_texture;

            uint32_t albedo_texture;
            uint32_t normal_texture;
            uint32_t metallic_roughness_texture;
            uint32_t occlusion_texture;

            float transmission;
            float texture_scale_x;
            float texture_scale_y;
            float _reserved;

        } properties;

        static_assert(sizeof(Properties) % 16 == 0);
    };

    class CommandBuffer;
    // Used for non-batched custom draw calls: skybox, debug line rendering, post-process compositing, etc.
    struct EffectMaterial : public Material {
        EffectMaterial(const std::string &name, const std::vector<std::string> &shader_files, const PipelineState &pipeline_state, const PipelineAttachmentInfo &attachment_infos);
        void bind(CommandBuffer *command_buffer);
        ~EffectMaterial();

        std::shared_ptr<Shader> shader;
    };

    // Material3D variant with an embedded user shader for forward/deferred passes.
    struct ShaderMaterial3D : public Material3D {
        ShaderMaterial3D(const std::string_view name,
                         const std::vector<std::string> &shader_files,
                         const PipelineState &pipeline_state,
                         const PipelineAttachmentInfo &attachment_info);
        ~ShaderMaterial3D();

        bool is_custom_shader() const override { return true; }
        Shader *get_custom_shader() const override { return shader.get(); }

      private:
        std::shared_ptr<Shader> shader;
    };

    struct ComputeShader {
        ComputeShader(const std::string &name, const std::string &shader_file);
        void bind(CommandBuffer *command_buffer);
        ~ComputeShader();

      private:
        std::shared_ptr<Shader> shader;
    };
} // namespace mirai