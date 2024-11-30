#include "SkyMaterial.hpp"

namespace mirai {
    ProceduralSkyMaterial::ProceduralSkyMaterial() : ShaderMaterial("ProceduralSky") {
        create_from_file({
            "SPIRV/fullscreen.vert.spv",
            "SPIRV/procedural_sky.frag.spv",
        });

        push_constant.data = &shader_inputs;
        push_constant.shader_stage = SHADER_STAGE_FRAGMENT;
        push_constant.offset = 0;
        push_constant.size = (uint32_t)sizeof(ShaderInputs);
        
    }

    void ProceduralSkyMaterial::bind(CommandBuffer *command_buffer, const FrameGraphNode *node, FrameGraph *frame_graph) {
        set_push_constant(&push_constant, 1);
        ShaderMaterial::bind(command_buffer, node, frame_graph);
    }
} // namespace mirai