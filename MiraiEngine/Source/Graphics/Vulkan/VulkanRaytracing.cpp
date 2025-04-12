#include "VulkanRaytracing.hpp"

#include "Math/MathUtils.hpp"

namespace mirai {

    void CreateBVH_BLAS(VkDevice device, VkBuffer vertex_buffer, uint32_t vertex_buffer_size, uint32_t vertex_stride, VkBuffer index_buffer, uint32_t index_buffer_size) {

        VkBufferDeviceAddressInfo buffer_address_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .pNext = nullptr,
            .buffer = vertex_buffer,
        };

        uint32_t max_primitives = index_buffer_size / (sizeof(uint32_t) * 3);
        uint32_t max_vertices = vertex_buffer_size / vertex_stride;

        VkDeviceAddress vertex_address = vkGetBufferDeviceAddress(device, &buffer_address_info);

        buffer_address_info.buffer = index_buffer;
        VkDeviceAddress index_address = vkGetBufferDeviceAddress(device, &buffer_address_info);

        const VkAccelerationStructureGeometryKHR geometries = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
            .geometry = {
                .triangles = {
                    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                    .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                    .vertexData = vertex_address,
                    .vertexStride = vertex_stride,
                    .maxVertex = max_vertices,
                    .indexType = VK_INDEX_TYPE_UINT32,
                    .indexData = index_address,
                    .transformData = {},
                },
            },
            // @TODO maybe need to change this later for transparent
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
        };

        VkAccelerationStructureBuildGeometryInfoKHR build_info = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .pNext = nullptr,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
            .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
            // @TODO can be made multiple later
            .geometryCount = 1,
            .pGeometries = &geometries,
        };

        VkAccelerationStructureBuildSizesInfoKHR size_info = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
        vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build_info, &max_primitives, &size_info);

        Log::Info("Scratch Buffer Size: ", utils::bytes_to_mb(size_info.buildScratchSize), " mb");
        Log::Info("BLAS Buffer Size: ", utils::bytes_to_mb(size_info.accelerationStructureSize), " mb");
    }
} // namespace mirai