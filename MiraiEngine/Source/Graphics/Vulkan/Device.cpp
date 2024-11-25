#include "Device.hpp"
#include <string.h>

#define GPU_TYPE_INTEGRATED 1
namespace mirai {
    bool is_device_extensions_available(VkPhysicalDevice physical_device, const std::vector<const char *> &requested_extensions) {
        uint32_t extension_count = 0;
        VK_CHECK(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr));
        std::vector<VkExtensionProperties> supported_extensions(extension_count);
        VK_CHECK(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, supported_extensions.data()));

        for (auto &requested : requested_extensions) {
            bool available = false;
            for (auto &supported : supported_extensions) {
                if (strcmp(requested, supported.extensionName) == 0) {
                    available = true;
                    break;
                }
            }

            if (!available) {
                Log::Error("VULKAN::Failed to find device extension: " + std::string(requested));
                return false;
            }
        }

        return true;
    }

    VkPhysicalDevice SelectPhysicalDevice(VkInstance instance, std::vector<GpuDevice> &gpus, const std::vector<const char *> &required_device_extensions) {
        uint32_t device_count = 0;
        VK_CHECK(vkEnumeratePhysicalDevices(instance, &device_count, nullptr));
        if (device_count == 0)
            Log::Fatal("VULKAN::No Vulkan Supported GPU Found");

        std::vector<VkPhysicalDevice> physical_devices(device_count);
        gpus.resize(device_count);

        VK_CHECK(vkEnumeratePhysicalDevices(instance, &device_count, physical_devices.data()));

        for (uint32_t i = 0; i < device_count; ++i) {
            VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
            vkGetPhysicalDeviceProperties2(physical_devices[i], &properties);
            gpus[i].device_type = static_cast<DeviceType>(properties.properties.deviceType);
            gpus[i].vendor = properties.properties.vendorID;
            gpus[i].name = properties.properties.deviceName;

            Log::Info("VULKAN::DeviceName: ", properties.properties.deviceName);
        }

#if GPU_TYPE_INTEGRATED
        DeviceType device_type = DeviceType::DEVICE_TYPE_INTEGRATED_GPU;
#else
        DeviceType device_type = DeviceType::DEVICE_TYPE_DISCRETE_GPU;
#endif
        VkPhysicalDevice physical_device = physical_devices[0];
        for (uint32_t i = 0; i < physical_devices.size(); ++i) {
            if (gpus[i].device_type == device_type) {
                Log::Info("VULKAN::Selected Device: ", gpus[i].name);
                physical_device = physical_devices[i];
                break;
            }
        }

        if (!is_device_extensions_available(physical_device, required_device_extensions))
            Log::Fatal("VULKAN::Physical device doesn't support required extensions...");

        return physical_device;
    }

    void GetDeviceQueueFamilies(VkPhysicalDevice physical_device, std::vector<uint32_t> &queue_family_indices) {
        uint32_t queue_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_count, nullptr);
        if (queue_count == 0)
            Log::Fatal("VULKAN::Queue count is zero");

        std::vector<VkQueueFamilyProperties> queue_family_properties(queue_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_count, queue_family_properties.data());

        queue_family_indices.resize(3);
        queue_family_indices[QUEUE_TYPE_GRAPHICS] = K_INVALID_QUEUE_ID;
        queue_family_indices[QUEUE_TYPE_TRANSFER] = K_INVALID_QUEUE_ID;
        queue_family_indices[QUEUE_TYPE_COMPUTE] = K_INVALID_QUEUE_ID;

        for (uint32_t i = 0; i < queue_count; ++i) {
            VkQueueFamilyProperties queue_family_property = queue_family_properties[i];
            if (queue_family_property.queueCount == 0)
                continue;

            // Search for main queue that should be able to do all work (graphics, compute and transfer)
            if ((queue_family_property.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) == (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
                queue_family_indices[QUEUE_TYPE_GRAPHICS] = i;
            }
            // Search for transfer queue
            if ((queue_family_property.queueFlags & VK_QUEUE_COMPUTE_BIT) == 0 && (queue_family_property.queueFlags & VK_QUEUE_TRANSFER_BIT)) {
                queue_family_indices[QUEUE_TYPE_TRANSFER] = i;
            }
        }

        ASSERT_MSG(queue_family_indices[QUEUE_TYPE_GRAPHICS] != K_INVALID_QUEUE_ID, "Graphics Queue is not supported...");
    }

    VkDevice CreateDevice(VkInstance instance, VkPhysicalDevice physical_device, const std::vector<uint32_t> &queue_family_indices, const std::vector<const char *> &required_extensions) {
        VkPhysicalDeviceDescriptorIndexingFeatures indexing_features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT, nullptr};
        VkPhysicalDeviceFeatures2 supported_features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &indexing_features};
        vkGetPhysicalDeviceFeatures2(physical_device, &supported_features);

        bool bindless_supported = indexing_features.descriptorBindingPartiallyBound && indexing_features.runtimeDescriptorArray;
        if (!bindless_supported) {
            Log::Fatal("VULKAN::Bindless resources is not supported ...");
        }

        Log::Info("VULKAN::Bindless Resources: Supported");

        VkPhysicalDeviceFeatures2 device_features2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        device_features2.features.fragmentStoresAndAtomics = true;
        device_features2.features.multiDrawIndirect = true;
        device_features2.features.pipelineStatisticsQuery = true;
        device_features2.features.shaderInt16 = true;
        device_features2.features.samplerAnisotropy = true;
        device_features2.features.geometryShader = true;
        device_features2.features.wideLines = true;
        device_features2.features.shaderInt64 = true;

        VkPhysicalDeviceVulkan11Features device_features11 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
        device_features11.shaderDrawParameters = true;

        VkPhysicalDeviceVulkan12Features device_features12 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        device_features12.drawIndirectCount = true;
        device_features12.shaderInt8 = true;
        device_features12.timelineSemaphore = true;
        device_features12.descriptorBindingPartiallyBound = true;
        device_features12.descriptorBindingSampledImageUpdateAfterBind = true;
        device_features12.descriptorBindingVariableDescriptorCount = true;
        device_features12.runtimeDescriptorArray = true;
        device_features12.shaderSampledImageArrayNonUniformIndexing = true;
        //  device_features12.shaderBufferInt64Atomics = true;

        VkPhysicalDeviceVulkan13Features device_features13 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        device_features13.dynamicRendering = true;
        device_features13.synchronization2 = true;

        device_features2.pNext = &device_features11;
        device_features11.pNext = &device_features12;
        device_features12.pNext = &device_features13;
        device_features13.pNext = nullptr;

        std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
        float queue_priorities[] = {0.0f};
        for (uint32_t i = 0; i < queue_family_indices.size(); ++i) {
            if (queue_family_indices[i] != K_INVALID_QUEUE_ID) {
                queue_create_infos.push_back(VkDeviceQueueCreateInfo{
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = queue_family_indices[i],
                    .queueCount = 1,
                    .pQueuePriorities = queue_priorities,
                });
            }
        }

        VkDeviceCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = &device_features2,
            .queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size()),
            .pQueueCreateInfos = queue_create_infos.data(),
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = static_cast<uint32_t>(required_extensions.size()),
            .ppEnabledExtensionNames = required_extensions.data(),
            .pEnabledFeatures = nullptr,
        };

        VkDevice device = VK_NULL_HANDLE;
        VK_CHECK(vkCreateDevice(physical_device, &createInfo, nullptr, &device));
        return device;
    }

} // namespace mirai