#include <string>

namespace mirai {
    struct ShaderMaterial;

    /**
     * Unified Material System for all the Renderable Object
     * 1. They should all have atleast common bindings (per_frame_data, transform_data, material_data)
     * 2. They should all have ability to specify specific data (instance_data)
     */
    struct Material {

        Material(const std::string &name) : name(name) {
        }

        std::string name;
        ShaderMaterial *shader_material;
    };
} // namespace mirai