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

        bool is_valid() const {
            return values.size() > 0;
        }
    };

    using Vec3Track = Track<glm::vec3>;
    using QuatTrack = Track<glm::fquat>;

    struct AnimationClip {
        std::string name;
        float start_time;
        float end_time;
        float tick_per_seconds;

        // Track for all the bones/node present in animation
        std::vector<Vec3Track> positions;
        std::vector<QuatTrack> rotations;
        std::vector<Vec3Track> scalings;

        float get_duration() const {
            return end_time - start_time;
        }

        float calculate_time_scale(float start, float end, float time) const {
            return glm::clamp((time - start) / (end - start), 0.0f, 1.0f);
        }

        glm::vec3 interpolate_vec3_track(const Vec3Track &track, uint32_t current_index, float time) const {
            if (track.values.size() == 1)
                return track.values[0];

            InterpolationMode interpolation_mode = track.interpolation_mode;
            if (interpolation_mode == InterpolationMode::Step)
                return track.values[current_index];
            else if (interpolation_mode == InterpolationMode::Cubic)
                Log::Warn("Cubic interpolation not impelmented, falling back to linear");

            uint32_t next_index = (current_index + 1) % cast_u32(track.values.size());
            float dt = calculate_time_scale(track.timestamps[current_index], track.timestamps[next_index], time);
            return glm::lerp(track.values[current_index], track.values[next_index], dt);
        }

        glm::fquat interpolate_quat_track(const QuatTrack &track, uint32_t current_index, float time) const {
            if (track.values.size() == 1)
                return track.values[0];

            InterpolationMode interpolation_mode = track.interpolation_mode;
            if (interpolation_mode == InterpolationMode::Step)
                return track.values[current_index];
            else if (interpolation_mode == InterpolationMode::Cubic)
                Log::Warn("Cubic interpolation not impelmented, falling back to linear");

            uint32_t next_index = (current_index + 1) % cast_u32(track.values.size());
            float dt = calculate_time_scale(track.timestamps[current_index], track.timestamps[next_index], time);
            return glm::slerp(track.values[current_index], track.values[next_index], dt);
        }

        bool has_animation(uint32_t node_or_bone_index) const {
            ASSERT(node_or_bone_index <= positions.size());
            return positions[node_or_bone_index].is_valid() || rotations[node_or_bone_index].is_valid() || scalings[node_or_bone_index].is_valid();
        }

        glm::mat4 sample_mat4(uint32_t node_or_bone_index, float time) const;
        void sample_TRS(uint32_t node_or_bone_index, float time, glm::vec3 &position, glm::fquat &rotation, glm::vec3 &scale) const;
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
    struct Skeleton {
        std::string name;

        // Bone related informations
        std::vector<int> parents;
        std::vector<glm::mat4> local_transforms;
        std::vector<glm::mat4> inv_bind_transforms;
        std::vector<std::string> names;

        std::vector<uint32_t> supported_animations;
        Pose current_pose;

        void resize(uint32_t joint_count) {
            parents.resize(joint_count);
            local_transforms.resize(joint_count);
            inv_bind_transforms.resize(joint_count);
            names.resize(joint_count);
        }

        void add_bone(int index, int parent, const std::string &name, const glm::mat4 &local_transform, const glm::mat4 &inv_bind_transform) {
            ASSERT(index < parents.size());
            parents[index] = parent;
            names[index] = name;
            local_transforms[index] = local_transform;
            inv_bind_transforms[index] = inv_bind_transform;
        }

        void calculate_inv_bind_transform() {
            std::vector<glm::mat4> global_transforms(parents.size());
            for (uint32_t i = 0; i < parents.size(); ++i) {
                ASSERT(parents[i] < int(i));
                if (parents[i] == -1)
                    global_transforms[i] = local_transforms[i];
                else
                    global_transforms[i] = global_transforms[parents[i]] * local_transforms[i];

                inv_bind_transforms[i] = glm::inverse(global_transforms[i]);
            }
        }
    };

} // namespace mirai