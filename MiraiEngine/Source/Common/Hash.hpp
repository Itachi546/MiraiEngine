#pragma once

#include <functional>

namespace mirai {
namespace utils {
    // Godot String::hash()
    inline uint32_t djb2_hash_string(const std::string &message) {
        /* simple djb2 hashing */
        uint32_t hashv = 5381;
        for (auto c : message) {
            hashv = ((hashv << 3) + hashv) + c;
        }
        return hashv;
    }

    template <typename... Args>
    void hash_combine(std::size_t &seed, const Args &...args) {
        ((seed = seed ^ (std::hash<Args>{}(args) + 0x9e3779b9 + (seed << 6) + (seed >> 2))), ...);
    }
}
} // namespace mirai::utils