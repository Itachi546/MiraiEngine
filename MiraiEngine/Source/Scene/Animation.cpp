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

} // namespace mirai