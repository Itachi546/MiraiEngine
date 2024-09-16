#pragma once

#include "Common/CommonInclude.hpp"

#ifdef MIRAI_PLATFORM_WINDOW
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#define VULKAN_1_3
#define VK_NO_PROTOTYPES
#include <volk/volk.h>
/*
#define VMA_IMPLEMENTATION
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vma/vk_mem_alloc.h>
*/
#include "Graphics/RenderingDevice.h"
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

} // namespace mirai