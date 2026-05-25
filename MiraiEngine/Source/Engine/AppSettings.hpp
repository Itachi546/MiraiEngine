#pragma once

#include <stdint.h>
#include "Graphics/RenderingDevice.hpp"

namespace mirai {
    namespace AppSettings {
        // Window Specific Settings
        extern uint32_t default_window_width;
        extern uint32_t default_window_height;

        extern bool enable_vsync;

        extern float resolution_scale;

        // Renderer Settings
        extern RenderMode render_mode;
        extern bool enable_rt_shadow;
        extern bool enable_debug_draw;
        extern bool enable_debug_light_tile;
        extern bool enable_taa;

        extern float ibl_contribution;

        constexpr uint32_t K_SKINNED_VERTEX_OUTPUT_SIZE = 32;
        constexpr uint32_t K_LIGHT_TILE_SIZE = 16;
        constexpr uint32_t K_MAX_LIGHT_PER_TILE = 256;

        // Vulkan Specific settings
        constexpr uint32_t K_RESOURCE_DESCRIPTOR_LIMIT = 65536;
        constexpr uint32_t K_PER_FRAME_RESOURCE_DESCRIPTOR_LIMIT = 1024;
        constexpr uint32_t K_SAMPLER_DESCRIPTOR_LIMIT = 16;

        constexpr uint32_t K_NUM_THREAD = 3;
        constexpr uint32_t K_NUM_COMMAND_BUFFER_PER_THREAD = 3;
        constexpr uint32_t K_MAX_FRAME_IN_FLIGHTS = 3;

        constexpr uint32_t K_MAX_BINDLESS_TEXTURE_COUNT = 4096;

        inline uint32_t get_width() {
            return cast_u32(default_window_width * resolution_scale);
        }

        inline uint32_t get_height() {
            return cast_u32(default_window_height * resolution_scale);
        }
    }; // namespace AppSettings

    namespace EnvironmentSettings {
        constexpr uint32_t K_CUBEMAP_SIZE = 512;
        constexpr uint32_t K_IRRADIANCE_MAP_SIZE = 64;
        constexpr uint32_t K_PREFILTER_MAP_SIZE = 512;
        constexpr uint32_t K_PREFILTER_MAP_MAX_MIP_LEVELS = 7;
        constexpr uint32_t K_BRDF_MAP_SIZE = 512;
    }; // namespace EnvironmentSettings

} // namespace mirai