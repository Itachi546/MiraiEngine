#include "VulkanRenderingDevice.hpp"

#include "Instance.hpp"
#include "Device.hpp"
#include "Swapchain.h"

namespace mirai
{

    VulkanRenderingDevice *VulkanRenderingDevice::Instance = nullptr;

    void VulkanRenderingDevice::set_debug_marker_object_name(VkObjectType objectType, uint64_t handle, const char *objectName)
    {
        if (!enable_validation)
            return;

        VkDebugUtilsObjectNameInfoEXT name_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .objectType = objectType,
            .objectHandle = handle,
            .pObjectName = objectName,
        };

        VK_CHECK(vkSetDebugUtilsObjectNameEXT(device, &name_info));
    }

    VulkanRenderingDevice::VulkanRenderingDevice()
    {
        instance_extensions = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME,
#ifdef MIRAI_PLATFORM_WINDOW
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#endif
        };

        device_extensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        };

        validation_layers = {
            "VK_LAYER_KHRONOS_validation",
            "VK_LAYER_KHRONOS_synchronization2",
        };

        if (enable_validation)
        {
            instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            instance_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        }

        instance = CreateInstance(validation_layers, instance_extensions, enable_validation);
        volkLoadInstance(instance);

        if (enable_validation)
            debug_utils_messenger = CreateDebugUtilMessanger(instance);
        else
            debug_utils_messenger = VK_NULL_HANDLE;

        physical_device = SelectPhysicalDevice(instance, gpus, device_extensions);

        GetDeviceQueueFamilies(physical_device, queue_family_indices);

        uint32_t graphics_queue = queue_family_indices[QueueType_Graphics];
        if (!PhysicalDeviceSupportPresentation(instance, physical_device, graphics_queue))
            Log::Fatal("Selected Physical Device Doesn't Support Presentation!!!");

        device = CreateDevice(instance, physical_device, queue_family_indices, device_extensions);

        device_queues.resize(queue_family_indices.size());
        vkGetDeviceQueue(device, graphics_queue, 0, &device_queues[QueueType_Graphics]);

        uint32_t transfer_queue = queue_family_indices[QueueType_Transfer];
        if (transfer_queue != K_INVALID_QUEUE_ID)
            vkGetDeviceQueue(device, transfer_queue, 0, &device_queues[QueueType_Transfer]);

        uint32_t compute_queue = queue_family_indices[QueueType_Compute];
        if (compute_queue != K_INVALID_QUEUE_ID)
            vkGetDeviceQueue(device, compute_queue, 0, &device_queues[QueueType_Compute]);

        surface = CreateSurface(instance, physical_device, graphics_queue);

        swapchain = std::make_unique<VulkanSwapchain>();
        swapchain->swapchain = VK_NULL_HANDLE;
        CreateSwapchain(swapchain.get(), physical_device, device, surface, true);
        for (uint32_t i = 0; i < swapchain->image_count; ++i)
        {
            std::string image_name = "swapchain_image_" + std::to_string(i);
            set_debug_marker_object_name(VK_OBJECT_TYPE_IMAGE, (uint64_t)swapchain->images[i], image_name.c_str());

            std::string image_view_name = "swapchain_image_view" + std::to_string(i);
            set_debug_marker_object_name(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)swapchain->image_views[i], image_view_name.c_str());
        }
    }

    VulkanRenderingDevice::~VulkanRenderingDevice()
    {
        vkDestroySwapchainKHR(device, swapchain->swapchain, nullptr);
        for (auto &image_view : swapchain->image_views)
        {
            vkDestroyImageView(device, image_view, nullptr);
        }

        swapchain = nullptr;
        vkDestroySurfaceKHR(instance, surface, nullptr);

        vkDestroyDevice(device, nullptr);
        if (debug_utils_messenger != VK_NULL_HANDLE)
            vkDestroyDebugUtilsMessengerEXT(instance, debug_utils_messenger, nullptr);
        vkDestroyInstance(instance, nullptr);
    }
} // namespace mirai
