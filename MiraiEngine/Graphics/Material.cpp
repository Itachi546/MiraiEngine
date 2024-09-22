#include "Material.hpp"
#include "Common/Hash.hpp"
#include "MaterialCache.hpp"

namespace mirai
{
    Material::Material(const std::string &name) : name(name),
                                                  cull_mode(CullMode::CULL_MODE_BACK),
                                                  front_face(FrontFace::FRONT_FACE_COUNTER_CLOCKWISE),
                                                  enable_depth_test(false),
                                                  enable_depth_write(false),
                                                  hash(0)
    {
    }

    void Material::create_from_file(const std::vector<std::string> &shader_files)
    {
        uint32_t shader_count = static_cast<uint32_t>(shader_files.size());
        std::vector<ShaderID> shaders(shader_count);

        for (uint32_t i = 0; i < shader_count; ++i)
            shaders[i] = rendering_utils::create_shader_module_from_file(shader_files[i]);

        uint32_t hash = utils::djb2_hash_string(name);
        MaterialCache::get()->register_shader(hash, std::move(shaders));

        calculate_hash();
        /*
        RasterizationState rs = RasterizationState::create();
        DepthState ds = DepthState::create();
        BlendState bs = BlendState::create();
        Format format = FORMAT_B8G8R8A8_UNORM;

        PipelineDescription pipeline_description;
        pipeline_description.shader_count = 2;
        pipeline_description.shaders = shaders.data();
        pipeline_description.rasterization_state = &rs;
        pipeline_description.depth_state = &ds;
        pipeline_description.blend_state = &bs;
        pipeline_description.color_attachment_count = 1;
        pipeline_description.color_attachment_formats = &format;

        PipelineID pipeline = RenderingDevice::get()->create_graphics_pipeline(&pipeline_description, name);

        RenderingDevice::get()->destroy_shaders(shaders.data(), shader_count);
        */
    }

    void Material::calculate_hash()
    {
        hash = 0;
        utils::hash_combine(hash, name, (int)cull_mode, (int)front_face, enable_depth_test, enable_depth_write);
    }

} // namespace mirai