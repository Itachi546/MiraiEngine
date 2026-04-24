#include "SkinningComputePass.hpp"
#include "RenderPassData.hpp"
#include "Scene/FrameGraph.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"

namespace mirai {
    SkinningComputePass::SkinningComputePass(FrameGraph *frame_graph, FrameGraphBlackboard *board) {
        frame_graph->add_callback_pass<SkinningComputePassData>(
            "SkinningComputePass",
            [](FrameGraph::FrameGraphBuilder &builder, SkinningComputePassData &data) {
                
            },
            [](const SkinningComputePassData &data, const FrameGraphPassResource &resources, void *context) {

            });
    }
} // namespace mirai
