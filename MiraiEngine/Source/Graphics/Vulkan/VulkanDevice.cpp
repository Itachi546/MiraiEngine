#include "VulkanDevice.hpp"
#include <string.h>

namespace mirai {

    bool IsExtensionAvailable(const std::vector<VkExtensionProperties> &supported_extensions, const char *required_extension) {
        for (auto &supported : supported_extensions) {
            if (strcmp(required_extension, supported.extensionName) == 0) {
                return true;
            }
        }
        return false;
    }

    bool IsExtensionsAvailable(const std::vector<VkExtensionProperties> &supported_extensions, const std::vector<const char *> &required_extensions) {
        for (auto &requested : required_extensions) {
            if (!IsExtensionAvailable(supported_extensions, requested)) {
                Log::Warn("Extension not supported: ", requested);
                return false;
            }
        }
        return true;
    }

    void EnumeratePhysicalDevices(VkInstance instance, std::vector<PhysicalDeviceInfo> &device_infos) {
        uint32_t device_count = 0;
        VK_CHECK(vkEnumeratePhysicalDevices(instance, &device_count, nullptr));
        if (device_count == 0)
            Log::Fatal("VULKAN::No Vulkan Supported GPU Found");

        device_infos.resize(device_count);

        std::vector<VkPhysicalDevice> physical_devices(device_count);
        VK_CHECK(vkEnumeratePhysicalDevices(instance, &device_count, physical_devices.data()));

        for (uint32_t i = 0; i < device_count; ++i) {
            VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
            vkGetPhysicalDeviceProperties2(physical_devices[i], &properties);

            PhysicalDeviceInfo &device_info = device_infos[i];
            device_info.vendor_info.device_type = static_cast<DeviceType>(properties.properties.deviceType);
            device_info.vendor_info.vendor = properties.properties.vendorID;
            device_info.vendor_info.name = properties.properties.deviceName;
            device_info.physical_device = physical_devices[i];
            Log::Info("VULKAN::DeviceName: ", properties.properties.deviceName);

            uint32_t extension_count = 0;
            VK_CHECK(vkEnumerateDeviceExtensionProperties(physical_devices[i], nullptr, &extension_count, nullptr));
            device_info.supported_extensions.resize(extension_count);
            VK_CHECK(vkEnumerateDeviceExtensionProperties(physical_devices[i], nullptr, &extension_count, device_info.supported_extensions.data()));
        }
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

    VkDevice CreateDevice(VkPhysicalDevice physical_device, const std::vector<uint32_t> &queue_family_indices, const std::vector<const char *> &required_extensions, bool support_raytracing) {
        VkPhysicalDeviceDescriptorIndexingFeatures indexing_features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT, nullptr};
        VkPhysicalDeviceFeatures2 supported_features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &indexing_features};
        vkGetPhysicalDeviceFeatures2(physical_device, &supported_features);

        bool bindless_supported = indexing_features.descriptorBindingPartiallyBound && indexing_features.runtimeDescriptorArray;
        if (!bindless_supported) {
            Log::Fatal("VULKAN::FEATURE::Bindless Resource (Not Supported)");
        }

        Log::Info("VULKAN::FEATURE::Bindless Resource (Supported)");
        if (support_raytracing)
            Log::Info("VULKAN::FEATURE::Raytracing (Supported)");
        else
            Log::Warn("VULKAN::FEATURE::Raytracing (Not Supported)");

        VkPhysicalDeviceFeatures2 device_features2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        device_features2.features.fragmentStoresAndAtomics = true;
        device_features2.features.multiDrawIndirect = true;
        device_features2.features.pipelineStatisticsQuery = true;
        device_features2.features.shaderInt16 = true;
        device_features2.features.samplerAnisotropy = true;
        device_features2.features.geometryShader = true;
        device_features2.features.wideLines = true;
        device_features2.features.shaderInt64 = true;
        device_features2.features.depthClamp = true;
        device_features2.features.imageCubeArray = true;
        device_features2.features.fillModeNonSolid = true;

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
        device_features12.uniformAndStorageBuffer8BitAccess = true;

        VkPhysicalDeviceVulkan13Features device_features13 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        device_features13.dynamicRendering = true;
        device_features13.synchronization2 = true;
        device_features13.shaderDemoteToHelperInvocation = true;

        VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
            .pNext = nullptr,
            .accelerationStructure = true,
            .descriptorBindingAccelerationStructureUpdateAfterBind = true,
        };
        device_features2.pNext = &device_features11;
        device_features11.pNext = &device_features12;
        device_features12.pNext = &device_features13;

        VkPhysicalDeviceRayQueryFeaturesKHR ray_query_feature = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
            .pNext = &acceleration_structure_features,
            .rayQuery = true,
        };

        VkPhysicalDeviceDescriptorHeapFeaturesEXT descriptor_heap_feature = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_FEATURES_EXT,
            .pNext = nullptr,
            .descriptorHeap = true,
            .descriptorHeapCaptureReplay = false,
        };
        device_features13.pNext = &descriptor_heap_feature;

        if (support_raytracing) {
            // Required by raytracing
            device_features12.descriptorIndexing = true;
            device_features12.bufferDeviceAddress = true;
            descriptor_heap_feature.pNext = &ray_query_feature;
        }

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
    } // namespace mirai

} // namespace mirai