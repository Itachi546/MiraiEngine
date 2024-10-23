namespace mirai
{
    namespace utils
    {
        inline uint8_t pack_float_to_u8(float val)
        {
            return uint8_t(val * 127.0f + 127.5f);
        }

        inline uint32_t pack_vec3_to_u32(float x, float y, float z)
        {
            return (pack_float_to_u8(x) << 24) |
                   (pack_float_to_u8(y) << 16) |
                   (pack_float_to_u8(z) << 8);
        }
    } // namespace utils
} // namespace mirai