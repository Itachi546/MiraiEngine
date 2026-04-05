#include "Material.hpp"

namespace mirai {
    Material::Material(const std::string_view name) : name(name) {
        pipeline_state = {};
        current_key = pipeline_state.get_hash();
    }

    void Material::on_change_material() {
        uint32_t new_key = pipeline_state.get_hash();
        if (new_key == current_key)
            return;
        current_key = new_key;
    }

    void Material::update_from_pipeline_state(const PipelineState &pipeline_state) {
        this->pipeline_state = pipeline_state;
        on_change_material();
    }

} // namespace mirai