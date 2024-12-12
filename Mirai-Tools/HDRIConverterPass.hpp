#include "Graphics/RenderingDevice.hpp"
#include "Scene/FrameGraph.hpp"
#include "Scene/ShaderMaterial.hpp"

using namespace mirai;

struct HDRIConverterPass : public FrameGraphRenderer {

    HDRIConverterPass(int width, int height) : FrameGraphRenderer("cubemap_gen_pass"), width(width), height(height) {
        dirty = false;
    }

    void set_size(int width, int height) {
        this->width = width;
        this->height = height;
    }

    void set_hdri_texture(TextureID texture) {
        hdri = texture;
    }

    void set_dirty(bool state) {
        this->dirty = state;
    }

    void initialize(FrameGraph *frame_graph, const FrameGraphNode *node) override;

    void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) override;

    ~HDRIConverterPass();

    TextureID cubemap, hdri;
    int width, height;
    std::unique_ptr<ComputeShader> shader;
    UniformSetID uniform_set;
    bool dirty;
};