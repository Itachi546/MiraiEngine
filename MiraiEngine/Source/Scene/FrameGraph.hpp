#pragma once

#include <string_view>
#include "FrameGraphNode.hpp"

namespace mirai {

    class FrameGraph;

    struct FrameGraphPassResource {
        FrameGraphPassResource(const FrameGraphPassResource &) = delete;
        FrameGraphPassResource(FrameGraphPassResource &&) = delete;
        FrameGraphPassResource &operator=(const FrameGraphPassResource &) = delete;
        FrameGraphPassResource &operator=(FrameGraphPassResource &&) = delete;

        FrameGraphPassResource(const FrameGraph &frame_graph, const PassNode &pass_node) : frame_graph(frame_graph), pass_node(pass_node) {
        }

        template <typename T>
        const T &get(FrameGraphResourceHandle handle) const;

        std::vector<ResourceAccessDeclaration> get_resource_access_states() const;

      private:
        const FrameGraph &frame_graph;
        const PassNode &pass_node;
    };

    class FrameGraph {
      public:
        FrameGraph() = default;
        FrameGraph(const FrameGraph &) = delete;
        FrameGraph(FrameGraph &&) = delete;
        FrameGraph &operator=(const FrameGraph &) = delete;
        FrameGraph &operator=(FrameGraph &&) = delete;

        struct NoData {};
        template <typename Data = NoData, typename Setup, typename Execute>
        const Data &add_callback_pass(const std::string_view name, Setup &&setup, Execute &&exec);

        PassNode &create_pass_node(const std::string_view name, std::unique_ptr<FrameGraphPassBase> pass);

        void set_disable_resource_aliasing(bool value) {
            disable_resource_aliasing = value;
        }

        void compile();

        void execute(void *context);

        FrameGraphResourceHandle create_texture(const std::string_view name, const TextureDescription &desc);
        FrameGraphResourceHandle create_buffer(const std::string_view name, const BufferDescription &desc);

        struct FrameGraphBuilder {
            FrameGraphBuilder(FrameGraph *frame_graph, uint32_t pass_index) : frame_graph(frame_graph), pass_index(pass_index) {}

            FrameGraphResourceHandle create_texture(const std::string_view name, const TextureDescription &desc);
            FrameGraphResourceHandle create_buffer(const std::string_view name, const BufferDescription &desc);
            void read(FrameGraphResourceHandle resource, const AccessDeclaration &access);
            void write(FrameGraphResourceHandle resource, const AccessDeclaration &access);
            void set_side_effect();
            void set_compute_pass();

            void present(FrameGraphResourceHandle resource);

            FrameGraph *frame_graph;
            uint32_t pass_index;
        };

        TextureID get_present_texture() const {
            const ResourceNode &resource = resources[present_texture];
            return resource.get<FrameGraphTexture>().id;
        }

        const TextureDescription &get_texture_description(FrameGraphResourceHandle resource_id) const {
            const ResourceNode &resource = resources[resource_id];
            ASSERT(resource.resource_type == ResourceType::Texture);
            return resource.get<FrameGraphTexture>().desc;
        }

        ~FrameGraph();

      private:
        friend struct FrameGraphPassResource;
        friend class CommandBuffer;
        bool disable_resource_aliasing = false;
        std::vector<ResourceNode> resources;
        std::vector<PassNode> passes;
        FrameGraphResourceHandle present_texture = UINT32_MAX;
    };

    template <typename Data, typename Setup, typename Execute>
    inline const Data &FrameGraph::add_callback_pass(const std::string_view name, Setup &&setup, Execute &&exec) {
        static_assert(std::is_invocable_v<Setup, FrameGraphBuilder &, Data &>, "Invalid Callback setup");
        static_assert(std::is_invocable_v<Execute, const Data &, FrameGraphPassResource &, void *>, "Invalid execute callback");
        static_assert(sizeof(Execute) < 2024, "Execute capture too much data");

        auto &pass_node = create_pass_node(name, std::make_unique<FrameGraphPass<Data, Execute>>(std::forward<Execute>(exec)));

        FrameGraphBuilder builder{
            this,
            static_cast<uint32_t>(passes.size() - 1),
        };

        Data &data = static_cast<FrameGraphPass<Data, Execute> *>(pass_node.pass.get())->data;
        std::invoke(setup, builder, data);
        return data;
    }
    template <typename T>
    inline const T &FrameGraphPassResource::get(FrameGraphResourceHandle handle) const {
        assert(pass_node.reads_resource(handle) || pass_node.writes_resource(handle));
        const ResourceNode *resource = &frame_graph.resources[handle];
        return resource->get<T>();
    }
} // namespace mirai