#include "AnimationDebug.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Camera.hpp"
#include "Math/Transformation.hpp"
#include "Scene/Animation.hpp"

void DrawSkeleton(const Skeleton &skeleton, glm::mat4 &VP, const glm::mat4 &root_transform, ImDrawList *draw_list, float width, float height, Scene *scene, float current_time) {
    static const uint32_t color = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
    static const uint32_t color2 = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
    std::vector<glm::mat4> cached_transforms(skeleton.parents.size());
    std::vector<glm::vec3> positions(skeleton.parents.size());

    AnimationClip &clip = scene->animation_clips[skeleton.supported_animations[0]];
    uint32_t joint_count = cast_u32(skeleton.parents.size());
    std::vector<glm::mat4> parent_transforms(joint_count);
    for (uint32_t i = 0; i < joint_count; ++i) {
        int parent_node = skeleton.parents[i];
        ASSERT(parent_node < int(i));

        glm::mat4 parent_transform = parent_node == -1 ? root_transform : parent_transforms[parent_node];
        glm::mat4 bone_animation_transform = clip.sample_mat4(i, current_time);

        // Since bone is already relative to origin we consider it to be in bone space already
        // we don't need global inverse bind transform
        glm::mat4 final_transform = parent_transform * bone_animation_transform;
        glm::vec3 bone_pos = final_transform[3];
        positions[i] = world_to_normalized_window_pos(bone_pos, VP);

        // Cache parent transform
        parent_transforms[i] = final_transform;
    }

    for (uint32_t i = 1; i < skeleton.parents.size(); ++i) {
        glm::vec3 p0 = positions[skeleton.parents[i]];
        glm::vec3 p1 = positions[i];

        if (p0.z <= -1.0f || p0.z > 1.0f || p1.z <= -1.0f || p1.z >= 1.0f)
            continue;
        draw_list->AddLine(ImVec2(p0.x * width, p0.y * height), ImVec2(p1.x * width, p1.y * height), color, 2.0f);
        draw_list->AddCircleFilled(ImVec2{p1.x * width, p1.y * height}, 5.0f, color2);
    }
}

void DrawPose(Scene *scene, Entity entity, ImDrawList *draw_list, float width, float height) {
    auto &comp_manager = scene->ecs->component_manager;
    AnimatorComponent *animator = comp_manager->get_component<AnimatorComponent>(entity);
    Skeleton &skeleton = scene->skeletons[animator->skeleton_index];
    TransformComponent *transform = comp_manager->get_component<TransformComponent>(entity);

    glm::mat4 VP = scene->get_camera()->get_view_projection_transform();
    DrawSkeleton(skeleton, VP, transform->world_transform, draw_list, width, height, scene, animator->current_time);
}

void add_skeleton_debug_ui(Scene *scene, Entity entity) {
    const ImU32 flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
                        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetWindowSize(io.DisplaySize);
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, 0);
    ImGui::PushStyleColor(ImGuiCol_Border, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    ImGui::Begin("Skeleton", NULL, flags);
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    float width = io.DisplaySize.x;
    float height = io.DisplaySize.y;
#if 1
    TransformComponent *transform = scene->ecs->component_manager->get_component<TransformComponent>(entity);
    AnimatorComponent *animator_comp = scene->ecs->component_manager->get_component<AnimatorComponent>(entity);
    ASSERT(animator_comp != nullptr);
    glm::mat4 VP = scene->get_camera()->get_view_projection_transform();
    DrawSkeleton(scene->skeletons[animator_comp->skeleton_index], VP, transform->world_transform, draw_list, width, height, scene, animator_comp->current_time);
#else
    DrawPose(scene, entity, draw_list, width, height);
#endif
}