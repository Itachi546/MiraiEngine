#include "Swapchain.h"

#include "Device/Window.hpp"

#ifdef MIRAI_PLATFORM_WINDOW
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

namespace mirai
{

    bool PhysicalDeviceSupportPresentation(VkInstance instance, VkPhysicalDevice physicalDevice, uint32_t graphics_queue_index)
    {
        static PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR vk_check_presentation_support = (PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR)VK_LOAD_FUNCTION(instance, "vkGetPhysicalDeviceWin32PresentationSupportKHR");
        return vk_check_presentation_support(physicalDevice, graphics_queue_index);
        return false;
    }

    static VkSurfaceKHR create_win32_surface(VkInstance instance)
    {
        GLFWwindow *window = static_cast<GLFWwindow *>(Window::get()->get_window_ptr());
        HWND hwnd = glfwGetWin32Window(window);

        VkWin32SurfaceCreateInfoKHR create_info = {
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .hinstance = GetModuleHandle(0),
            .hwnd = hwnd,
        };

        VkSurfaceKHR surface = VK_NULL_HANDLE;
        PFN_vkCreateWin32SurfaceKHR create_win32_surface = (PFN_vkCreateWin32SurfaceKHR)VK_LOAD_FUNCTION(instance, "vkCreateWin32SurfaceKHR");
        VK_CHECK(create_win32_surface(instance, &create_info, nullptr, &surface));
        return surface;
    }

    VkSurfaceKHR CreateSurface(VkInstance instance, VkPhysicalDevice physical_device, uint32_t graphics_queue_index)
    {
        VkSurfaceKHR surface = VK_NULL_HANDLE;
#ifdef MIRAI_PLATFORM_WINDOW
        surface = create_win32_surface(instance);
#else
#error "Unsupported Platform"
#endif
        VkBool32 present_support = false;
        VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, graphics_queue_index, surface, &present_support));
        if (!present_support)
        {
            Log::Fatal("Presentation is supported by the device");
        }
        return surface;
    }

    static void create_swapchain(VulkanSwapchain *swapchain, VkDevice device, VkSurfaceKHR surface)
    {
        VkSwapchainCreateInfoKHR createInfo = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = surface,
            .minImageCount = swapchain->image_count,
            .imageFormat = swapchain->surface_format.format,
            .imageColorSpace = swapchain->surface_format.colorSpace,
            .imageExtent = VkExtent2D{swapchain->width, swapchain->height},
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .preTransform = swapchain->current_transform,
            .compositeAlpha = swapchain->composite_mode,
            .presentMode = swapchain->present_mode,
            .oldSwapchain = swapchain->swapchain,
        };

        VkSwapchainKHR vk_swapchain = VK_NULL_HANDLE;
        VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, nullptr, &vk_swapchain));
        swapchain->swapchain = vk_swapchain;
    }

    void CreateSwapchain(VulkanSwapchain *swapchain, VkPhysicalDevice physical_device, VkDevice device, VkSurfaceKHR surface, bool vsync)
    {
        VkSurfaceCapabilitiesKHR surface_caps{};
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_caps));
        if (surface_caps.currentExtent.width == 0 || surface_caps.currentExtent.height == 0)
            return;

        swapchain->width = surface_caps.currentExtent.width;
        swapchain->height = surface_caps.currentExtent.height;
        swapchain->image_count = std::min(std::max(2u, surface_caps.maxImageCount), 4u);

        uint32_t format_count = 0;
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr));
        std::vector<VkSurfaceFormatKHR> surface_formats(format_count);
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, surface_formats.data()));

        swapchain->surface_format.format = VK_FORMAT_B8G8R8A8_UNORM;
        swapchain->surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        bool found = false;
        for (auto &supported : surface_formats)
        {
            if (supported.format == swapchain->surface_format.format && supported.colorSpace == swapchain->surface_format.colorSpace)
            {
                found = true;
                break;
            }
        }

        if (!found)
            Log::Fatal("Couldn't find required surface format for swapchain");

        uint32_t present_mode_count = 0;
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr));
        std::vector<VkPresentModeKHR> present_modes(present_mode_count);
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, present_modes.data()));

        swapchain->present_mode = vsync ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
        found = false;
        for (auto &present_mode : present_modes)
        {
            if (present_mode == swapchain->present_mode)
            {
                found = true;
                break;
            }
        }
        // Fallback to FIFO_KHR if not found
        if (!found)
        {
            swapchain->present_mode = VK_PRESENT_MODE_FIFO_KHR;
            Log::Warn("Fallbacking to FIFO_MODE_KHR");
        }

        swapchain->composite_mode =
            (surface_caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
                ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
            : (surface_caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
                ? VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR
            : (surface_caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
                ? VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR
                : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;

        swapchain->current_transform = surface_caps.currentTransform;
        create_swapchain(swapchain, device, surface);

        uint32_t imageCount = 0;
        VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain->swapchain, &imageCount, nullptr));
        swapchain->images.resize(imageCount);
        swapchain->image_views.resize(imageCount);

        VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain->swapchain, &imageCount, swapchain->images.data()));
        swapchain->image_count = imageCount;

        VkImageViewCreateInfo image_view_create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapchain->surface_format.format,
            .components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A},
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        };

        for (uint32_t i = 0; i < imageCount; ++i)
        {
            image_view_create_info.image = swapchain->images[i];
            VK_CHECK(vkCreateImageView(device, &image_view_create_info, nullptr, &swapchain->image_views[i]));
        }
    }
} // namespace mirai