#pragma once
namespace mirai {

    class FrameGraph;
    class FrameGraphBlackBoard;

    class SkinningComputePass {
      public:
        SkinningComputePass(FrameGraph *frame_graph, FrameGraphBlackBoard *board);
        ~SkinningComputePass() = default;
    };

} // namespace mirai