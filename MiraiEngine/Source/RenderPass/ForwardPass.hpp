#pragma once

namespace mirai {

    class FrameGraph;
    class FrameGraphBlackBoard;
    class ForwardPass {
      public:
        ForwardPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board);
        ~ForwardPass() = default;
    };
} // namespace mirai