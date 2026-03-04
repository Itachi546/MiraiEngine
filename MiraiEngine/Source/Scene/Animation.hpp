#pragma once

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
    /*
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
    */
    template <typename T>
    struct Track {
        std::vector<T> values;
        std::vector<float> timestamps;
        InterpolationMode interpolation_mode;

        uint32_t find_keyframe(float time) const {
            auto found = std::upper_bound(timestamps.begin(), timestamps.end(), time);
            if (found == timestamps.begin())
                return 0;
            if (found == timestamps.end())
                return cast_u32(timestamps.size()) - 2;
            return cast_u32(std::distance(timestamps.begin(), found)) - 1;
        }
    };

    using Vec3Track = Track<glm::vec3>;
    using QuatTrack = Track<glm::fquat>;

    struct AnimationClip {
        std::string name;
        float start_time;
        float end_time;
        float tick_per_seconds;

        Vec3Track positions;
        QuatTrack rotations;
        Vec3Track scalings;

        float get_duration() const {
            return end_time - start_time;
        }

        float calculate_time_scale(float start, float end, float time) const {
            return glm::clamp((time - start) / (end - start), 0.0f, 1.0f);
        }

        glm::vec3 interpolate_position(uint32_t index, float time, InterpolationMode interpolation_mode) const {
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

        glm::fquat interpolate_rotation(uint32_t index, float time, InterpolationMode interpolation_mode) const {
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

        glm::vec3 interpolate_scaling(uint32_t index, float time, InterpolationMode interpolation_mode) const {
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

        glm::mat4 sample(float time) const;
    };

    /*
 // Hold complete animation data for a node
 struct Animator {
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
 */
    struct SkeletonComponent {
        std::string name;

        // Bone related informations
        std::vector<int> parents;
        std::vector<glm::mat4> local_transforms;
        std::vector<glm::mat4> inv_bind_matrices;
        std::vector<std::string> names;

        void add_bone(int parent, const std::string &name) {
            parents.push_back(parent);
            names.push_back(name);
        }
    };

    struct Pose {
        std::vector<glm::mat4> matrix_palletes;
        std::vector<glm::vec3> joints_position;
        std::vector<glm::fquat> joints_rotation;
        std::vector<glm::vec3> joints_scaling;

        void resize(uint32_t size) {
            matrix_palletes.resize(size);
            joints_position.resize(size);
            joints_rotation.resize(size);
            joints_scaling.resize(size);
        }
    };

} // namespace mirai