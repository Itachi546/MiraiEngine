#include "FrameGraph.hpp"
#include <cassert>
#include <stack>
#include <unordered_map>

#include <iostream>

namespace mirai {
    PassNode &FrameGraph::create_pass_node(const std::string_view name, std::unique_ptr<FrameGraphPassBase> pass) {
        uint32_t id = static_cast<uint32_t>(passes.size());
        return passes.emplace_back(name, id, std::move(pass));
    }

    void FrameGraph::compile() {
        const uint32_t INVALID_PRODUCER = UINT32_MAX;
        // Update reference for the passes and resources
        for (uint32_t i = 0; i < passes.size(); ++i) {
            PassNode &pass_node = passes[i];
            pass_node.ref_count = static_cast<uint32_t>(pass_node.writes.size());
            for (auto read : pass_node.reads) {
                resources[read].ref_count++;
            }

            for (auto write : pass_node.writes) {
                assert(resources[write].producer == INVALID_PRODUCER);
                resources[write].producer = i;
            }
        }

        // Cull passes/resources
        std::stack<ResourceNode *> unreferenced_resources;
        for (auto &resource : resources) {
            if (resource.ref_count == 0)
                unreferenced_resources.push(&resource);
        }

        while (!unreferenced_resources.empty()) {
            ResourceNode *resource = unreferenced_resources.top();
            unreferenced_resources.pop();

            uint32_t producer_index = resource->producer;
            PassNode *producer = &passes[producer_index];
            if (producer_index == INVALID_PRODUCER || producer->has_side_effect) {
                continue;
            }

            assert(producer->ref_count >= 1);

            if (--producer->ref_count == 0) {
                for (auto &read : producer->reads) {
                    ResourceNode *referenced = &resources[read];
                    if (--referenced->ref_count == 0)
                        unreferenced_resources.push(referenced);
                }
            }
        }

        // Calculate resource lifetime
        struct ResourceLifetime {
            uint32_t created_by;
            uint32_t last_used_by;
        };

        std::unordered_map<FrameGraphResourceHandle, ResourceLifetime> resources_lifetime;
        for (uint32_t i = 0; i < passes.size(); ++i) {
            PassNode &pass = passes[i];
            if (!pass.can_execute())
                continue;

            for (auto &write : pass.writes) {
                auto found = resources_lifetime.find(write);
                assert(found == resources_lifetime.end());
                resources_lifetime[write].created_by = i;
                resources_lifetime[write].last_used_by = UINT32_MAX;
            }

            for (auto &read : pass.reads) {
                auto found = resources_lifetime.find(read);
                assert(found != resources_lifetime.end());
                resources_lifetime[read].last_used_by = i;
            }
        }

        /*
         // Skip resource aliasing if flag is set
         if (!disable_resource_aliasing) {
             // Create aliases
             std::vector<FrameGraphResourceHandle> expired_resources;
             for (uint32_t i = 1; i < passes.size(); ++i) {
                 PassNode *pass = &passes[i];
                 if (pass->has_side_effect)
                     continue;

                 for (auto &read : pass->reads) {
                     auto found = resources_lifetime.find(read);
                     assert(found != resources_lifetime.end());
                     if (found->second.last_used_by == i)
                         expired_resources.push_back(read);
                 }

                 for (auto &write : pass->writes) {
                     ResourceNode *resource = &resources[write];

                     for (auto expired : expired_resources) {
                         auto found = resources_lifetime.find(expired);
                         assert(found != resources_lifetime.end());
                         if (found->second.last_used_by >= i)
                             continue;

                         if (*resource == resources[expired]) {
                             std::cout << "Resource Aliased: " << resource->name << " uses resource " << resources[expired].name << std::endl;
                             expired_resources.erase(std::remove(expired_resources.begin(), expired_resources.end(), expired), expired_resources.end());
                             break;
                         }
                     }
                 }
             }
         }
         */

        RenderingDevice *device = RenderingDevice::get();
        // Allocate actual resources
        for (auto &resource : resources) {
            std::variant<FrameGraphBuffer, FrameGraphTexture> &raw_resource = resource.resource;
            if (resource.resource_type == FrameGraphResourceType::Buffer) {
                FrameGraphBuffer &buffer = resource.get<FrameGraphBuffer>();
                buffer.buffer = device->create_buffer(&buffer.desc, resource.name);
            } else if (resource.resource_type == FrameGraphResourceType::Texture) {
                FrameGraphTexture &texture = resource.get<FrameGraphTexture>();
                texture.texture = device->create_texture(&texture.desc, resource.name);
            }
        }
    }

    void FrameGraph::execute(void *context) {
        for (const auto &pass_node : passes) {
            if (pass_node.can_execute()) {
                FrameGraphPassResource resource{*this, pass_node};
                std::invoke(*pass_node.pass, resource, context);
            }
        }
    }

    FrameGraphResourceHandle FrameGraph::create_texture(const std::string_view name, const TextureDescription &desc) {
        uint32_t resource_id = static_cast<uint32_t>(resources.size());
        resources.emplace_back(name, FrameGraphTexture{.texture = TextureID{K_INVALID_ID}, .desc = desc}, FrameGraphResourceType::Texture);
        return resource_id;
    }

    FrameGraphResourceHandle FrameGraph::create_buffer(const std::string_view name, const BufferDescription &desc) {
        uint32_t resource_id = static_cast<uint32_t>(resources.size());
        resources.emplace_back(name, FrameGraphBuffer{.buffer = BufferID{K_INVALID_ID}, .desc = desc}, FrameGraphResourceType::Buffer);
        return resource_id;
    }

    FrameGraphResourceHandle FrameGraph::FrameGraphBuilder::create_texture(const std::string_view name, const TextureDescription &desc) {
        return frame_graph->create_texture(name, desc);
    }

    FrameGraphResourceHandle FrameGraph::FrameGraphBuilder::create_buffer(const std::string_view name, const BufferDescription &desc) {
        return frame_graph->create_buffer(name, desc);
    }

    void FrameGraph::FrameGraphBuilder::read(FrameGraphResourceHandle resource) {
        assert(resource < frame_graph->resources.size());
        ResourceNode *resource_node = &frame_graph->resources[resource];

        if (!resource_node->is_read_by_pass(pass_index)) {
            resource_node->read_by.push_back(resource);
        }

        auto &pass = frame_graph->passes[pass_index];
        pass._read(resource);
    }

    void FrameGraph::FrameGraphBuilder::write(FrameGraphResourceHandle resource) {
        assert(resource < frame_graph->resources.size());
        ResourceNode *resource_node = &frame_graph->resources[resource];

        assert(resource_node->producer == UINT32_MAX && "Resource already written by other");
        auto &pass = frame_graph->passes[pass_index];
        pass._write(resource);
    }

    void FrameGraph::FrameGraphBuilder::set_side_effect() {
        frame_graph->passes[pass_index].has_side_effect = true;
    }
} // namespace mirai