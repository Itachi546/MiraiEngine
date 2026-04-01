#pragma once

#include "Scene/FrameGraph.hpp"
#include "Scene/Shader.hpp"

namespace mirai {
    struct CascadedShadowPassData {
        FrameGraphResourceHandle output;
    };

    struct FinalCompositePassData {
        FrameGraphResourceHandle output;
    };

    struct DepthPrePassData {
        FrameGraphResourceHandle output;
    };
} // namespace mirai