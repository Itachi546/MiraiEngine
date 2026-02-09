#pragma once

#include "Graphics/RenderingDevice.hpp"
#include "Scene/Shader.hpp"

namespace mirai {
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

        TextureID get_prefilter_map() const {
            return prefilter_texture;
        }

        TextureID get_brdf_texture() const {
            return brdf_texture;
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

        uint32_t get_prefilter_map_size() const {
            return prefilter_map_size;
        }

        uint32_t get_brdf_texture_size() const {
            return brdf_texture_size;
        }

        std::string &get_hdri_path() {
            return hdri_path;
        }

        ~EnvironmentMap();

      private:
        std::string hdri_path;
        TextureID cubemap_texture, irradiance_texture, prefilter_texture, brdf_texture;
        SamplerID default_sampler;
        uint32_t cubemap_size = 512;
        uint32_t irradiance_map_size = 64;
        uint32_t prefilter_map_size = 512;
        uint32_t brdf_texture_size = 512;
        uint32_t prefilter_num_mip_levels = 7;

        void generate_cubemap(CommandBuffer *command_buffer, Shader *cubemap_shader);
        void convolute_diffuse_cubemap(CommandBuffer *command_buffer, Shader *convolute_shader);
        void convolute_specular_cubemap(CommandBuffer *command_buffer, Shader *prefilter_shader);
        void integrate_brdf_texture(CommandBuffer *command_buffer, Shader *integrate_brdf_shader);

        void initialize_textures();
        void create_pbr_env_map();
    };
} // namespace mirai