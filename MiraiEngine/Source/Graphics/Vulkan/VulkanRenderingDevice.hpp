#pragma once

#include "Graphics/RenderingDevice.hpp"
#include "Vulkan.hpp"
#include "Shader.hpp"
#include "Common/ResourcePool.hpp"

#include <vector>
#include <memory>
#include <unordered_map>

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

        ShaderID create_shader(uint32_t *code, uint32_t code_size_in_bytes, const std::string &debug_name = "") override;

        PipelineID create_graphics_pipeline(PipelineDescription *pipeline_description, const std::string &debug_name = "") override;
        PipelineID create_compute_pipeline(ShaderID shader, const std::string &debug_name = "") override;

        UniformSetID create_uniform_set(UniformLayout *uniforms, uint32_t uniform_count, uint32_t set, const std::string &debug_name = "") override;
        void update_uniform_set(UniformSetID uniform_set, UniformBinding *bindings, uint32_t binding_count) override;

        BufferID create_buffer(BufferDescription *buffer_description, const std::string &debug_name) override;
        uint8_t *map_buffer(BufferID buffer) override;
        void unmap_buffer(BufferID buffer) override;

        QueryID create_query(uint32_t query_count) override;
        void query(CommandBuffer *command_buffer, QueryID query, uint32_t query_index) override;
        void resolve_query(QueryID query, uint64_t *resolve_output, uint32_t start, uint32_t count) override;
        void reset_query(CommandBuffer *command_buffer, QueryID query, uint32_t start, uint32_t count) override;

        float get_timestamp_period() override;

        TextureID create_texture(TextureDescription *texture_description, const std::string &debug_name) override;
        SamplerID create_sampler(SamplerDescription *desc);

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

        void destroy_shaders(ShaderID *shaders, uint32_t count) override;

        void destroy_pipelines(PipelineID *pipelines, uint32_t count) override;

        void destroy_buffers(BufferID *buffers, uint32_t count) override;

        void destroy_queries(QueryID *queries, uint32_t count) override;

        void destroy_textures(TextureID *textures, uint32_t count) override;

        void destroy_uniform_sets(UniformSetID *uniform_sets, uint32_t count) override;

        void begin_debug_utils_label(CommandBuffer *command_buffer, const char *name, float *colors);
        void end_debug_utils_label(CommandBuffer *command_buffer);

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
        bool supports_raytracing() override {
            return has_rt_support;
        }

        uint32_t get_current_frame() override {
            return current_frame;
        }

        uint32_t get_swapchain_image_count() override;

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

        std::vector<const char *> requested_instance_extensions;
        std::vector<const char *> requested_validation_layers;
        std::vector<const char *> requested_device_extensions;

        ResourcePool<VulkanShader> resource_pool_shaders;
        ResourcePool<VulkanPipeline> resource_pool_pipelines;
        ResourcePool<VulkanTexture> resource_pool_textures;
        ResourcePool<VulkanBuffer> resource_pool_buffers;
        ResourcePool<VulkanUniformSet> resource_pool_uniform_sets;
        ResourcePool<VulkanQuery> resource_pool_queries;
        std::unordered_map<uint64_t, VkDescriptorSetLayout> descriptor_set_layouts_cache;
        std::unordered_map<uint64_t, VkSampler> sampler_caches;

        static const uint32_t K_NUM_THREAD = 2;
        static const uint32_t K_NUM_COMMAND_BUFFER_PER_THREAD = 3;
        static const uint32_t K_MAX_FRAME_IN_FLIGHTS = 2;
        uint32_t current_frame = 0;

        bool vsync = true;
        bool has_rt_support = false;

        VkPhysicalDeviceProperties2 physical_device_properties;
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