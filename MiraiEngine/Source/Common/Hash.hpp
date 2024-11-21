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

    inline void string_hash_to_colors(const std::string &message, float *out_color) {
        uint32_t hash = djb2_hash_string(message);
        out_color[0] = float((hash >> 24) & 0xff) / 255.0f;
        out_color[1] = float((hash >> 16) & 0xff) / 255.0f;
        out_color[2] = float((hash >> 8) & 0xff) / 255.0f;
    }

    template <typename... Args>
    void hash_combine(std::size_t &seed, const Args &...args) {
        ((seed = seed ^ (std::hash<Args>{}(args) + 0x9e3779b9 + (seed << 6) + (seed >> 2))), ...);
    }
}
} // namespace mirai::utils