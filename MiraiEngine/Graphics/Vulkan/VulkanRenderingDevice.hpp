#pragma once

#include "Graphics/RenderingDevice.h"
#include "Vulkan.hpp"

#include <vector>
#include <memory>

namespace mirai
{
    struct VulkanSwapchain;

    class VulkanRenderingDevice
    {
      public:
        VulkanRenderingDevice();

        void set_enable_validation(bool enable_validation)
        {
            enable_validation = enable_validation;
        }

        bool is_validation_enabled() const
        {
            return enable_validation;
        }

        VkInstance get_vulkan_instance() const { return instance; }
        VkDevice get_vulkan_device() const { return device; }
        VkPhysicalDevice get_physical_device() const { return physical_device; }
        VkSurfaceKHR get_surface() const { return surface; }
        const VulkanSwapchain *get_swapchain() const { return swapchain.get(); }

        VkQueue get_device_queue(QueueType queue_type) { return device_queues[queue_type]; }
        uint32_t get_queue_family_indices(QueueType queue_type) { return queue_family_indices[queue_type]; }

        ~VulkanRenderingDevice();

      private:
        void set_debug_marker_object_name(VkObjectType objectType, uint64_t handle, const char *objectName);

        static VulkanRenderingDevice *Instance;

        std::vector<const char *> instance_extensions;
        std::vector<const char *> validation_layers;
        std::vector<const char *> device_extensions;

        VkInstance instance;
        VkDevice device;
        VkPhysicalDevice physical_device;
        VkSurfaceKHR surface;

        std::vector<GpuDevice> gpus;
        std::vector<uint32_t> queue_family_indices;
        std::vector<VkQueue> device_queues;
        std::unique_ptr<VulkanSwapchain> swapchain;

        VkDebugUtilsMessengerEXT debug_utils_messenger;
        bool enable_validation;
    };
} // namespace mirai