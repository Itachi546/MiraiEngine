#pragma once

#include <string>

#include "Math/Math.hpp"
#include "Common/Hash.hpp"

namespace mirai {
    class ShaderMaterial;

    enum MaterialFlags {
        FLAG_EMPTY = 0,
        FLAG_OPAQUE = 1 << 0,
        FLAG_ALPHA_BLEND = 1 << 1,
        FLAG_ALPHA_MASK = 1 << 2,
        FLAG_DOUBLE_SIDED = 1 << 3,
        FLAG_SPECULAR_GLOSSINESS_WORKFLOW = 1 << 4,
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

        virtual bool supports_multidraw_indirect() const {
            return false;
        }

        virtual bool is_transparent() const = 0;

        virtual const char *get_material_type_name() const = 0;
        virtual uint32_t get_material_id() const = 0;

        std::string name;
    };

    struct StandardPBRMaterial : public Material {
        StandardPBRMaterial(const std::string &name) : Material(name) {
        }

        void *get_instance_data() override {
            return &instance_data;
        }

        uint32_t get_instance_data_size() const override {
            return sizeof(PBRProperties);
        }

        bool supports_multidraw_indirect() const override {
            return true;
        }

        bool is_transparent() const override {
            return ((instance_data.flags & FLAG_ALPHA_BLEND) == FLAG_ALPHA_BLEND) || ((instance_data.flags & FLAG_ALPHA_MASK) == FLAG_ALPHA_MASK);
        }

        const char *get_material_type_name() const override {
            return "StandardPBRMaterial";
        }

        uint32_t get_material_id() const override {
            static uint32_t id = utils::djb2_hash_string("StandardPBRMaterial");
            return id;
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

} // namespace mirai