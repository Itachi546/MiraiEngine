#include "AnimationDebug.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Camera.hpp"
#include "Math/Transformation.hpp"
#include "Scene/Animation.hpp"

void DrawSkeleton(const Skeleton &skeleton, glm::mat4 &VP, const glm::mat4 &root_transform, ImDrawList *draw_list, float width, float height, Scene *scene) {
    static const uint32_t color = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
    static const uint32_t color2 = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
    std::vector<glm::mat4> cached_transforms(skeleton.parents.size());
    std::vector<glm::vec3> positions(skeleton.parents.size());

    AnimationClip &clip = scene->animation_clips[skeleton.supported_animations[0]];
    static float dt = 0.0016f;
    for (uint32_t i = 0; i < skeleton.parents.size(); ++i) {
        glm::vec3 bone_pos = root_transform * skeleton.current_pose.matrix_palletes[i] * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        positions[i] = world_to_normalized_window_pos(bone_pos, VP);
    }

    for (uint32_t i = 1; i < skeleton.parents.size(); ++i) {
        glm::vec3 p0 = positions[skeleton.parents[i]];
        glm::vec3 p1 = positions[i];

        if (p0.z <= -1.0f || p0.z > 1.0f || p1.z <= -1.0f || p1.z >= 1.0f)
            continue;
        draw_list->AddLine(ImVec2(p0.x * width, p0.y * height), ImVec2(p1.x * width, p1.y * height), color, 2.0f);
        draw_list->AddCircleFilled(ImVec2{p1.x * width, p1.y * height}, 5.0f, color2);
    }

    dt += 0.0016f;
}

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

    glm::mat4 VP = scene->get_camera()->get_view_projection_transform();
    DrawSkeleton(skeleton, VP, transform->world_transform, draw_list, width, height, scene);
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
    DrawSkeleton(scene->skeletons[animator_comp->skeleton_index], VP, transform->world_transform, draw_list, width, height, scene);
#else
    DrawPose(scene, entity, draw_list, width, height);
#endif
}