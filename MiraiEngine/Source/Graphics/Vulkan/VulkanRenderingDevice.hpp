#pragma once

#include "Graphics/RenderingDevice.hpp"
#include "Vulkan.hpp"
#include "VulkanShader.hpp"
#include "Common/ResourcePool.hpp"
#include "Common/HashMap.hpp"

#include <vector>
#include <memory>

VK_DEFINE_HANDLE(VmaAllocator)
VK_DEFINE_HANDLE(VmaAllocation)

namespace mirai {
    struct VulkanSwapchain;
    class CommandBuffer;

    struct VulkanAccelerationStructure {
        BufferID blas_buffer{K_INVALID_ID};
        BufferID tlas_buffer{K_INVALID_ID};
        BufferID tlas_instance_buffer{K_INVALID_ID};
        std::vector<VkAccelerationStructureKHR> blas;
        VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;
    };

    class VulkanRenderingDevice : public RenderingDevice {
      public:
        VulkanRenderingDevice();
        VkInstance get_vulkan_instance() const { return instance; }
        VkDevice get_vulkan_device() const { return device; }
        VkPhysicalDevice get_physical_device() const { return physical_device; }
        VkSurfaceKHR get_surface() const { return surface; }
        VulkanSwapchain *get_swapchain() { return swapchain.get(); }

        VkQueue get_device_queue(QueueType queue_type) { return device_queues[queue_type]; }
        uint32_t get_queue_family_indices(QueueType queue_type) { return queue_family_indices[queue_type]; }

        PipelineID create_graphics_pipeline(PipelineDescription *pipeline_description, const std::string &debug_name = "") override;
        PipelineID create_compute_pipeline(const ShaderProgram &shader_file, const std::string &debug_name = "") override;

        void write_resource_descriptors(const DescriptorInfo *descriptor_infos, uint32_t descriptor_count, void *start_address, uint32_t descriptor_size) override;
        void write_sampler_descriptors(const SamplerDescription *samplers, uint32_t sampler_count, void *start_address) override;

        uint32_t get_resource_descriptor_size() const;
        uint32_t get_sampler_descriptor_size() const;

        uint32_t calculate_resource_descriptors_size(uint32_t descriptor_count) const;
        uint32_t calculate_sampler_descriptors_size(uint32_t descriptor_count) const;

        UniformSetID create_uniform_set_from_descriptor_pool(UniformLayout *uniforms, uint32_t uniform_count, uint32_t set, VkDescriptorPool descriptor_pool, const std::string &debug_name);
        UniformSetID create_uniform_set(UniformLayout *uniforms, uint32_t uniform_count, uint32_t set, const std::string &debug_name = "") override;
        void update_uniform_set(UniformSetID uniform_set, UniformBinding *bindings, uint32_t binding_count) override;

        BufferID create_buffer(BufferDescription *buffer_description, const std::string &debug_name) override;
        void resize_buffer(BufferDescription *buffer_description, BufferID resize_buffer, bool should_copy_data, const std::string &debug_name);
        uint8_t *map_buffer(BufferID buffer) override;
        void unmap_buffer(BufferID buffer) override;

        QueryID create_query(uint32_t query_count) override;
        void query(CommandBuffer *command_buffer, QueryID query, uint32_t query_index) override;
        void resolve_query(QueryID query, uint64_t *resolve_output, uint32_t start, uint32_t count) override;
        void reset_query(CommandBuffer *command_buffer, QueryID query, uint32_t start, uint32_t count) override;

        float get_timestamp_period() const override;

        TextureID create_texture(TextureDescription *texture_description, const std::string &debug_name) override;

        void generate_mipmap(CommandBuffer *command_buffer, TextureID texture_id, PipelineStage src_pipeline_stage) override;

        void add_bindless_texture(BindlessTextureEntry *textures, uint32_t texture_count) override;

        void new_frame() override;

        CommandBuffer *get_command_buffer(uint32_t thread_id = 0) override;

        void queue_command_buffer(CommandBuffer *command_buffer) override {
            queued_command_buffer.push_back(command_buffer);
        }

        void submit_command_buffer_immediate(CommandBuffer *command_buffer) override;

        void wait() override;

        void present() override;

        void destroy_pipelines(PipelineID *pipelines, uint32_t count) override;

        void destroy_buffers(BufferID *buffers, uint32_t count) override;

        void destroy_queries(QueryID *queries, uint32_t count) override;

        void destroy_textures(TextureID *textures, uint32_t count) override;

        void destroy_uniform_sets(UniformSetID *uniform_sets, uint32_t count) override;

        VulkanPipeline *access_pipeline(PipelineID pipeline) {
            return resource_pool_pipelines.access(pipeline);
        }

        VulkanTexture *access_texture(TextureID texture) {
            return resource_pool_textures.access(texture);
        }

        VulkanBuffer *access_buffer(BufferID buffer) {
            return resource_pool_buffers.access(buffer);
        }

        VulkanUniformSet *access_uniform_set(UniformSetID uniform_set) {
            return resource_pool_uniform_sets.access(uniform_set);
        }

        VkDescriptorPool create_descriptor_pool(VkDescriptorPoolCreateFlags create_flags, VkDescriptorPoolSize *pools = nullptr, uint32_t pool_count = 0, uint32_t max_sets = 512);

        // Raytracing utilities
        void create_acceleration_structure(const AccelerationStructureMeshInfo *meshes, uint32_t mesh_count);
        bool supports_raytracing() const override {
            return has_rt_support;
        }

        uint32_t get_current_frame() const override {
            return current_frame;
        }

        uint32_t get_swapchain_image_count() const override;

        ~VulkanRenderingDevice();

        VkInstance instance;
        VkDevice device;
        VkPhysicalDevice physical_device;
        VkSurfaceKHR surface;
        std::vector<uint32_t> queue_family_indices;
        std::vector<VkQueue> device_queues;
        std::unique_ptr<VulkanSwapchain> swapchain;
        std::vector<VkDescriptorPool> descriptor_pools;

      private:
        friend class CommandBuffer;

        void set_debug_marker_object_name(VkObjectType objectType, uint64_t handle, const char *objectName);

        VkSemaphore create_semaphore(const std::string &name);

        VmaAllocator create_allocator();

        void initialize_bindless_descriptor();

        VkFence create_fence(const std::string &name, bool signalled = false);

        void create_acceleration_structure_geometry_info(const AccelerationStructureBufferInfo &vertex_buffer, const AccelerationStructureBufferInfo &index_buffer, VkAccelerationStructureGeometryKHR &geometry);
        void create_blas(const AccelerationStructureMeshInfo *meshes, uint32_t mesh_count, std::vector<VkAccelerationStructureKHR> &out_blas, BufferID &blas_buffer_id, std::vector<VkDeviceSize> &out_blas_compacted_offset, std::vector<VkDeviceSize> &out_blas_compacted_size);
        void create_tlas(BufferID instance_buffer, uint32_t primitive_count, VkAccelerationStructureKHR &tlas, BufferID &tlas_buffer_id);
        VkBuffer create_vk_buffer(BufferDescription *buffer_description, VmaAllocation &allocation, const std::string &debug_name);

        std::vector<const char *> requested_instance_extensions;
        std::vector<const char *> requested_validation_layers;
        std::vector<const char *> requested_device_extensions;

        ResourcePool<VulkanPipeline> resource_pool_pipelines;
        ResourcePool<VulkanTexture> resource_pool_textures;
        ResourcePool<VulkanBuffer> resource_pool_buffers;
        ResourcePool<VulkanUniformSet> resource_pool_uniform_sets;
        ResourcePool<VulkanQuery> resource_pool_queries;
        HashMap<uint64_t, VkDescriptorSetLayout> descriptor_set_layouts_cache;

        uint32_t current_frame = 0;

        bool vsync;
        bool has_rt_support = false;

        VkPhysicalDeviceProperties2 physical_device_properties;

        VkPhysicalDeviceDescriptorHeapPropertiesEXT descriptor_heap_properties;
        uint32_t resource_descriptor_size = 0;

        VmaAllocator vma_allocator;

        // Bindless descriptor set
        VkDescriptorSetLayout bindless_descriptor_layout;
        VkDescriptorSet bindless_descriptor_set;
        VkDescriptorPool bindless_descriptor_pool;

        std::vector<VkCommandPool> command_pools;
        std::vector<std::unique_ptr<CommandBuffer>> command_buffers;
        std::vector<CommandBuffer *> queued_command_buffer;

        std::vector<VkSemaphore> image_acquire_semaphore;
        std::vector<VkSemaphore> render_finished_semaphore;
        std::vector<VkFence> in_flight_fences;

        std::vector<GpuVendorInfo> all_vendor_infos;
        VkDebugReportCallbackEXT debug_report_callback;

        VulkanAccelerationStructure acceleration_structure;
    };
} // namespace mirai