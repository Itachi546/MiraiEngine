#pragma once

#include "RenderingDevice.hpp"

namespace mirai {

    struct GPUSamplerDescriptorHeap {
        BufferID buffer;
        void *ptr;
        uint32_t size;
        uint32_t descriptor_size;
    };

    struct GPUResourceDescriptorHeap {
        BufferID buffer;
        void *ptr;
        uint32_t size;
        uint32_t descriptor_size;

        uint32_t offset;
        uint32_t per_frame_current_offset;
        uint32_t per_frame_current_offset_end;

        void new_frame(uint32_t frame_index);

        uint32_t push_descriptor(RenderingDevice *device, const DescriptorInfo *descriptor_infos, uint32_t descriptor_info_count);

        uint32_t push_descriptor_per_frame(RenderingDevice *device, const DescriptorInfo *descriptor_infos, uint32_t descriptor_info_count);
    };
}; // namespace mirai