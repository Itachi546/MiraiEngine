#pragma once

#include "Graphics/RenderingDevice.hpp"
#include "Vulkan.hpp"
#include "VulkanShader.hpp"
#include "Common/ResourcePool.hpp"
#include "Common/HashMap.hpp"

#include <deque>
#include <vector>
#include <memory>

VK_DEFINE_HANDLE(VmaAllocator)
VK_DEFINE_HANDLE(VmaAllocation)

namespace mirai {
    struct VulkanSwapchain;
    class CommandBuffer;
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

        PipelineID create_graphics_pipeline(const PipelineDescription *pipeline_description, const std::string &debug_name = "") override;
        PipelineID create_compute_pipeline(const ShaderProgram &shader_file, const std::string &debug_name = "") override;

        void write_resource_descriptors(const DescriptorInfo *descriptor_infos, uint32_t descriptor_count, void *start_address, uint32_t descriptor_size) override;
        void write_sampler_descriptors(const SamplerDescription *samplers, uint32_t sampler_count, void *start_address) override;

        uint32_t get_resource_descriptor_size() const override;
        uint32_t get_sampler_descriptor_size() const override;

        uint32_t calculate_resource_descriptors_size(uint32_t descriptor_count) const override;
        uint32_t calculate_sampler_descriptors_size(uint32_t descriptor_count) const override;

        BufferID create_buffer(const BufferDescription *buffer_description, const std::string &debug_name) override;
        void resize_buffer(const BufferDescription *buffer_description, BufferID resize_buffer, bool should_copy_data, const std::string &debug_name) override;
        uint8_t *map_buffer(BufferID buffer) override;
        void unmap_buffer(BufferID buffer) override;
        uint64_t get_buffer_device_address(BufferID buffer) override;

        QueryID create_query(uint32_t query_count) override;
        void query(CommandBuffer *command_buffer, QueryID query, uint32_t query_index) override;
        void resolve_query(QueryID query, uint64_t *resolve_output, uint32_t start, uint32_t count) override;
        void reset_query(CommandBuffer *command_buffer, QueryID query, uint32_t start, uint32_t count) override;

        float get_timestamp_period() const override;

        TextureID create_texture(const TextureDescription *texture_description, const std::string &debug_name) override;

        void generate_mipmap(CommandBuffer *command_buffer, TextureID texture_id, PipelineStage src_pipeline_stage) override;

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

        VulkanPipeline *access_pipeline(PipelineID pipeline) {
            return resource_pool_pipelines.access(pipeline);
        }

        VulkanTexture *access_texture(TextureID texture) {
            return resource_pool_textures.access(texture);
        }

        VulkanBuffer *access_buffer(BufferID buffer) {
            return resource_pool_buffers.access(buffer);
        }

        // Raytracing utilities
        void refit_blas(CommandBuffer *command_buffer, const std::vector<BLASDescription> &blas_descriptions, const std::vector<AccelerationStructureID> &blases, BufferID blas_buffer, uint32_t flags) override;
        // @NOTE when creating new blas, when mesh is added to scene (in editor), we should wait for device, destroy the blas buffer, blas and create new one
        // since it takes alot of memory
        void create_blas(const std::vector<BLASDescription> &blas_descriptions, std::vector<AccelerationStructure *> &blases, BufferID &out_buffer, uint32_t creation_flag) override;
        void create_tlas(CommandBuffer *command_buffer, uint32_t primitive_count, BufferView instance_buffer_view, BufferID &tlas_buffer_id, AccelerationStructure *out_tlas);

        void destroy_acceleration_structures(AccelerationStructureID *acceleration_structures, uint32_t acceleration_structure_count) override;

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

      private:
        friend class CommandBuffer;

        void set_debug_marker_object_name(VkObjectType objectType, uint64_t handle, const char *objectName);

        VkSemaphore create_semaphore(const std::string &name);

        VmaAllocator create_allocator();

        struct BlasTempInfo {
            VkAccelerationStructureGeometryKHR geometry;
            VkAccelerationStructureBuildRangeInfoKHR build_ranges;
            VkDeviceSize blas_offset;
            VkDeviceSize blas_size;
            VkDeviceSize scratch_size;
        };
        void create_blas_internal(VulkanBuffer *scratch_buffer, VulkanBuffer *blas_buffer, const std::vector<BlasTempInfo> &blas_temp_infos,
                                  std::vector<VkAccelerationStructureBuildGeometryInfoKHR> &build_infos,
                                  std::vector<VkAccelerationStructureKHR> &blases,
                                  bool should_compact = true, uint64_t *out_compacted_size_ptr = nullptr);

        VkFence create_fence(const std::string &name, bool signalled = false);
        void create_set_and_binding_mappings(const VulkanShader &shader, uint32_t push_constants_size, std::vector<VkDescriptorSetAndBindingMappingEXT> &mappings);
        void create_acceleration_structure_geometry_info(const BLASDescription &blas_desc, VkAccelerationStructureGeometryKHR &geometry);
        VkBuffer create_vk_buffer(const BufferDescription *buffer_description, VmaAllocation &allocation, const std::string &debug_name);
        void destroy_resources(bool force = false);

        std::vector<const char *> requested_instance_extensions;
        std::vector<const char *> requested_validation_layers;
        std::vector<const char *> requested_device_extensions;

        ResourcePool<VulkanPipeline> resource_pool_pipelines;
        ResourcePool<VulkanTexture> resource_pool_textures;
        ResourcePool<VulkanBuffer> resource_pool_buffers;
        ResourcePool<VulkanQuery> resource_pool_queries;
        ResourcePool<VkAccelerationStructureKHR> resource_pool_acceleration_structures;

        uint32_t current_frame = 0;
        uint64_t frame_count = 0;
        bool vsync;
        bool has_rt_support = false;

        VkPhysicalDeviceProperties2 physical_device_properties;
        VkPhysicalDeviceAccelerationStructurePropertiesKHR acceleration_structure_properties;
        VkPhysicalDeviceDescriptorHeapPropertiesEXT descriptor_heap_properties;

        uint32_t resource_descriptor_size = 0;

        VmaAllocator vma_allocator;

        std::vector<VkCommandPool> command_pools;
        std::vector<std::unique_ptr<CommandBuffer>> command_buffers;
        std::vector<CommandBuffer *> queued_command_buffer;

        std::vector<VkSemaphore> image_acquire_semaphore;
        std::vector<VkSemaphore> render_finished_semaphore;
        std::vector<VkFence> in_flight_fences;

        std::vector<GpuVendorInfo> all_vendor_infos;
        VkDebugReportCallbackEXT debug_report_callback;

        // Acceleration Structure buffers
        BufferID tlas_scratch_buffer{K_INVALID_ID};
        BufferID blas_scratch_buffer{K_INVALID_ID};

        // Keep track of destroyed resources
        std::deque<std::pair<ID, uint64_t>> destroyed_buffers;
        std::deque<std::pair<ID, uint64_t>> destroyed_textures;
        std::deque<std::pair<ID, uint64_t>> destroyed_pipelines;
        std::deque<std::pair<ID, uint64_t>> destroyed_queries;
    };
} // namespace mirai