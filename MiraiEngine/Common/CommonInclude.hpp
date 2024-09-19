#pragma once

#include <cassert>
#include <cstdint>

#if defined(_WIN32) || defined(_WIN64)
#define MIRAI_PLATFORM_WINDOW
#endif

#define ASSERT(cond) assert(cond)
#define ASSERT_MSG(cond, message) assert(cond &&message)

constexpr const uint32_t K_INVALID_RESOURCE_HANDLE = UINT32_MAX;
