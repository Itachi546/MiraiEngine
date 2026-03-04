#include "AnimationDebug.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Camera.hpp"
#include "Math/Transformation.hpp"
#include "Scene/Animation.hpp"

void DrawPose(Scene *scene, Entity entity, ImDrawList *draw_list, float width, float height) {
    /*
    Pose &animatedPose = skeleton.GetAnimatedPose();
    uint32_t count = animatedPose.GetSize();
    auto camera = mScene.GetCamera();
    static const uint32_t color = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
    for (uint32_t i = 0; i < count; ++i) {
        int parent = animatedPose.GetParent(i);
        if (parent == -1)
            continue;

        glm::mat4 parentTransform = animatedPose.GetGlobalTransform(parent).CalculateWorldMatrix();
        glm::mat4 currentTransform = animatedPose.GetGlobalTransform(i).CalculateWorldMatrix();

        glm::vec3 parentBonePosition = worldTransform * parentTransform * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        glm::vec3 currentBonePosition = worldTransform * currentTransform * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

        glm::vec4 p0 = camera->ComputeNDCCoordinate(parentBonePosition);  //*glm::vec2(mWidth, mHeight);
        glm::vec4 p1 = camera->ComputeNDCCoordinate(currentBonePosition); // *glm::vec2(mWidth, mHeight);

        if (p0.z > 1.0f || p0.z < -1.0 || p1.z > 1.0f || p1.z < -1.0f)
            continue;
        drawList->AddLine(ImVec2(p0.x * mWidth, p0.y * mHeight), ImVec2(p1.x * mWidth, p1.y * mHeight), color, 2.0f);
    }
    */
    auto &comp_manager = scene->ecs->component_manager;
    AnimatorComponent *animator = comp_manager->get_component<AnimatorComponent>(entity);
    Skeleton &skeleton = scene->skeletons[animator->skeleton_index];
    TransformComponent *transform = comp_manager->get_component<TransformComponent>(entity);

    static const uint32_t color = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
    std::vector<glm::mat4> cached_transforms(skeleton.names.size());

    Camera *camera = scene->get_camera();
    glm::mat4 VP = camera->get_view_projection_transform();

    for (uint32_t i = 0; i < skeleton.names.size(); ++i) {
        int parent = skeleton.parents[i];
        ASSERT(parent < int(i));
        glm::mat4 parent_transform = parent == -1 ? transform->world_transform : cached_transforms[parent];
        glm::mat4 child_transform = parent_transform * skeleton.local_transforms[i];

        cached_transforms[i] = child_transform;
        if (parent == -1)
            continue;

        glm::vec3 parent_bone_pos = parent_transform * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        glm::vec3 child_bone_pos = child_transform * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

        glm::vec3 p0 = world_to_normalized_window_pos(parent_bone_pos, VP);
        glm::vec3 p1 = world_to_normalized_window_pos(child_bone_pos, VP);
        draw_list->AddLine(ImVec2(p0.x * width, p0.y * height), ImVec2(p1.x * width, p1.y * height), color, 2.0f);
    }
}

void add_animation_debug_ui(Scene *scene) {
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
    auto animator_comp_ptr = scene->ecs->component_manager->get_component_array<AnimatorComponent>();
    auto &entities = animator_comp_ptr->entities;
    /*
    Camera *camera = scene->get_camera();
    glm::mat4 VP = camera->get_view_projection_transform();
    glm::vec3 p0 = world_to_normalized_window_pos(glm::vec3(0.0f), VP);
    glm::vec3 p1 = world_to_normalized_window_pos(glm::vec3(0.0f, 1.0f, 0.0f), VP);
    static const uint32_t color = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
    draw_list->AddLine(ImVec2(p0.x * width, p0.y * height), ImVec2(p1.x * width, p1.y * height), color, 2.0f);
    */

    for (uint32_t i = 0; i < entities.size(); ++i) {
        DrawPose(scene, entities[i], draw_list, width, height);
    }
}