#pragma once

#include "Math/Math.hpp"

namespace mirai {
    inline float halton(int i, int b) {
        // Creates a halton sequence of values between 0 and 1.
        // https://en.wikipedia.org/wiki/Halton_sequence
        // Used for jittering based on a constant set of 2D points.
        float f = 1.0f;
        float r = 0.0f;
        while (i > 0) {
            f = f / float(b);
            r = r + f * float(i % b);
            i = i / b;
        }
        return r;
    }

    inline glm::vec2 halton23_sequence(int index) {
        return glm::vec2{halton(index, 2), halton(index, 3)};
    }

    inline float randomFloat01() {
        return float(rand()) / float(RAND_MAX);
    }

} // namespace mirai