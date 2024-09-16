#pragma once

#include <cassert>
#include <cstdint>

#define ARRAYSIZE(a) (sizeof(a) / sizeof(a[0]))

#define ASSERT(cond) assert(cond)
#define ASSERT_MSG(message) assert(0 && message)

constexpr const uint32_t K_INVALID_RESOURCE_HANDLE = UINT32_MAX;