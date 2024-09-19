#pragma once

#include "Graphics/RenderingDevice.hpp"
#include "Vulkan.hpp"

#include <vector>
#include <memory>

namespace mirai
{
    struct VulkanSwapchain;
    class CommandBuffer;

    class VulkanRenderingDevice
    {
      public:
        VulkanRenderingDevice();

        void set_enable_validation(bool enable_validation)
        {
            enable_validation = enable_validation;
        }

        static VulkanRenderingDevice *get()
        {
            return Instance;
        }

        bool is_validation_enabled() const
        {
            return enable_validation;
        }

        VkInstance get_vulkan_instance() const { return instance; }
        VkDevice get_vulkan_device() const { return device; }
        VkPhysicalDevice get_physical_device() const { return physical_device; }
        VkSurfaceKHR get_surface() const { return surface; }
        VulkanSwapchain *get_swapchain() { return swapchain.get(); }

        VkQueue get_device_queue(QueueType queue_type) { return device_queues[queue_type]; }
        uint32_t get_queue_family_indices(QueueType queue_type) { return queue_family_indices[queue_type]; }

        VkSemaphore create_semaphore();

        VkFence create_fence(bool signalled = false);

        void new_frame();

        uint32_t get_max_frame_in_flights() { return K_MAX_FRAME_IN_FLIGHTS; }

        CommandBuffer *get_command_buffer(uint32_t thread_id = 0);

        void queue_command_buffer(CommandBuffer *command_buffer)
        {
            queued_command_buffer.push_back(command_buffer);
        }

        void present();

        ~VulkanRenderingDevice();

      private:
        void set_debug_marker_object_name(VkObjectType objectType, uint64_t handle, const char *objectName);

        static VulkanRenderingDevice *Instance;

        std::vector<const char *> instance_extensions;
        std::vector<const char *> validation_layers;
        std::vector<const char *> device_extensions;

        static const uint32_t K_NUM_THREAD = 1;
        static const uint32_t K_NUM_COMMAND_BUFFER_PER_THREAD = 3;
        static const uint32_t K_MAX_FRAME_IN_FLIGHTS = 2;
        uint32_t current_frame = 0;

        VkInstance instance;
        VkDevice device;
        VkPhysicalDevice physical_device;
        VkSurfaceKHR surface;

        std::vector<VkCommandPool> command_pools;
        std::vector<std::unique_ptr<CommandBuffer>> command_buffers;
        std::vector<CommandBuffer *> queued_command_buffer;

        VkSemaphore image_acquire_semaphore[K_MAX_FRAME_IN_FLIGHTS];
        VkSemaphore render_finished_semaphore[K_MAX_FRAME_IN_FLIGHTS];
        VkFence in_flight_fences[K_MAX_FRAME_IN_FLIGHTS];

        std::vector<GpuDevice> gpus;
        std::vector<uint32_t> queue_family_indices;
        std::vector<VkQueue> device_queues;
        std::unique_ptr<VulkanSwapchain> swapchain;

        VkDebugUtilsMessengerEXT debug_utils_messenger;
        bool enable_validation;
    };
} // namespace mirai