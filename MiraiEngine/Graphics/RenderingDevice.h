#pragma once

#include "Common/CommonInclude.hpp"
#include <string>

namespace mirai
{
    enum class DeviceType
    {
        DEVICE_TYPE_OTHER = 0x0,
        DEVICE_TYPE_INTEGRATED_GPU = 0x1,
        DEVICE_TYPE_DISCRETE_GPU = 0x2,
        DEVICE_TYPE_VIRTUAL_GPU = 0x3,
        DEVICE_TYPE_CPU = 0x4,
        DEVICE_TYPE_MAX = 0x5
    };

    constexpr const uint32_t K_INVALID_QUEUE_ID = UINT32_MAX;
    enum QueueType
    {
        QueueType_Graphics = 0,
        QueueType_Compute = 1,
        QueueType_Transfer = 2,
    };

    struct GpuDevice
    {
        std::string name;
        uint32_t vendor;
        DeviceType device_type;
    };
}; // namespace mirai