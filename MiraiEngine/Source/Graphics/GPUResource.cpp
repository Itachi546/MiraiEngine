#include "GPUResource.hpp"
#include "Engine/AppSettings.hpp"

namespace mirai {

    void GPUResourceDescriptorHeap::new_frame(uint32_t frame_index) {
        per_frame_current_offset = (AppSettings::K_RESOURCE_DESCRIPTOR_LIMIT + frame_index * AppSettings::K_PER_FRAME_RESOURCE_DESCRIPTOR_LIMIT);
        per_frame_current_offset_end = per_frame_current_offset + AppSettings::K_PER_FRAME_RESOURCE_DESCRIPTOR_LIMIT;
    }

    DescriptorOffset GPUResourceDescriptorHeap::push_descriptors(RenderingDevice *device, const DescriptorInfo *descriptor_infos, uint32_t descriptor_info_count) {
        uint32_t current_offset = offset;
        ASSERT(current_offset + descriptor_info_count <= AppSettings::K_RESOURCE_DESCRIPTOR_LIMIT);

        device->write_resource_descriptors(descriptor_infos, descriptor_info_count, static_cast<uint8_t *>(ptr) + current_offset * descriptor_size, descriptor_size);
        offset += descriptor_info_count;

        return current_offset;
    }

    DescriptorOffset GPUResourceDescriptorHeap::push_descriptors_per_frame(RenderingDevice *device, const DescriptorInfo *descriptor_infos, uint32_t descriptor_info_count) {
        uint32_t current_offset = per_frame_current_offset;
        ASSERT(per_frame_current_offset + descriptor_info_count <= per_frame_current_offset_end);

        uint8_t *descriptor_buffer_ptr = static_cast<uint8_t *>(ptr) + per_frame_current_offset * descriptor_size;
        device->write_resource_descriptors(descriptor_infos, descriptor_info_count, descriptor_buffer_ptr, descriptor_size);
        per_frame_current_offset += descriptor_info_count;

        // We need index into descriptor rather than actual address
        return current_offset;
    }

    DescriptorOffset GPUSamplerDescriptorHeap::push_descriptors(RenderingDevice *device, const SamplerDescription *samplers, uint32_t sampler_count) {
        uint32_t current_offset = offset;
        device->write_sampler_descriptors(samplers, sampler_count, static_cast<uint8_t *>(ptr) + offset * descriptor_size);
        offset += sampler_count;
        return current_offset;
    }
} // namespace mirai