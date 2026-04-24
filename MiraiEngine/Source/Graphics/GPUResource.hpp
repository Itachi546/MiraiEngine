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

        // Allocate certain space, currently used for bindless texture
        DescriptorOffset allocate(uint32_t descriptor_count);

        // Push descriptor at particular index, used after allocation
        DescriptorOffset push_descriptor_at_index(RenderingDevice *device, const DescriptorInfo *descriptor_infos, uint32_t descriptor_info_count, uint32_t index);

        // Push static descriptor
        DescriptorOffset push_descriptors(RenderingDevice *device, const DescriptorInfo *descriptor_infos, uint32_t descriptor_info_count);

        // Push per frame descriptor
        DescriptorOffset push_descriptors_per_frame(RenderingDevice *device, const DescriptorInfo *descriptor_infos, uint32_t descriptor_info_count);

      private:
        uint32_t offset = 0;
        uint32_t per_frame_current_offset = 0;
        uint32_t per_frame_current_offset_end = 0;
    };

    struct GPUBufferAllocation {
        BufferID buffer;
        uint32_t offset;
        uint32_t size;
        uint8_t *ptr;

        void init(const BufferDescription &buffer_desc, const std::string &debug_name) {
            RenderingDevice *device = RenderingDevice::get();
            this->buffer = device->create_buffer(&buffer_desc, debug_name);
            this->size = buffer_desc.size;
            this->offset = 0;
            if (buffer_desc.allocation_type == MEMORY_ALLOCATION_TYPE_CPU)
                this->ptr = device->map_buffer(buffer);
            else
                this->ptr = nullptr;
        }

        bool can_allocate(uint32_t required_size) {
            if (required_size >= (size - offset))
                return false;
            return true;
        }

        BufferView allocate(uint32_t required_size, uint32_t alignment = 64) {
            required_size = align_memory(required_size, alignment);
            if (!can_allocate(required_size)) {
                ASSERT_MSG(0, "Cannot allocate from buffer");
                // @TODO handle this
            }

            BufferView buffer_view;
            buffer_view.buffer = buffer;
            buffer_view.offset = offset;
            buffer_view.size = required_size;
            buffer_view.ptr = ptr + offset;
            offset += required_size;
            return buffer_view;
        }

        void destroy() {
            RenderingDevice::get()->destroy_buffers(&buffer, 1);
        }
    };

    struct GPUBufferLinearAllocator : public GPUBufferAllocation {
        void reset() {
            offset = 0;
        }
    };

}; // namespace mirai