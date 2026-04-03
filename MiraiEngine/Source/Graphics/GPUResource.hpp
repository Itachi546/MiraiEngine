#pragma once

#include "RenderingDevice.hpp"

namespace mirai {

    struct GPUSamplerDescriptorHeap {
        BufferID buffer;
        uint32_t size;
        uint32_t descriptor_size;
        void *ptr;
    };

    struct GPUResourceDescriptorHeap {
      public:
        BufferID buffer;
        uint32_t size;
        uint32_t descriptor_size;
        void *ptr;

        void new_frame(uint32_t frame_index);

        DescriptorOffset push_descriptors(RenderingDevice *device, const DescriptorInfo *descriptor_infos, uint32_t descriptor_info_count);

        DescriptorOffset push_descriptors_per_frame(RenderingDevice *device, const DescriptorInfo *descriptor_infos, uint32_t descriptor_info_count);

      private:
        uint32_t offset = 0;
        uint32_t per_frame_current_offset = 0;
        uint32_t per_frame_current_offset_end = 0;
    };

    struct GpuBufferSubAllocation {
        BufferID buffer;
        uint32_t offset;
        uint32_t size;

        void init(BufferID buffer, uint32_t size, uint32_t offset = 0) {
            this->buffer = buffer;
            this->size = size;
            this->offset = offset;
        }

        bool can_allocate(uint32_t required_size) {
            if (required_size >= (size - offset))
                return false;
            return true;
        }

        std::optional<BufferView> allocate(uint32_t required_size) {
            if (!can_allocate(required_size))
                return {};
            BufferView buffer_view;
            buffer_view.buffer = buffer;
            buffer_view.offset = offset;
            buffer_view.size = required_size;
            offset += required_size;
            return buffer_view;
        }
    };
}; // namespace mirai