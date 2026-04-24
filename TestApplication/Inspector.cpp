#include "Inspector.hpp"
#include "Scene/ECS.hpp"
#include "ImGuiService.hpp"
#include "Scene/TextureCache.hpp"
#include "AnimationDebug.hpp"
#include "Common/HashSet.hpp"
#include <set>
#include <sstream>

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

void add_pbr_standard_material_ui(Material3D *material) {
    if (material->is_transparent())
        ImGui::Text("%s: %s", "Type", "Transparent");
    else if (material->is_alpha_mask())
        ImGui::Text("%s: %s", "Type", "AlphaMask");
    else
        ImGui::Text("%s: %s", "Type", "Opaque");

    const char *alpha_mode = "Opaque\0Blend\0Mask\0\0";
    int current_mode = material->get_alpha_mode();
    if (ImGui::Combo("Alpha Mode", &current_mode, alpha_mode)) {
        material->set_alpha_mode(AlphaMode(current_mode));
        material->dirty = true;
    }

    material->dirty |= ImGui::ColorEdit4("albedo", &material->properties.albedo[0]);
    material->dirty |= ImGui::DragFloat("roughness", &material->properties.roughness_factor, 0.01f, 0.0f, 1.0f);
    material->dirty |= ImGui::DragFloat("metallic", &material->properties.metallic_factor, 0.01f, 0.0f, 1.0f);
    material->dirty |= ImGui::ColorEdit3("emissive", &material->properties.emissive_factor[0]);
    material->dirty |= ImGui::DragFloat("texture scale (x)", &material->properties.texture_scale_x, 0.1f, 0.0f, 100.0f);
    material->dirty |= ImGui::DragFloat("texture scale (y)", &material->properties.texture_scale_y, 0.1f, 0.0f, 100.0f);
    material->dirty |= ImGui::DragFloat("transmission", &material->properties.transmission, 0.1f, 0.0f, 1.0f);
    ImGui::Text("%s(%u)", "albedo_texture", material->properties.albedo_texture);
    // add_selectable_image_button("albedo_texture", material->properties.albedo_texture, selected_texture_ptr);

    ImGui::Text("%s(%u)", "metallic_roughness_texture", material->properties.metallic_roughness_texture);
    // add_selectable_image_button("metallic_roughness_texture", material->properties.metallic_roughness_texture, selected_texture_ptr);

    ImGui::Text("%s(%u)", "occlusion_texture", material->properties.occlusion_texture);
    // add_selectable_image_button("occlusion_texture", material->properties.occlusion_texture, selected_texture_ptr);

    ImGui::Text("%s(%u)", "normal_texture", material->properties.normal_texture);
    // add_selectable_image_button("normal_texture", material->properties.normal_texture, selected_texture_ptr);

    ImGui::Text("%s(%u)", "emissive_texture", material->properties.emissive_texture);
    // add_selectable_image_button("emissive_texture", material->properties.emissive_texture, selected_texture_ptr);
    /*
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
    */
}

void add_material_component_ui(MeshComponent *mesh_component, Scene *scene, Entity entity) {
    if (!mesh_component)
        return;
    HashSet<uint32_t> unique_materials;
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
                add_pbr_standard_material_ui(material.get());
                ImGui::Separator();
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
}

static const char *light_type_array[] = {
    "Directional",
    "Point",
    "Spot",
};

void add_light_component_ui(LightComponent *light, Entity entity) {
    if (!light)
        return;
    if (ImGui::CollapsingHeader("LightComponent")) {
        ImGui::PushID(entity);
        ImGui::Text("Light Type:%s", light_type_array[light->light_type]);
        ImGui::DragFloat("Intensity", &light->intensity, 0.2f, 0.0f, 200.0f);
        ImGui::ColorPicker3("Color", &light->color[0]);
        if (light->light_type == LIGHT_TYPE_DIRECTIONAL) {
            ImGui::Checkbox("Enable Shadow", &light->cast_shadow);
        } else if (light->light_type == LIGHT_TYPE_POINT) {
            ImGui::SliderFloat("Radius", &light->radius, 0.0f, 20.0f);
        } else if (light->light_type == LIGHT_TYPE_SPOT) {
            ImGui::SliderFloat("Height", &light->radius, 0.0f, 20.0f);
            float inner_angle = glm::degrees(light->inner_cone_angle);
            float outer_angle = glm::degrees(light->outer_cone_angle);
            if (ImGui::SliderFloat("Outer Angle", &outer_angle, 0.0f, 90.0f)) {
                light->outer_cone_angle = glm::radians(outer_angle);
            }
            if (ImGui::SliderFloat("Inner Angle", &inner_angle, 0.0f, outer_angle)) {
                light->inner_cone_angle = glm::radians(inner_angle);
            }
        }
        ImGui::PopID();
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
        for (int i = 0; i < 3; i++) {
            if (rotation[i] < -180.0f)
                rotation[i] += 360.0f;
            if (rotation[i] > 180.0f)
                rotation[i] -= 360.0f;
        }

        if (ImGui::DragFloat3("rotation", &rotation[0], 1.0f, -180.0f, 180.0f)) {
            transform_component->rotation = glm::fquat(glm::radians(rotation));
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

        if (skeleton->supported_animations.size() > 0 && animator_component->current_animation_clip != K_INVALID_ANIMATION_CLIP) {
            std::stringstream ss;
            for (uint32_t animation_index : skeleton->supported_animations) {
                AnimationClip *current_animation = &scene->animation_clips[animation_index];
                ss << current_animation->name << '\0';
            }
            ss << '\0';

            static int current_animation_clip = 0;
            if (ImGui::Combo("Target", &current_animation_clip, ss.str().c_str())) {
                animator_component->current_animation_clip = skeleton->supported_animations[current_animation_clip];
            }

            AnimationClip *current_animation = &scene->animation_clips[animator_component->current_animation_clip];
            ImGui::Text("Animation: %s\n", current_animation->name.c_str());
            ImGui::Text("Duration: %.2fs", current_animation->get_duration());
            ImGui::Text("Tick per seconds: %d", cast_int(current_animation->tick_per_seconds));
        }

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
    ImGui::Text("Components(%s)", name.c_str());

    add_transform_component(comp_manager->get_component<TransformComponent>(entity), entity);
    add_material_component_ui(comp_manager->get_component<MeshComponent>(entity), scene, entity);
    add_animator_component(comp_manager->get_component<AnimatorComponent>(entity), scene, entity);
    add_light_component_ui(comp_manager->get_component<LightComponent>(entity), entity);
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
