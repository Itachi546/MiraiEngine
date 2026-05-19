#pragma once

#include <memory>
#include <vector>

#include "RenderingDevice.hpp"
#include "Scene/Scene.hpp"
#include "Scene/RenderBatch.hpp"
#include "Math/Math.hpp"
#include "GPUResource.hpp"
#include "Math/Frustum.hpp"
namespace mirai {
    class CommandBuffer;
    class ShaderRegistry;
    class TextureCache;
    class FrameGraph;
    class FrameGraphBlackBoard;
    class Renderer;
    class ShadowSystem;
    struct LineRenderer;
    struct RenderContext {
        Renderer *renderer;
        CommandBuffer *command_buffer;
    };

    class Renderer {
      public:
        Renderer();
        Renderer(const Renderer &) = delete;
        Renderer operator=(const Renderer &) = delete;

        static Renderer *get() {
            return Instance;
        }

        Scene *get_scene() {
            return scene.get();
        }

        FrameGraph *get_frame_graph() { return frame_graph.get(); }
        FrameGraphBlackBoard *get_frame_graph_blackboard() {
            return frame_graph_blackboard.get();
        }

        void add_bindless_texture(TextureID texture);

        GPULinearAllocator *get_per_frame_gpu_allocator() {
            ASSERT(frame_flight_index < AppSettings::K_MAX_FRAME_IN_FLIGHTS);
            return &per_frame_allocator[frame_flight_index];
        }

        // Helper function to upload batch data to per frame staging buffer
        void upload_batch_data(std::vector<RenderBatch> &batches, uint32_t current_frame);

        // Used for resources with default descriptor parameter
        DescriptorOffset get_or_create_descriptor(ID resource_id, DescriptorType descriptor_type);

        // For descriptor with custom parameter, we hash the string name and store it
        DescriptorOffset get_or_create_descriptor(const std::string &name, const DescriptorInfo &descriptor_info);

        ~Renderer();

        GPUResourceDescriptorHeap resource_heap;
        GPUSamplerDescriptorHeap sampler_heap;

        DescriptorOffset transform_descriptor;
        DescriptorOffset material_descriptor;
        DescriptorOffset light_descriptor;

        DescriptorOffset per_frame_data_descriptor;
        DescriptorOffset cascade_data_descriptor;

        // Global Geometry Buffer Allocator
        std::unique_ptr<GPUPagedAllocator> geometry_buffer_allocator;

        // Uniform Buffer
        std::unique_ptr<GPULinearAllocator> gpu_allocator;

        BufferView transform_buffer;
        BufferView material_buffer;
        BufferView light_buffer;
        
        BufferID blas_buffer_static;
        BufferID blas_buffer_dynamic;
        BufferID tlas_buffer;

        // Acceleration Structure — single instance, GPU-only, safe to reuse each frame (fence wait guarantees GPU idle)
        AccelerationStructure tlas;
        std::vector<AccelerationStructureID> blas;

        // Per frame Uniform Set
        std::vector<RenderBatch> main_render_batches;

        bool freeze_frustum = false;
        bool show_aabbs = false;
        bool disable_punctual_lights = true;
        FrustumPlanes freezed_frustum_planes;
        glm::mat4 freezed_inv_VP;

        uint32_t total_visible_entities = 0;
        uint32_t total_lights = 0;

        TextureID blue_noise_texture128;
        uint64_t frame_id;

      private:
        static Renderer *Instance;
        std::unique_ptr<Scene> scene;
        std::unique_ptr<RenderingDevice> device;
        std::unique_ptr<TextureCache> texture_cache;
        std::unique_ptr<FrameGraph> frame_graph;
        std::unique_ptr<FrameGraphBlackBoard> frame_graph_blackboard;
        std::unique_ptr<ShaderRegistry> shader_registry;
        std::unique_ptr<ShadowSystem> shadow_system;
        std::unique_ptr<LineRenderer> line_renderer;

        // Skinning
        std::unique_ptr<ComputeShader> skinning_shader;
        std::unique_ptr<ComputeShader> patch_buffer_shader;

        void upload_per_frame_data();

        void initialize();

        void on_load_resources();

        void set_scene(std::unique_ptr<Scene> scene) {
            this->scene = std::move(scene);
        }

        void update();

        void render();

        void create_batches();

        void create_blas();

        void create_tlas(CommandBuffer *command_buffer);

        void update_skinned_mesh(CommandBuffer *command_buffer);

        void upload_transforms(CommandBuffer *command_buffer);
        void upload_lights(CommandBuffer *command_buffer);
        void upload_materials(CommandBuffer *command_buffer);

        void prepare_buffer_for_shader_read(CommandBuffer *command_buffer);

        void initialize_scene_default_meshes(CommandBuffer *command_buffer);

        struct BufferPatch {
            // Buffer in shader are access in the size of 4 bytes, so the offset should be specified in
            // 4 byte
            uint32_t offset;
            // No of element in 4 byte size
            uint32_t count;
        };

        void dispatch_patch_copy(CommandBuffer *command_buffer, const BufferView &patch_buffer, const BufferView &src_buffer, const BufferView &dst_buffer, uint32_t total_patches);

        uint32_t frame_flight_index;
        const uint32_t k_staging_buffer_size_per_frame = 4 * 1024 * 1024;
        uint32_t bindless_texture_count = 0;
        GPULinearAllocator per_frame_allocator[AppSettings::K_MAX_FRAME_IN_FLIGHTS];
        HashMap<uint64_t, DescriptorOffset> descriptor_map;

        friend class Engine;
    };
} // namespace mirai
