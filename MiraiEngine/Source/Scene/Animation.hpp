#include "Math/Math.hpp"

#include <vector>
#include <string>
#include "Common/CommonInclude.hpp"
#include "Engine/Log.hpp"

namespace mirai {

    enum class InterpolationMode {
        Linear,
        Step,
        Cubic
    };

    /**
     * Keep track of single animation track (rotation/translation/scale)
     */
    template <typename T>
    struct AnimationTrack {
        std::vector<T> values;
        std::vector<float> timestamps;
        InterpolationMode interpolation_mode = InterpolationMode::Linear;

        uint32_t find_keyframe(float time) const {
            auto found = std::upper_bound(timestamps.begin(), timestamps.end(), time);
            if (found == timestamps.begin())
                return 0;
            if (found == timestamps.end())
                return cast_u32(timestamps.size()) - 2;
            return cast_u32(std::distance(timestamps.begin(), found)) - 1;
        }
    };

    using AnimationTrackVec3 = AnimationTrack<glm::vec3>;
    using AnimationTrackQuat = AnimationTrack<glm::fquat>;

    // Hold complete animation data for a node
    struct AnimationComponent {
        std::string name;
        float start_time;
        float end_time;
        bool looping = true;
        float dt = 0.0f;

        AnimationTrackVec3 positions;
        AnimationTrackQuat rotations;
        AnimationTrackVec3 scalings;

        float get_duration() const {
            return end_time - start_time;
        }

        float calculate_time_scale(float start, float end, float time) {
            return glm::clamp((time - start) / (end - start), 0.0f, 1.0f);
        }

        glm::vec3 interpolate_position(uint32_t index, float time, InterpolationMode interpolation_mode) {
            if (positions.values.size() == 1)
                return positions.values[0];

            if (interpolation_mode == InterpolationMode::Step)
                return positions.values[index];
            else if (interpolation_mode == InterpolationMode::Cubic)
                Log::Warn("Cubic interpolation not impelmented, falling back to linear");

            uint32_t next_index = (index + 1) % cast_u32(positions.values.size());
            float dt = calculate_time_scale(positions.timestamps[index], positions.timestamps[next_index], time);
            return glm::lerp(positions.values[index], positions.values[next_index], dt);
        }

        glm::fquat interpolate_rotation(uint32_t index, float time, InterpolationMode interpolation_mode) {
            if (rotations.values.size() == 1)
                return rotations.values[0];

            if (interpolation_mode == InterpolationMode::Step)
                return rotations.values[index];
            else if (interpolation_mode == InterpolationMode::Cubic)
                Log::Warn("Cubic interpolation not impelmented, falling back to linear");

            uint32_t next_index = (index + 1) % cast_u32(rotations.values.size());
            float dt = calculate_time_scale(rotations.timestamps[index], rotations.timestamps[next_index], time);
            return glm::slerp(rotations.values[index], rotations.values[next_index], dt);
        }

        glm::vec3 interpolate_scaling(uint32_t index, float time, InterpolationMode interpolation_mode) {
            if (scalings.values.size() == 1)
                return scalings.values[0];
            if (interpolation_mode == InterpolationMode::Step)
                return scalings.values[index];
            else if (interpolation_mode == InterpolationMode::Cubic)
                Log::Warn("Cubic interpolation not impelmented, falling back to linear");

            uint32_t next_index = (index + 1) % cast_u32(scalings.values.size());
            float dt = calculate_time_scale(scalings.timestamps[index], scalings.timestamps[next_index], time);
            return glm::lerp(scalings.values[index], scalings.values[next_index], dt);
        }

        glm::mat4 calculate_transform(float time);
    };

} // namespace mirai