#include "RTGroundTruthPass.hpp"
#include "Graphics/Renderer.hpp"
#include "Scene/FrameGraph.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"
#include "RenderPassData.hpp"

namespace mirai {

    RTGroundTruthPass::RTGroundTruthPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {
        frame_graph->add_callback_pass<RTGroundTruthPassData>(
            "RTGroundTruthPass",
            [board](FrameGraph::Builder &builder, RTGroundTruthPassData &data) {
                data.shader = std::make_shared<RTShader>("RTGroundTruthShader",
                                                         "SPIRV/gt-path-trace.rgen.spv",
                                                         std::vector<std::string>{"SPIRV/gt-path-trace.rchit.spv"},
                                                         std::vector<std::string>{"SPIRV/gt-path-trace.rmiss.spv"});
                board->add<RTGroundTruthPassData>(data.shader);
            },
            [](const RTGroundTruthPassData &data, FrameGraphPassResource &resource, void *ctx) {

            });
    }
} // namespace mirai