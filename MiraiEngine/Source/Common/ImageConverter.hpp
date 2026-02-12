#pragma once

#include <string>
#include <vector>

#include "dds.hpp"

namespace mirai {
    void ConvertImage(const std::string &input_image, std::vector<uint8_t *> out_bytes, uint32_t out_width, uint32_t out_height, dds::DXGI_FORMAT format);
} // namespace mirai