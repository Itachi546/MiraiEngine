#pragma once

#include <unordered_map>
#include "Graphics/RenderingDevice.hpp"
#include "Engine/Log.hpp"

namespace mirai {

    class TextureCache {
      public:
        TextureCache();
        TextureCache(const TextureCache&) = delete;
        void operator=(const TextureCache&) = delete;

        static TextureCache *get() {
            return Instance;
        }

        TextureID get_texture_id(const std::string &name);

        void add_texture(const std::string &name, TextureID texture);

        uint32_t get_texture_count() const {
            return cast_u32(textures_map.size());
        }

        ~TextureCache() {
            RenderingDevice *device = RenderingDevice::get();
            Log::Info("Destroying ", textures_map.size(), " sampler textures...");
            for (auto &[key, val] : textures_map) {
                device->destroy_textures(&val, 1);
            }
        }

      private:
        static TextureCache *Instance;
        std::unordered_map<uint32_t, TextureID> textures_map;
    };

} // namespace mirai