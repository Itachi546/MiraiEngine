#pragma once

#include "Graphics/RenderingDevice.hpp"
#include "Common/ResourcePool.hpp"
#include "Common/Hash.hpp"

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

namespace mirai {

    constexpr const char *COLOR_ATTACHMENT_OUTPUT_NAME = "main_color_attachment";
    constexpr const char *DEPTH_ATTACHMENT_OUTPUT_NAME = "main_depth_attachment";

    class CommandBuffer;
    class Scene;
    class FrameGraph;
    struct FrameGraphNode;

    using FrameGraphNodeHandle = uint32_t;

    enum FrameGraphResourceType {
        FRAMEGRAPH_RESOURCE_TYPE_SWAPCHAIN,
        FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT,
        FRAMEGRAPH_RESOURCE_TYPE_TEXTURE,
        FRAMEGRAPH_RESOURCE_TYPE_REFERENCE,
        FRAMEGRAPH_RESOURCE_TYPE_INVALID
    };

    struct FrameGraphResourceOutput {
        std::string name;
        FrameGraphResourceType resource_type;
        uint32_t width, height;
        Format format;
        AttachmentLoadOp load_op;
        Color clear_color;
    };

    struct FrameGraphResourceInput {
        std::string name;
        FrameGraphResourceType resource_type;
        AttachmentLoadOp load_op;
    };

    class FrameGraphRenderPass {
      public:
        FrameGraphRenderPass(const std::string &name) : name(name) {
            device = RenderingDevice::get();
        }

        virtual void initialize(FrameGraph *frame_graph, const FrameGraphNode *node) {}

        virtual void update(FrameGraph *frame_graph, const FrameGraphNode *node) {}

        virtual void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) = 0;

      protected:
        std::string name;
        RenderingDevice *device;
    };

    struct FrameGraphNodeDescription {
        std::string name;
        bool enabled;
        bool is_compute_pass;
        std::vector<FrameGraphResourceInput> inputs;
        std::vector<FrameGraphResourceOutput> outputs;
        std::shared_ptr<FrameGraphRenderPass> renderer;
    };

    using FrameGraphResourceHandle = uint32_t;

    struct FrameGraphAttachmentInfo {
        Color clear_color;
        FrameGraphResourceHandle resource_handle;
    };

    struct FrameGraphRenderingInfo {
        std::vector<FrameGraphAttachmentInfo> attachment_info;
        uint32_t depth_attachment_index = ~0u;
        bool has_stencil_attachment = false;
    };

    struct FrameGraphResourceInfo {

        uint32_t width;
        uint32_t height;
        uint32_t depth;
        TextureID texture;
        Format format;
        AttachmentLoadOp load_op;
    };

    struct FrameGraphResource {
        FrameGraphResourceType resource_type;
        FrameGraphResourceInfo resource_info;
    };

    struct FrameGraphNode {
        std::string name;
        bool enabled;

        std::vector<FrameGraphResourceHandle> inputs;
        std::vector<FrameGraphResourceHandle> outputs;
        std::shared_ptr<FrameGraphRenderPass> renderer;
        FrameGraphRenderingInfo rendering_info;
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

        FrameGraphResourceHandle create_node_output(const FrameGraphResourceOutput *output);
        FrameGraphResourceHandle create_node_input(const FrameGraphResourceInput *input);
        void get_output_attachment_size(uint32_t *width, uint32_t *height, const FrameGraphResourceOutput *output);
        void add_renderpass_info(FrameGraphResourceHandle handle, FrameGraphRenderingInfo &rendering_info,
                                 Color clear_color = {0.0f, 0.0f, 0.0f, 1.0f}, AttachmentLoadOp load_op = LOAD_OP_LOAD);

        ~FrameGraphBuilder();

      private:
        RenderingDevice *device;
        ResourcePool<FrameGraphNode> resource_pool_nodes;
        std::unordered_map<uint32_t, uint32_t> nodes_maps;

        ResourcePool<FrameGraphResource> resource_pool_resources;
        std::unordered_map<uint32_t, uint32_t> resources_map;
    };

    class FrameGraph {
      public:
        FrameGraph(FrameGraphBuilder *builder);

        void load_from_file(const std::string &filename);

        // Renderer must be set for each pass before calling compile function
        // use set_renderer() function
        void compile();

        void render(CommandBuffer *command_buffer, Scene *scene);

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

        bool set_renderer(const std::string &name, std::shared_ptr<FrameGraphRenderPass> renderer) {
            for (auto &desc : node_descriptions) {
                if (desc.name == name) {
                    desc.renderer = renderer;
                    return true;
                }
            }

            return false;
        }

      private:
        std::string name;
        FrameGraphBuilder *builder;
        std::vector<FrameGraphNodeDescription> node_descriptions;
        std::vector<FrameGraphNodeHandle> node_handles;
    };
} // namespace mirai