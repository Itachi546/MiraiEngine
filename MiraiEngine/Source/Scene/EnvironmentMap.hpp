#pragma once

#include "Graphics/RenderingDevice.hpp"

namespace mirai {
    struct ComputeShader;
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

        std::string &get_hdri_path() {
            return hdri_path;
        }

        ~EnvironmentMap();

      private:
        std::string hdri_path;
        TextureID cubemap_texture, irradiance_texture, prefilter_texture, brdf_texture;

        void generate_cubemap(CommandBuffer *command_buffer, ComputeShader *cubemap_shader, uint32_t *descriptors, uint32_t descriptor_count);
        // void convolute_diffuse_cubemap(CommandBuffer *command_buffer, Shader *convolute_shader);
        // void convolute_specular_cubemap(CommandBuffer *command_buffer, Shader *prefilter_shader);
        // void integrate_brdf_texture(CommandBuffer *command_buffer, Shader *integrate_brdf_shader);
        void initialize_textures();

        // void create_pbr_env_map();
    };
} // namespace mirai