#pragma once

#include "Scene/FrameGraph.hpp"

namespace mirai {
    struct CascadedShadowPassData {
        FrameGraphResourceHandle output;
    };

    struct FinalCompositePassData {
        FrameGraphResourceHandle output;
    };
} // namespace mirai