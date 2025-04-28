#pragma once

#include <cassert>
#include <cstdint>

#if defined(_WIN32) || defined(_WIN64)
#define MIRAI_PLATFORM_WINDOW
#endif

#define ASSERT(cond) assert(cond)
#define ASSERT_MSG(cond, message) assert(message &&cond)

constexpr const uint32_t K_INVALID_RESOURCE_HANDLE = UINT32_MAX;

#define K_SWAPCHAIN_TEXTURE_HANDLE TextureID(UINT32_MAX - 1)

#define cast_u32(v) (static_cast<uint32_t>(v))
#define cast_float(v) (static_cast<float>(v))
#define cast_int(v) (static_cast<int>(v))

#define align_memory(size, alignment) ((size + alignment - 1) & ~(alignment - 1))
