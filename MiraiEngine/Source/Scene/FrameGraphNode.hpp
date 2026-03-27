#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <memory>
#include <vector>
#include <cassert>
#include "Graphics/RenderingDevice.hpp"

namespace mirai {
    constexpr const uint32_t K_INVALID_FRAMEGRAPH_RESOURCE = UINT32_MAX;

    using FrameGraphResourceHandle = uint32_t;
    struct FrameGraphPassResource;

    enum class FrameGraphResourceType {
        Texture = 0,
        Buffer = 1
    };

    struct FrameGraphBuffer {
        BufferID buffer;
        BufferDescription desc;
    };

    struct FrameGraphTexture {
        TextureID texture;
        TextureDescription desc;
    };

    struct GraphNode {
        std::string name;
        uint32_t ref_count;
        GraphNode(const std::string_view name) : name(name), ref_count(0) {}
    };

    struct ResourceNode : public GraphNode {
        // Pass that write to this resource
        uint32_t producer = UINT32_MAX;
        // List of pass that read this resource
        std::vector<uint32_t> read_by;
        std::variant<FrameGraphBuffer, FrameGraphTexture> resource;
        FrameGraphResourceType resource_type;

        ResourceNode(const std::string_view name, std::variant<FrameGraphBuffer, FrameGraphTexture> resource, FrameGraphResourceType resource_type) : GraphNode(name), resource(resource), resource_type(resource_type) {
        }

        template <typename T>
        const T &get() const {
            return std::get<T>(resource);
        }

        template <typename T>
        T &get() {
            return std::get<T>(resource);
        }

        bool is_read_by_pass(uint32_t pass_id) {
            auto found = std::find(read_by.begin(), read_by.end(), pass_id);
            return found != read_by.end();
        }

        bool operator==(const ResourceNode &other) const {
            return resource_type == other.resource_type && resource == other.resource;
        }
    };

    struct FrameGraphPassBase {
        FrameGraphPassBase() = default;
        FrameGraphPassBase(const FrameGraphPassBase &) = delete;
        FrameGraphPassBase(FrameGraphPassBase &&) = delete;

        FrameGraphPassBase &operator=(const FrameGraphPassBase &) = delete;
        FrameGraphPassBase &operator=(FrameGraphPassBase &&) = delete;

        virtual void operator()(FrameGraphPassResource &, void *) = 0;
    };

    template <typename Data, typename Execute>
    struct FrameGraphPass : public FrameGraphPassBase {
        explicit FrameGraphPass(Execute &&exec) : exec_function{std::forward<Execute>(exec)} {
        }

        void operator()(FrameGraphPassResource &resource, void *context) {
            exec_function(data, resource, context);
        }

        Execute exec_function;
        Data data{};
    };

    struct PassNode : public GraphNode {
        uint32_t pass_id;
        std::unique_ptr<FrameGraphPassBase> pass;
        std::vector<FrameGraphResourceHandle> reads;
        std::vector<FrameGraphResourceHandle> writes;
        bool has_side_effect;

        PassNode(const std::string_view name, uint32_t pass_id, std::unique_ptr<FrameGraphPassBase> pass) : GraphNode(name), pass_id(pass_id), pass(std::move(pass)), has_side_effect(false) {
        }

        void _read(FrameGraphResourceHandle resource) {
            assert(!writes_resource(resource));
            auto found = std::find(reads.begin(), reads.end(), resource);
            if (found == reads.end())
                reads.push_back(resource);
        }

        void _write(FrameGraphResourceHandle resource) {
            assert(!reads_resource(resource));
            auto found = std::find(writes.begin(), writes.end(), resource);
            if (found == writes.end())
                writes.push_back(resource);
        }

        bool can_execute() const {
            return ref_count > 0 || has_side_effect;
        }

        bool reads_resource(FrameGraphResourceHandle resource) const {
            auto found = std::find(reads.begin(), reads.end(), resource);
            return found != reads.end();
        }

        bool writes_resource(FrameGraphResourceHandle resource) const {
            auto found = std::find(writes.begin(), writes.end(), resource);
            return found != writes.end();
        }
    };

} // namespace mirai