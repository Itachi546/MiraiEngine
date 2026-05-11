#pragma once

namespace mirai {

    class FrameGraph;
    class FrameGraphBlackBoard;

    class RTShadowPass {
      public:
        RTShadowPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board);
        ~RTShadowPass() = default;
    };
} // namespace mirai