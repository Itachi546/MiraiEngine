#pragma once

#include "Common/CommonInclude.hpp"
#include "Common/Color.hpp"
#include <string>
#include <vector>
#include <optional>

#include <glm/glm.hpp>

namespace mirai
{
    enum class DeviceType
    {
        DEVICE_TYPE_OTHER = 0x0,
        DEVICE_TYPE_INTEGRATED_GPU = 0x1,
        DEVICE_TYPE_DISCRETE_GPU = 0x2,
        DEVICE_TYPE_VIRTUAL_GPU = 0x3,
        DEVICE_TYPE_CPU = 0x4,
        DEVICE_TYPE_MAX = 0x5
    };

    constexpr const uint32_t K_INVALID_QUEUE_ID = UINT32_MAX;
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
}; // namespace mirai