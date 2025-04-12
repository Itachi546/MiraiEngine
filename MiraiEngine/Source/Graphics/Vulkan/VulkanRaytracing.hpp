#pragma once

#include "Vulkan.hpp"

namespace mirai {

    void CreateBVH_BLAS(VkDevice device, VkBuffer vertex_buffer, uint32_t vertex_buffer_size, uint32_t vertex_stride, VkBuffer index_buffer, uint32_t index_buffer_size);
}