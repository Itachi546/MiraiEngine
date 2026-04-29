#include "Animation.hpp"
namespace mirai {

    glm::mat4 AnimationClip::sample_mat4(uint32_t node_or_bone_index, float dt) const {
        glm::mat4 result{1.0f};
        ASSERT(node_or_bone_index < positions.size());

        const Vec3Track &current_position_track = positions[node_or_bone_index];
        if (current_position_track.values.size() > 0) {
            uint32_t position_index = current_position_track.find_keyframe(dt);
            glm::vec3 position = interpolate_vec3_track(current_position_track, position_index, dt);
            result = glm::translate(result, position);
        }

        const QuatTrack &current_rotation_track = rotations[node_or_bone_index];
        if (current_rotation_track.values.size() > 0) {
            uint32_t rotation_index = current_rotation_track.find_keyframe(dt);
            glm::fquat rotation = interpolate_quat_track(current_rotation_track, rotation_index, dt);
            result = result * glm::mat4_cast(rotation);
        }

        const Vec3Track &current_scaling_track = scalings[node_or_bone_index];
        if (current_scaling_track.values.size() > 0) {
            uint32_t scaling_index = current_scaling_track.find_keyframe(dt);
            glm::vec3 scaling = interpolate_vec3_track(current_scaling_track, scaling_index, dt);
            result = glm::scale(result, scaling);
        }
        return result;
    }

    void AnimationClip::sample_TRS(uint32_t node_or_bone_index, float dt, glm::vec3 &position, glm::fquat &rotation, glm::vec3 &scale) const {
        ASSERT(node_or_bone_index < positions.size());
        const Vec3Track &current_position_track = positions[node_or_bone_index];
        if (current_position_track.values.size() > 0) {
            uint32_t position_index = current_position_track.find_keyframe(dt);
            position = interpolate_vec3_track(current_position_track, position_index, dt);
        }
        const QuatTrack &current_rotation_track = rotations[node_or_bone_index];
        if (current_rotation_track.values.size() > 0) {
            uint32_t rotation_index = current_rotation_track.find_keyframe(dt);
            rotation = interpolate_quat_track(current_rotation_track, rotation_index, dt);
        }

        const Vec3Track &current_scaling_track = scalings[node_or_bone_index];
        if (current_scaling_track.values.size() > 0) {
            uint32_t scaling_index = current_scaling_track.find_keyframe(dt);
            scale = interpolate_vec3_track(current_scaling_track, scaling_index, dt);
        }
    }

    AnimationPlayer::AnimationPlayer(const SkeletalAsset *skeletal_asset) : skeletal_asset(skeletal_asset), blend_duration(0.2f), blend_time(0.2f), paused(false) {
        uint32_t bone_count = cast_u32(skeletal_asset->skeleton.names.size());
        matrix_palletes.resize(bone_count);
        current_animation_state.clip_index = K_INVALID_ANIMATION_CLIP;
        current_animation_state.time = 0.0f;
        current_animation_state.pose.resize(bone_count);

        target_animation_state.clip_index = 0;
        target_animation_state.time = 0.0f;
        target_animation_state.pose.resize(bone_count);
    }

} // namespace mirai