#pragma once

#include "Common/CommonInclude.hpp"
#include "Common/Color.hpp"

#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace mirai {

    constexpr const uint32_t K_INVALID_QUEUE_ID = UINT32_MAX;
    constexpr const uint32_t K_INVALID_ID = UINT32_MAX;
    // Reserved for swapchain
    constexpr const uint32_t K_SWAPCHAIN_TEXTURE_ID = UINT32_MAX - 1;

    struct ID {
        uint32_t id = 0;
        inline ID() { id = K_INVALID_ID; };
        explicit ID(uint32_t _id) : id(_id) {}

        bool is_valid() const { return id != K_INVALID_ID; }

        // size_t operator=(const ID &id) const { return id.id; }
    };
#define DEFINE_ID(m_name)                                                                    \
    struct m_name##ID : public ID {                                                          \
        inline operator bool() const { return id != 0; }                                     \
        inline m_name##ID &operator=(m_name##ID p_other) {                                   \
            id = p_other.id;                                                                 \
            return *this;                                                                    \
        }                                                                                    \
        inline bool operator<(const m_name##ID &p_other) const { return id < p_other.id; }   \
        inline bool operator==(const m_name##ID &p_other) const { return id == p_other.id; } \
        inline bool operator!=(const m_name##ID &p_other) const { return id != p_other.id; } \
        inline m_name##ID(const m_name##ID &p_other) : ID(p_other.id) {}                     \
        inline m_name##ID(ID id) : ID(id) {}                                                 \
        inline explicit m_name##ID(uint32_t p_int) : ID(p_int) {}                            \
        inline m_name##ID() = default;                                                       \
    };

    DEFINE_ID(Pipeline)
    DEFINE_ID(Texture)
    DEFINE_ID(Buffer)
    DEFINE_ID(Query)
    DEFINE_ID(AccelerationStructure)

    enum class RenderMode {
        RENDERMODE_FORWARD,
        RENDERMODE_DEFERRED,
    };

    enum class DeviceType {
        DEVICE_TYPE_OTHER = 0x0,
        DEVICE_TYPE_INTEGRATED_GPU = 0x1,
        DEVICE_TYPE_DISCRETE_GPU = 0x2,
        DEVICE_TYPE_VIRTUAL_GPU = 0x3,
        DEVICE_TYPE_CPU = 0x4,
        DEVICE_TYPE_MAX = 0x5
    };

    enum QueueType {
        QUEUE_TYPE_GRAPHICS = 0,
        QUEUE_TYPE_COMPUTE = 1,
        QUEUE_TYPE_TRANSFER = 2,
    };

    enum class ResourceType {
        Texture = 0,
        Buffer = 1
    };

    /*
    struct AccelerationStructureBufferInfo {
        BufferID buffer;
        uint32_t offset;
        uint32_t count;
        uint32_t stride;
    };
    */
    struct BLASDescription {
        BufferID vertex_buffer;
        BufferID index_buffer;
        uint32_t vertex_offset;
        uint32_t index_offset;
        uint32_t vertex_stride;
        uint32_t vertex_count;
    };
    /*
    struct AccelerationStructureMeshInfo {
        AccelerationStructureBufferInfo vertex_buffer;
        AccelerationStructureBufferInfo index_buffer;
        float transform[3][4];
    };
    */
    struct GpuVendorInfo {
        std::string name;
        uint32_t vendor;
        DeviceType device_type;
    };

    enum Colorspace {
        COLOR_SPACE_SRGB = 0,
        COLOR_SPACE_LINEAR,
        COLOR_SPACE_MAX
    };

    enum Format {
        FORMAT_B8G8R8A8_UNORM = 0,
        FORMAT_R8G8B8A8_UNORM,
        FORMAT_R8G8B8A8_SRGB,
        FORMAT_R8G8B8_UNORM,
        FORMAT_R8G8B8_SRGB,
        FORMAT_R8G8_UNORM,
        FORMAT_R8_UNORM,
        FORMAT_R16_UNORM,
        FORMAT_R16_SFLOAT,
        FORMAT_R32_SFLOAT,
        FORMAT_R16G16_SFLOAT,
        FORMAT_R16G16B16_SFLOAT,
        FORMAT_R16G16B16A16_SFLOAT,
        FORMAT_R32G32B32A32_SFLOAT,
        FORMAT_R32G32B32_SFLOAT,
        FORMAT_R32G32_SFLOAT,
        FORMAT_D16_UNORM,
        FORMAT_D32_SFLOAT,
        FORMAT_D32_SFLOAT_S8_UINT,
        FORMAT_D24_UNORM_S8_UINT,
        FORMAT_BC1_UNORM,
        FORMAT_BC3_UNORM,
        FORMAT_BC5_UNORM,
        FORMAT_BC7_SRGB_BLOCK,
        FORMAT_BC7_UNORM_BLOCK,
        FORMAT_UNDEFINED,
        FORMAT_MAX
    };

    enum Topology {
        TOPOLOGY_POINT_LIST = 0,
        TOPOLOGY_LINE_LIST = 1,
        TOPOLOGY_LINE_STRIP = 2,
        TOPOLOGY_TRIANGLE_LIST = 3,
        TOPOLOGY_TRIANGLE_STRIP = 4,
        TOPOLOGY_TRIANGLE_FAN = 5,
        TOPOLOGY_LINE_LIST_WITH_ADJACENCY = 6,
        TOPOLOGY_LINE_STRIP_WITH_ADJACENCY = 7,
        TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY = 8,
        TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY = 9,
        TOPOLOGY_PATCH_LIST = 10,
        TOPOLOGY_MAX = 11
    };

    enum CullMode {
        CULL_MODE_NONE = 0,
        CULL_MODE_FRONT,
        CULL_MODE_BACK,
        CULL_MODE_FRONT_AND_BACK,
        CULL_MODE_MAX
    };

    enum FrontFace {
        FRONT_FACE_COUNTER_CLOCKWISE = 0,
        FRONT_FACE_CLOCKWISE = 1,
        FRONT_FACE_MAX
    };

    enum PolygonMode {
        POLYGON_MODE_FILL = 0,
        POLYGON_MODE_LINE,
        POLYGON_MODE_POINT,
        POLYGON_MODE_MAX
    };

    struct Viewport {
        float x, y;
        float width, height;
        float min_depth, max_depth;
    };

    struct RasterizationState {
        float line_width;
        CullMode cull_mode;
        PolygonMode polygon_mode;
        FrontFace front_face;
        bool conservative_mode;
        bool enable_depth_clamp;
        bool enable_depth_bias;

        static RasterizationState create() {
            return RasterizationState{
                .line_width = 1.0f,
                .cull_mode = CULL_MODE_BACK,
                .polygon_mode = POLYGON_MODE_FILL,
                .front_face = FRONT_FACE_CLOCKWISE,
                .conservative_mode = false,
                .enable_depth_clamp = false,
                .enable_depth_bias = false,
            };
        }
    };

    enum CompareOp {
        COMPARE_OP_NEVER = 0,
        COMPARE_OP_LESS = 1,
        COMPARE_OP_EQUAL = 2,
        COMPARE_OP_LESS_OR_EQUAL = 3,
        COMPARE_OP_GREATER = 4,
        COMPARE_OP_NOT_EQUAL = 5,
        COMPARE_OP_GREATER_OR_EQUAL = 6,
        COMPARE_OP_ALWAYS = 7,
    };

    struct DepthState {
        bool enable_depth_test;
        bool enable_depth_write;
        float max_depth_bounds, min_depth_bounds;
        CompareOp compare_op;

        static DepthState create() {
            return DepthState{
                .enable_depth_test = false,
                .enable_depth_write = false,
                .max_depth_bounds = 1.0f,
                .min_depth_bounds = 0.0f,
                .compare_op = COMPARE_OP_LESS_OR_EQUAL,
            };
        }
    };

    enum BlendMode {
        BLEND_MODE_MIX = 0,
        BLEND_MODE_ADD,
        BLEND_MODE_SUB,
        BLEND_MODE_MUL,
        BLEND_MODE_PREMULT_ALPHA,
        BLEND_MODE_COUNT
    };

    struct BlendState {
        bool enable;

        static BlendState create() {
            return BlendState{
                .enable = false,
            };
        }
    };

    struct VertexAttributeDescription {
        uint32_t binding;
        uint32_t location;
        Format format;
        uint32_t offset;
    };

    struct VertexBindingDescription {
        uint32_t stride;
        uint32_t binding;

        VertexAttributeDescription *attributes;
        uint32_t attribute_count;
    };

    struct ShaderProgram {
        std::vector<uint8_t> byte_code;
    };

    struct PipelineDescription {
        const std::vector<ShaderProgram> &shader_programs;

        Topology topology = TOPOLOGY_TRIANGLE_LIST;
        RasterizationState *rasterization_state;
        DepthState *depth_state;

        VertexBindingDescription *vertex_description;

        uint32_t color_attachment_count;
        BlendState *blend_state;
        Format *color_attachment_formats;
        Format depth_attachment_format;
    };

    enum TextureType {
        TEXTURE_TYPE_1D = 0,
        TEXTURE_TYPE_2D = 1,
        TEXTURE_TYPE_3D = 2,
        TEXTURE_TYPE_CUBE = 3
    };

    enum SamplerAddressMode {
        SAMPLER_ADDRESS_MODE_REPEAT = 0,
        SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT = 1,
        SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE = 2,
        SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER = 3,
        SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE = 4,
    };

    enum FilterMode {
        FILTER_NEAREST = 0,
        FILTER_LINEAR = 1,
    };

    enum SamplerMipmapMode {
        SAMPLER_MIPMAP_NEAREST = 0,
        SAMPLER_MIPMAP_LINEAR = 1,
    };

    enum FlagAccessFlag : uint64_t {
        ACCESS_FLAG_NONE = 0ULL,
        ACCESS_FLAG_INDIRECT_COMMAND_READ = 0x00000001ULL,
        ACCESS_FLAG_INPUT_ATTACHMENT_READ = 0x00000010ULL,
        ACCESS_FLAG_SHADER_READ = 0x00000020ULL,
        ACCESS_FLAG_SHADER_WRITE = 0x00000040ULL,
        ACCESS_FLAG_COLOR_ATTACHMENT_READ = 0x00000080ULL,
        ACCESS_FLAG_COLOR_ATTACHMENT_WRITE = 0x00000100ULL,
        ACCESS_FLAG_DEPTH_STENCIL_ATTACHMENT_READ = 0x00000200ULL,
        ACCESS_FLAG_DEPTH_STENCIL_ATTACHMENT_WRITE = 0x00000400ULL,
        ACCESS_FLAG_TRANSFER_READ = 0x00000800ULL,
        ACCESS_FLAG_TRANSFER_WRITE = 0x00001000ULL
    };

    enum ImageLayout {
        IMAGE_LAYOUT_UNDEFINED = 0,
        IMAGE_LAYOUT_GENERAL = 1,
        IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL = 2,
        IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL = 3,
        IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL = 4,
        IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL = 5,
        IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL = 6,
        IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL = 7,
        IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL = 1000117000,
        IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL = 1000117001,
        IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL = 1000241000,
        IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL = 1000241001,
        IMAGE_LAYOUT_READ_ONLY_OPTIMAL = 1000314000,
        IMAGE_LAYOUT_ATTACHMENT_OPTIMAL = 1000314001,
        IMAGE_LAYOUT_PRESENT_SRC_KHR = 1000001002,
        IMAGE_LAYOUT_MAX_ENUM = 0x7FFFFFFF
    };

    enum PipelineStage : uint64_t {
        PIPELINE_STAGE_NONE = 0ULL,
        PIPELINE_STAGE_TOP_OF_PIPE_BIT = 0x00000001ULL,
        PIPELINE_STAGE_DRAW_INDIRECT_BIT = 0x00000002ULL,
        PIPELINE_STAGE_VERTEX_INPUT_BIT = 0x00000004ULL,
        PIPELINE_STAGE_VERTEX_SHADER_BIT = 0x00000008ULL,
        PIPELINE_STAGE_GEOMETRY_SHADER_BIT = 0x00000040ULL,
        PIPELINE_STAGE_FRAGMENT_SHADER_BIT = 0x00000080ULL,
        PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT = 0x00000100ULL,
        PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT = 0x00000200ULL,
        PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT = 0x00000400ULL,
        PIPELINE_STAGE_COMPUTE_SHADER_BIT = 0x00000800ULL,
        PIPELINE_STAGE_ALL_TRANSFER_BIT = 0x00001000ULL,
        PIPELINE_STAGE_TRANSFER_BIT = 0x00001000ULL,
        PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT = 0x00002000ULL,
        PIPELINE_STAGE_HOST_BIT = 0x00004000ULL,
        PIPELINE_STAGE_ALL_GRAPHICS_BIT = 0x00008000ULL,
        PIPELINE_STAGE_ALL_COMMANDS_BIT = 0x00010000ULL,
        PIPELINE_STAGE_COPY_BIT = 0x100000000ULL,
        PIPELINE_STAGE_RESOLVE_BIT = 0x200000000ULL,
        PIPELINE_STAGE_BLIT_BIT = 0x400000000ULL,
        PIPELINE_STAGE_CLEAR_BIT = 0x800000000ULL,
        PIPELINE_STAGE_INDEX_INPUT_BIT = 0x1000000000ULL,
        PIPELINE_STAGE_VERTEX_ATTRIBUTE_INPUT_BIT = 0x2000000000ULL,
        PIPELINE_STAGE_PRE_RASTERIZATION_SHADERS_BIT = 0x4000000000ULL,
        PIPELINE_STAGE_MAX
    };

    struct AccessDeclaration {
        uint64_t access_flags = ACCESS_FLAG_NONE;
        uint64_t stage_mask = PIPELINE_STAGE_NONE;
        // Only applicable for texture
        ImageLayout layout = IMAGE_LAYOUT_UNDEFINED;
    };

    struct ResourceAccessDeclaration {
        ID resource;
        ResourceType resource_type;
        const AccessDeclaration *declaration;
    };

    struct SamplerDescription {
        SamplerAddressMode address_mode_u, address_mode_v, address_mode_w;
        FilterMode min_filter, mag_filter;
        SamplerMipmapMode mipmap_mode;
        float lod_bias;
        float max_anisotropy;
        float min_lod;
        float max_lod;
        bool enable_anisotropy;

        static SamplerDescription create() {
            return {
                .address_mode_u = SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .address_mode_v = SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .address_mode_w = SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .min_filter = FILTER_LINEAR,
                .mag_filter = FILTER_LINEAR,
                .mipmap_mode = SAMPLER_MIPMAP_LINEAR,
                .lod_bias = 0,
                .max_anisotropy = 16,
                .min_lod = 0,
                .max_lod = 16,
                .enable_anisotropy = false,
            };
        }
    };

    enum TextureUsageBits {
        TEXTURE_USAGE_TRANSFER_SRC_BIT = (1 << 0),
        TEXTURE_USAGE_TRANSFER_DST_BIT = (1 << 1),
        TEXTURE_USAGE_SAMPLED_BIT = (1 << 2),
        TEXTURE_USAGE_COLOR_ATTACHMENT_BIT = (1 << 3),
        TEXTURE_USAGE_DEPTH_ATTACHMENT_BIT = (1 << 4),
        TEXTURE_USAGE_STENCIL_ATTACHMENT_BIT = (1 << 5),
        TEXTURE_USAGE_INPUT_ATTACHMENT_BIT = (1 << 6),
        TEXTURE_USAGE_STORAGE_BIT = (1 << 7),
    };

    enum BufferUsageBits {
        BUFFER_USAGE_TRANSFER_SRC_BIT = (1 << 0),
        BUFFER_USAGE_TRANSFER_DST_BIT = (1 << 1),
        BUFFER_USAGE_UNIFORM_BUFFER_BIT = (1 << 4),
        BUFFER_USAGE_STORAGE_BUFFER_BIT = (1 << 5),
        BUFFER_USAGE_INDEX_BUFFER_BIT = (1 << 6),
        BUFFER_USAGE_VERTEX_BUFFER_BIT = (1 << 7),
        BUFFER_USAGE_INDIRECT_BUFFER_BIT = (1 << 8),
        BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT = 0x00020000,
        BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT = 0x10000000,
        BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT = 0x00080000,
    };

    enum MemoryAllocationType {
        MEMORY_ALLOCATION_TYPE_CPU,
        MEMORY_ALLOCATION_TYPE_GPU
    };

    struct BufferView {
        BufferID buffer;
        uint32_t offset;
        uint32_t size;
        // ptr is only valid for mapped buffer
        // is already mapped at offset
        uint8_t *ptr;

        bool operator==(const BufferView &other) const {
            return this->buffer == other.buffer && this->size == other.size && this->offset == other.offset;
        }
    };
    struct BufferDescription {
        uint32_t size;
        uint32_t usage_flags;
        MemoryAllocationType allocation_type;
    };

    enum TextureCreationFlag {
        TEXTURE_CREATION_FLAG_IMAGE_VIEW_PER_MIP = 1 << 0,
    };
    struct TextureDescription {
        uint32_t create_flags;
        uint32_t width, height, depth;
        uint32_t mip_levels, array_layers;
        TextureType texture_type;
        Format format;
        uint64_t usage_flags;
    };

    enum AttachmentLoadOp {
        LOAD_OP_LOAD = 0,
        LOAD_OP_CLEAR = 1,
        LOAD_OP_DONT_CARE = 2,
    };

    enum AttachmentStoreOp {
        STORE_OP_STORE = 0,
        STORE_OP_DONT_CARE = 0
    };

    struct AttachmentInfo {
        TextureID texture;
        AttachmentLoadOp load_op;
        AttachmentStoreOp store_op;
        Color clear_color;
    };

    enum ShaderStage {
        SHADER_STAGE_VERTEX = 0x00000001,
        SHADER_STAGE_TESSELLATION_CONTROL = 0x00000002,
        SHADER_STAGE_TESSELLATION_EVALUATION = 0x00000004,
        SHADER_STAGE_GEOMETRY = 0x00000008,
        SHADER_STAGE_FRAGMENT = 0x00000010,
        SHADER_STAGE_COMPUTE = 0x00000020,
        SHADER_STAGE_ALL_GRAPHICS = 0x0000001F,
        SHADER_STAGE_ALL = 0x7FFFFFFF,
    };

    enum DescriptorType {
        SampledImage,
        StorageImage,
        UniformBuffer,
        StorageBuffer
    };

    struct DescriptorInfo {
        DescriptorType type;
        ID resource;

        union {
            struct {
                // Only used for buffer
                size_t offset;
                // If the size specified is greater than the actual size of buffer, the sized is clamped to buffer size
                // We can specify the whole region of buffer by using large value like UINT64_MAX
                size_t size;
            } buffer_info;

            struct {
                uint32_t base_mip_level;
                uint32_t mip_level_count;

                uint32_t array_layer;
                uint32_t array_layer_count;
            } image_info;
        };
    };

    using DescriptorOffset = uint32_t;

    struct BufferCopyRegion {
        uint64_t src_offset;
        uint64_t dst_offset;
        uint64_t size;
    };

    struct ImageCopyRegion {
        uint32_t buffer_offset;
        uint32_t width;
        uint32_t height;
        uint32_t mip_level;
        uint32_t array_layer;
    };

    struct DrawIndexedIndirectCommand {
        uint32_t index_count;
        uint32_t instance_count;
        uint32_t first_index;
        uint32_t vertex_offset_bytes;
        uint32_t first_instance;

        DrawIndexedIndirectCommand(uint32_t index_count, uint32_t instance_count, uint32_t first_index, uint32_t vertex_offset, uint32_t first_instance) : index_count(index_count),
                                                                                                                                                           instance_count(instance_count),
                                                                                                                                                           first_index(first_index),
                                                                                                                                                           vertex_offset_bytes(vertex_offset),
                                                                                                                                                           first_instance(first_instance) {
        }
    };

    class CommandBuffer;
    class RenderingDevice {
      public:
        RenderingDevice() {
            Instance = this;
        }

        RenderingDevice(const RenderingDevice &) = delete;
        void operator=(const RenderingDevice &) = delete;

        static RenderingDevice *get() {
            return Instance;
        }

        virtual void new_frame() = 0;

        virtual void present() = 0;

        virtual PipelineID create_graphics_pipeline(const PipelineDescription *pipeline_description, const std::string &debug_name = "") = 0;

        virtual PipelineID create_compute_pipeline(const ShaderProgram &shader, const std::string &debug_name) = 0;

        virtual void write_resource_descriptors(const DescriptorInfo *descriptor_infos, uint32_t descriptor_count, void *start_address, uint32_t descriptor_size) = 0;
        virtual void write_sampler_descriptors(const SamplerDescription *samplers, uint32_t sampler_count, void *start_address) = 0;

        virtual uint32_t get_resource_descriptor_size() const = 0;
        virtual uint32_t get_sampler_descriptor_size() const = 0;

        virtual uint32_t calculate_resource_descriptors_size(uint32_t descriptor_count) const = 0;
        virtual uint32_t calculate_sampler_descriptors_size(uint32_t descriptor_count) const = 0;

        virtual BufferID create_buffer(const BufferDescription *buffer_description, const std::string &debug_name) = 0;
        virtual void resize_buffer(const BufferDescription *buffer_description, BufferID resize_buffer, bool should_copy_data, const std::string &debug_name) = 0;
        virtual uint8_t *map_buffer(BufferID buffer) = 0;
        virtual void unmap_buffer(BufferID buffer) = 0;

        virtual QueryID create_query(uint32_t query_count) = 0;
        virtual void query(CommandBuffer *command_buffer, QueryID query, uint32_t query_index) = 0;
        virtual void resolve_query(QueryID query, uint64_t *resolve_output, uint32_t start, uint32_t count) = 0;
        virtual void reset_query(CommandBuffer *command_buffer, QueryID query, uint32_t start, uint32_t count) = 0;

        virtual float get_timestamp_period() const = 0;

        virtual TextureID create_texture(const TextureDescription *texture_description, const std::string &debug_name) = 0;
        virtual void generate_mipmap(CommandBuffer *command_buffer, TextureID texture_id, PipelineStage src_pipeline_stage) = 0;

        virtual CommandBuffer *get_command_buffer(uint32_t thread_id = 0) = 0;

        virtual void queue_command_buffer(CommandBuffer *command_buffer) = 0;

        virtual void submit_command_buffer_immediate(CommandBuffer *command_buffer) = 0;

        virtual void wait() = 0;

        virtual void destroy_pipelines(PipelineID *pipelines, uint32_t count) = 0;
        virtual void destroy_buffers(BufferID *buffers, uint32_t count) = 0;
        virtual void destroy_queries(QueryID *queries, uint32_t count) = 0;
        virtual void destroy_textures(TextureID *textures, uint32_t count) = 0;

        uint64_t get_memory_usage() const {
            return total_memory_usage;
        }

        virtual bool supports_raytracing() const {
            return false;
        }

        // Raytracing stuff
        virtual void create_blas(const std::vector<BLASDescription> &blas_descriptions, std::vector<AccelerationStructureID *> &blases, BufferID &out_buffer) = 0;
        virtual void destroy_acceleration_structures(AccelerationStructureID *acceleration_structures, uint32_t acceleration_structure_count) = 0;

        virtual uint32_t get_current_frame() const = 0;
        virtual uint32_t get_swapchain_image_count() const = 0;

        virtual ~RenderingDevice() = default;

      protected:
        uint64_t total_memory_usage = 0;
        static RenderingDevice *Instance;
    };

    namespace rendering_utils {
        void upload_default_samplers(RenderingDevice *device, void *ptr);
        void copy_texture_immediate(TextureID dst, void *data, uint32_t size);
        inline uint32_t get_workgroup_size(uint32_t work_size, uint32_t local_workgroup_size) {
            return (work_size + local_workgroup_size - 1) / local_workgroup_size;
        }
        TextureID load_texture2d_from_path(const std::string &path);
    } // namespace rendering_utils
}; // namespace mirai