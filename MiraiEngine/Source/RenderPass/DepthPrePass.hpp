#pragma once

namespace mirai {
    class FrameGraph;
    class FrameGraphBlackBoard;
    class DepthPrePass {
      public:
        DepthPrePass(FrameGraph *frame_graph, FrameGraphBlackBoard *board);
        ~DepthPrePass() = default;
    };
} // namespace mirai