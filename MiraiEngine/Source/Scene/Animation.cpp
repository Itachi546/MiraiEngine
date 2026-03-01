#include "Animation.hpp"
namespace mirai {

    glm::mat4 AnimationComponent::calculate_transform(float delta_time) {
        float duration = get_duration();
        dt += delta_time;
        if (looping && dt > end_time)
            dt = fmod(dt - start_time, duration) + start_time;

        glm::mat4 result{1.0f};
        if (positions.values.size() > 0) {
            uint32_t position_index = positions.find_keyframe(dt);
            glm::vec3 position = interpolate_position(position_index, dt, positions.interpolation_mode);
            result = glm::translate(result, position);
        }

        if (rotations.values.size() > 0) {
            uint32_t rotation_index = rotations.find_keyframe(dt);
            glm::fquat rotation = interpolate_rotation(rotation_index, dt, rotations.interpolation_mode);
            result = result * glm::mat4_cast(rotation);
        }

        if (scalings.values.size() > 0) {
            uint32_t scaling_index = scalings.find_keyframe(dt);
            glm::vec3 scaling = interpolate_scaling(scaling_index, dt, scalings.interpolation_mode);
            result = glm::scale(result, scaling);
        }

        return result;
    }

} // namespace mirai