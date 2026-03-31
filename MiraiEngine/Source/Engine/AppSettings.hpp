#pragma once

#include <stdint.h>
#include "Graphics/RenderingDevice.hpp"

namespace mirai {
namespace AppSettings {
    // Window Specific Settings
    extern uint32_t default_window_width;
    extern uint32_t default_window_height;

    extern float resolution_scale;

    // Renderer Settings
    extern RenderMode render_mode;
    extern bool enable_rt_shadow;

    // Vulkan Specific settings
    constexpr uint32_t K_RESOURCE_DESCRIPTOR_LIMIT = 65536;
    constexpr uint32_t K_PER_FRAME_RESOURCE_DESCRIPTOR_LIMIT = 1024;
    constexpr uint32_t K_SAMPLER_DESCRIPTOR_LIMIT = 16;

    constexpr uint32_t K_NUM_THREAD = 2;
    constexpr uint32_t K_NUM_COMMAND_BUFFER_PER_THREAD = 3;
    constexpr uint32_t K_MAX_FRAME_IN_FLIGHTS = 2;
};
} // namespace mirai::AppSettings