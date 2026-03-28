#pragma once

namespace mirai {
    struct Shader;
    class FrameGraph;
    class FrameGraphBlackBoard;
    class DepthPrePass {
      public:
        DepthPrePass(FrameGraph *frame_graph, FrameGraphBlackBoard *board);
        ~DepthPrePass() = default;
    };
} // namespace mirai