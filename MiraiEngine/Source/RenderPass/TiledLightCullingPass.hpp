#pragma once

namespace mirai {
    class FrameGraph;
    class FrameGraphBlackBoard;

    class TiledLightCullingPass {
      public:
        TiledLightCullingPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board);
        ~TiledLightCullingPass() = default;
    };

} // namespace mirai