#include "AppSettings.hpp"

namespace mirai {
namespace AppSettings {

    // Window Specific Settings
    uint32_t default_window_width = 1920;
    uint32_t default_window_height = 1080;

    // Renderer Settings
    RenderMode render_mode = RenderMode::RENDERMODE_DEFERRED;
    bool enable_rt_shadow = false;
}
} // namespace mirai::AppSettings