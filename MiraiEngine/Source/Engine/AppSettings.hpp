#pragma once

#include <stdint.h>
#include "Graphics/RenderingDevice.hpp"

namespace mirai {
namespace AppSettings {
    // Window Specific Settings
    extern uint32_t default_window_width;
    extern uint32_t default_window_height;

    // Renderer Settings
    extern RenderMode render_mode;
    extern bool enable_rt_shadow;
};
} // namespace mirai::AppSettings