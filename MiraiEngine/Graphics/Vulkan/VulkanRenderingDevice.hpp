#pragma once

#include "Graphics/RenderingDevice.hpp"
#include "Vulkan.hpp"
#include "Shader.hpp"
#include "Common/ResourcePool.hpp"

#include <vector>
#include <memory>

VK_DEFINE_HANDLE(VmaAllocator)
VK_DEFINE_HANDLE(VmaAllocation)

namespace mirai
{
    struct VulkanSwapchain;
    class CommandBuffer;
    struct VulkanPipeline
    {
        VkPipeline pipeline;
        VkPipelineBindPoint bind_point;
        std::vector<VkDescriptorSetLayout> set_layouts;
        VkPipelineLayout pipeline_layout;
    };

    struct VulkanTexture
    {
        uint32_t width, height, depth;
        uint32_t mip_levels, array_layers;

        VkImageAspectFlags image_aspect;
        VkFormat format;
        VkImageType image_type;

        VkImage image;
        VkImageView image_view;
        VmaAllocation allocation;

        VkImageLayout current_layout;
        VkSampler sampler;
    };

    class VulkanRenderingDevice : public RenderingDevice
    {
      public:
        VulkanRenderingDevice(bool enable_validation);

        VkInstance get_vulkan_instance() const { return instance; }
        VkDevice get_vulkan_device() const { return device; }
        VkPhysicalDevice get_physical_device() const { return physical_device; }
        VkSurfaceKHR get_surface() const { return surface; }
        VulkanSwapchain *get_swapchain() { return swapchain.get(); }

        VkQueue get_device_queue(QueueType queue_type) { return device_queues[queue_type]; }
        uint32_t get_queue_family_indices(QueueType queue_type) { return queue_family_indices[queue_type]; }

        ShaderID create_shader(uint32_t *code, uint32_t code_size_in_bytes, const std::string &debug_name = "") override;

        PipelineID create_graphics_pipeline(PipelineDescription *pipeline_description, const std::string &debug_name = "") override;

        TextureID create_texture(TextureDescription *texture_description, const std::string &debug_name);

        void new_frame() override;

        uint32_t get_max_frame_in_flights() { return K_MAX_FRAME_IN_FLIGHTS; }

        CommandBuffer *get_command_buffer(uint32_t thread_id = 0) override;

        void queue_command_buffer(CommandBuffer *command_buffer) override
        {
            queued_command_buffer.push_back(command_buffer);
        }

        void wait();

        void present() override;

        void destroy_shaders(ShaderID *shaders, uint32_t count) override;

        void destroy_pipeline(PipelineID *pipelines, uint32_t count) override;

        void destroy_texture(TextureID *textures, uint32_t count) override;

        VulkanPipeline *access_pipeline(PipelineID pipeline)
        {
            return resource_pool_pipelines.access(pipeline);
        }

        VulkanTexture *access_texture(TextureID texture)
        {
            return resource_pool_textures.access(texture);
        }

        ~VulkanRenderingDevice();

      private:
        void set_debug_marker_object_name(VkObjectType objectType, uint64_t handle, const char *objectName);

        VkSemaphore create_semaphore(const std::string &name);

        VmaAllocator create_allocator();

        VkFence create_fence(const std::string &name, bool signalled = false);

        VkSampler create_sampler(SamplerDescription *desc);

        std::vector<const char *> instance_extensions;
        std::vector<const char *> validation_layers;
        std::vector<const char *> device_extensions;

        ResourcePool<VulkanShader> resource_pool_shaders;
        ResourcePool<VulkanPipeline> resource_pool_pipelines;
        ResourcePool<VulkanTexture> resource_pool_textures;

        static const uint32_t K_NUM_THREAD = 1;
        static const uint32_t K_NUM_COMMAND_BUFFER_PER_THREAD = 3;
        static const uint32_t K_MAX_FRAME_IN_FLIGHTS = 2;
        uint32_t current_frame = 0;
        uint64_t total_memory_usage = 0;
        bool vsync = true;

        VkInstance instance;
        VkDevice device;
        VkPhysicalDevice physical_device;
        VmaAllocator vma_allocator;
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
    };
} // namespace mirai