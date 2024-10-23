namespace mirai
{
    namespace utils
    {
        inline uint8_t PackFloatToU8(float val)
        {
            return uint8_t(val * 127.0f + 127.5f);
        }
    } // namespace utils
} // namespace mirai