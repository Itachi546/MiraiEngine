#pragma once

namespace mirai {
    struct Color {
        union {
            struct
            {
                float r, g, b, a;
            };
            float data[4];
        };

        Color() = default;

        Color(float r, float g, float b, float a) : r(r), g(g), b(b), a(a) {
        }

        Color(uint32_t hex) {
            r = float((hex >> 24) & 0xff) / 255.0f;
            g = float((hex >> 16) & 0xff) / 255.0f;
            b = float((hex >> 8) & 0xff) / 255.0f;
            a = float(hex & 0xff) / 255.0f;
        }
    };
} // namespace mirai