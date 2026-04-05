#include "GPUResource.hpp"
#include "Engine/AppSettings.hpp"

namespace mirai {

    void GPUResourceDescriptorHeap::new_frame(uint32_t frame_index) {
        per_frame_current_offset = (AppSettings::K_RESOURCE_DESCRIPTOR_LIMIT + frame_index * AppSettings::K_PER_FRAME_RESOURCE_DESCRIPTOR_LIMIT);
        per_frame_current_offset_end = per_frame_current_offset + AppSettings::K_PER_FRAME_RESOURCE_DESCRIPTOR_LIMIT;
    }

    DescriptorOffset GPUResourceDescriptorHeap::allocate(uint32_t descriptor_count) {
        uint32_t current_offset = offset;
        offset += descriptor_count;
        return current_offset;
    }

    DescriptorOffset GPUResourceDescriptorHeap::push_descriptor_at_index(RenderingDevice *device, const DescriptorInfo *descriptor_infos, uint32_t descriptor_info_count, uint32_t index) {
        uint32_t current_offset = index;
        ASSERT(current_offset + descriptor_info_count <= AppSettings::K_RESOURCE_DESCRIPTOR_LIMIT);
        device->write_resource_descriptors(descriptor_infos, descriptor_info_count, static_cast<uint8_t *>(ptr) + current_offset * descriptor_size, descriptor_size);
        return index;
    }

    DescriptorOffset GPUResourceDescriptorHeap::push_descriptors(RenderingDevice *device, const DescriptorInfo *descriptor_infos, uint32_t descriptor_info_count) {
        uint32_t current_offset = push_descriptor_at_index(device, descriptor_infos, descriptor_info_count, offset);
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
} // namespace mirai