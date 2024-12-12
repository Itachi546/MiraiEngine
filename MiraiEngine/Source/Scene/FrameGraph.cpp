#include "FrameGraph.hpp"
#include "Engine/Log.hpp"
#include <json.hpp>
#include <fstream>

namespace mirai {
    FrameGraphBuilder::FrameGraphBuilder() : resource_pool_nodes(64, "frame_graph_node"),
                                             resource_pool_resources(512, "frame_graph_resources"),
                                             device(RenderingDevice::get()) {
    }

    void FrameGraphBuilder::get_output_attachment_size(uint32_t *width, uint32_t *height, const FrameGraphResourceOutput *output) {
        if (output->resource_type == FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT) {
            *width = output->width;
            *height = output->height;
        } else if (output->resource_type == FRAMEGRAPH_RESOURCE_TYPE_REFERENCE) {
            auto found = resources_map.find(utils::djb2_hash_string(output->name));
            ASSERT(found != resources_map.end());
            FrameGraphResource *resource = resource_pool_resources.access(found->second);
            *width = resource->resource_info.width;
            *height = resource->resource_info.height;
        }
    }

    void FrameGraphBuilder::add_renderpass_info(Format format, TextureID texture_id, FrameGraphRenderpassInfo &renderpass, const Color &clear_color, AttachmentLoadOp load_op) {
        // Check if input consists of an attachment
        // if such is the case we have to specify it while rendering
        if (is_depth_format(format)) {
            renderpass.depth_attachment_index = (uint32_t)renderpass.attachment_info.size();
            renderpass.has_stencil_attachment = is_stencil_format(format);
        }

        renderpass.attachment_info.push_back(FrameGraphAttachmentInfo{
            .clear_color = clear_color,
            .format = format,
            .load_op = load_op,
            .texture = texture_id,
        });
    }

    void FrameGraphBuilder::create_resource_state(FrameGraphResourceType resource_type, AttachmentLoadOp load_op, FrameGraphResourceState *state, bool is_input_resource) {
        Format format = FORMAT_B8G8R8A8_UNORM;
        if (state->resource_handle != K_INVALID_RESOURCE_HANDLE) {
            FrameGraphResource *resource = resource_pool_resources.access(state->resource_handle);
            format = resource->resource_info.format;
        }

        switch (resource_type) {
        case FRAMEGRAPH_RESOURCE_TYPE_TEXTURE:
            ASSERT(is_input_resource == true);
            state->access_flags |= ACCESS_FLAG_SHADER_READ;
            state->layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            state->stage_mask = PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;
        case FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT:
            if (is_depth_format(format)) {
                state->access_flags = ACCESS_FLAG_DEPTH_STENCIL_ATTACHMENT_WRITE;
                if (load_op == LOAD_OP_LOAD)
                    state->access_flags |= is_input_resource ? ACCESS_FLAG_DEPTH_STENCIL_ATTACHMENT_READ : 0;
                state->stage_mask = PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

                state->layout = is_stencil_format(format) ? IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            } else {
                state->access_flags = ACCESS_FLAG_COLOR_ATTACHMENT_WRITE;
                if (load_op == LOAD_OP_LOAD)
                    state->access_flags |= ACCESS_FLAG_COLOR_ATTACHMENT_READ;
                state->layout = IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                state->stage_mask = PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            }
            break;
            /*
        case FRAMEGRAPH_RESOURCE_TYPE_REFERENCE:
            ASSERT(is_input_resource == false);
            state->access_flags |= ACCESS_FLAG_SHADER_WRITE;
        */
        }
    }

    FrameGraphNodeHandle FrameGraphBuilder::create_node(const FrameGraphNodeDescription &node_description) {
        uint32_t node_index = resource_pool_nodes.obtain();
        FrameGraphNode *node = resource_pool_nodes.access(node_index);
        node->name = node_description.name;
        node->enabled = node_description.enabled;

        FrameGraphRenderpassInfo &renderpass = node->renderpass_info;

        std::unordered_map<std::string, FrameGraphResourceState> resource_state_map;

        for (uint32_t i = 0; i < node_description.inputs.size(); ++i) {
            const FrameGraphResourceInput *input_desc = &node_description.inputs[i];
            FrameGraphResourceHandle resource_handle = create_node_input(input_desc);
            ASSERT(resource_handle != K_INVALID_RESOURCE_HANDLE);

            FrameGraphResource *resource = resource_pool_resources.access(resource_handle);
            Format format = resource->resource_info.format;

            switch (input_desc->resource_type) {
            case FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT:
                add_renderpass_info(format, resource->handle, renderpass, input_desc->load_op);
                break;
            }
            node->inputs.push_back(resource_handle);

            const std::string &resource_name = input_desc->name;
            auto found = resource_state_map.find(resource_name);
            if (found == resource_state_map.end())
                resource_state_map[resource_name] = FrameGraphResourceState{.resource_handle = resource_handle};
            create_resource_state(input_desc->resource_type, input_desc->load_op, &resource_state_map[resource_name], true);
        }

        ASSERT(node_description.outputs.size() > 0);
        uint32_t width = 0;
        uint32_t height = 0;
        get_output_attachment_size(&width, &height, &node_description.outputs[0]);

        for (uint32_t i = 0; i < node_description.outputs.size(); ++i) {
            const FrameGraphResourceOutput *output = &node_description.outputs[i];
            if (output->resource_type == FRAMEGRAPH_RESOURCE_TYPE_EXTERNAL_REFERENCE)
                continue;
#ifdef _DEBUG
            if (i > 0) {
                uint32_t output_width, output_height;
                get_output_attachment_size(&output_width, &output_height, output);
                ASSERT(width == output_width);
                ASSERT(height == output_height);
            }
#endif
            const FrameGraphResourceType &resource_type = output->resource_type;
            FrameGraphResourceHandle resource_handle = create_node_output(output);
            FrameGraphResource *resource = resource_pool_resources.access(resource_handle);
            node->outputs.push_back(resource_handle);

            switch (resource_type) {
            case FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT:
                add_renderpass_info(output->format, resource->handle, renderpass, output->clear_color, output->load_op);
                break;
            }

            const std::string &resource_name = output->name;
            auto found = resource_state_map.find(resource_name);
            if (found == resource_state_map.end())
                resource_state_map[resource_name] = FrameGraphResourceState{.resource_handle = resource_handle};
            create_resource_state(output->resource_type, output->load_op, &resource_state_map[resource_name], false);
        }

        for (auto &entry : resource_state_map)
            node->resources_state.push_back(entry.second);

        node->width = width;
        node->height = height;

        node->renderer = node_description.renderer;

        nodes_maps.insert(std::make_pair(utils::djb2_hash_string(node->name), node_index));

        return FrameGraphNodeHandle{node_index};
    }

    FrameGraphResourceHandle FrameGraphBuilder::create_node_output(const FrameGraphResourceOutput *output) {
        // SamplerDescription sampler_desc = SamplerDescription::create();
        uint32_t handle = K_INVALID_RESOURCE_HANDLE;

        switch (output->resource_type) {
        case FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT: {
            handle = resource_pool_resources.obtain();
            FrameGraphResource *resource = resource_pool_resources.access(handle);
            if (output->name == "swapchain") {
                resource->name = output->name;
                resource->handle = K_SWAPCHAIN_TEXTURE_HANDLE;
                resource->resource_info.format = output->format;
                resources_map.insert(std::make_pair(utils::djb2_hash_string(output->name), handle));
            } else {
                TextureDescription desc = {
                    .width = output->width,
                    .height = output->height,
                    .depth = 1,
                    .mip_levels = 1,
                    .array_layers = 1,
                    .texture_type = TEXTURE_TYPE_2D,
                    .format = output->format,
                    .usage_flags = 0,
                    .sampler_desc = nullptr,
                };

                SamplerDescription sampler = SamplerDescription::create();
                if (is_depth_format(output->format)) {
                    desc.usage_flags = TEXTURE_USAGE_DEPTH_ATTACHMENT_BIT;
                    if (is_stencil_format(output->format))
                        desc.usage_flags |= TEXTURE_USAGE_STENCIL_ATTACHMENT_BIT;
                    else {
                        desc.usage_flags |= TEXTURE_USAGE_SAMPLED_BIT; // if the image is not stencil format then it is most likely to be used as sampler
                    }
                    sampler.min_filter = FILTER_NEAREST;
                    sampler.mag_filter = FILTER_NEAREST;
                } else
                    desc.usage_flags = TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | TEXTURE_USAGE_SAMPLED_BIT;

                if ((desc.usage_flags & TEXTURE_USAGE_SAMPLED_BIT) == TEXTURE_USAGE_SAMPLED_BIT)
                    desc.sampler_desc = &sampler;

                TextureID texture = device->create_texture(&desc, output->name.c_str());
                resource->name = output->name;
                resource->handle = texture;
                resource->resource_info.width = output->width;
                resource->resource_info.height = output->height;
                resource->resource_info.depth = 1;
                resource->resource_info.format = output->format;
                resources_map.insert(std::make_pair(utils::djb2_hash_string(output->name), handle));
            }
            break;
        }
        case FRAMEGRAPH_RESOURCE_TYPE_REFERENCE: {
            auto found = resources_map.find(utils::djb2_hash_string(output->name));
            ASSERT(found != resources_map.end());
            handle = found->second;
            break;
        }
        case FRAMEGRAPH_RESOURCE_TYPE_EXTERNAL_REFERENCE:
            break;
        default:
            ASSERT_MSG(0, "Unknow framegraph output resource type");
            break;
        }
        return FrameGraphResourceHandle{handle};
    }

    FrameGraphResourceHandle FrameGraphBuilder::create_node_input(const FrameGraphResourceInput *input) {
        uint32_t handle = K_INVALID_RESOURCE_HANDLE;
        switch (input->resource_type) {
        case FRAMEGRAPH_RESOURCE_TYPE_TEXTURE:
        case FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT: {
            auto found = resources_map.find(utils::djb2_hash_string(input->name));
            ASSERT(found->second != K_INVALID_ID);
            handle = found->second;
        } break;
        default:
            ASSERT_MSG(0, "Unknown framegraph input attachment");
        }
        return FrameGraphResourceHandle{handle};
    }

    FrameGraphBuilder::~FrameGraphBuilder() {
        for (auto &[key, val] : resources_map) {
            FrameGraphResource *resource = resource_pool_resources.access(val);
            if (resource->handle.is_valid() && resource->handle != K_SWAPCHAIN_TEXTURE_HANDLE) {
                TextureID texture_id = resource->handle;
                device->destroy_textures(&texture_id, 1);
            }
        }
        resource_pool_resources.release_all();
        resource_pool_nodes.release_all();
    }

    static FrameGraphResourceType get_resource_type_from_string(std::string input_type) {
        if (input_type == "attachment")
            return FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT;
        else if (input_type == "texture")
            return FRAMEGRAPH_RESOURCE_TYPE_TEXTURE;
        else if (input_type == "reference")
            return FRAMEGRAPH_RESOURCE_TYPE_REFERENCE;

        ASSERT_MSG(0, "Invalid FrameGraphResourceType");
        return FRAMEGRAPH_RESOURCE_TYPE_INVALID;
    }

    static Format get_texture_format(const std::string &inputFormat) {
        if (inputFormat == "B8G8R8A8_UNORM") {
            return FORMAT_B8G8R8A8_UNORM;
        } else if (inputFormat == "R16G16B16A16_SFLOAT") {
            return FORMAT_R16G16B16A16_SFLOAT;
        } else if (inputFormat == "R16G16B16_SFLOAT") {
            return FORMAT_R16G16B16_SFLOAT;
        } else if (inputFormat == "R32G32B32_SFLOAT") {
            return FORMAT_R32G32B32_SFLOAT;
        } else if (inputFormat == "R32G32B32A32_SFLOAT") {
            return FORMAT_R32G32B32A32_SFLOAT;
        } else if (inputFormat == "D32_SFLOAT") {
            return FORMAT_D32_SFLOAT;
        } else if (inputFormat == "D32_SFLOAT_S8_UINT") {
            return FORMAT_D32_SFLOAT_S8_UINT;
        } else if (inputFormat == "R16_SFLOAT") {
            return FORMAT_R16_SFLOAT;
        } else if (inputFormat == "D24_UNORM_S8_UINT")
            return FORMAT_D24_UNORM_S8_UINT;

        ASSERT(!"Undefined input format");
        return FORMAT_UNDEFINED;
    }

    static AttachmentLoadOp get_attachment_load_op(const std::string &op) {
        if (op == "LOAD_OP_CLEAR")
            return LOAD_OP_CLEAR;
        else if (op == "LOAD_OP_LOAD")
            return LOAD_OP_LOAD;

        return LOAD_OP_DONT_CARE;
    }

    FrameGraph::FrameGraph(FrameGraphBuilder *builder) : builder(builder), name("default_framegraph") {
    }

    void FrameGraph::load_from_file(const std::string &filename) {
        using json = nlohmann::json;
        std::ifstream json_file(filename);
        if (!json_file) {
            Log::Error("Failed to load framegraph: " + filename);
            return;
        }

        Log::Info("Parsing FrameGraph: " + filename);

        // Start parsing frame graph
        json data = json::parse(json_file);

        this->name = data.value("name", "");
        Log::Info("FrameGraph Name: " + name);

        json passes = data["passes"];
        Log::Info("Total passes: " + std::to_string(passes.size()));

        for (std::size_t i = 0; i < passes.size(); ++i) {
            json pass = passes[i];
            bool enabled = pass.value("enabled", false);
            if (!enabled)
                continue;

            FrameGraphNodeDescription node_description;
            node_description.name = pass.value("name", "");
            node_description.is_compute_pass = pass.value("type", "") == "compute" ? true : false;
            node_description.enabled = enabled;

            json inputs = pass["inputs"];
            json outputs = pass["outputs"];

            node_description.inputs.resize(inputs.size());
            node_description.outputs.resize(outputs.size());

            // Parse Inputs for the pass
            for (std::size_t j = 0; j < inputs.size(); ++j) {
                json passInput = inputs[j];
                FrameGraphResourceInput &resource = node_description.inputs[j];
                resource.name = passInput.value("name", "");
                std::string resourceType = passInput.value("type", "");
                resource.load_op = get_attachment_load_op(passInput.value("op", "LOAD_OP_LOAD"));
                resource.resource_type = get_resource_type_from_string(resourceType);
            }

            for (std::size_t j = 0; j < outputs.size(); ++j) {
                json passOutput = outputs[j];
                FrameGraphResourceOutput &resource = node_description.outputs[j];

                resource.name = passOutput.value("name", "");
                resource.load_op = LOAD_OP_CLEAR;

                std::string resourceType = passOutput.value("type", "");
                resource.resource_type = get_resource_type_from_string(resourceType);
                switch (resource.resource_type) {
                case FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT:
                case FRAMEGRAPH_RESOURCE_TYPE_TEXTURE: {
                    if (resource.name == "swapchain") {
                        resource.format = FORMAT_B8G8R8A8_UNORM;
                    } else {
                        bool external = passOutput.value("external", false);
                        if (external) {
                            resource.resource_type = FRAMEGRAPH_RESOURCE_TYPE_EXTERNAL_REFERENCE;
                        } else {
                            json resolution = passOutput["resolution"];
                            resource.width = resolution[0];
                            resource.height = resolution[1];
                            resource.format = get_texture_format(passOutput["format"]);
                            resource.load_op = get_attachment_load_op(passOutput["op"]);

                            json clear_color = passOutput["clear_color"];
                            if (clear_color.size() == 4) {
                                resource.clear_color = {clear_color[0], clear_color[1], clear_color[2], clear_color[3]};
                            } else {
                                if (is_depth_format(resource.format))
                                    resource.clear_color = {1.0f, 0.0f, 0.0f, 1.0f};
                            }
                        }
                    }
                    break;
                }
                }
            }
            node_descriptions.push_back(node_description);
        }
    }

    void FrameGraph::compile() {
        std::unordered_map<FrameGraphResourceHandle, FrameGraphResourceState *> resource_state_map;
        for (uint32_t i = 0; i < node_descriptions.size(); ++i) {
            FrameGraphNodeHandle node_handle = builder->create_node(node_descriptions[i]);
            FrameGraphNode *node = builder->get_node(node_handle);
            ASSERT_MSG(node->renderer != nullptr, "Did you forgot to call set_renderer() before compile?");
            node->renderer->initialize(this, node);
            node_handles.push_back(node_handle);
        }
    }

    void FrameGraph::update(Scene *scene) {
        for (auto handle : node_handles) {
            FrameGraphNode *node = builder->get_node(handle);
            node->renderer->update(this, node, scene);
        }
    }

    void FrameGraph::render(CommandBuffer *command_buffer, Scene *scene) {
        for (auto handle : node_handles) {
            FrameGraphNode *node = builder->get_node(handle);
            node->renderer->render(command_buffer, this, node, scene);
        }
    }
} // namespace mirai