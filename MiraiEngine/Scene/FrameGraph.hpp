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
    };

    class FrameGraphRenderPass {
      public:
        FrameGraphRenderPass(const std::string &name) : name(name) {}

        virtual void initialize(FrameGraph *frame_graph, const FrameGraphNode *node) {}

        virtual void update(FrameGraph *frame_graph, const FrameGraphNode *node) {}

        virtual void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, const FrameGraphNode *node, Scene *scene) = 0;

        void set_size(uint32_t width, uint32_t height) {
            this->width = width;
            this->height = height;
        }

        uint32_t get_width() const { return width; }
        uint32_t get_height() const { return height; }

      protected:
        std::string name;
        uint32_t width, height;
    };

    struct FrameGraphNodeDescription {
        std::string name;
        bool enabled;
        std::vector<FrameGraphResourceInput> inputs;
        std::vector<FrameGraphResourceOutput> outputs;
        std::shared_ptr<FrameGraphRenderPass> renderer;
    };

    struct FrameGraphAttachmentInfo {
        Color clear_color;
        Format format;
        AttachmentLoadOp load_op;
    };

    struct FrameGraphRenderingInfo {
        std::vector<FrameGraphAttachmentInfo> attachment_info;
        uint32_t depth_attachment_index = ~0u;
        bool has_stencil_attachment = false;
    };

    struct FrameGraphResource {
        FrameGraphResourceType resource_type;
        TextureID texture;
    };

    using FrameGraphResourceHandle = uint32_t;

    struct FrameGraphNode {
        std::string name;
        bool enabled;

        std::vector<FrameGraphResourceHandle> inputs;
        std::vector<FrameGraphResourceHandle> outputs;
        std::shared_ptr<FrameGraphRenderPass> renderer;
        FrameGraphRenderingInfo rendering_info;
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

      private:
        FrameGraphBuilder *builder;
        std::vector<FrameGraphNodeDescription> node_descriptions;
        std::vector<FrameGraphNodeHandle> node_handles;
    };
} // namespace mirai