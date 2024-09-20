#pragma once

#include "Common/CommonInclude.hpp"
#include "Common/Color.hpp"
#include <glm/glm.hpp>

#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace mirai
{

    constexpr const uint32_t K_INVALID_QUEUE_ID = UINT32_MAX;
    constexpr const uint32_t K_INVALID_ID = UINT32_MAX;

    struct ID
    {
        uint32_t id = 0;
        inline ID() = default;
        ID(uint32_t _id) : id(_id) {}

        bool is_valid() const { return id != K_INVALID_ID; }

        size_t operator=(const ID &id) const { return id.id; }
    };
#define DEFINE_ID(m_name)                                                                    \
    struct m_name##ID : public ID                                                            \
    {                                                                                        \
        inline operator bool() const { return id != 0; }                                     \
        inline m_name##ID &operator=(m_name##ID p_other)                                     \
        {                                                                                    \
            id = p_other.id;                                                                 \
            return *this;                                                                    \
        }                                                                                    \
        inline bool operator<(const m_name##ID &p_other) const { return id < p_other.id; }   \
        inline bool operator==(const m_name##ID &p_other) const { return id == p_other.id; } \
        inline bool operator!=(const m_name##ID &p_other) const { return id != p_other.id; } \
        inline m_name##ID(const m_name##ID &p_other) : ID(p_other.id) {}                     \
        inline explicit m_name##ID(uint32_t p_int) : ID(p_int) {}                            \
        inline m_name##ID() = default;                                                       \
    };

    DEFINE_ID(Pipeline)
    DEFINE_ID(Shader)

    enum class DeviceType
    {
        DEVICE_TYPE_OTHER = 0x0,
        DEVICE_TYPE_INTEGRATED_GPU = 0x1,
        DEVICE_TYPE_DISCRETE_GPU = 0x2,
        DEVICE_TYPE_VIRTUAL_GPU = 0x3,
        DEVICE_TYPE_CPU = 0x4,
        DEVICE_TYPE_MAX = 0x5
    };

    enum QueueType
    {
        QUEUE_TYPE_GRAPHICS = 0,
        QUEUE_TYPE_COMPUTE = 1,
        QUEUE_TYPE_TRANSFER = 2,
    };

    struct GpuDevice
    {
        std::string name;
        uint32_t vendor;
        DeviceType device_type;
    };

    enum Format
    {
        FORMAT_B8G8R8A8_UNORM = 0,
        FORMAT_R8G8B8A8_UNORM,
        FORMAT_R8G8B8A8_SRGB,
        FORMAT_R8G8B8_UNORM,
        FORMAT_R8G8_UNORM,
        FORMAT_R8_UNORM,
        FORMAT_R16_SFLOAT,
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
        FORMAT_UNDEFINED,
        FORMAT_MAX
    };

    enum AttachmentType
    {
        ATTACHMENT_TYPE_IMAGE,
        ATTACHMENT_TYPE_DEPTH,
        ATTACHMENT_TYPE_SWAPCHAIN
    };

    struct Attachment
    {
        uint32_t binding;
        std::string name;
        AttachmentType type;
        Format format;
        Color clear_color;
    };

    struct RenderPass
    {
        std::vector<Attachment> color_attachments;
        std::optional<Attachment> depth_attachments;
        uint32_t width, height;
    };

    struct PipelineDescription
    {
    };

    class CommandBuffer;

    class RenderingDevice
    {
      public:
        RenderingDevice()
        {
            Instance = this;
        }

        static RenderingDevice *get()
        {
            return Instance;
        }

        void set_validation(bool validation)
        {
            this->enable_validation = validation;
        }

        bool is_validation_enabled()
        {
            return this->enable_validation;
        }

        virtual void new_frame() = 0;

        virtual void present() = 0;

        virtual ShaderID create_shader(uint32_t *code, uint32_t code_size_in_bytes) = 0;

        virtual CommandBuffer *get_command_buffer(uint32_t thread_id = 0) = 0;

        virtual void queue_command_buffer(CommandBuffer *command_buffer) = 0;

        virtual void destroy_shaders(ShaderID *shader, uint32_t count) = 0;

        virtual ~RenderingDevice() = default;

      protected:
        bool enable_validation;

        static RenderingDevice *Instance;
    };

    namespace rendering_utils
    {
        ShaderID create_shader_module_from_file(const std::string &filename);
    }
}; // namespace mirai