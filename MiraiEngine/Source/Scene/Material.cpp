#include "Material.hpp"

namespace mirai {
    Material::Material(const std::string_view name) : name(name) {
        pipeline_state = {};
        current_key = pipeline_state.calculate_material_key();
    }

    void Material::on_change_material() {
        MaterialKey new_key = pipeline_state.calculate_material_key();
        if (new_key == current_key)
            return;
        current_key = new_key;
    }

    void Material::update_from_pipeline_state(const PipelineState &pipeline_state) {
        this->pipeline_state = pipeline_state;
        on_change_material();
    }

} // namespace mirai