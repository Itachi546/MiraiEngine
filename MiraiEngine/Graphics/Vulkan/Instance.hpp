#pragma once

#include "Vulkan.hpp"

namespace mirai {
    VkInstance CreateInstance(const std::vector<const char *> &validation_layers, const std::vector<const char *> &instance_extensions, bool enable_validation);

    VkDebugUtilsMessengerEXT CreateDebugUtilMessanger(VkInstance instance);
} // namespace mirai