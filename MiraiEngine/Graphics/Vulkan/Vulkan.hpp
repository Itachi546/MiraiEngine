#pragma once

#include "Common/CommonInclude.hpp"

#ifdef MIRAI_PLATFORM_WINDOW
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#define VULKAN_1_3
#define VK_NO_PROTOTYPES
#include <volk.h>

#include "Graphics/RenderingDevice.hpp"
#include "Engine/Log.hpp"

namespace mirai
{
    constexpr const uint32_t VULKAN_API_VERSION VK_API_VERSION_1_3;

#define VK_CHECK(x)                                           \
    do                                                        \
    {                                                         \
        VkResult err = x;                                     \
        if (err)                                              \
        {                                                     \
            Log::Error("VulkanError::", std::to_string(err)); \
        }                                                     \
    } while (0)

#define VK_LOAD_FUNCTION(instance, pFuncName) (vkGetInstanceProcAddr(instance, pFuncName))

    constexpr const VkFormat RD_FORMAT_TO_VK_FORMAT[Format::FORMAT_MAX] = {
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_R8G8B8_UNORM,
        VK_FORMAT_R8G8_UNORM,
        VK_FORMAT_R8_UNORM,
        VK_FORMAT_R16_SFLOAT,
        VK_FORMAT_R16G16_SFLOAT,
        VK_FORMAT_R16G16B16_SFLOAT,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_FORMAT_R32G32B32_SFLOAT,
        VK_FORMAT_R32G32_SFLOAT,
        VK_FORMAT_D16_UNORM,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_UNDEFINED,
    };
} // namespace mirai