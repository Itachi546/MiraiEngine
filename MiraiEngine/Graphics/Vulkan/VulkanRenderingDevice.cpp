#include "VulkanRenderingDevice.hpp"

#include "Instance.hpp"
#include "Device.hpp"
#include "Swapchain.h"
#include "CommandBuffer.hpp"
#include "VulkanUtils.hpp"

namespace mirai
{

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

    VulkanRenderingDevice::VulkanRenderingDevice() : resource_pool_pipelines(128, "Pipeline"),
                                                     resource_pool_shaders(32, "Shader")
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

        uint32_t graphics_queue = queue_family_indices[QUEUE_TYPE_GRAPHICS];
        if (!PhysicalDeviceSupportPresentation(instance, physical_device, graphics_queue))
            Log::Fatal("Selected Physical Device Doesn't Support Presentation!!!");

        device = CreateDevice(instance, physical_device, queue_family_indices, device_extensions);

        device_queues.resize(queue_family_indices.size());
        vkGetDeviceQueue(device, graphics_queue, 0, &device_queues[QUEUE_TYPE_GRAPHICS]);

        uint32_t transfer_queue = queue_family_indices[QUEUE_TYPE_TRANSFER];
        if (transfer_queue != K_INVALID_QUEUE_ID)
            vkGetDeviceQueue(device, transfer_queue, 0, &device_queues[QUEUE_TYPE_TRANSFER]);

        uint32_t compute_queue = queue_family_indices[QUEUE_TYPE_COMPUTE];
        if (compute_queue != K_INVALID_QUEUE_ID)
            vkGetDeviceQueue(device, compute_queue, 0, &device_queues[QUEUE_TYPE_COMPUTE]);

        surface = CreateSurface(instance, physical_device, graphics_queue);

        swapchain = std::make_unique<VulkanSwapchain>();
        swapchain->swapchain = VK_NULL_HANDLE;
        CreateSwapchain(swapchain.get(), physical_device, device, surface, vsync);
        for (uint32_t i = 0; i < swapchain->image_count; ++i)
        {
            std::string image_name = "swapchain_image_" + std::to_string(i);
            set_debug_marker_object_name(VK_OBJECT_TYPE_IMAGE, (uint64_t)swapchain->images[i], image_name.c_str());

            std::string image_view_name = "swapchain_image_view" + std::to_string(i);
            set_debug_marker_object_name(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)swapchain->image_views[i], image_view_name.c_str());
        }

        // Initialize CommandPool and CommandBuffer
        uint32_t pool_counts = K_MAX_FRAME_IN_FLIGHTS * K_NUM_THREAD;

        VkCommandPoolCreateInfo command_pool_create_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = graphics_queue,
        };
        command_pools.resize(pool_counts);

        for (uint32_t i = 0; i < pool_counts; ++i)
            VK_CHECK(vkCreateCommandPool(device, &command_pool_create_info, nullptr, &command_pools[i]));

        uint32_t buffer_counts = pool_counts * K_NUM_COMMAND_BUFFER_PER_THREAD;

        VkCommandBufferAllocateInfo command_buffer_allocate_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        for (uint32_t i = 0; i < buffer_counts; ++i)
        {
            uint32_t command_pool_index = i / K_NUM_COMMAND_BUFFER_PER_THREAD;
            VkCommandPool command_pool = command_pools[command_pool_index];

            command_buffer_allocate_info.commandPool = command_pool;

            auto &command_buffer = command_buffers.emplace_back(std::make_unique<CommandBuffer>());
            VK_CHECK(vkAllocateCommandBuffers(device, &command_buffer_allocate_info, &command_buffer->command_buffer));
        }

        for (uint32_t i = 0; i < K_MAX_FRAME_IN_FLIGHTS; ++i)
        {
            image_acquire_semaphore[i] = create_semaphore();
            render_finished_semaphore[i] = create_semaphore();
            in_flight_fences[i] = create_fence(true);
        }
    }

    VkSemaphore VulkanRenderingDevice::create_semaphore()
    {
        VkSemaphoreCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };

        VkSemaphore semaphore = VK_NULL_HANDLE;
        VK_CHECK(vkCreateSemaphore(device, &create_info, nullptr, &semaphore));
        return semaphore;
    }

    VkFence VulkanRenderingDevice::create_fence(bool signalled)
    {
        VkFenceCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = signalled ? VK_FENCE_CREATE_SIGNALED_BIT : VkFenceCreateFlags{0},
        };
        VkFence fence = VK_NULL_HANDLE;
        VK_CHECK(vkCreateFence(device, &create_info, nullptr, &fence));
        return fence;
    }

    ShaderID VulkanRenderingDevice::create_shader(uint32_t *code, uint32_t code_size_in_bytes)
    {
        uint32_t shader_id = resource_pool_shaders.obtain();
        VulkanShader *shader = resource_pool_shaders.access(shader_id);
        CreateShader(shader, device, code, code_size_in_bytes);
        return ShaderID{shader_id};
    }

    void VulkanRenderingDevice::new_frame()
    {
        vkWaitForFences(device, 1, &in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &in_flight_fences[current_frame]);

        // Reset command pool
        uint32_t command_pool_begin = current_frame * K_NUM_THREAD;
        for (uint32_t i = command_pool_begin; i < K_NUM_THREAD; ++i)
            vkResetCommandPool(device, command_pools[i], 0);

        VkSurfaceCapabilitiesKHR surface_caps = {};
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_caps));
        uint32_t width = swapchain->width;
        uint32_t height = swapchain->height;

        bool resized = (width != surface_caps.currentExtent.width) || (height != surface_caps.currentExtent.height);
        if (resized)
        {
            swapchain->width = surface_caps.currentExtent.width;
            swapchain->height = surface_caps.currentExtent.height;
            ResizeSwapchain(swapchain.get(), physical_device, device, surface, vsync);
            // @TODO Handle swapchain resize
        }

        uint32_t &current_image_index = swapchain->current_image_index;
        VK_CHECK(vkAcquireNextImageKHR(device, swapchain->swapchain, UINT64_MAX, image_acquire_semaphore[current_frame], VK_NULL_HANDLE, &current_image_index));
    }

    CommandBuffer *VulkanRenderingDevice::get_command_buffer(uint32_t thread_id)
    {
        ASSERT_MSG(thread_id < K_NUM_THREAD, "ThreadID exceed the number of threads");
        uint32_t index = current_frame * K_NUM_THREAD * K_NUM_COMMAND_BUFFER_PER_THREAD + thread_id;
        return command_buffers[index].get();
    }

    void VulkanRenderingDevice::present()
    {
        std::vector<VkCommandBufferSubmitInfo> command_buffer_submit_infos(queued_command_buffer.size());

        VkImageLayout current_layout = swapchain->get_current_image_layout();
        if (current_layout != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
        {

            VkImageMemoryBarrier present_barrier = CreateImageMemoryBarrier(swapchain->get_current_image(),
                                                                            VK_IMAGE_ASPECT_COLOR_BIT,
                                                                            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                                                            0,
                                                                            current_layout,
                                                                            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
            vkCmdPipelineBarrier(queued_command_buffer[0]->command_buffer,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                 VK_DEPENDENCY_BY_REGION_BIT,
                                 0, nullptr,
                                 0, nullptr,
                                 1, &present_barrier);
            swapchain->set_current_image_layout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        }

        for (uint32_t i = 0; i < queued_command_buffer.size(); ++i)
        {
            VK_CHECK(vkEndCommandBuffer(queued_command_buffer[i]->command_buffer));
            command_buffer_submit_infos[i].sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
            command_buffer_submit_infos[i].commandBuffer = queued_command_buffer[i]->command_buffer;
        }

        VkSemaphoreSubmitInfo semaphore_wait_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = image_acquire_semaphore[current_frame],
            .stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        };

        VkSemaphoreSubmitInfo semaphore_signal_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = render_finished_semaphore[current_frame],
            .stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        };

        VkSubmitInfo2 submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .waitSemaphoreInfoCount = 1,
            .pWaitSemaphoreInfos = &semaphore_wait_info,
            .commandBufferInfoCount = static_cast<uint32_t>(command_buffer_submit_infos.size()),
            .pCommandBufferInfos = command_buffer_submit_infos.data(),
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos = &semaphore_signal_info,
        };

        VK_CHECK(vkQueueSubmit2(device_queues[QUEUE_TYPE_GRAPHICS], 1, &submit_info, in_flight_fences[current_frame]));

        queued_command_buffer.clear();

        VkPresentInfoKHR present_info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &render_finished_semaphore[current_frame],
            .swapchainCount = 1,
            .pSwapchains = &swapchain->swapchain,
            .pImageIndices = &swapchain->current_image_index,
        };

        VK_CHECK(vkQueuePresentKHR(device_queues[QUEUE_TYPE_GRAPHICS], &present_info));
        current_frame = (current_frame + 1) % K_MAX_FRAME_IN_FLIGHTS;
    }

    void VulkanRenderingDevice::destroy_shaders(ShaderID *shader_id, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            VulkanShader *shader = resource_pool_shaders.access(shader_id[i]);
            DestroyShader(shader, device);
            resource_pool_shaders.release(shader_id[i]);
        }
    }

    VulkanRenderingDevice::~VulkanRenderingDevice()
    {
        VK_CHECK(vkWaitForFences(device, K_MAX_FRAME_IN_FLIGHTS, in_flight_fences, VK_TRUE, UINT64_MAX));

        for (auto &fence : in_flight_fences)
            vkDestroyFence(device, fence, nullptr);

        for (auto &command_pool : command_pools)
            vkDestroyCommandPool(device, command_pool, nullptr);

        for (auto &semaphore : image_acquire_semaphore)
            vkDestroySemaphore(device, semaphore, nullptr);
        for (auto &semaphore : render_finished_semaphore)
            vkDestroySemaphore(device, semaphore, nullptr);
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
