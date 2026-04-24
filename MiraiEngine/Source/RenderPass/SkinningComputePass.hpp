#pragma once
namespace mirai {

    class FrameGraph;
    class FrameGraphBlackboard;

    class SkinningComputePass {
      public:
        SkinningComputePass(FrameGraph *frame_graph, FrameGraphBlackboard *board);
        ~SkinningComputePass() = default;
    };

} // namespace mirai