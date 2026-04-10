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

    struct FrameGraphBuffer {
        ID id;
        BufferDescription desc;
    };

    struct FrameGraphTexture {
        ID id;
        TextureDescription desc;
    };

    struct FrameGraphAccessDeclaration {
        FrameGraphResourceHandle resource;
        AccessDeclaration access;
    };

    struct GraphNode {
        std::string name;
        uint32_t ref_count;
        GraphNode(const std::string_view name) : name(name), ref_count(0) {}

        virtual ~GraphNode() = default;
    };

    struct ResourceNode : public GraphNode {
        // Pass that write to this resource
        uint32_t producer = UINT32_MAX;
        // List of pass that read this resource
        std::vector<uint32_t> read_by;
        std::variant<FrameGraphBuffer, FrameGraphTexture> resource;
        ResourceType resource_type;
        uint64_t access_flag;

        ResourceNode(const std::string_view name, std::variant<FrameGraphBuffer, FrameGraphTexture> resource, ResourceType resource_type) : GraphNode(name), resource(resource), resource_type(resource_type), access_flag(0) {
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
    };

    struct FrameGraphPassBase {
        FrameGraphPassBase() = default;
        FrameGraphPassBase(const FrameGraphPassBase &) = delete;
        FrameGraphPassBase(FrameGraphPassBase &&) = delete;

        FrameGraphPassBase &operator=(const FrameGraphPassBase &) = delete;
        FrameGraphPassBase &operator=(FrameGraphPassBase &&) = delete;

        virtual void operator()(FrameGraphPassResource &, void *) = 0;

        virtual ~FrameGraphPassBase() = default;
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
        std::vector<FrameGraphAccessDeclaration> reads;
        std::vector<FrameGraphAccessDeclaration> writes;

        bool has_side_effect;
        bool is_compute_pass;

        PassNode(const std::string_view name, uint32_t pass_id, std::unique_ptr<FrameGraphPassBase> pass) : GraphNode(name), pass_id(pass_id), pass(std::move(pass)), has_side_effect(false), is_compute_pass(false) {
        }

        void _read(FrameGraphResourceHandle resource, const AccessDeclaration &access) {
            assert(!writes_resource(resource));
            if (!reads_resource(resource))
                reads.push_back(FrameGraphAccessDeclaration{.resource = resource, .access = access});
        }

        void _write(FrameGraphResourceHandle resource, const AccessDeclaration &access) {
            assert(!reads_resource(resource));
            if (!writes_resource(resource))
                writes.push_back({.resource = resource, .access = access});
        }

        bool can_execute() const {
            return ref_count > 0 || has_side_effect;
        }

        bool reads_resource(FrameGraphResourceHandle resource) const {
            auto found = std::find_if(reads.begin(), reads.end(), [=](const FrameGraphAccessDeclaration &resource_state) {
                return resource_state.resource == resource;
            });
            return found != reads.end();
        }

        bool writes_resource(FrameGraphResourceHandle resource) const {
            auto found = std::find_if(writes.begin(), writes.end(), [=](const FrameGraphAccessDeclaration &resource_state) {
                return resource_state.resource == resource;
            });
            return found != writes.end();
        }
    };

} // namespace mirai