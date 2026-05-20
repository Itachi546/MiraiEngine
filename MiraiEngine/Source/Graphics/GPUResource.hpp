#pragma once

#include "RenderingDevice.hpp"

namespace mirai {
    struct GPUSamplerDescriptorHeap {
        BufferID buffer;
        uint64_t size;
        uint32_t descriptor_size;
        void *ptr;
    };

    struct GPUResourceDescriptorHeap {
      public:
        BufferID buffer;
        uint64_t size;
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

    struct GPULinearAllocator {
        BufferID buffer;
        uint64_t offset;
        uint64_t size;
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

        bool can_allocate(uint64_t required_size) {
            if (required_size > (size - offset))
                return false;
            return true;
        }

        BufferView allocate(uint64_t required_size, uint64_t alignment = 64) {
            if (alignment > 0)
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

        void reset() {
            offset = 0;
        }

        void shutdown() {
            RenderingDevice::get()->destroy_buffers(&buffer, 1);
        }
    };

    /*
        Used for allocating memory that is resident only in GPU and not accessed by CPU
        For now, paged allocator is responsible for allocating buffer and releasing it
    */
    struct GPUPagedAllocator {
        void init(uint64_t page_size = 64 * 1024 * 1024) {
            this->page_size = page_size;
        }

        BufferView allocate(uint64_t size, uint64_t alignment = 64);

        void shutdown();

        struct Allocation {
            BufferID id;
            uint64_t current_offset;
        };
        std::vector<Allocation> allocations;

      private:
        uint64_t page_size = 0;

        bool can_allocate(const Allocation &allocation, uint64_t required_size);

        uint32_t find_existing_page(uint64_t required_size);
    };

    struct GPUIndexAllocator {
      public:
        static uint32_t allocate_index() {
            if (!free_lists.empty()) {
                uint32_t index = free_lists.back();
                free_lists.pop_back();
                return index;
            }
            return next_index++;
        }

        static void free(uint32_t index) {
            free_lists.push_back(index);
        }

      private:
        static std::vector<uint32_t> free_lists;
        static uint32_t next_index;
    };

}; // namespace mirai