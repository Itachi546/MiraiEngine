#pragma once

#include <algorithm>

namespace mirai {
    struct Color {
        union {
            struct
            {
                float r, g, b, a;
            };
            float data[4];
        };

        Color() {
            r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
        }

        Color(float r, float g, float b, float a) : r(r), g(g), b(b), a(a) {
        }

        Color(uint32_t hex) {
            r = float((hex >> 24) & 0xff) / 255.0f;
            g = float((hex >> 16) & 0xff) / 255.0f;
            b = float((hex >> 8) & 0xff) / 255.0f;
            a = float(hex & 0xff) / 255.0f;
        }
    };
    inline uint32_t to_byte(float v) {
        v = std::clamp(v, 0.0f, 1.0f);
        return static_cast<uint32_t>(v * 255.0f + 0.5f); // round to nearest
    }

    inline uint32_t rgba_to_u32(float *rgba) {
        uint32_t r = to_byte(rgba[0]);
        uint32_t g = to_byte(rgba[1]);
        uint32_t b = to_byte(rgba[2]);
        uint32_t a = to_byte(rgba[3]);

        return (r) | (g << 8) | (b << 16) | (a << 24);
    }
} // namespace mirai