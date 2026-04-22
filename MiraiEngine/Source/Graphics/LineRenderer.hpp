#pragma once

#include "Math/Math.hpp"
#include "Graphics/RenderingDevice.hpp"

namespace mirai {

    class CommandBuffer;
    struct EffectMaterial;
    struct LineRenderer {

        LineRenderer();
        LineRenderer(const LineRenderer &) = delete;
        void operator=(const LineRenderer &) = delete;

        void new_frame(uint32_t current_frame_index);

        void render(CommandBuffer *command_buffer, const glm::mat4 &VP);

        void add_line(const glm::vec3 &s, const glm::vec3 &e, uint32_t color = 0xffffffff);
        void add_aabb(const AABB &aabb, uint32_t color = 0x00ff00ff);
        void add_frustum(const std::array<glm::vec3, 8> &points, uint32_t color);
        void add_circle(const glm::vec3 &p, float r, uint32_t color = 0xffffffff);
        void add_cone(const glm::vec3 &p, const glm::vec3 &d, float height, float angle, uint32_t color = 0xffffffff);

        ~LineRenderer();

        BufferID buffer;
        std::shared_ptr<EffectMaterial> shader;
        uint32_t line_count = 0;

        static LineRenderer *get() {
            return Instance;
        }

      private:
        static LineRenderer *Instance;
        struct Vertex {
            float x, y, z;
            uint32_t color;
        };

        struct Line {
            Vertex start;
            Vertex end;
        };

        const uint32_t K_MAX_LINE_COUNT = 100'000;
        Line *line_array = nullptr;
        uint32_t per_frame_offset = 0;
    };
} // namespace mirai