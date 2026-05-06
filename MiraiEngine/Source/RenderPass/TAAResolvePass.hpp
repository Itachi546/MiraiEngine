#pragma once

namespace mirai {

    class FrameGraph;
    class FrameGraphBlackBoard;
    class TAAResolvePass {
      public:
        TAAResolvePass(FrameGraph *frame_graph, FrameGraphBlackBoard *board);
        ~TAAResolvePass() = default;
    };
} // namespace mirai