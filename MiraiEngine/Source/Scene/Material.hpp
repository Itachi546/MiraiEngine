#pragma once

#include <string>

#include "Math/Math.hpp"
#include "Common/Hash.hpp"
#include "Shader.hpp"
namespace mirai {

    constexpr const uint32_t K_MAX_MATERIAL_INSTANCE_DATA_SIZE = 64;
    enum MaterialFlags {
        FLAG_EMPTY = 0,
        FLAG_OPAQUE = 1 << 0,
        FLAG_ALPHA_BLEND = 1 << 1,
        FLAG_ALPHA_MASK = 1 << 2,
        FLAG_DOUBLE_SIDED = 1 << 3,
        FLAG_SPECULAR_GLOSSINESS_WORKFLOW = 1 << 4,
    };

    struct ShaderPassKey {
        union {
            struct {
                uint64_t shader_pass : 32;
                uint64_t custom_shader_id : 32;
            } fields;
            uint64_t key;
        };

        ShaderPassKey(uint64_t key) : key(key) {
        }

        ShaderPassKey() {
            this->fields.shader_pass = SHADER_PASS_COUNT;
            this->fields.custom_shader_id = 0;
        }

        bool is_valid() {
            return this->key != UINT64_MAX;
        }

        bool operator==(const ShaderPassKey &other) const {
            return this->key == other.key;
        }
    };

    /**
     * Unified Material System for all the Renderable Object
     * 1. They should all have atleast common bindings (per_frame_data, transform_data, material_data)
     * 2. They should all have ability to specify specific data (instance_data)
     */
    struct Material {

        Material(const std::string &name) : name(name) {
        }

        virtual void *get_instance_data() = 0;

        virtual uint32_t get_instance_data_size() const = 0;

        virtual bool is_transparent() const = 0;

        /**
         * ShaderID consist's of two part
         * 1. ShaderPass enum value
         * 2. CustomShader id
         * ShaderPass enum is used later to retrive pipeline for default shaders whereas
         * CustomShaderID is used for custom shader types
         * id = shader_pass_id << 32 | custom_shader_id
         */
        virtual ShaderPassKey get_shader_key() const = 0;

        virtual ~Material() = default;

        std::string name;
    };

    struct UnlitMaterial : public Material {
        UnlitMaterial(const std::string &name) : Material(name) {
            instance_data.flags = FLAG_OPAQUE;
            instance_data.albedo = glm::vec4(1.0f);
            instance_data.unlit_texture_id = K_INVALID_ID;
            instance_data.padding[0] = 0;
            instance_data.padding[1] = 0;
        }

        void *get_instance_data() override {
            return &instance_data;
        }

        uint32_t get_instance_data_size() const override {
            return sizeof(UnlitProperties);
        }

        bool is_transparent() const {
            return ((instance_data.flags & FLAG_ALPHA_BLEND) == FLAG_ALPHA_BLEND) || ((instance_data.flags & FLAG_ALPHA_MASK) == FLAG_ALPHA_MASK);
        }

        ShaderPassKey get_shader_key() const override {
            ShaderPassKey shader_key;
            shader_key.fields.shader_pass = is_transparent() ? SHADER_PASS_FORWARD_UNLIT_TRANSPARENT : SHADER_PASS_FORWARD_UNLIT;
            return shader_key;
        }

        struct UnlitProperties {
            glm::vec4 albedo;
            uint32_t unlit_texture_id;
            uint32_t flags;
            uint32_t padding[2];
        } instance_data;
        static_assert(sizeof(UnlitProperties) % 16 == 0);
    };

    struct StandardPBRMaterial : public Material {
        StandardPBRMaterial(const std::string &name) : Material(name) {
            instance_data.flags = FLAG_OPAQUE;
            instance_data.albedo = glm::vec4(1.0f);
            instance_data.emissive_factor = glm::vec4(0.0f);
            instance_data.metallic_factor = 0.1f;
            instance_data.roughness_factor = 0.9f;
            instance_data.transmission = 0.0f;
            instance_data.emissive_texture = K_INVALID_ID;
            instance_data.albedo_texture = K_INVALID_ID;
            instance_data.normal_texture = K_INVALID_ID;
            instance_data.metallic_roughness_texture = K_INVALID_ID;
            instance_data.occlusion_texture = K_INVALID_ID;
        }

        void *get_instance_data() override {
            return &instance_data;
        }

        uint32_t get_instance_data_size() const override {
            return sizeof(PBRProperties);
        }

        bool is_transparent() const override {
            return ((instance_data.flags & FLAG_ALPHA_BLEND) == FLAG_ALPHA_BLEND) || ((instance_data.flags & FLAG_ALPHA_MASK) == FLAG_ALPHA_MASK);
        }

        ShaderPassKey get_shader_key() const override {
            ShaderPassKey shader_key;
            shader_key.fields.shader_pass = is_transparent() ? SHADER_PASS_PBR_FORWARD_TRANSPARENT : SHADER_PASS_PBR_FORWARD;
            return shader_key;
        }

        struct PBRProperties {
            glm::vec4 albedo;

            glm::vec3 emissive_factor;
            float metallic_factor;

            float roughness_factor;
            float transmission;
            uint32_t flags = 0;
            uint32_t emissive_texture;

            uint32_t albedo_texture;
            uint32_t normal_texture;
            uint32_t metallic_roughness_texture;
            uint32_t occlusion_texture;

        } instance_data;
        static_assert(sizeof(PBRProperties) % 16 == 0);
    };

    struct ShaderMaterial : public Material {
        ShaderMaterial(const std::string &name, ShaderPassKey pass_key) : Material(name), pass_key(pass_key) {
        }

        ShaderPassKey get_shader_key() const override {
            return pass_key;
        }

        void set_instance_data(uint8_t *data, uint32_t size) {
            instance_data.clear();
            const uint32_t alignment = 16;
            uint32_t allocation_size = (size + alignment - 1) & ~(alignment - 1);
            instance_data.resize(allocation_size);
            std::memcpy(instance_data.data(), data, size);
        }

        void *get_instance_data() override {
            return instance_data.data();
        }

        uint32_t get_instance_data_size() const override {
            return cast_u32(instance_data.size());
        }

        bool is_transparent() const {
            return false;
        }

      private:
        ShaderPassKey pass_key;
        std::vector<uint8_t> instance_data;
    };

} // namespace mirai