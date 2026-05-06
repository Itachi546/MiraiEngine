#pragma once

namespace mirai {
    class FrameGraph;
    class FrameGraphBlackBoard;

    class BloomPass {
      public:
        BloomPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board);
        ~BloomPass() = default;
    };
} // namespace mirai