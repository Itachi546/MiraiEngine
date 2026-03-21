#pragma once

#include "Graphics/RenderingDevice.hpp"
#include "Common/ResourcePool.hpp"
#include "Common/Hash.hpp"
#include "Common/HashMap.hpp"

#include <vector>
#include <string>
#include <memory>
#include <algorithm>

namespace mirai {

    class CommandBuffer;
    class Scene;
    class Renderer;
    class FrameGraph;
    struct FrameGraphNode;

    using FrameGraphNodeHandle = uint32_t;

    enum FrameGraphResourceType {
        FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT,
        FRAMEGRAPH_RESOURCE_TYPE_TEXTURE,
        FRAMEGRAPH_RESOURCE_TYPE_REFERENCE,
        FRAMEGRAPH_RESOURCE_TYPE_EXTERNAL_REFERENCE,
        FRAMEGRAPH_RESOURCE_TYPE_BUFFER,
        FRAMEGRAPH_RESOURCE_TYPE_INVALID
    };

    struct FrameGraphResourceOutput {
        std::string name;
        FrameGraphResourceType resource_type;
        uint32_t width, height;
        uint32_t array_layers;
        Format format;
        AttachmentLoadOp load_op;
        Color clear_color;
    };

    struct FrameGraphResourceInput {
        std::string name;
        FrameGraphResourceType resource_type;
        AttachmentLoadOp load_op;
    };

    class FrameGraphRenderer;
    struct FrameGraphNodeDescription {
        std::string name;
        bool enabled;
        bool is_compute_pass;
        std::vector<FrameGraphResourceInput> inputs;
        std::vector<FrameGraphResourceOutput> outputs;
        std::shared_ptr<FrameGraphRenderer> renderer;
    };

    using FrameGraphResourceHandle = uint32_t;

    struct FrameGraphResource {
        std::string name;
        TextureID handle;
        struct {
            uint32_t width;
            uint32_t height;
            uint32_t depth;
            Format format;
            uint32_t array_layers;
        } resource_info;
        bool external;
    };

    struct FrameGraphResourceState {
        FrameGraphResourceHandle resource_handle = K_INVALID_RESOURCE_HANDLE;
        uint64_t access_flags = ACCESS_FLAG_NONE;
        ImageLayout layout = IMAGE_LAYOUT_UNDEFINED;
        uint64_t stage_mask = PIPELINE_STAGE_NONE;
    };

    struct FrameGraphAttachmentInfo {
        Color clear_color;
        Format format;
        AttachmentLoadOp load_op;
        TextureID texture;
    };

    struct FrameGraphRenderpassInfo {
        std::vector<FrameGraphAttachmentInfo> attachment_info;
        uint32_t depth_attachment_index = ~0u;
        bool has_stencil_attachment = false;
    };

    class FrameGraphRenderer {
      public:
        FrameGraphRenderer(const std::string &name) : name(name) {
            device = RenderingDevice::get();
        }

        virtual void initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) {}

        virtual void update(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) {}

        virtual void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) = 0;

        virtual ~FrameGraphRenderer() = default;

      protected:
        std::string name;
        RenderingDevice *device;
    };

    struct FrameGraphNode {
        std::string name;
        bool enabled;

        std::vector<FrameGraphResourceHandle> inputs;
        std::vector<FrameGraphResourceHandle> outputs;
        std::shared_ptr<FrameGraphRenderer> renderer;
        std::vector<FrameGraphResourceState> resources_state;

        FrameGraphRenderpassInfo renderpass_info;
        uint32_t width, height;
    };

    class FrameGraphBuilder {
      public:
        FrameGraphBuilder();
        FrameGraphNodeHandle create_node(const FrameGraphNodeDescription &node_description);

        FrameGraphNode *get_node(FrameGraphNodeHandle handle) {
            return resource_pool_nodes.access(handle);
        }

        FrameGraphNode *get_node(const std::string &name) {
            auto found = nodes_maps.find(utils::djb2_hash_string(name));
            if (found == nodes_maps.end())
                return nullptr;
            return resource_pool_nodes.access(found->second);
        }

        FrameGraphResource *get_resource(FrameGraphResourceHandle handle) {
            return resource_pool_resources.access(handle);
        }

        FrameGraphResource *get_resource(const std::string &name) {
            auto found = resources_map.find(utils::djb2_hash_string(name));
            if (found != resources_map.end())
                return resource_pool_resources.access(found->second);
            return nullptr;
        }

        FrameGraphResourceHandle create_node_output(const FrameGraphResourceOutput *output, bool compute_pass);
        FrameGraphResourceHandle create_node_input(const FrameGraphResourceInput *input);
        void get_output_attachment_size(uint32_t *width, uint32_t *height, const FrameGraphResourceOutput *output);
        void add_renderpass_info(Format format, TextureID texture_id, FrameGraphRenderpassInfo &rendering_info,
                                 const Color &clear_color = {0.0f, 0.0f, 0.0f, 1.0f}, AttachmentLoadOp load_op = LOAD_OP_LOAD);

        void create_resource_state(FrameGraphResourceType resource_type,
                                   AttachmentLoadOp load_op,
                                   FrameGraphResourceState *state,
                                   bool compute_pass,
                                   bool is_input_resource);

        ~FrameGraphBuilder();

      private:
        RenderingDevice *device;
        ResourcePool<FrameGraphNode> resource_pool_nodes;
        HashMap<uint32_t, uint32_t> nodes_maps;

        ResourcePool<FrameGraphResource> resource_pool_resources;
        HashMap<uint32_t, uint32_t> resources_map;
    };

    class FrameGraph {
      public:
        FrameGraph(FrameGraphBuilder *builder);

        void load_from_file(const std::string &filename);

        void render(CommandBuffer *command_buffer, Renderer *renderer);
        void update(Renderer *renderer);

        void add_node(const FrameGraphNodeDescription &node) {
            node_descriptions.push_back(node);
        }

        FrameGraphNode *get_node(const std::string &name) {
            return builder->get_node(name);
        }

        FrameGraphNode *get_node(FrameGraphNodeHandle node) {
            return builder->get_node(node);
        }

        FrameGraphResource *get_resource(FrameGraphResourceHandle handle) {
            return builder->get_resource(handle);
        }

        FrameGraphResource *get_resource(const std::string &name) {
            return builder->get_resource(name);
        }

        bool set_renderer(const std::string &name, std::shared_ptr<FrameGraphRenderer> renderer) {
            auto found = std::find_if(node_descriptions.begin(), node_descriptions.end(), [&name](const FrameGraphNodeDescription &node_description) {
                return name == node_description.name;
            });

            if (found != node_descriptions.end()) {
                found->renderer = renderer;
                return true;
            }
            return false;
        }

        FrameGraphRenderer *get_renderer(const std::string &name) {
            auto found = std::find_if(node_descriptions.begin(), node_descriptions.end(), [&name](const FrameGraphNodeDescription &node_description) {
                return name == node_description.name;
            });

            if (found != node_descriptions.end())
                return found->renderer.get();
            return nullptr;
        }

      private:
        std::string name;
        FrameGraphBuilder *builder;
        std::vector<FrameGraphNodeDescription> node_descriptions;
        std::vector<FrameGraphNodeHandle> node_handles;

        // Renderer must be set for each pass before calling compile function
        // use set_renderer() function
        void compile(Renderer *renderer);

        friend class Renderer;
    };
} // namespace mirai