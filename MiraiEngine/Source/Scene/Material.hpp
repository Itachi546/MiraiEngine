#pragma once

#include <string>
#include "Math/Math.hpp"
#include "Common/Hash.hpp"
#include "Engine/Log.hpp"
#include "Shader.hpp"
#include "Engine/AppSettings.hpp"

namespace mirai {

    constexpr const uint32_t K_MAX_MATERIAL_INSTANCE_DATA_SIZE = 64;
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

      protected:
        MaterialState material_state;
    };

    struct Material3D : public Material {
        Material3D(const std::string_view name) : Material(name) {
            // Pipeline state is pass-controlled — nothing to set here.
        }

        bool is_transparent() const {
            return material_state.alpha_mode == ALPHA_MODE_BLEND;
        }

        bool is_alpha_mask() const {
            return material_state.alpha_mode == ALPHA_MODE_MASK;
        }

        struct Properties {
            glm::vec4 albedo;

            glm::vec3 emissive_factor;
            float metallic_factor;

            float roughness_factor;
            float alpha_cutoff;
            uint32_t reserved = 0;
            uint32_t emissive_texture;

            uint32_t albedo_texture;
            uint32_t normal_texture;
            uint32_t metallic_roughness_texture;
            uint32_t occlusion_texture;

        } properties;
    };

    class CommandBuffer;
    struct ShaderMaterial : public Material {
        ShaderMaterial(const std::string &name, const std::vector<std::string> &shader_files, const PipelineState &pipeline_state, const PipelineAttachmentInfo &attachment_infos);
        void bind(CommandBuffer *command_buffer);
        ~ShaderMaterial();

        std::shared_ptr<Shader> shader;
    };

    struct ComputeShader {
        ComputeShader(const std::string &name, const std::string &shader_file);
        void bind(CommandBuffer *command_buffer);
        ~ComputeShader();

      private:
        std::shared_ptr<Shader> shader;
    };

    // struct ShaderPassKey {
    //     union {
    //         struct {
    //             uint64_t shader_pass : 32;
    //             uint64_t custom_shader_id : 32;
    //         } fields;
    //         uint64_t key;
    //     };

    //     ShaderPassKey(uint64_t key) : key(key) {
    //     }

    //     ShaderPassKey() {
    //         this->fields.shader_pass = SHADER_PASS_COUNT;
    //         this->fields.custom_shader_id = 0;
    //     }

    //     bool is_valid() {
    //         return this->key != UINT64_MAX;
    //     }

    //     bool operator==(const ShaderPassKey &other) const {
    //         return this->key == other.key;
    //     }
    // };

    // /**
    //  * Unified Material System for all the Renderable Object
    //  * 1. They should all have atleast common bindings (per_frame_data, transform_data, material_data)
    //  * 2. They should all have ability to specify specific data (instance_data)
    //  */
    // struct Material {

    //     Material(const std::string &name) : name(name), dirty(false) {
    //     }

    //     virtual void *get_instance_data() = 0;

    //     virtual uint32_t get_instance_data_size() const = 0;

    //     virtual bool is_transparent() const = 0;

    //     virtual bool has_alpha_mask() const = 0;

    //     virtual const char *get_material_type_name() = 0;

    //     /**
    //      * ShaderID consist's of two part
    //      * 1. ShaderPass enum value
    //      * 2. CustomShader id
    //      * ShaderPass enum is used later to retrive pipeline for default shaders whereas
    //      * CustomShaderID is used for custom shader types
    //      * id = shader_pass_id << 32 | custom_shader_id
    //      */
    //     virtual ShaderPassKey get_shader_key(RenderMode render_mode) const = 0;

    //     virtual ~Material() = default;

    //     std::string name;
    //     bool dirty;
    // };

    // struct UnlitMaterial : public Material {
    //     UnlitMaterial(const std::string &name) : Material(name) {
    //         instance_data.flags = FLAG_OPAQUE;
    //         instance_data.albedo = glm::vec4(1.0f);
    //         instance_data.unlit_texture_id = K_INVALID_ID;
    //         instance_data.padding[0] = 0;
    //         instance_data.padding[1] = 0;
    //     }

    //     void *get_instance_data() override {
    //         return &instance_data;
    //     }

    //     uint32_t get_instance_data_size() const override {
    //         return sizeof(UnlitProperties);
    //     }

    //     const char *get_material_type_name() override {
    //         return "UnlitMaterial";
    //     }

    //     bool is_transparent() const override {
    //         return ((instance_data.flags & FLAG_ALPHA_BLEND) == FLAG_ALPHA_BLEND);
    //     }

    //     bool has_alpha_mask() const override {
    //         return ((instance_data.flags & FLAG_ALPHA_MASK) == FLAG_ALPHA_MASK);
    //     }

    //     ShaderPassKey get_shader_key(RenderMode render_mode) const override {
    //         ShaderPassKey shader_key;
    //         switch (render_mode) {
    //         case RenderMode::RENDERMODE_DEFERRED: {
    //             shader_key.fields.shader_pass = is_transparent() ? SHADER_PASS_DEFERRED_UNLIT_TRANSPARENT : SHADER_PASS_DEFERRED_UNLIT;
    //             break;
    //         }
    //         case RenderMode::RENDERMODE_FORWARD: {
    //             shader_key.fields.shader_pass = is_transparent() ? SHADER_PASS_FORWARD_UNLIT_TRANSPARENT : SHADER_PASS_FORWARD_UNLIT;
    //             break;
    //         }
    //         default:
    //             Log::Fatal("Unknown render mode");
    //         };
    //         return shader_key;
    //     }

    //     struct UnlitProperties {
    //         glm::vec4 albedo;
    //         uint32_t unlit_texture_id;
    //         uint32_t flags;
    //         uint32_t padding[2];
    //     } instance_data;
    //     static_assert(sizeof(UnlitProperties) % 16 == 0);
    // };

    // struct StandardPBRMaterial : public Material {
    //     StandardPBRMaterial(const std::string &name) : Material(name) {
    //         instance_data.flags = FLAG_OPAQUE;
    //         instance_data.albedo = glm::vec4(1.0f);
    //         instance_data.emissive_factor = glm::vec4(0.0f);
    //         instance_data.metallic_factor = 0.1f;
    //         instance_data.roughness_factor = 0.9f;
    //         instance_data.alpha_cutoff = 0.9f;
    //         instance_data.emissive_texture = K_INVALID_ID;
    //         instance_data.albedo_texture = K_INVALID_ID;
    //         instance_data.normal_texture = K_INVALID_ID;
    //         instance_data.metallic_roughness_texture = K_INVALID_ID;
    //         instance_data.occlusion_texture = K_INVALID_ID;
    //     }

    //     void *get_instance_data() override {
    //         return &instance_data;
    //     }

    //     uint32_t get_instance_data_size() const override {
    //         return sizeof(PBRProperties);
    //     }

    //     bool is_transparent() const override {
    //         return ((instance_data.flags & FLAG_ALPHA_BLEND) == FLAG_ALPHA_BLEND);
    //     }

    //     bool has_alpha_mask() const override {
    //         return ((instance_data.flags & FLAG_ALPHA_MASK) == FLAG_ALPHA_MASK);
    //     }

    //     const char *get_material_type_name() override {
    //         return "StandardPBRMaterial";
    //     }

    //     ShaderPassKey get_shader_key(RenderMode render_mode) const override {
    //         ShaderPassKey shader_key;
    //         switch (render_mode) {
    //         case RenderMode::RENDERMODE_DEFERRED: {
    //             if (is_transparent())
    //                 shader_key.fields.shader_pass = SHADER_PASS_PBR_DEFERRED_TRANSPARENT;
    //             else if ((instance_data.flags & FLAG_ALPHA_MASK) == FLAG_ALPHA_MASK)
    //                 shader_key.fields.shader_pass = SHADER_PASS_PBR_DEFERRED_ALPHA;
    //             else
    //                 shader_key.fields.shader_pass = SHADER_PASS_PBR_DEFERRED;
    //             break;
    //         }
    //         case RenderMode::RENDERMODE_FORWARD: {
    //             if (is_transparent())
    //                 shader_key.fields.shader_pass = SHADER_PASS_PBR_FORWARD_TRANSPARENT;
    //             else if ((instance_data.flags & FLAG_ALPHA_MASK) == FLAG_ALPHA_MASK)
    //                 shader_key.fields.shader_pass = SHADER_PASS_PBR_FORWARD_TRANSPARENT;
    //             else
    //                 shader_key.fields.shader_pass = SHADER_PASS_PBR_FORWARD;
    //             break;
    //         }
    //         default:
    //             Log::Fatal("Unknown render mode");
    //         };
    //         return shader_key;
    //     }

    //     struct PBRProperties {
    //         glm::vec4 albedo;

    //         glm::vec3 emissive_factor;
    //         float metallic_factor;

    //         float roughness_factor;
    //         float alpha_cutoff;
    //         uint32_t flags = 0;
    //         uint32_t emissive_texture;

    //         uint32_t albedo_texture;
    //         uint32_t normal_texture;
    //         uint32_t metallic_roughness_texture;
    //         uint32_t occlusion_texture;

    //     } instance_data;
    //     static_assert(sizeof(PBRProperties) % 16 == 0);
    // };

    // struct ShaderMaterial : public Material {
    //     ShaderMaterial(const std::string &name, ShaderPassKey pass_key) : Material(name), pass_key(pass_key) {
    //     }

    //     ShaderPassKey get_shader_key(RenderMode render_mode) const override {
    //         return pass_key;
    //     }

    //     void set_instance_data(uint8_t *data, uint32_t size) {
    //         instance_data.clear();
    //         const uint32_t alignment = 16;
    //         uint32_t allocation_size = (size + alignment - 1) & ~(alignment - 1);
    //         instance_data.resize(allocation_size);
    //         std::memcpy(instance_data.data(), data, size);
    //     }

    //     const char *get_material_type_name() override {
    //         return "ShaderMaterial";
    //     }

    //     void *get_instance_data() override {
    //         return instance_data.data();
    //     }

    //     uint32_t get_instance_data_size() const override {
    //         return cast_u32(instance_data.size());
    //     }

    //     bool is_transparent() const {
    //         return false;
    //     }

    //     bool has_alpha_mask() const override {
    //         return false;
    //     }

    //   private:
    //     ShaderPassKey pass_key;
    //     std::vector<uint8_t> instance_data;
    // };

} // namespace mirai