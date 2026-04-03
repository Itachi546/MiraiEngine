#include "FrameGraph.hpp"
#include "Common/HashMap.hpp"

#include <cassert>
#include <stack>

#include <iostream>

namespace mirai {
    PassNode &FrameGraph::create_pass_node(const std::string_view name, std::unique_ptr<FrameGraphPassBase> pass) {
        uint32_t id = static_cast<uint32_t>(passes.size());
        return passes.emplace_back(name, id, std::move(pass));
    }

    void FrameGraph::compile() {
        const uint32_t INVALID_PRODUCER = UINT32_MAX;

        // Update reference for the passes and resources and it's access flag
        for (uint32_t i = 0; i < passes.size(); ++i) {
            PassNode &pass_node = passes[i];
            pass_node.ref_count = static_cast<uint32_t>(pass_node.writes.size());
            for (auto read : pass_node.reads) {
                resources[read.resource].ref_count++;
                resources[read.resource].access_flag |= read.access.access_flags;
            }

            for (auto write : pass_node.writes) {
                resources[write.resource].producer = i;
                resources[write.resource].access_flag |= write.access.access_flags;
            }
        }

        // Present texture should also update reference
        resources[present_texture].ref_count += 1;
        resources[present_texture].access_flag |= ACCESS_FLAG_TRANSFER_READ;

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
                    ResourceNode *referenced = &resources[read.resource];
                    if (--referenced->ref_count == 0)
                        unreferenced_resources.push(referenced);
                }
            }
        }

        // Calculate resource lifetime and state
        struct ResourceLifetime {
            uint32_t created_by;
            uint32_t last_used_by;
        };

        HashMap<FrameGraphResourceHandle, ResourceLifetime> resources_lifetime;
        for (uint32_t i = 0; i < passes.size(); ++i) {
            PassNode &pass = passes[i];

            if (!pass.can_execute())
                continue;

            for (auto &write : pass.writes) {
                FrameGraphResourceHandle resource = write.resource;
                auto found = resources_lifetime.find(resource);
                // If this is not the first write, then update only last_used_by field
                if (found != resources_lifetime.end()) {
                    resources_lifetime[resource].last_used_by = i;
                } else {
                    // On first write, we update the create_by and initialize last_used_by field
                    resources_lifetime[resource].created_by = i;
                    resources_lifetime[resource].last_used_by = UINT32_MAX;
                }
            }

            for (auto &read : pass.reads) {
                auto found = resources_lifetime.find(read.resource);
                assert(found != resources_lifetime.end());
                resources_lifetime[read.resource].last_used_by = i;
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
            if (resource.resource_type == ResourceType::Buffer) {
                FrameGraphBuffer &buffer = resource.get<FrameGraphBuffer>();
                ASSERT(!buffer.id.is_valid());
                buffer.id = device->create_buffer(&buffer.desc, resource.name);
            } else if (resource.resource_type == ResourceType::Texture) {
                FrameGraphTexture &texture = resource.get<FrameGraphTexture>();
                ASSERT(!texture.id.is_valid());
                texture.id = device->create_texture(&texture.desc, resource.name);
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
        resources.emplace_back(name, FrameGraphTexture{.id = TextureID{K_INVALID_ID}, .desc = desc}, ResourceType::Texture);
        return resource_id;
    }

    FrameGraphResourceHandle FrameGraph::create_buffer(const std::string_view name, const BufferDescription &desc) {
        uint32_t resource_id = static_cast<uint32_t>(resources.size());
        resources.emplace_back(name, FrameGraphBuffer{.id = BufferID{K_INVALID_ID}, .desc = desc}, ResourceType::Buffer);
        return resource_id;
    }

    FrameGraph::~FrameGraph() {
        std::vector<BufferID> buffers;
        std::vector<TextureID> textures;
        for (auto &resource : resources) {
            ID id = std::visit([](const auto &d) { return d.id; }, resource.resource);
            if (resource.resource_type == ResourceType::Buffer) {
                buffers.push_back(id);
            } else {
                textures.push_back(id);
            }
        }

        RenderingDevice *device = RenderingDevice::get();
        device->destroy_buffers(buffers.data(), cast_u32(buffers.size()));
        device->destroy_textures(textures.data(), cast_u32(textures.size()));
    }

    FrameGraphResourceHandle FrameGraph::FrameGraphBuilder::create_texture(const std::string_view name, const TextureDescription &desc) {
        return frame_graph->create_texture(name, desc);
    }

    FrameGraphResourceHandle FrameGraph::FrameGraphBuilder::create_buffer(const std::string_view name, const BufferDescription &desc) {
        return frame_graph->create_buffer(name, desc);
    }

    void FrameGraph::FrameGraphBuilder::read(FrameGraphResourceHandle resource, const AccessDeclaration &access) {
        assert(resource < frame_graph->resources.size());
        ResourceNode *resource_node = &frame_graph->resources[resource];

        if (!resource_node->is_read_by_pass(pass_index)) {
            resource_node->read_by.push_back(resource);
        }

        auto &pass = frame_graph->passes[pass_index];
        pass._read(resource, access);
    }

    void FrameGraph::FrameGraphBuilder::write(FrameGraphResourceHandle resource, const AccessDeclaration &access) {
        assert(resource < frame_graph->resources.size());
        ResourceNode *resource_node = &frame_graph->resources[resource];

        assert(resource_node->producer == UINT32_MAX && "Resource already written by other");
        auto &pass = frame_graph->passes[pass_index];
        pass._write(resource, access);
    }

    void FrameGraph::FrameGraphBuilder::set_side_effect() {
        frame_graph->passes[pass_index].has_side_effect = true;
    }
    void FrameGraph::FrameGraphBuilder::set_compute_pass() {
        frame_graph->passes[pass_index].is_compute_pass = true;
    }

    void FrameGraph::FrameGraphBuilder::present(FrameGraphResourceHandle resource) {
        ASSERT_MSG(frame_graph->present_texture == UINT32_MAX, "Present texture is already assigned");
        frame_graph->present_texture = resource;
    }

    std::vector<ResourceAccessDeclaration> FrameGraphPassResource::get_resource_access_states() const {
        uint32_t total_writes = cast_u32(pass_node.writes.size());
        uint32_t total_reads = cast_u32(pass_node.reads.size());

        std::vector<ResourceAccessDeclaration> result(total_reads + total_writes);

        for (uint32_t i = 0; i < total_writes; ++i) {
            const FrameGraphAccessDeclaration &write = pass_node.writes[i];
            const ResourceNode *resource = &frame_graph.resources[write.resource];
            result[i].resource = std::visit([](const auto &d) { return d.id; }, resource->resource);
            result[i].resource_type = resource->resource_type;
            result[i].declaration = &write.access;
        }

        for (uint32_t i = 0; i < total_reads; ++i) {
            const FrameGraphAccessDeclaration &read = pass_node.reads[i];
            const ResourceNode *resource = &frame_graph.resources[read.resource];

            result[total_writes + i].resource = std::visit([](const auto &d) { return d.id; }, resource->resource);
            result[total_writes + i].resource_type = resource->resource_type;
            result[total_writes + i].declaration = &read.access;
        }
        return result;
    }

} // namespace mirai