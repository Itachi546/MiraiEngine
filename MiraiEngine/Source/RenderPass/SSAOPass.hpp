#pragma once

namespace mirai {
    class FrameGraph;
    class FrameGraphBlackBoard;

    class SSAOPass {
      public:
        SSAOPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board);

        ~SSAOPass() = default;
    };
} // namespace mirai