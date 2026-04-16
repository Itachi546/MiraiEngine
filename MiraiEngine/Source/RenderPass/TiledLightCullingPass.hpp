#pragma once

namespace mirai {
    class FrameGraph;
    class FrameGraphBlackboard;

    class TiledLightCullingPass {
      public:
        TiledLightCullingPass(FrameGraph *frame_graph, FrameGraphBlackboard *board);
        ~TiledLightCullingPass() = default;
    };

} // namespace mirai