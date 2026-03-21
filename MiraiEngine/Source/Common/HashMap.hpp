#pragma once
#define USE_STL_UNORDERED_MAP 0

#if USE_STL_UNORDERED_MAP == 1
#include <unordered_map>
#else
#include <stdexcept>
#include "flat_hash_map.hpp"
#endif

namespace mirai {
    template <typename K, typename V, typename H = std::hash<K>, typename E = std::equal_to<K>, typename A = std::allocator<std::pair<const K, V>>>
#if USE_STL_UNORDERED_MAP == 1
    using HashMap = std::unordered_map<K, V, H, E, A>;
#else
    using HashMap = ska::flat_hash_map<K, V, H, E, A>;
#endif
} // namespace mirai