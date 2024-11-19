#include "TextRenderManager.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Common/Font.hpp"

#include <algorithm>

namespace mirai {

    const uint32_t K_MAX_TEXTS = 1000;

    TextRenderer::TextRenderer(Font *font) : font(font) {
        BufferDescription buffer_desc = {
            .size = K_MAX_TEXTS * sizeof(glm::vec4) * 4,
            .usage_flags = BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .allocation_type = MEMORY_ALLOCATION_TYPE_CPU,
        };
        vertex_count = 0;
        vertex_buffer = RenderingDevice::get()->create_buffer(&buffer_desc, "font_renderer_buffer");
        vertex_array = (glm::vec4 *)RenderingDevice::get()->map_buffer(vertex_buffer);

        UniformLayout mesh_data_layout[] = {
            {.binding = 0, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_VERTEX},
        };
        uniform_set = RenderingDevice::get()->create_uniform_set(mesh_data_layout, 1, 0, "font_uniform_set");

        UniformBinding binding[] = {
            {.resource_id = vertex_buffer},
        };
        RenderingDevice::get()->update_uniform_set(uniform_set, binding, 1);
    }

    void TextRenderer::AddText(const std::string &str, const glm::vec2 &position, const uint32_t color) {
        ASSERT(font != nullptr);

        glm::vec2 offset = position;
        for (uint32_t i = 0; i < str.size(); ++i) {
            uint32_t char_index = (uint32_t)str[i];
            ASSERT(char_index >= 0 && char_index < 255);
            PushChar(char_index, offset, color);
        }
    }

    TextureID TextRenderer::get_font_texture() {
        return font->texture;
    }

    void TextRenderer::PushChar(uint32_t char_index, glm::vec2 &position, uint32_t color) {
        FontCharacterInfo &char_info = font->character_info[char_index];

        glm::vec2 positions[] = {
            glm::vec2(position.x, position.y),                                      // BL
            glm::vec2(position.x + char_info.width, position.y),                    // BR
            glm::vec2(position.x, position.y - char_info.height),                   // TL
            glm::vec2(position.x + char_info.width, position.y - char_info.height), // TR
        };

        glm::vec2 font_texture_size = glm::vec2(font->width, font->height);
        glm::vec2 uvs[] = {
            glm::vec2(char_info.x, char_info.y + char_info.height) / font_texture_size,                   // TL
            glm::vec2(char_info.x + char_info.width, char_info.y + char_info.height) / font_texture_size, // TR
            glm::vec2(char_info.x, char_info.y) / font_texture_size,                                      // BL
            glm::vec2(char_info.x + char_info.width, char_info.y) / font_texture_size,                    // BR
        };

        // Create Triangle 0
        glm::vec4 &v0 = vertex_array[vertex_count++]; // BL
        v0 = glm::vec4{positions[0], uvs[0]};

        glm::vec4 &v1 = vertex_array[vertex_count++]; // BR
        v1 = glm::vec4{positions[1], uvs[1]};

        glm::vec4 &v2 = vertex_array[vertex_count++]; // TL
        v2 = glm::vec4{positions[2], uvs[2]};

        glm::vec4 &v3 = vertex_array[vertex_count++]; // TL
        v3 = glm::vec4{positions[2], uvs[2]};

        glm::vec4 &v4 = vertex_array[vertex_count++]; // BR
        v4 = glm::vec4{positions[1], uvs[1]};

        glm::vec4 &v5 = vertex_array[vertex_count++]; // TR
        v5 = glm::vec4{positions[3], uvs[3]};

        position.x += char_info.advance + char_info.width;
    }

    TextRenderManager *TextRenderManager::Instance = nullptr;

    TextRenderer::~TextRenderer() {
        RenderingDevice::get()->destroy_buffers(&vertex_buffer, 1);
    }

    TextRenderManager::TextRenderManager() {
        Instance = this;
        shader = std::make_shared<ShaderMaterial>("FontShader");
        shader->create_from_file({
            "SPIRV/font.vert.spv",
            "SPIRV/font.frag.spv",
        });
        shader->set_depth_test(false);
        shader->set_depth_write(false);
    }

    TextRenderManager::~TextRenderManager() {
        for (auto &renderer : renderers) {
            renderer.reset();
        }
        renderers.clear();
    }

    TextRenderer *TextRenderManager::get_renderer_by_font(Font *font) {
        auto found = std::find_if(renderers.begin(), renderers.end(), [&](std::shared_ptr<TextRenderer> renderer) {
            return font->texture == renderer->font->texture;
        });
        if (found != renderers.end()) {
            return found->get();
        } else {
            std::shared_ptr<TextRenderer> renderer = std::make_shared<TextRenderer>(font);
            renderers.push_back(renderer);
            return renderers.back().get();
        }
    }
} // namespace mirai