#pragma once
namespace mirai {

    class FrameGraph;
    class FrameGraphBlackBoard;

    class FinalCompositePass {
      public:
        FinalCompositePass(FrameGraph *frame_graph, FrameGraphBlackBoard *board);
        ~FinalCompositePass() = default;

      private:
    };

}; // namespace mirai