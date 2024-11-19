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

    inline uint32_t mb_to_bytes(uint32_t mb) {
        return mb * 1024 * 1024;
    }

    inline float bytes_to_mb(uint32_t bytes) {
        return float(bytes) / (1024.0f * 1024.0f);
    }

    inline uint32_t kb_to_bytes(uint32_t kb) {
        return kb * 1024;
    }

    inline float bytes_to_kb(uint32_t bytes) {
        return float(bytes) / 1024.0f;
    }
}
} // namespace mirai::utils