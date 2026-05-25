#include "GPUResource.hpp"
#include "Engine/AppSettings.hpp"

constexpr uint32_t K_INVALID_PAGE_ID = UINT32_MAX;
namespace mirai {

    std::vector<uint32_t> GPUIndexAllocator::free_lists = {};
    uint32_t GPUIndexAllocator::next_index = 0;

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
        ASSERT(index + descriptor_info_count <= AppSettings::K_RESOURCE_DESCRIPTOR_LIMIT);
        device->write_resource_descriptors(descriptor_infos, descriptor_info_count, static_cast<uint8_t *>(ptr) + index * descriptor_size, descriptor_size);
        return index;
    }

    DescriptorOffset GPUResourceDescriptorHeap::push_descriptors(RenderingDevice *device, const DescriptorInfo *descriptor_infos, uint32_t descriptor_info_count) {
        uint32_t current_offset = offset;
        // Writing descriptor in batch trigger access violation while debugging in nsight, we 
        // will keep single write for now
        for (uint32_t i = 0; i < descriptor_info_count; ++i) {
            push_descriptor_at_index(device, descriptor_infos + i, 1, offset);
            offset += 1;
        }
        return current_offset;
    }

    DescriptorOffset GPUResourceDescriptorHeap::push_descriptors_per_frame(RenderingDevice *device, const DescriptorInfo *descriptor_infos, uint32_t descriptor_info_count) {
        uint32_t current_offset = per_frame_current_offset;
        ASSERT(per_frame_current_offset + descriptor_info_count <= per_frame_current_offset_end);
        for (uint32_t i = 0; i < descriptor_info_count; ++i) {
            uint8_t *descriptor_buffer_ptr = static_cast<uint8_t *>(ptr) + per_frame_current_offset * descriptor_size;
            device->write_resource_descriptors(descriptor_infos + i, 1, descriptor_buffer_ptr, descriptor_size);
            per_frame_current_offset += 1;
        }
        // We need index into descriptor rather than actual address
        return current_offset;
    }

    BufferView GPUPagedAllocator::allocate(uint64_t size, uint64_t alignment) {
        uint64_t required_size = align_memory(size, alignment);

        // Check if we can allocate from existing pages
        uint32_t page = find_existing_page(required_size);
        if (page == K_INVALID_PAGE_ID) {
            // @TODOf We don't allow allocation from other page for now, due to RT pipeline
            ASSERT(allocations.size() <= 1);
            // Create new allocation
            BufferDescription buffer_desc = {
                .size = page_size,
                .usage_flags = BUFFER_USAGE_STORAGE_BUFFER_BIT | BUFFER_USAGE_TRANSFER_DST_BIT | BUFFER_USAGE_STORAGE_BUFFER_BIT | BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT | BUFFER_USAGE_INDEX_BUFFER_BIT,
                .allocation_type = MEMORY_ALLOCATION_TYPE_GPU,
            };
            BufferID buffer = RenderingDevice::get()->create_buffer(&buffer_desc, "GPUBufferPage" + std::to_string(allocations.size()));
            allocations.push_back(Allocation{
                .id = buffer,
                .current_offset = required_size,
            });

            return BufferView{buffer, 0, required_size, nullptr};

        } else {
            Allocation &allocation = allocations[page];
            uint64_t offset = allocation.current_offset;
            allocation.current_offset += required_size;
            return BufferView{allocation.id, offset, required_size, nullptr};
        }
    }

    void GPUPagedAllocator::shutdown() {
        RenderingDevice *device = RenderingDevice::get();
        for (auto &allocation : allocations)
            device->destroy_buffers(&allocation.id, 1);
    }

    bool GPUPagedAllocator::can_allocate(const Allocation &allocation, uint64_t required_size) {
        if (allocation.current_offset + required_size > page_size)
            return false;

        return true;
    }

    uint32_t GPUPagedAllocator::find_existing_page(uint64_t required_size) {
        for (uint32_t i = 0; i < allocations.size(); ++i) {
            if (can_allocate(allocations[i], required_size))
                return i;
        }
        return K_INVALID_PAGE_ID;
    }

} // namespace mirai