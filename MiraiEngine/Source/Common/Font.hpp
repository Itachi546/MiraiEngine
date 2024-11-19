#pragma once

#include "Graphics/RenderingDevice.hpp"

#include <string>
#include <memory>
#include <array>

namespace mirai {

    struct FontCharacterInfo {
        int x;
        int y;
        int width;
        int height;
        int origin_x;
        int origin_y;
        int advance;
    };

    struct Font {
        Font() = default;
        Font(const Font &) = delete;
        Font &operator=(Font &) = delete;

        void set_texture(TextureID texture) {
            this->texture = texture;
        }

        ~Font() {
            RenderingDevice::get()->destroy_textures(&texture, 1);
        }

        TextureID texture;
        std::array<FontCharacterInfo, 512> character_info;
        uint32_t width, height;
        uint32_t font_size;
    };

    std::unique_ptr<Font> LoadFont(const std::string &name);
} // namespace mirai