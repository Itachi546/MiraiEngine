#include <iomanip>
#include <sstream>

namespace mirai {
namespace utils {
    inline uint8_t pack_float_to_u8(float val) {
        return uint8_t(val * 127.0f + 127.5f);
    }

    inline uint32_t pack_vec3_to_u32(float x, float y, float z) {
        return (pack_float_to_u8(x) << 24) |
               (pack_float_to_u8(y) << 16) |
               (pack_float_to_u8(z) << 8);
    }

    template <typename T>
    inline uint64_t mb_to_bytes(T mb) {
        return mb * 1024 * 1024;
    }

    template <typename T>
    inline float bytes_to_mb(T bytes) {
        return float(bytes) / (1024.0f * 1024.0f);
    }

    template <typename T>
    inline uint64_t kb_to_bytes(T kb) {
        return kb * 1024;
    }

    template <typename T>
    inline float bytes_to_kb(T bytes) {
        return float(bytes) / 1024.0f;
    }

    inline std::string precision(float val, int precision) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(precision) << val;
        return ss.str();
    }
}
} // namespace mirai::utils