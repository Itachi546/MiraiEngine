#pragma once

namespace mirai {

    class FrameGraph;
    class FrameGraphBlackBoard;
    class ForwardPass {
      public:
        ForwardPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board);
    };
} // namespace mirai