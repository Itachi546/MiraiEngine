#include "Font.hpp"

#define STB_IMAGE_IMPELMENTATION
#include <stb_image.h>
#include <json.hpp>
#include <fstream>

namespace mirai {
    using json = nlohmann::json;

    std::unique_ptr<Font> LoadFont(const std::string &name) {
        std::string texture_path = "Assets/Fonts/" + name + ".png";
        FILE *file = fopen(texture_path.c_str(), "rb");
        if (file == nullptr)
            return nullptr;
        std::unique_ptr<FILE, int (*)(FILE *)> file_ptr(file, fclose);

        std::ifstream meta_file("Assets/Fonts/" + name + ".json");
        if (!meta_file)
            return nullptr;

        int width, height, num_channel;
        unsigned char *data = stbi_load_from_file(file, &width, &height, &num_channel, 1);
        file_ptr.reset();
        file_ptr = nullptr;

        if (data == nullptr)
            return nullptr;

        SamplerDescription sampler_desc = SamplerDescription::create();
        sampler_desc.enable_anisotropy = false;
        TextureDescription tex_desc = {
            .width = static_cast<uint32_t>(width),
            .height = static_cast<uint32_t>(height),
            .depth = 1,
            .mip_levels = 1,
            .array_layers = 1,
            .texture_type = TEXTURE_TYPE_2D,
            .format = FORMAT_R8_UNORM,
            .usage_flags = TEXTURE_USAGE_TRANSFER_DST_BIT | TEXTURE_USAGE_SAMPLED_BIT,
            .sampler_desc = &sampler_desc,
        };
        TextureID texture = RenderingDevice::get()->create_texture(&tex_desc, "font_texture_" + name);
        rendering_utils::copy_texture_immediate(texture, data, width * height);
        RenderingDevice::get()->add_bindless_texture(&texture, 1);
        stbi_image_free(data);

        // RenderingDevice::get()->add_bindless_texture(&texture, 1);

        std::unique_ptr<Font> font = std::make_unique<Font>();
        font->set_texture(texture);
        font->width = width;
        font->height = height;
        // Load metadata file
        json json_data = json::parse(meta_file);
        json_data.at("size").get_to(font->font_size);

        auto characters = json_data["characters"];
        for (auto c = characters.begin(); c != characters.end(); ++c) {
            std::string key = c.key();
            ASSERT(key.size() == 1);
            int char_index = static_cast<int>(key[0]);
            ASSERT(char_index > 0 && char_index < 512);
            FontCharacterInfo &char_info = font->character_info[char_index];

            auto &json_char_info = c.value();
            json_char_info.at("x").get_to(char_info.x);
            json_char_info.at("y").get_to(char_info.y);
            json_char_info.at("width").get_to(char_info.width);
            json_char_info.at("height").get_to(char_info.height);
            json_char_info.at("originX").get_to(char_info.origin_x);
            json_char_info.at("originY").get_to(char_info.origin_y);
            json_char_info.at("advance").get_to(char_info.advance);
            /*
            char_info.y = c["y"];
            char_info.width = c["width"];
            char_info.height = c["height"];
            char_info.origin_x = c["originX"];
            char_info.origin_y = c["originY"];
            char_info.advance = c["advance"];
            */
        }
        return std::move(font);
    }

} // namespace mirai