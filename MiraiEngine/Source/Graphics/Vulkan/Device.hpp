#pragma once

#include "Vulkan.hpp"

#include <vector>

namespace mirai {
    struct PhysicalDeviceInfo {
        VkPhysicalDevice physical_device;
        std::vector<VkExtensionProperties> supported_extensions;
        GpuVendorInfo vendor_info;
    };

    bool IsExtensionAvailable(const std::vector<VkExtensionProperties> &supported_extensions, const char *required_extension);
    bool IsExtensionsAvailable(const std::vector<VkExtensionProperties> &supported_extensions, const std::vector<const char *> &required_extensions);
    void EnumeratePhysicalDevices(VkInstance instance, std::vector<PhysicalDeviceInfo> &device_infos);
    // VkPhysicalDevice SelectPhysicalDevice(VkInstance instance, std::vector<GpuDevice> &gpus, const std::vector<const char *> &required_device_extensions);

    VkDevice CreateDevice(VkInstance instance, VkPhysicalDevice physical_device, const std::vector<uint32_t> &queue_family_indices, const std::vector<const char *> &required_extensions);

    void GetDeviceQueueFamilies(VkPhysicalDevice physical_device, std::vector<uint32_t> &queue_family_indices);
} // namespace mirai