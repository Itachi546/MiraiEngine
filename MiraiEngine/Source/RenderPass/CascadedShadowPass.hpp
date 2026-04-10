#pragma once

namespace mirai {

    class FrameGraph;
    class FrameGraphBlackBoard;

    class CascadedShadowPass {
      public:
        CascadedShadowPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board);
        ~CascadedShadowPass() = default;
    };

} // namespace mirai