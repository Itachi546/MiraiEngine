#pragma once

#include "Vulkan.hpp"

namespace mirai {
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

} // namespace mirai