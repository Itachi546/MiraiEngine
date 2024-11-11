#include "RenderingDevice.hpp"

#include "Common/FileUtils.hpp"
#include "Engine/Log.hpp"

namespace mirai {
    RenderingDevice *RenderingDevice::Instance = nullptr;

    ShaderID rendering_utils::create_shader_module_from_file(const std::string &filename) {
        std::optional<std::string> result = utils::read_file_binary(filename);
        if (result.has_value()) {
            std::string content = result.value();
            return RenderingDevice::get()->create_shader((uint32_t *)(content.c_str()), static_cast<uint32_t>(content.length()), filename);
        } else {
            Log::Error("Error loading file: ", filename);
            return ShaderID{K_INVALID_ID};
        }
    }

} // namespace mirai