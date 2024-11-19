#include "TextureCache.hpp"
#include "Common/Hash.hpp"

namespace mirai {

    TextureCache *TextureCache::Instance = nullptr;

    TextureCache::TextureCache() {
        Instance = this;
    }

    TextureID TextureCache::get_texture_id(const std::string &name) {
        auto found = textures_map.find(utils::djb2_hash_string(name));
        if (found == textures_map.end())
            return TextureID{K_INVALID_ID};
        else
            return found->second;
    }

    void TextureCache::add_texture(const std::string &name, TextureID texture) {
        textures_map.insert(std::make_pair(utils::djb2_hash_string(name), texture));
    }

}; // namespace mirai