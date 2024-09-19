#pragma once

#include "Vulkan.hpp"
#include <vector>

namespace mirai
{
    struct VulkanSwapchain
    {
        VkSwapchainKHR swapchain;
        std::vector<VkImage> images;
        std::vector<VkImageView> image_views;
        uint32_t width, height;
        uint32_t image_count;
        uint32_t current_image_index;
        VkSurfaceFormatKHR surface_format;
        VkPresentModeKHR present_mode;
        VkCompositeAlphaFlagBitsKHR composite_mode;
        VkSurfaceTransformFlagBitsKHR current_transform;
        std::vector<VkImageLayout> image_layouts;

        VkImageView get_current_image_view()
        {
            return image_views[current_image_index];
        }

        VkImage get_current_image()
        {
            return images[current_image_index];
        }

        VkImageLayout get_current_image_layout()
        {
            return image_layouts[current_image_index];
        }

        void set_current_image_layout(VkImageLayout layout)
        {
            image_layouts[current_image_index] = layout;
        }
    };

    bool PhysicalDeviceSupportPresentation(VkInstance instance, VkPhysicalDevice physicalDevice, uint32_t graphics_queue_index);

    VkSurfaceKHR CreateSurface(VkInstance instance, VkPhysicalDevice physical_device, uint32_t graphics_queue_index);

    void CreateSwapchain(VulkanSwapchain *swapchain, VkPhysicalDevice physical_device, VkDevice device, VkSurfaceKHR surface, bool vsync);

    void ResizeSwapchain(VulkanSwapchain *swapchain, VkPhysicalDevice physical_device, VkDevice device, VkSurfaceKHR surface, bool vsync);

} // namespace mirai