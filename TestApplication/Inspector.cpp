#include "Inspector.hpp"
#include "Scene/ECS.hpp"
#include "ImGuiService.hpp"
#include "Scene/TextureCache.hpp"
#include "AnimationDebug.hpp"
#include <set>

Entity selected_entity = K_INVALID_ENTITY;
const ImVec2 UI_TEXTURE_SIZE = ImVec2{64, 64};
const uint32_t UI_IMAGE_GRID_SIZE = 4;
const char *LOADED_IMAGE_POPUP_NAME = "LoadedImages";

#define K_INVALID_TEXTURE UINT32_MAX

bool add_selectable_image_button(const char *id, uint32_t &texture_id, uint32_t *&selection) {
    if (texture_id != K_INVALID_TEXTURE) {
        std::string formatted_id = id + std::to_string(texture_id);

        if (ImGuiService::AddImageButton(formatted_id.c_str(), texture_id, UI_TEXTURE_SIZE)) {
            selection = &texture_id;
            ImGui::OpenPopup(LOADED_IMAGE_POPUP_NAME);
            return true;
        }
    } else {
        ImGui::SameLine();
        std::string formatted_id = std::string("Add Texture##") + id + std::to_string(texture_id);
        if (ImGui::Button(formatted_id.c_str())) {
            selection = &texture_id;
            selection = &texture_id;
            ImGui::OpenPopup(LOADED_IMAGE_POPUP_NAME);
            return true;
        }
    }
    return false;
}

uint32_t add_image_selection_popup(uint32_t current_texture) {
    uint32_t selection = current_texture;
    if (ImGui::BeginPopup(LOADED_IMAGE_POPUP_NAME)) {
        auto &textures = TextureCache::get()->textures_map;
        uint32_t count = 0;
        for (auto [key, val] : textures) {
            count++;
            std::string id = std::to_string(key) + "_" + std::to_string(val);
            if (ImGuiService::AddImageButton(id.c_str(), val.id, UI_TEXTURE_SIZE)) {
                selection = val.id;
            }
            if (count > UI_IMAGE_GRID_SIZE) {
                count = 0;
            }
            if (count != 0)
                ImGui::SameLine();
        }
        if (ImGui::Button("Reset texture")) {
            selection = K_INVALID_TEXTURE;
        }
        ImGui::EndPopup();
    }
    return selection;
}

uint32_t *selected_texture_ptr = nullptr;

void add_pbr_standard_material_ui(StandardPBRMaterial *material) {
    ImGui::Text("%s: %s", "material_type", material->is_transparent() ? "Transparent" : "Opaque");
    material->dirty |= ImGui::ColorEdit4("albedo", &material->instance_data.albedo[0]);
    material->dirty |= ImGui::DragFloat("roughness", &material->instance_data.roughness_factor, 0.01f, 0.0f, 1.0f);
    material->dirty |= ImGui::DragFloat("metallic", &material->instance_data.metallic_factor, 0.01f, 0.0f, 1.0f);
    material->dirty |= ImGui::ColorEdit3("emissive", &material->instance_data.emissive_factor[0]);

    ImGui::Text("%s(%u)", "albedo_texture", material->instance_data.albedo_texture);
    add_selectable_image_button("albedo_texture", material->instance_data.albedo_texture, selected_texture_ptr);

    ImGui::Text("%s(%u)", "metallic_roughness_texture", material->instance_data.metallic_roughness_texture);
    add_selectable_image_button("metallic_roughness_texture", material->instance_data.metallic_roughness_texture, selected_texture_ptr);

    ImGui::Text("%s(%u)", "occlusion_texture", material->instance_data.occlusion_texture);
    add_selectable_image_button("occlusion_texture", material->instance_data.occlusion_texture, selected_texture_ptr);

    ImGui::Text("%s(%u)", "normal_texture", material->instance_data.normal_texture);
    add_selectable_image_button("normal_texture", material->instance_data.normal_texture, selected_texture_ptr);

    ImGui::Text("%s(%u)", "emissive_texture", material->instance_data.emissive_texture);
    add_selectable_image_button("emissive_texture", material->instance_data.emissive_texture, selected_texture_ptr);

    if (selected_texture_ptr == nullptr)
        return;

    uint32_t selected_texture = add_image_selection_popup(*selected_texture_ptr);
    if (selected_texture != *selected_texture_ptr) {
        if (selected_texture != *selected_texture_ptr) {
            *selected_texture_ptr = selected_texture;
            selected_texture_ptr = nullptr;
            material->dirty = true;
        }
    }
}

void add_material_component_ui(MeshComponent *mesh_component, Scene *scene, Entity entity) {
    if (!mesh_component)
        return;
    std::set<uint32_t> unique_materials;
    for (auto &mesh_subset : mesh_component->mesh_subsets) {
        unique_materials.emplace(mesh_subset.material_index);
    }
    if (unique_materials.size() == 0)
        return;

    if (ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_CollapsingHeader)) {
        for (auto material_index : unique_materials) {
            auto &material = scene->materials[material_index];
            if (material == nullptr)
                continue;

            ImGui::PushID(entity * K_MAX_ENTITIES + material_index);
            std::string mat_name = material->name.size() > 0 ? material->name : "unnamed" + std::to_string(material_index);
            if (ImGui::TreeNodeEx(mat_name.c_str())) {
                std::string material_type_name = material->get_material_type_name();
                if (material_type_name == "StandardPBRMaterial")
                    add_pbr_standard_material_ui(reinterpret_cast<StandardPBRMaterial *>(material.get()));
                else {
                    ImGui::Text("Unknown material type name");
                }
                ImGui::Separator();
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
}

void add_entity_ui(std::unique_ptr<ComponentManager> &comp_manager, Entity entity, Scene *scene) {
    auto name_component = comp_manager->get_component<NameComponent>(entity);
    std::string name = "unnamed" + std::to_string(entity);
    if (name_component != nullptr && name_component->name.size() > 0) {
        name = name_component->name;
    }
    if (ImGui::CollapsingHeader(name.c_str())) {
        auto mesh_component = comp_manager->get_component<MeshComponent>(entity);
        if (mesh_component == nullptr)
            return;

        add_material_component_ui(mesh_component, scene, entity);
    }
}

void add_entity_hierarchy(Entity parent, std::unique_ptr<ComponentManager> &comp_manager, uint32_t depth) {
    HierarchyComponent *comp = comp_manager->get_component<HierarchyComponent>(parent);
    for (auto children : comp->childrens) {
        auto name_component = comp_manager->get_component<NameComponent>(children);

        std::string name = "unnamed" + std::to_string(children);
        if (name_component != nullptr && name_component->name.size() > 0) {
            name = name_component->name;
        }
        auto child_comp = comp_manager->get_component<HierarchyComponent>(children);

        ImGuiTreeNodeFlags flags = selected_entity == children ? ImGuiTreeNodeFlags_Selected : 0;
        flags |= ImGuiTreeNodeFlags_DrawLinesFull | ImGuiTreeNodeFlags_DrawLinesToNodes;

        if (child_comp->childrens.size() > 0) {
            flags |= ImGuiTreeNodeFlags_Framed;
            if (ImGui::TreeNodeEx(name.c_str(), flags)) {
                if (ImGui::IsItemClicked())
                    selected_entity = children;
                add_entity_hierarchy(children, comp_manager, depth + 1);
                ImGui::TreePop();
            }
        } else {
            flags |= ImGuiTreeNodeFlags_Leaf;
            if (ImGui::TreeNodeEx(name.c_str(), flags)) {
                if (ImGui::IsItemClicked())
                    selected_entity = children;
                ImGui::TreePop();
            }
        }
    }
}

void add_transform_component(TransformComponent *transform_component, Entity entity) {
    if (!transform_component)
        return;
    if (ImGui::CollapsingHeader("TransformComponent")) {
        ImGui::PushID(entity);
        std::string id = std::to_string(entity);
        bool changed = ImGui::DragFloat3("position", &transform_component->position[0]);

        glm::vec3 rotation = glm::degrees(glm::eulerAngles(transform_component->rotation));
        if (ImGui::DragFloat3("rotation", &rotation[0], 1.0f, 0.0f, 360.0f)) {
            transform_component->rotation = glm::fquat(rotation);
            changed |= true;
        }
        changed |= ImGui::DragFloat3("scale", &transform_component->scale[0]);
        transform_component->dirty = changed;
        ImGui::PopID();
    }
}

void add_skeleton_hierarchy(const Skeleton *skeleton, int node) {
    std::string name = skeleton->names[node];
    std::vector<int> childrens;
    for (uint32_t i = 0; i < skeleton->parents.size(); ++i) {
        if (skeleton->parents[i] == node) {
            childrens.push_back(i);
        }
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DrawLinesFull | ImGuiTreeNodeFlags_DrawLinesToNodes;
    if (childrens.size() == 0) {
        flags |= ImGuiTreeNodeFlags_Leaf;
        if (ImGui::TreeNodeEx(name.c_str(), flags)) {
            ImGui::TreePop();
        }
    } else {
        if (ImGui::TreeNodeEx(name.c_str(), flags)) {
            for (auto children : childrens) {
                add_skeleton_hierarchy(skeleton, children);
            }
            ImGui::TreePop();
        }
    }
}

Entity last_selected_entity = K_INVALID_ENTITY;
bool show_skeleton = false;
void add_animator_component(AnimatorComponent *animator_component, Scene *scene, Entity entity) {
    if (!animator_component)
        return;

    if (ImGui::CollapsingHeader("AnimatorComponent")) {
        ImGui::Text("Current Time: %.2f", animator_component->current_time);
        ImGui::Text("Skeleton Index: %d", animator_component->skeleton_index);

        Skeleton *skeleton = &scene->skeletons[animator_component->skeleton_index];
        std::string name = skeleton->name.size() > 0 ? skeleton->name : "unnamed";
        ImGui::Text("Skeleton Name: %s", skeleton->name.c_str());

        if (last_selected_entity != entity) {
            show_skeleton = false;
            last_selected_entity = entity;
        }

        ImGui::Checkbox("Show skeleton", &show_skeleton);

        add_skeleton_hierarchy(skeleton, 0);
    }
}

void add_entity_components(Entity entity, Scene *scene) {
    auto &comp_manager = scene->ecs->component_manager;
    auto name_component = comp_manager->get_component<NameComponent>(entity);
    std::string name = name_component != nullptr ? name_component->name : "unnamed";

    ImGui::Separator();
    std::string title = "Components(" + name + ")";
    ImGui::Text(title.c_str());

    add_transform_component(comp_manager->get_component<TransformComponent>(entity), entity);
    add_material_component_ui(comp_manager->get_component<MeshComponent>(entity), scene, entity);
    add_animator_component(comp_manager->get_component<AnimatorComponent>(entity), scene, entity);
}

void add_entity_inspector_ui(Scene *scene) {
    ImGui::Begin("Entity Inspector");
    auto &comp_manager = scene->ecs->component_manager;

    add_entity_hierarchy(scene->entities[0], comp_manager, 0);

    if (selected_entity != K_INVALID_ENTITY) {
        add_entity_components(selected_entity, scene);
    }
    ImGui::End();
}

void add_skeleton_debug_ui(Scene *scene) {
    if (show_skeleton)
        add_skeleton_debug_ui(scene, last_selected_entity);
}
