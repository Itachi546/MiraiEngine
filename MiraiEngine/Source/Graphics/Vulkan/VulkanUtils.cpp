#include "VulkanUtils.hpp"
#include "Common/Hash.hpp"

#include "Graphics/RenderingDevice.hpp"
namespace mirai {
    VkImageMemoryBarrier CreateImageMemoryBarrier(VkImage image,
                                                  VkImageAspectFlags aspect,
                                                  VkAccessFlags src_access_mask,
                                                  VkAccessFlags dst_access_mask,
                                                  VkImageLayout old_layout,
                                                  VkImageLayout new_layout,
                                                  uint32_t src_queue_family,
                                                  uint32_t dst_queue_family,
                                                  uint32_t base_mip_level,
                                                  uint32_t base_array_level,
                                                  uint32_t level_count,
                                                  uint32_t layer_count) {
        return {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = src_access_mask,
            .dstAccessMask = dst_access_mask,
            .oldLayout = old_layout,
            .newLayout = new_layout,
            .srcQueueFamilyIndex = src_queue_family,
            .dstQueueFamilyIndex = dst_queue_family,
            .image = image,
            .subresourceRange = {
                .aspectMask = aspect,
                .baseMipLevel = base_mip_level,
                .levelCount = level_count,
                .baseArrayLayer = base_array_level,
                .layerCount = layer_count,
            },
        };
    }

    VkImageMemoryBarrier2 CreateImageMemoryBarrier2(VkImage image,
                                                    VkPipelineStageFlags2 src_stage_mask,
                                                    VkAccessFlags2 src_access_mask,
                                                    VkPipelineStageFlags2 dst_stage_mask,
                                                    VkAccessFlags2 dst_access_mask,
                                                    VkImageLayout old_layout,
                                                    VkImageLayout new_layout,
                                                    VkImageAspectFlags aspect,
                                                    uint32_t src_queue_family,
                                                    uint32_t dst_queue_family,
                                                    uint32_t base_mip_level,
                                                    uint32_t base_array_level,
                                                    uint32_t level_count,
                                                    uint32_t layer_count) {
        return {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = src_stage_mask,
            .srcAccessMask = src_access_mask,
            .dstStageMask = dst_stage_mask,
            .dstAccessMask = dst_access_mask,
            .oldLayout = old_layout,
            .newLayout = new_layout,
            .srcQueueFamilyIndex = src_queue_family,
            .dstQueueFamilyIndex = dst_queue_family,
            .image = image,
            .subresourceRange = {
                .aspectMask = aspect,
                .baseMipLevel = base_mip_level,
                .levelCount = level_count,
                .baseArrayLayer = base_array_level,
                .layerCount = layer_count,
            },
        };
    }

    VkBufferMemoryBarrier2 CreateBufferMemoryBarrier2(VkBuffer buffer,
                                                      VkPipelineStageFlags2 src_stage,
                                                      VkAccessFlagBits2 src_access,
                                                      VkPipelineStageFlags2 dst_stage,
                                                      VkAccessFlags2 dst_access,
                                                      uint64_t offset,
                                                      uint64_t size) {
        return {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = src_stage,
            .srcAccessMask = src_access,
            .dstStageMask = dst_stage,
            .dstAccessMask = dst_access,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = buffer,
            .offset = offset,
            .size = size,
        };
    }

    uint64_t CalculateSamplerHash(const SamplerDescription *desc) {
        uint64_t hash = 0;
        utils::hash_combine(hash, cast_u32(desc->address_mode_u),
                            cast_u32(desc->address_mode_u),
                            cast_u32(desc->address_mode_w),
                            cast_u32(desc->min_filter),
                            cast_u32(desc->mag_filter),
                            cast_u32(desc->mipmap_mode),
                            desc->lod_bias,
                            desc->max_anisotropy,
                            desc->min_lod,
                            desc->max_lod,
                            cast_u32(desc->enable_anisotropy));
        return hash;
    }
} // namespace mirai