#pragma once

#define USE_STL_UNORDERED_SET 0

#if USE_STL_UNORDERED_SET == 1
#include <unordered_set>
#else
#include "flat_hash_map.hpp"
#endif

namespace mirai {
    template <typename T>
#if USE_STL_UNORDERED_SET == 1
    using HashSet = std::unordered_set<T>;
#else
    using HashSet = ska::flat_hash_set<T>;
#endif
} // namespace mirai