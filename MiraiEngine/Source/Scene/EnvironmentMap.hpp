#pragma once

#include "Graphics/RenderingDevice.hpp"

namespace mirai {

    class EnvironmentMap {
      public:
        EnvironmentMap(const std::string &hdri_path);

        TextureID get_cubemap() {
            return cubemap_texture;
        }

        void set_cubemap_size(uint32_t size) {
            this->cubemap_size = size;
        }

        uint32_t get_cubemap_size() {
            return cubemap_size;
        }

        std::string &get_hdri_path() {
            return hdri_path;
        }

        ~EnvironmentMap();

      private:
        std::string hdri_path;
        TextureID cubemap_texture;
        uint32_t cubemap_size = 512;
        void generate(TextureID hdri_texture);
    };
} // namespace mirai