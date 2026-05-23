#pragma once

namespace mirai {
    class FrameGraph;
    class FrameGraphBlackBoard;

    class RTGroundTruthPass {
      public:
        RTGroundTruthPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board);
        ~RTGroundTruthPass() = default;
    };
} // namespace mirai