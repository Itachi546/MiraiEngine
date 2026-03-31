#include "GPUResource.hpp"
#include "Engine/AppSettings.hpp"

namespace mirai {

    void GPUResourceDescriptorHeap::new_frame(uint32_t frame_index) {
        per_frame_current_offset = (AppSettings::K_RESOURCE_DESCRIPTOR_LIMIT + frame_index * AppSettings::K_PER_FRAME_RESOURCE_DESCRIPTOR_LIMIT);
        per_frame_current_offset_end = per_frame_current_offset + AppSettings::K_PER_FRAME_RESOURCE_DESCRIPTOR_LIMIT;
    }

    uint32_t GPUResourceDescriptorHeap::push_descriptor(RenderingDevice *device, const DescriptorInfo *descriptor_infos, uint32_t descriptor_info_count) {
        return 0;
    }

    uint32_t GPUResourceDescriptorHeap::push_descriptor_per_frame(RenderingDevice *device, const DescriptorInfo *descriptor_infos, uint32_t descriptor_info_count) {
        uint32_t current_offset = per_frame_current_offset;
        ASSERT(per_frame_current_offset + descriptor_info_count <= per_frame_current_offset_end);

        for (uint32_t i = 0; i < descriptor_info_count; ++i) {
            uint8_t descriptor_buffer[128];
            device->write_resource_descriptor(descriptor_infos[i], descriptor_buffer, 128);

            std::memcpy(static_cast<uint8_t *>(ptr) + per_frame_current_offset * descriptor_size, descriptor_buffer, descriptor_size);
            per_frame_current_offset++;
        }

        // We need index into descriptor rather than actual address
        return current_offset;
    }
} // namespace mirai