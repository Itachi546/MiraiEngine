#pragma once

namespace mirai {

    class FrameGraph;
    class FrameGraphBlackBoard;

    class ViewNormalDepthPass {
      public:
        ViewNormalDepthPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board);
        ~ViewNormalDepthPass() = default;
    };
} // namespace mirai