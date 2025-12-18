#pragma once

#include "RenderingDevice.hpp"
#include "Math/Math.hpp"

#include <map>
#include <vector>
#include <memory>

namespace mirai {

    struct Shader;
    struct Font;
    struct TextRenderManager;

    class TextRenderer {
      public:
        void AddText(const std::string &string, const glm::vec2 &position, float font_size = 16, const uint32_t color = 0xffffffff);

        TextureID get_font_texture();

        void reset() {
            vertex_count = 0;
        }

        ~TextRenderer();

        friend struct TextRenderManager;

        TextRenderer(Font *font);
        Font *font;

        BufferID vertex_buffer;
        glm::vec4 *vertex_array;
        uint32_t vertex_count;

        void PushChar(uint32_t char_index, glm::vec2 &position, uint32_t color, float font_size);

        UniformSetID uniform_set;
    };

    struct TextRenderManager {

        TextRenderManager();
        TextRenderManager(const TextRenderManager &) = delete;
        void operator=(const TextRenderManager &) = delete;

        ~TextRenderManager();

        TextRenderer *get_renderer_by_font(Font *font);

        TextRenderer *get_default() {
            return renderers[0].get();
        }

        static TextRenderManager *get() {
            return Instance;
        }

        static TextRenderManager *Instance;
        std::vector<std::shared_ptr<TextRenderer>> renderers;
        Shader *shader;
    };

}; // namespace mirai