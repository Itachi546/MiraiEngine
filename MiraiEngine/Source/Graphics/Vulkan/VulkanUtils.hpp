#pragma once

#include "Vulkan.hpp"
#include <vector>

namespace mirai {
    struct SamplerDescription;
    VkImageMemoryBarrier CreateImageMemoryBarrier(VkImage image,
                                                  VkImageAspectFlags aspect,
                                                  VkAccessFlags src_access_mask,
                                                  VkAccessFlags dst_access_mask,
                                                  VkImageLayout old_layout,
                                                  VkImageLayout new_layout,
                                                  uint32_t src_queue_family = VK_QUEUE_FAMILY_IGNORED,
                                                  uint32_t dst_queue_family = VK_QUEUE_FAMILY_IGNORED,
                                                  uint32_t base_mip_level = 0,
                                                  uint32_t base_array_level = 0,
                                                  uint32_t level_count = VK_REMAINING_MIP_LEVELS,
                                                  uint32_t layer_count = VK_REMAINING_ARRAY_LAYERS);

    VkImageMemoryBarrier2 CreateImageMemoryBarrier2(VkImage image,
                                                    VkPipelineStageFlags2 src_stage_mask,
                                                    VkAccessFlags2 src_access_mask,
                                                    VkPipelineStageFlags2 dst_stage_mask,
                                                    VkAccessFlags2 dst_access_mask,
                                                    VkImageLayout old_layout,
                                                    VkImageLayout new_layout,
                                                    VkImageAspectFlags aspect,
                                                    uint32_t src_queue_family = VK_QUEUE_FAMILY_IGNORED,
                                                    uint32_t dst_queue_family = VK_QUEUE_FAMILY_IGNORED,
                                                    uint32_t base_mip_level = 0,
                                                    uint32_t base_array_level = 0,
                                                    uint32_t level_count = VK_REMAINING_MIP_LEVELS,
                                                    uint32_t layer_count = VK_REMAINING_ARRAY_LAYERS);

    VkBufferMemoryBarrier2 CreateBufferMemoryBarrier2(VkBuffer buffer,
                                                      VkPipelineStageFlags2 src_stage,
                                                      VkAccessFlagBits2 src_access,
                                                      VkPipelineStageFlags2 dst_stage,
                                                      VkAccessFlags2 dst_access,
                                                      uint64_t offset = 0,
                                                      uint64_t size = VK_WHOLE_SIZE);

    uint64_t CalculateSamplerHash(const SamplerDescription *desc);

    inline VkDeviceAddress GetBufferDeviceAddress(VkDevice device, VkBuffer buffer) {
        VkBufferDeviceAddressInfo buffer_address_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .pNext = nullptr,
            .buffer = buffer,
        };
        return vkGetBufferDeviceAddress(device, &buffer_address_info);
    }

    inline bool is_stencil_format(VkFormat format) {
        switch (format) {
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
            return true;
        default:
            return false;
        }
        return false;
    }

    inline bool is_depth_format(VkFormat format) {
        switch (format) {
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT:
        case VK_FORMAT_D16_UNORM:
            return true;
        default:
            return false;
        }
        return false;
    }

} // namespace mirai