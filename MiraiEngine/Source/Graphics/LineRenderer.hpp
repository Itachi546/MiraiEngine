#pragma once

#include "Math/Math.hpp"
#include "Graphics/RenderingDevice.hpp"

namespace mirai {

    class CommandBuffer;
    struct Shader;
    struct LineRenderer {

        LineRenderer();
        LineRenderer(const LineRenderer &) = delete;
        void operator=(const LineRenderer &) = delete;

        void NewFrame();

        void AddLine(glm::vec3 s, glm::vec3 e, uint32_t color = 0xffffffff);
        void AddAABB(const AABB &aabb, uint32_t color = 0x00ff00ff);
        void AddFrustum(const std::array<glm::vec3, 8> &points, uint32_t color);

        ~LineRenderer();

        BufferID buffer;
        Shader *shader;
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
    };
} // namespace mirai