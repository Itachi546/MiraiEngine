#pragma once

#define USE_STL_UNORDERED_SET 0
#endif

#if USE_STL_UNORDERED_SET == 1
#include <unordered_set>
#else
#include "External/flat_hash_map.hpp"
#endif

namespace mirai {
    template <typename T>
#if USE_STL_UNORDERED_SET == 1
    using hash_set = std::unordered_set<T>;
#else
    using hash_map = ska::flat_hash_set<T>;
#endif
} // namespace mirai