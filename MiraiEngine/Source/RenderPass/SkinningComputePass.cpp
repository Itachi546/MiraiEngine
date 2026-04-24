#include "SkinningComputePass.hpp"
#include "RenderPassData.hpp"
#include "Scene/FrameGraph.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"

namespace mirai {
    const uint32_t K_DEFAULT_SKINNED_BUFFER_SIZE = 8 * 1024 * 1024; // 8mb
    SkinningComputePass::SkinningComputePass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {
        frame_graph->add_callback_pass<SkinningComputePassData>(
            "SkinningComputePass",
            [board](FrameGraph::Builder &builder, SkinningComputePassData &data) {
                data.output_buffer = builder.create_buffer("SkinnedOutputBuffer", {
                                                                                      .size = K_DEFAULT_SKINNED_BUFFER_SIZE,
                                                                                      .usage_flags = BUFFER_USAGE_STORAGE_BUFFER_BIT | BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                                                      .allocation_type = MEMORY_ALLOCATION_TYPE_GPU,
                                                                                  });
                builder.write(data.output_buffer, {
                                                      .access_flags = ACCESS_FLAG_SHADER_WRITE,
                                                      .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                  });

                data.shader = std::make_shared<ComputeShader>("SkinningCS", "SPIRV/skinning.comp.spv");
                board->add<SkinningComputePassData>(data);
            },

            [](const SkinningComputePassData &data, const FrameGraphPassResource &resources, void *context) {

            });
    }
} // namespace mirai
