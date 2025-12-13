#include "PipelineLoader.hpp"
#include "Scene/PipelineHashMap.hpp"
#include "Engine/Log.hpp"
#include "Graphics/StringEnumLookup.hpp"
#include <json.hpp>
#include <fstream>

namespace mirai {
    void preload_shaders(PipelineHashMap *pipeline_hashmap) {
        using json = nlohmann::json;
        std::ifstream json_file("Assets/default-pipelines.json");
        if (!json_file) {
            Log::Fatal("Failed to compile pipeline");
        }
        json data = json::parse(json_file);

        for (std::size_t i = 0; i < data.size(); ++i) {
            json pipeline_info = data[i];
            const std::string &name = pipeline_info.value("name", "");
            std::vector<std::string> shaders_path = pipeline_info["shaders"].get<std::vector<std::string>>();
            json render_state = pipeline_info["state"];

            PipelineState pipeline_state = {};
            pipeline_state.render_state.fields.cull_mode = get_cull_mode(render_state.value("cull-mode", "CULL_MODE_BACK"));
            pipeline_state.render_state.fields.front_face = get_front_face(render_state.value("front-face", "FRONT_FACE_COUNTER_CLOCKWISE"));
            pipeline_state.render_state.fields.depth_test = render_state.value("depth-test", false);
            pipeline_state.render_state.fields.depth_write = render_state.value("depth-write", false);
            pipeline_state.render_state.fields.depth_clamp = render_state.value("depth-clamp", false);
            pipeline_state.render_state.fields.blend_mode = render_state.value("blend-mode", false);
            pipeline_state.render_state.fields.depth_op = get_compare_op(render_state.value("depth-op", "COMPARE_OP_LESS_OR_EQUAL"));
            pipeline_state.render_state.fields.topology = get_topology(render_state.value("topology", "TOPOLOGY_TRIANGLE_LIST"));
            pipeline_state.render_state.fields.polygon_mode = get_polygon_mode(render_state.value("polygon-mode", "POLYGON_MODE_FILL"));
            pipeline_state.render_state.fields.pass_mode = get_pass_mode(render_state.value("pass", ""));

            PipelineAttachmentInfo attachment_info{};
            if (render_state["color-attachments-format"] != nullptr) {
                std::vector<std::string> color_attachments_format = render_state["color-attachments-format"].get<std::vector<std::string>>();
                attachment_info.color_attachments_format.resize(color_attachments_format.size());
                for (uint32_t f = 0; f < color_attachments_format.size(); ++f)
                    attachment_info.color_attachments_format[f] = get_texture_format(color_attachments_format[f]);
            }
            if (render_state["depth-attachment-format"] != nullptr) {
                attachment_info.has_depth_attachment = true;
                attachment_info.depth_attachment_format = get_texture_format(render_state["depth-attachment-format"]);
            }
            PipelineHashMap::get()->add_or_get_graphics_pipeline(pipeline_state, attachment_info, shaders_path, name);
            Log::Debug("Compiled ", name, " Hash: ", pipeline_state.get_hash(), " RenderState: ", pipeline_state.render_state.hash);
        }
    }

} // namespace mirai