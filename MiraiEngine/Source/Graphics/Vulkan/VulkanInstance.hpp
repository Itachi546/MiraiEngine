#pragma once

#include "Vulkan.hpp"

namespace mirai {
    VkInstance CreateInstance(const std::vector<const char *> &validation_layers, const std::vector<const char *> &instance_extensions);

    VkDebugReportCallbackEXT RegisterDebugCallback(VkInstance instance);
} // namespace mirai