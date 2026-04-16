#include "TiledLightCullingPass.hpp"
#include "RenderPassData.hpp"
#include "Scene/FrameGraph.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"

namespace mirai {
    TiledLightCullingPass::TiledLightCullingPass(FrameGraph *frame_graph, FrameGraphBlackboard *board) {
        frame_graph->add_callback_pass<TiledLightCullingPassData>(
            "TiledLightCullingFrustumGen",
            [board](FrameGraph::FrameGraphBuilder &builder, TiledLightCullingPassData &data) {

            },
            [](const TiledLightCullingPassData &data, const FrameGraphPassResource &pass_resource, void *ctx) {

            });
    }
} // namespace mirai