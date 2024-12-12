#include "ShaderMaterial.hpp"
#include "Common/Hash.hpp"
#include "ShaderMaterialCache.hpp"
#include "FrameGraph.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"

namespace mirai {
    ShaderMaterial::ShaderMaterial(const std::string &name) : name(name),
                                                              cull_mode(CullMode::CULL_MODE_BACK),
                                                              front_face(FrontFace::FRONT_FACE_COUNTER_CLOCKWISE),
                                                              depth_compare_op(COMPARE_OP_LESS_OR_EQUAL),
                                                              topology(TOPOLOGY_TRIANGLE_LIST),
                                                              enable_depth_test(false),
                                                              enable_depth_write(false),
                                                              enable_blend(false),
                                                              hash(0),
                                                              is_resource_updated(true),
                                                              pipeline{K_INVALID_ID} {
    }

    void ShaderMaterial::create_from_file(const std::vector<std::string> &shader_files) {
        uint32_t shader_count = static_cast<uint32_t>(shader_files.size());
        shader_hash = 0;
        for (uint32_t i = 0; i < shader_count; ++i) {
            uint32_t hash = utils::djb2_hash_string(shader_files[i]);
            utils::hash_combine(shader_hash, hash);
            auto found = ShaderMaterialCache::get()->get_shader(hash);
            if (found.is_valid()) {
                shaders.push_back(found);
            } else {
                ShaderID shader = rendering_utils::create_shader_module_from_file(shader_files[i]);
                ShaderMaterialCache::get()->cache_shader(hash, shader);
                shaders.push_back(shader);
            }
        }
    }

    void ShaderMaterial::bind(CommandBuffer *command_buffer, const FrameGraphRenderpassInfo *renderpass) {
        if (dirty) {
            calculate_hash();
            dirty = false;
        }

        if (!pipeline.is_valid()) {
            pipeline = ShaderMaterialCache::get()->get_pipeline(hash);
            if (!pipeline.is_valid())
                pipeline = create_pipeline(renderpass);
        }

        command_buffer->bind_pipeline(pipeline,
                                      uniform_sets.data(),
                                      static_cast<uint32_t>(uniform_sets.size()),
                                      push_constants.data(),
                                      static_cast<uint32_t>(push_constants.size()));
    }

    void ShaderMaterial::calculate_hash() {
        hash = shader_hash;
        utils::hash_combine(hash,
                            (int)cull_mode,
                            (int)front_face,
                            enable_depth_test,
                            enable_depth_write,
                            depth_compare_op,
                            enable_blend,
                            topology);
    }

    PipelineID ShaderMaterial::create_pipeline(const FrameGraphRenderpassInfo *renderpass) {
        RasterizationState rs = RasterizationState::create();
        rs.cull_mode = cull_mode;
        rs.front_face = front_face;

        BlendState bs = BlendState::create();
        if (enable_blend)
            bs.enable = true;
        PipelineDescription pipeline_description;

        ASSERT(shaders.size() > 0);

        pipeline_description.topology = topology;
        pipeline_description.shader_count = static_cast<uint32_t>(shaders.size());
        pipeline_description.shaders = shaders.data();
        pipeline_description.rasterization_state = &rs;
        pipeline_description.blend_state = &bs;

        DepthState ds = DepthState::create();
        std::vector<Format> color_attachment_formats;

        for (uint32_t i = 0; i < renderpass->attachment_info.size(); ++i) {
            const FrameGraphAttachmentInfo *attachment = &renderpass->attachment_info[i];
            if (i == renderpass->depth_attachment_index) {
                ds.enable_depth_write = enable_depth_write;
                ds.enable_depth_test = enable_depth_test;
                ds.compare_op = depth_compare_op;
                pipeline_description.depth_attachment_format = attachment->format;
            } else {
                color_attachment_formats.push_back(attachment->format);
            }
        }
        pipeline_description.depth_state = &ds;
        pipeline_description.color_attachment_count = static_cast<uint32_t>(color_attachment_formats.size());
        pipeline_description.color_attachment_formats = color_attachment_formats.data();

        PipelineID pipeline = RenderingDevice::get()->create_graphics_pipeline(&pipeline_description, name);
        ShaderMaterialCache::get()->add_pipeline(hash, pipeline);

        return pipeline;
    }

    ComputeShader::ComputeShader(const std::string &name) : name(name) {
    }

    void ComputeShader::create_from_file(const std::string &file) {
        ShaderID shader = rendering_utils::create_shader_module_from_file(file);
        create_pipeline(shader);
        RenderingDevice::get()->destroy_shaders(&shader, 1);
    }

    void ComputeShader::bind(CommandBuffer *command_buffer) {
        ASSERT(pipeline.is_valid());
        command_buffer->bind_pipeline(pipeline,
                                      uniform_sets.data(),
                                      static_cast<uint32_t>(uniform_sets.size()),
                                      push_constants.data(),
                                      static_cast<uint32_t>(push_constants.size()));
    }

    ComputeShader::~ComputeShader() {
        RenderingDevice::get()->destroy_pipelines(&pipeline, 1);
    }

    void ComputeShader::create_pipeline(ShaderID shader) {
        RenderingDevice *device = RenderingDevice::get();
        pipeline = device->create_compute_pipeline(shader, name);
    }

} // namespace mirai