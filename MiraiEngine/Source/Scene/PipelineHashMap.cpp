#include "PipelineHashMap.hpp"
#include "Common/Hash.hpp"

namespace mirai {
    PipelineHashMap *PipelineHashMap::Instance = nullptr;

    PipelineHashMap::PipelineHashMap() {
        ASSERT(Instance == nullptr);
        Instance = this;
    }

    PipelineID PipelineHashMap::add_or_get_graphics_pipeline(const PipelineState &pipeline_state, const PipelineAttachmentInfo &attachment_info, const std::vector<std::string> &shader_files, const std::string &name) {
        uint64_t pipeline_key = pipeline_state.get_hash();
        auto found = pipeline_map.find(pipeline_key);
        if (found != pipeline_map.end())
            return found->second;

        RasterizationState rs = RasterizationState::create();

        PipelineState::PipelineRenderState render_state = pipeline_state.render_state;
        rs.cull_mode = CullMode(render_state.fields.cull_mode);
        rs.front_face = FrontFace(render_state.fields.front_face);
        rs.enable_depth_clamp = static_cast<bool>(render_state.fields.depth_clamp);
        rs.polygon_mode = PolygonMode(render_state.fields.polygon_mode);

        BlendState bs = BlendState::create();
        if (render_state.fields.blend_mode > 0)
            bs.enable = true;
        PipelineDescription pipeline_description;

        ASSERT(shader_files.size() > 0);
        std::vector<ShaderID> shader_modules;
        for (uint32_t i = 0; i < shader_files.size(); ++i) {
            ShaderID shader = rendering_utils::create_shader_module_from_file(shader_files[i]);
            shader_modules.push_back(shader);
        }

        pipeline_description.topology = Topology(render_state.fields.topology);
        pipeline_description.shader_count = static_cast<uint32_t>(shader_modules.size());
        pipeline_description.shaders = shader_modules.data();
        pipeline_description.rasterization_state = &rs;
        pipeline_description.blend_state = &bs;

        DepthState ds = DepthState::create();
        std::vector<Format> color_attachment_formats;

        if (attachment_info.color_attachments_format.size() > 0) {
            color_attachment_formats.insert(color_attachment_formats.end(),
                                            attachment_info.color_attachments_format.begin(),
                                            attachment_info.color_attachments_format.end());
        }

        if (attachment_info.has_depth_attachment) {
            ds.enable_depth_write = render_state.fields.depth_write;
            ds.enable_depth_test = render_state.fields.depth_test;
            ds.compare_op = CompareOp(render_state.fields.depth_op);
            pipeline_description.depth_attachment_format = attachment_info.depth_attachment_format;
        }

        pipeline_description.depth_state = &ds;
        pipeline_description.color_attachment_count = static_cast<uint32_t>(color_attachment_formats.size());
        pipeline_description.color_attachment_formats = color_attachment_formats.data();

        PipelineID pipeline = RenderingDevice::get()->create_graphics_pipeline(&pipeline_description, name);

        RenderingDevice::get()->destroy_shaders(shader_modules.data(), cast_u32(shader_modules.size()));

        pipeline_map.insert(std::make_pair(pipeline_key, pipeline));

        return pipeline;
    }

    PipelineID PipelineHashMap::get_from_state_hash(uint64_t pipeline_key) {
        auto found = pipeline_map.find(pipeline_key);
        if (found != pipeline_map.end())
            return found->second;
        return PipelineID{K_INVALID_ID};
    }

    void PipelineHashMap::destroy() {
        std::vector<PipelineID> pipelines;
        pipelines.reserve(pipeline_map.size());
        for (auto &[key, val] : pipeline_map)
            pipelines.push_back(val);
        RenderingDevice::get()->destroy_pipelines(pipelines.data(), cast_u32(pipelines.size()));
        pipeline_map.clear();
    }

    PipelineID PipelineHashMap::add_or_get_compute_pipeline(const std::string &shader_files, const std::string &name) {
        uint64_t key = utils::djb2_hash_string(name);
        auto found = pipeline_map.find(key);
        if (found != pipeline_map.end())
            return found->second;

        ShaderID shader = rendering_utils::create_shader_module_from_file(shader_files);
        PipelineID pipeline = RenderingDevice::get()->create_compute_pipeline(shader, name);
        pipeline_map.insert(std::make_pair(key, pipeline));
        RenderingDevice::get()->destroy_shaders(&shader, 1);
        return pipeline;
    }

    PipelineHashMap::~PipelineHashMap() {
        pipeline_map.clear();
    }

} // namespace mirai