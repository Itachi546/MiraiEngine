#pragma once

#include "Graphics/RenderingDevice.hpp"

namespace mirai {
    class ComputeShader;
    class EnvironmentMap {
      public:
        EnvironmentMap(const std::string &hdri_path);

        EnvironmentMap();

        TextureID get_cubemap() const {
            return cubemap_texture;
        }

        TextureID get_irradiance_map() const {
            return irradiance_texture;
        }

        void set_cubemap_size(uint32_t size) {
            this->cubemap_size = size;
        }

        uint32_t get_cubemap_size() const {
            return cubemap_size;
        }

        uint32_t get_irradiance_map_size() const {
            return irradiance_map_size;
        }

        std::string &get_hdri_path() {
            return hdri_path;
        }

        ~EnvironmentMap();

      private:
        std::string hdri_path;
        TextureID cubemap_texture, irradiance_texture;
        uint32_t cubemap_size = 512;
        uint32_t irradiance_map_size = 64;

        void generate_cubemap(CommandBuffer *command_buffer, ComputeShader &cubemap_shader);
        void convolute_cubemap(CommandBuffer *command_buffer, ComputeShader &convolute_shader);

        void initialize_textures();
    };
} // namespace mirai