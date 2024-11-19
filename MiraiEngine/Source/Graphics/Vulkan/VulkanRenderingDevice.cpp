#include "VulkanRenderingDevice.hpp"

#include "Instance.hpp"
#include "Device.hpp"
#include "Swapchain.h"
#include "CommandBuffer.hpp"
#include "VulkanUtils.hpp"
#include "Common/Hash.hpp"

#define VMA_IMPLEMENTATION
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vma/vk_mem_alloc.h>

#include <unordered_map>
#include <algorithm>

namespace mirai {
    void VulkanRenderingDevice::set_debug_marker_object_name(VkObjectType objectType, uint64_t handle, const char *objectName) {
        if (!enable_validation)
            return;

        VkDebugUtilsObjectNameInfoEXT name_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .objectType = objectType,
            .objectHandle = handle,
            .pObjectName = objectName,
        };

        VK_CHECK(vkSetDebugUtilsObjectNameEXT(device, &name_info));
    }

    VulkanRenderingDevice::VulkanRenderingDevice(bool enable_validation) : resource_pool_pipelines(128, "Pipeline"),
                                                                           resource_pool_shaders(32, "Shader"),
                                                                           resource_pool_textures(1024, "Texture"),
                                                                           resource_pool_buffers(256, "Buffer"),
                                                                           resource_pool_uniform_sets(256, "UniformSet"),
                                                                           RenderingDevice(enable_validation) {
        instance_extensions = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME,
#ifdef MIRAI_PLATFORM_WINDOW
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#endif
        };

        device_extensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        };

        validation_layers = {
            "VK_LAYER_KHRONOS_validation",
            "VK_LAYER_KHRONOS_synchronization2",
        };

        if (enable_validation) {
            instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        instance = CreateInstance(validation_layers, instance_extensions, enable_validation);
        volkLoadInstance(instance);

        if (enable_validation)
            debug_utils_messenger = CreateDebugUtilMessanger(instance);
        else
            debug_utils_messenger = VK_NULL_HANDLE;

        physical_device = SelectPhysicalDevice(instance, gpus, device_extensions);

        GetDeviceQueueFamilies(physical_device, queue_family_indices);

        uint32_t graphics_queue = queue_family_indices[QUEUE_TYPE_GRAPHICS];
        if (!PhysicalDeviceSupportPresentation(instance, physical_device, graphics_queue))
            Log::Fatal("VULKAN::Selected Physical Device Doesn't Support Presentation!!!");

        device = CreateDevice(instance, physical_device, queue_family_indices, device_extensions);

        vma_allocator = create_allocator();

        device_queues.resize(queue_family_indices.size());
        vkGetDeviceQueue(device, graphics_queue, 0, &device_queues[QUEUE_TYPE_GRAPHICS]);

        uint32_t transfer_queue = queue_family_indices[QUEUE_TYPE_TRANSFER];
        if (transfer_queue != K_INVALID_QUEUE_ID)
            vkGetDeviceQueue(device, transfer_queue, 0, &device_queues[QUEUE_TYPE_TRANSFER]);

        uint32_t compute_queue = queue_family_indices[QUEUE_TYPE_COMPUTE];
        if (compute_queue != K_INVALID_QUEUE_ID)
            vkGetDeviceQueue(device, compute_queue, 0, &device_queues[QUEUE_TYPE_COMPUTE]);

        surface = CreateSurface(instance, physical_device, graphics_queue);

        swapchain = std::make_unique<VulkanSwapchain>();
        swapchain->swapchain = VK_NULL_HANDLE;
        CreateSwapchain(swapchain.get(), physical_device, device, surface, vsync);
        for (uint32_t i = 0; i < swapchain->image_count; ++i) {
            std::string image_name = "swapchain_image_" + std::to_string(i);
            set_debug_marker_object_name(VK_OBJECT_TYPE_IMAGE, (uint64_t)swapchain->images[i], image_name.c_str());

            std::string image_view_name = "swapchain_image_view" + std::to_string(i);
            set_debug_marker_object_name(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)swapchain->image_views[i], image_view_name.c_str());
        }

        // Initialize CommandPool and CommandBuffer
        uint32_t pool_counts = K_MAX_FRAME_IN_FLIGHTS * K_NUM_THREAD;

        VkCommandPoolCreateInfo command_pool_create_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = graphics_queue,
        };
        command_pools.resize(pool_counts);

        for (uint32_t i = 0; i < pool_counts; ++i)
            VK_CHECK(vkCreateCommandPool(device, &command_pool_create_info, nullptr, &command_pools[i]));

        uint32_t buffer_counts = pool_counts * K_NUM_COMMAND_BUFFER_PER_THREAD;

        VkCommandBufferAllocateInfo command_buffer_allocate_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        for (uint32_t i = 0; i < buffer_counts; ++i) {
            uint32_t command_pool_index = i / K_NUM_COMMAND_BUFFER_PER_THREAD;
            VkCommandPool command_pool = command_pools[command_pool_index];

            command_buffer_allocate_info.commandPool = command_pool;

            auto &command_buffer = command_buffers.emplace_back(std::make_unique<CommandBuffer>());
            command_buffer->queue_family_indices = graphics_queue;

            VK_CHECK(vkAllocateCommandBuffers(device, &command_buffer_allocate_info, &command_buffer->command_buffer));

            command_buffer->fence = create_fence("command_buffer_fence");
        }

        for (uint32_t i = 0; i < K_MAX_FRAME_IN_FLIGHTS; ++i) {
            std::string index = std::to_string(i);
            image_acquire_semaphore[i] = create_semaphore("image_acquire_semaphore" + index);
            render_finished_semaphore[i] = create_semaphore("render_finished_semaphore" + index);
            in_flight_fences[i] = create_fence("in_flight_fence" + index, true);
        }

        descriptor_pools.push_back(create_descriptor_pool(0));

        // Create Bindless descriptor set
        VkDescriptorPoolSize pools[] = {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, K_MAX_BINDLESS_RESOURCE},
        };

        bindless_descriptor_pool = create_descriptor_pool(VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT, pools, 1, 1);

        VkDescriptorSetLayoutBinding bindless_binding = {
            .binding = K_BINDLESS_TEXTURE_BINDING,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = K_MAX_BINDLESS_RESOURCE,
            .stageFlags = VK_SHADER_STAGE_ALL,
        };

        VkDescriptorBindingFlags bindless_flag = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                                                 VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
                                                 VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

        VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flag = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            .bindingCount = 1,
            .pBindingFlags = &bindless_flag,
        };
        bindless_descriptor_layout = CreateDescriptorSetLayout(device, &bindless_binding, 1, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT, &binding_flag);

        uint32_t max_binding = K_MAX_BINDLESS_RESOURCE - 1;
        VkDescriptorSetVariableDescriptorCountAllocateInfo count_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
            .descriptorSetCount = 1,
            .pDescriptorCounts = &max_binding,
        };

        VkDescriptorSetAllocateInfo bindless_set_allocate_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = &count_info,
            .descriptorPool = bindless_descriptor_pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &bindless_descriptor_layout,
        };

        VK_CHECK(vkAllocateDescriptorSets(device, &bindless_set_allocate_info, &bindless_descriptor_set));
    }

    VkDescriptorPool VulkanRenderingDevice::create_descriptor_pool(VkDescriptorPoolCreateFlags create_flags, VkDescriptorPoolSize *pools, uint32_t pool_count, uint32_t max_sets) {
        VkDescriptorPoolSize default_pools[] = {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 512},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 32},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 32},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 32},
        };

        uint32_t maxSets = pool_count > 0 ? max_sets : 512;
        VkDescriptorPoolCreateInfo descriptor_pool_create_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags = create_flags,
            .maxSets = maxSets,
            .poolSizeCount = pool_count == 0 ? (uint32_t)std::size(default_pools) : pool_count,
            .pPoolSizes = pool_count == 0 ? default_pools : pools,
        };

        VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
        VK_CHECK(vkCreateDescriptorPool(device, &descriptor_pool_create_info, nullptr, &descriptor_pool));
        return descriptor_pool;
    }

    VkSemaphore VulkanRenderingDevice::create_semaphore(const std::string &name) {
        VkSemaphoreCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };

        VkSemaphore semaphore = VK_NULL_HANDLE;
        VK_CHECK(vkCreateSemaphore(device, &create_info, nullptr, &semaphore));

        set_debug_marker_object_name(VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)semaphore, name.c_str());
        return semaphore;
    }

    VmaAllocator VulkanRenderingDevice::create_allocator() {
        VmaVulkanFunctions vulkan_functions = {};
        vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo create_info = {
            .physicalDevice = physical_device,
            .device = device,
            .pVulkanFunctions = &vulkan_functions,
            .instance = instance,
            .vulkanApiVersion = VULKAN_API_VERSION};

        VmaAllocator allocator = VK_NULL_HANDLE;
        VK_CHECK(vmaCreateAllocator(&create_info, &allocator));
        return allocator;
    }

    VkFence VulkanRenderingDevice::create_fence(const std::string &name, bool signalled) {
        VkFenceCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = signalled ? VK_FENCE_CREATE_SIGNALED_BIT : VkFenceCreateFlags{0},
        };
        VkFence fence = VK_NULL_HANDLE;
        VK_CHECK(vkCreateFence(device, &create_info, nullptr, &fence));
        set_debug_marker_object_name(VK_OBJECT_TYPE_FENCE, (uint64_t)fence, name.c_str());
        return fence;
    }

    VkSampler VulkanRenderingDevice::create_sampler(SamplerDescription *desc) {
        VkSamplerCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = nullptr,
            .magFilter = VkFilter(desc->min_filter),
            .minFilter = VkFilter(desc->mag_filter),
            .mipmapMode = VkSamplerMipmapMode(desc->mipmap_mode),
            .addressModeU = VkSamplerAddressMode(desc->address_mode_u),
            .addressModeV = VkSamplerAddressMode(desc->address_mode_v),
            .addressModeW = VkSamplerAddressMode(desc->address_mode_v),
            .mipLodBias = desc->lod_bias,
            .anisotropyEnable = desc->enable_anisotropy,
            .maxAnisotropy = desc->max_anisotropy,
            .minLod = desc->min_lod,
            .maxLod = desc->max_lod,
        };

        VkSampler sampler = VK_NULL_HANDLE;
        VK_CHECK(vkCreateSampler(device, &createInfo, nullptr, &sampler));
        return sampler;
    }

    ShaderID VulkanRenderingDevice::create_shader(uint32_t *code, uint32_t code_size_in_bytes, const std::string &debug_name) {
        uint32_t shader_id = resource_pool_shaders.obtain();
        VulkanShader *shader = resource_pool_shaders.access(shader_id);
        shader->support_bindless_texture = false;
        CreateShader(shader, device, code, code_size_in_bytes);
        set_debug_marker_object_name(VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)shader->shader, debug_name.c_str());
        return ShaderID{shader_id};
    }

    PipelineID VulkanRenderingDevice::create_graphics_pipeline(PipelineDescription *pipeline_description, const std::string &debug_name) {
        std::vector<VkPipelineShaderStageCreateInfo> shader_stage_create_infos(pipeline_description->shader_count);
        std::unordered_map<uint32_t, std::vector<VkReflectionDescriptorBinding>> descriptor_sets_map;
        std::unordered_map<uint32_t, VkPushConstantRange> push_constants_map;
        bool support_bindless_texture = false;
        for (uint32_t i = 0; i < pipeline_description->shader_count; ++i) {
            VulkanShader *shader = resource_pool_shaders.access(pipeline_description->shaders[i]);
            shader_stage_create_infos[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shader_stage_create_infos[i].module = shader->shader;
            shader_stage_create_infos[i].stage = shader->shader_stage;
            shader_stage_create_infos[i].pName = "main";

            if (shader->descriptor_sets.size() > 0) {
                for (auto &set : shader->descriptor_sets) {
                    auto found = descriptor_sets_map.find(set.set);
                    if (found != descriptor_sets_map.end()) {
                        // Merge the bindings if the binding index is same
                        MergeShaderBindings(found->second, set.bindings);
                    } else
                        descriptor_sets_map.insert(std::make_pair(set.set, set.bindings));
                }
            }
            MergePushConstants(push_constants_map, shader->push_constants);

            if (shader->support_bindless_texture) {
                ASSERT(shader->shader_stage == VK_SHADER_STAGE_FRAGMENT_BIT);
                support_bindless_texture = true;
            }
        }

        VkPipelineViewportStateCreateInfo viewport_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
        };

        VkPipelineRasterizationStateCreateInfo rasterization_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .depthClampEnable = pipeline_description->rasterization_state->enable_depth_clamp,
            .polygonMode = (VkPolygonMode)pipeline_description->rasterization_state->polygon_mode,
            .cullMode = (VkCullModeFlags)pipeline_description->rasterization_state->cull_mode,
            .frontFace = (VkFrontFace)pipeline_description->rasterization_state->front_face,
            .depthBiasEnable = false,
            .lineWidth = pipeline_description->rasterization_state->line_width,
        };

        VkPipelineMultisampleStateCreateInfo multisample_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        };

        VkPipelineDepthStencilStateCreateInfo depth_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = pipeline_description->depth_state->enable_depth_test,
            .depthWriteEnable = pipeline_description->depth_state->enable_depth_write,
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
            .stencilTestEnable = false,
            .minDepthBounds = pipeline_description->depth_state->min_depth_bounds,
            .maxDepthBounds = pipeline_description->depth_state->max_depth_bounds,
        };

        uint32_t attachment_count = pipeline_description->color_attachment_count;

        std::vector<VkPipelineColorBlendAttachmentState> attachment_blend_states(attachment_count);
        std::vector<VkFormat> color_attachment_formats(attachment_count);

        for (uint32_t i = 0; i < attachment_count; ++i) {
            attachment_blend_states[i].blendEnable = pipeline_description->blend_state->enable;
            if (pipeline_description->blend_state->enable) {
                attachment_blend_states[i].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                attachment_blend_states[i].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                attachment_blend_states[i].colorBlendOp = VK_BLEND_OP_ADD;
            }
            attachment_blend_states[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

            color_attachment_formats[i] = RD_FORMAT_TO_VK_FORMAT[pipeline_description->color_attachment_formats[i]];
        };

        VkFormat depth_format = RD_FORMAT_TO_VK_FORMAT[pipeline_description->depth_attachment_format];
        VkFormat stencil_format = is_stencil_format(pipeline_description->depth_attachment_format) ? depth_format : VK_FORMAT_UNDEFINED;

        VkPipelineRenderingCreateInfo rendering_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = attachment_count,
            .pColorAttachmentFormats = color_attachment_formats.data(),
            .depthAttachmentFormat = depth_format,
            .stencilAttachmentFormat = stencil_format,
        };

        VkPipelineVertexInputStateCreateInfo vertex_input_state = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkVertexInputBindingDescription binding_description = {};
        std::vector<VkVertexInputAttributeDescription> attribute_descriptions;

        VertexBindingDescription *vertex_binding_description = pipeline_description->vertex_description;
        if (vertex_binding_description != nullptr) {
            attribute_descriptions.resize(vertex_binding_description->attribute_count);
            binding_description = {
                .binding = vertex_binding_description->binding,
                .stride = vertex_binding_description->stride,
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            };

            for (uint32_t attribute = 0; attribute < vertex_binding_description->attribute_count; ++attribute) {
                VertexAttributeDescription &attribute_desc = vertex_binding_description->attributes[attribute];
                attribute_descriptions[attribute].binding = attribute_desc.binding;
                attribute_descriptions[attribute].format = RD_FORMAT_TO_VK_FORMAT[attribute_desc.format];
                attribute_descriptions[attribute].location = attribute_desc.format;
                attribute_descriptions[attribute].offset = attribute_desc.offset;
            }

            vertex_input_state.pVertexBindingDescriptions = &binding_description;
            vertex_input_state.pVertexAttributeDescriptions = attribute_descriptions.data();
            vertex_input_state.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
            vertex_input_state.vertexBindingDescriptionCount = 1;
        }

        VkPipelineInputAssemblyStateCreateInfo input_assembly_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VkPrimitiveTopology(pipeline_description->topology),
        };

        VkPipelineColorBlendStateCreateInfo blend_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = attachment_count,
            .pAttachments = attachment_blend_states.data(),
        };

        VkDynamicState dynamic_states[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };

        VkPipelineDynamicStateCreateInfo dynamic_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = 2,
            .pDynamicStates = dynamic_states,
        };

        uint32_t pipeline_id = resource_pool_pipelines.obtain();
        VulkanPipeline *pipeline = resource_pool_pipelines.access(pipeline_id);
        pipeline->bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
        pipeline->support_bindless_texture = support_bindless_texture;

        for (const auto &[key, val] : descriptor_sets_map) {
            uint64_t hash = GetDescriptorSetLayoutHash(val, key);
            auto found = descriptor_set_layouts_cache.find(hash);

            if (found == descriptor_set_layouts_cache.end()) {

                uint32_t binding_count = static_cast<uint32_t>(val.size());
                std::vector<VkDescriptorSetLayoutBinding> bindings(binding_count);
                for (uint32_t b = 0; b < val.size(); ++b) {
                    bindings[b].binding = val[b].binding;
                    bindings[b].descriptorCount = 1;
                    bindings[b].descriptorType = val[b].descriptor_type;
                    bindings[b].stageFlags = val[b].shader_stage;
                }
                VkDescriptorSetLayout set_layout = CreateDescriptorSetLayout(device, bindings.data(), binding_count, 0, nullptr);
                pipeline->set_layouts.push_back(set_layout);
                descriptor_set_layouts_cache.insert(std::make_pair(hash, set_layout));
            } else
                pipeline->set_layouts.push_back(found->second);
        }

        std::vector<VkDescriptorSetLayout> set_layouts = pipeline->set_layouts;
        if (support_bindless_texture)
            set_layouts.insert(set_layouts.begin() + K_BINDLESS_TEXTURE_SET, bindless_descriptor_layout);

        std::vector<VkPushConstantRange> push_constants;
        for (const auto &entry : push_constants_map)
            push_constants.push_back(entry.second);

        VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = static_cast<uint32_t>(set_layouts.size()),
            .pSetLayouts = set_layouts.data(),
            .pushConstantRangeCount = static_cast<uint32_t>(push_constants.size()),
            .pPushConstantRanges = push_constants.data(),
        };

        VK_CHECK(vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &pipeline->pipeline_layout));

        VkGraphicsPipelineCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = &rendering_info,
            .stageCount = pipeline_description->shader_count,
            .pStages = shader_stage_create_infos.data(),
            .pVertexInputState = &vertex_input_state,
            .pInputAssemblyState = &input_assembly_state,
            .pTessellationState = nullptr,
            .pViewportState = &viewport_state,
            .pRasterizationState = &rasterization_state,
            .pMultisampleState = &multisample_state,
            .pDepthStencilState = &depth_state,
            .pColorBlendState = &blend_state,
            .pDynamicState = &dynamic_state,
            .layout = pipeline->pipeline_layout,
        };

        VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &create_info, nullptr, &pipeline->pipeline));
        set_debug_marker_object_name(VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipeline->pipeline, debug_name.c_str());

        return PipelineID{pipeline_id};
    }

    UniformSetID VulkanRenderingDevice::create_uniform_set(UniformLayout *uniforms, uint32_t uniform_count, uint32_t set, const std::string &debug_name) {
        uint64_t hash = GetDescriptorSetLayoutHash(uniforms, uniform_count, set);
        auto found = descriptor_set_layouts_cache.find(hash);

        VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
        if (found == descriptor_set_layouts_cache.end()) {
            std::vector<VkDescriptorSetLayoutBinding> bindings(uniform_count);
            for (uint32_t i = 0; i < uniform_count; ++i) {
                bindings[i].binding = uniforms[i].binding;
                bindings[i].descriptorType = VkDescriptorType(uniforms[i].binding_type);
                bindings[i].descriptorCount = 1;
                bindings[i].stageFlags = VkShaderStageFlags(uniforms[i].shader_stage);
            }
            // Create DescriptorSetLayout
            set_layout = CreateDescriptorSetLayout(device, bindings.data(), uniform_count, 0, nullptr);
            descriptor_set_layouts_cache.insert(std::make_pair(hash, set_layout));
        } else
            set_layout = found->second;
        VkDescriptorSetAllocateInfo allocate_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = descriptor_pools.back(),
            .descriptorSetCount = 1,
            .pSetLayouts = &set_layout,
        };

        VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
        if (!vkAllocateDescriptorSets(device, &allocate_info, &descriptor_set)) {
            descriptor_pools.push_back(create_descriptor_pool(0));
            allocate_info.descriptorPool = descriptor_pools.back();
            VK_CHECK(vkAllocateDescriptorSets(device, &allocate_info, &descriptor_set));
        }

        if (debug_name.size() > 0)
            set_debug_marker_object_name(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)descriptor_set, debug_name.c_str());

        uint32_t id = resource_pool_uniform_sets.obtain();
        VulkanUniformSet *uniform_set = resource_pool_uniform_sets.access(id);
        uniform_set->descriptor_pool = descriptor_pools.back();
        uniform_set->descriptor_set = descriptor_set;
        uniform_set->set_id = set;
        uniform_set->uniform_layout.insert(uniform_set->uniform_layout.end(), uniforms, uniforms + uniform_count);
        return UniformSetID{id};
    }

    void VulkanRenderingDevice::update_uniform_set(UniformSetID uniform_set, UniformBinding *bindings, uint32_t binding_count) {
        std::vector<VkWriteDescriptorSet> write_sets(binding_count);
        // @TODO replace with custom allocator
        std::vector<VkDescriptorImageInfo> image_infos;
        std::vector<VkDescriptorBufferInfo> buffer_infos;
        image_infos.reserve(16), buffer_infos.reserve(16);

        VulkanUniformSet *vk_set = resource_pool_uniform_sets.access(uniform_set);
        for (uint32_t i = 0; i < binding_count; ++i) {
            UniformLayout &layout = vk_set->uniform_layout[i];
            UniformBinding &binding = bindings[i];
            write_sets[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_sets[i].dstBinding = layout.binding;
            write_sets[i].descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
            write_sets[i].descriptorCount = 1;

            switch (layout.binding_type) {
            case BINDING_TYPE_STORAGE_IMAGE: {
                VulkanTexture *texture = resource_pool_textures.access(binding.resource_id);
                VkDescriptorImageInfo &image_info = image_infos.emplace_back(VkDescriptorImageInfo{});
                image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                image_info.imageView = texture->image_view;

                write_sets[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                write_sets[i].pImageInfo = &image_info;
            } break;
            case BINDING_TYPE_UNIFORM_BUFFER: {
                VulkanBuffer *buffer = resource_pool_buffers.access(binding.resource_id);
                VkDescriptorBufferInfo &buffer_info = buffer_infos.emplace_back(VkDescriptorBufferInfo{});
                buffer_info.buffer = buffer->buffer;
                buffer_info.offset = binding.offset;
                buffer_info.range = binding.range;
                write_sets[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                write_sets[i].pBufferInfo = &buffer_info;
            } break;
            case BINDING_TYPE_STORAGE_BUFFER: {
                VulkanBuffer *buffer = resource_pool_buffers.access(binding.resource_id);
                VkDescriptorBufferInfo &buffer_info = buffer_infos.emplace_back(VkDescriptorBufferInfo{});
                buffer_info.buffer = buffer->buffer;
                buffer_info.offset = binding.offset;
                buffer_info.range = binding.range;
                write_sets[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                write_sets[i].pBufferInfo = &buffer_info;
            } break;
            case BINDING_TYPE_COMBINED_IMAGE_SAMPLER: {
                VulkanTexture *texture = resource_pool_textures.access(binding.resource_id);
                VkDescriptorImageInfo &image_info = image_infos.emplace_back(VkDescriptorImageInfo{});
                image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                image_info.imageView = texture->image_view;
                image_info.sampler = texture->sampler;
                write_sets[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                write_sets[i].pImageInfo = &image_info;
            } break;
            default:
                assert(0 && "Undefined Binding Type");
                break;
            }
        }

        for (auto &writeSet : write_sets)
            writeSet.dstSet = vk_set->descriptor_set;

        vkUpdateDescriptorSets(device, binding_count, write_sets.data(), 0, nullptr);
    }

    BufferID VulkanRenderingDevice::create_buffer(BufferDescription *buffer_description, const std::string &debug_name) {
        ASSERT_MSG(buffer_description->size > 0, "GPU Buffer cannot be empty");
        VkBufferCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = buffer_description->size,
            .usage = buffer_description->usage_flags,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };

        VmaAllocationCreateInfo allocation_create_info = {};

        switch (buffer_description->allocation_type) {
        case MEMORY_ALLOCATION_TYPE_CPU: {
            bool is_src = (buffer_description->usage_flags & BUFFER_USAGE_TRANSFER_SRC_BIT) > 0;
            bool is_dst = (buffer_description->usage_flags & BUFFER_USAGE_TRANSFER_DST_BIT) > 0;

            // This is a staging buffer
            allocation_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            allocation_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
            allocation_create_info.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
            allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
            break;
        }
        case MEMORY_ALLOCATION_TYPE_GPU: {
            allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            break;
        }
        }

        VkBuffer vk_buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = nullptr;
        VmaAllocationInfo allocation_info = {};
        VK_CHECK(vmaCreateBuffer(vma_allocator, &create_info, &allocation_create_info, &vk_buffer, &allocation, &allocation_info));
        set_debug_marker_object_name(VK_OBJECT_TYPE_BUFFER, (uint64_t)vk_buffer, debug_name.c_str());

        uint32_t buffer_id = resource_pool_buffers.obtain();
        VulkanBuffer *buffer = resource_pool_buffers.access(buffer_id);
        buffer->buffer = vk_buffer;
        buffer->allocation = allocation;
        buffer->size = buffer_description->size;
        buffer->buffer_ptr = nullptr;

        total_memory_usage += allocation->GetSize();
        return BufferID{buffer_id};
    }

    uint8_t *VulkanRenderingDevice::map_buffer(BufferID buffer) {
        VulkanBuffer *vk_buffer = resource_pool_buffers.access(buffer.id);
        if (vk_buffer->buffer_ptr == nullptr)
            VK_CHECK(vmaMapMemory(vma_allocator, vk_buffer->allocation, &vk_buffer->buffer_ptr));
        return reinterpret_cast<uint8_t *>(vk_buffer->buffer_ptr);
    }
    /*
    void VulkanRenderingDevice::CopyBuffer(CommandBufferID commandBuffer, BufferID src, BufferID dst, BufferCopyRegion *region)
    {
        VulkanBuffer *vkSrc = _buffers.Access(src.id);
        VulkanBuffer *vkDst = _buffers.Access(dst.id);
        vkCmdCopyBuffer(_commandBuffers[commandBuffer.id], vkSrc->buffer, vkDst->buffer, 1, (const VkBufferCopy *)region);
    }
    */
    TextureID VulkanRenderingDevice::create_texture(TextureDescription *texture_description, const std::string &debug_name) {
        uint32_t textureID = resource_pool_textures.obtain();
        VulkanTexture *texture = resource_pool_textures.access(textureID);
        texture->width = texture_description->width;
        texture->height = texture_description->height;
        texture->depth = texture_description->depth;
        texture->mip_levels = texture_description->mip_levels;
        texture->array_layers = texture_description->array_layers;
        texture->image_type = VkImageType(texture_description->texture_type);
        texture->current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        texture->access_flags = 0;

        // @TODO cache sampler
        if (texture_description->sampler_desc)
            texture->sampler = create_sampler(texture_description->sampler_desc);
        else
            texture->sampler = VK_NULL_HANDLE;

        VkImageUsageFlags usage = 0;
        if ((texture_description->usage_flags & TEXTURE_USAGE_TRANSFER_SRC_BIT))
            usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        if ((texture_description->usage_flags & TEXTURE_USAGE_TRANSFER_DST_BIT))
            usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if ((texture_description->usage_flags & TEXTURE_USAGE_SAMPLED_BIT))
            usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        if ((texture_description->usage_flags & TEXTURE_USAGE_COLOR_ATTACHMENT_BIT))
            usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if ((texture_description->usage_flags & TEXTURE_USAGE_DEPTH_ATTACHMENT_BIT))
            usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        if ((texture_description->usage_flags & TEXTURE_USAGE_INPUT_ATTACHMENT_BIT))
            usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
        if ((texture_description->usage_flags & TEXTURE_USAGE_STORAGE_BIT))
            usage |= VK_IMAGE_USAGE_STORAGE_BIT;

        VkImageAspectFlags image_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        if (texture_description->usage_flags & TEXTURE_USAGE_DEPTH_ATTACHMENT_BIT) {
            image_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (texture_description->usage_flags & TEXTURE_USAGE_STENCIL_ATTACHMENT_BIT)
                image_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        texture->format = RD_FORMAT_TO_VK_FORMAT[texture_description->format];
        texture->image_aspect = image_aspect;

        VkImageCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = texture->image_type,
            .format = texture->format,
            .extent = {texture->width, texture->height, texture->depth},
            .mipLevels = texture->mip_levels,
            .arrayLayers = texture->array_layers,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };

        VmaAllocationCreateInfo allocation_create_info = {};
        allocation_create_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        allocation_create_info.flags = 0;

        // Create Image
        VmaAllocationInfo allocation_info = {};
        VK_CHECK(vmaCreateImage(vma_allocator, &create_info, &allocation_create_info, &texture->image, &texture->allocation, &allocation_info));
        set_debug_marker_object_name(VK_OBJECT_TYPE_IMAGE, (uint64_t)texture->image, debug_name.c_str());
        total_memory_usage += texture->allocation->GetSize();

        VkImageViewCreateInfo imageViewCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = texture->image,
            .viewType = VkImageViewType(texture->image_type),
            .format = texture->format,
            .components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A},
            .subresourceRange = {
                .aspectMask = texture->image_aspect,
                .levelCount = texture->mip_levels,
                .layerCount = texture->array_layers,
            },
        };
        VK_CHECK(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &texture->image_view));
        set_debug_marker_object_name(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)texture->image_view, (debug_name + "_image_view").c_str());
        return TextureID{textureID};
    };

    void VulkanRenderingDevice::new_frame() {
        VK_CHECK(vkWaitForFences(device, 1, &in_flight_fences[current_frame], VK_TRUE, UINT64_MAX));
        vkResetFences(device, 1, &in_flight_fences[current_frame]);

        // Reset command pool
        uint32_t command_pool_begin = current_frame * K_NUM_THREAD;
        for (uint32_t i = command_pool_begin; i < K_NUM_THREAD; ++i)
            vkResetCommandPool(device, command_pools[i], 0);

        VkSurfaceCapabilitiesKHR surface_caps = {};
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_caps));
        uint32_t width = swapchain->width;
        uint32_t height = swapchain->height;

        bool resized = (width != surface_caps.currentExtent.width) || (height != surface_caps.currentExtent.height);
        if (resized) {
            swapchain->width = surface_caps.currentExtent.width;
            swapchain->height = surface_caps.currentExtent.height;
            ResizeSwapchain(swapchain.get(), physical_device, device, surface, vsync);
        }

        uint32_t &current_image_index = swapchain->current_image_index;
        VK_CHECK(vkAcquireNextImageKHR(device, swapchain->swapchain, UINT64_MAX, image_acquire_semaphore[current_frame], VK_NULL_HANDLE, &current_image_index));
    }

    CommandBuffer *VulkanRenderingDevice::get_command_buffer(uint32_t thread_id) {
        ASSERT_MSG(thread_id < K_NUM_THREAD, "ThreadID exceed the number of threads");
        uint32_t index = current_frame * K_NUM_THREAD * K_NUM_COMMAND_BUFFER_PER_THREAD + thread_id;
        return command_buffers[index].get();
    }

    void VulkanRenderingDevice::submit_command_buffer_immediate(CommandBuffer *command_buffer) {
        VkCommandBuffer vk_cmd_buffer = command_buffer->command_buffer;
        VK_CHECK(vkEndCommandBuffer(vk_cmd_buffer));

        VkPipelineStageFlags wait_stage_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 0,
            .pWaitDstStageMask = &wait_stage_mask,
            .commandBufferCount = 1,
            .pCommandBuffers = &vk_cmd_buffer,
        };

        VkQueue queue = device_queues[command_buffer->queue_family_indices];
        VK_CHECK(vkQueueSubmit(queue, 1, &submit_info, command_buffer->fence));
    }

    void VulkanRenderingDevice::wait() {
        VK_CHECK(vkDeviceWaitIdle(device));
    }

    void VulkanRenderingDevice::present() {
        std::vector<VkCommandBufferSubmitInfo> command_buffer_submit_infos(queued_command_buffer.size());

        VkImageLayout current_layout = swapchain->get_current_image_layout();
        if (current_layout != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {

            VkImageMemoryBarrier2 present_barrier = CreateImageMemoryBarrier2(swapchain->get_current_image(),
                                                                              VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                                              VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                                                              VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                                                                              VK_ACCESS_2_NONE,
                                                                              current_layout,
                                                                              VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                                                              VK_IMAGE_ASPECT_COLOR_BIT);

            VkDependencyInfo dependency_info = {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
                .memoryBarrierCount = 0,
                .bufferMemoryBarrierCount = 0,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = &present_barrier,
            };
            vkCmdPipelineBarrier2(queued_command_buffer[0]->command_buffer, &dependency_info);
            swapchain->set_current_image_layout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        }

        for (uint32_t i = 0; i < queued_command_buffer.size(); ++i) {
            VK_CHECK(vkEndCommandBuffer(queued_command_buffer[i]->command_buffer));
            command_buffer_submit_infos[i].sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
            command_buffer_submit_infos[i].commandBuffer = queued_command_buffer[i]->command_buffer;
        }

        VkSemaphoreSubmitInfo semaphore_wait_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = image_acquire_semaphore[current_frame],
            .stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        };

        VkSemaphoreSubmitInfo semaphore_signal_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = render_finished_semaphore[current_frame],
            .stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        };

        VkSubmitInfo2 submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .waitSemaphoreInfoCount = 1,
            .pWaitSemaphoreInfos = &semaphore_wait_info,
            .commandBufferInfoCount = static_cast<uint32_t>(command_buffer_submit_infos.size()),
            .pCommandBufferInfos = command_buffer_submit_infos.data(),
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos = &semaphore_signal_info,
        };

        VK_CHECK(vkQueueSubmit2(device_queues[QUEUE_TYPE_GRAPHICS], 1, &submit_info, in_flight_fences[current_frame]));

        queued_command_buffer.clear();

        VkPresentInfoKHR present_info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &render_finished_semaphore[current_frame],
            .swapchainCount = 1,
            .pSwapchains = &swapchain->swapchain,
            .pImageIndices = &swapchain->current_image_index,
        };

        VK_CHECK(vkQueuePresentKHR(device_queues[QUEUE_TYPE_GRAPHICS], &present_info));
        current_frame = (current_frame + 1) % K_MAX_FRAME_IN_FLIGHTS;
    }

    void VulkanRenderingDevice::destroy_shaders(ShaderID *shader_ids, uint32_t count) {
        for (uint32_t i = 0; i < count; ++i) {
            VulkanShader *shader = resource_pool_shaders.access(shader_ids[i]);
            DestroyShader(shader, device);
            resource_pool_shaders.release(shader_ids[i]);
        }
    }

    void VulkanRenderingDevice::destroy_pipelines(PipelineID *pipeline_ids, uint32_t count) {
        for (uint32_t i = 0; i < count; ++i) {
            VulkanPipeline *pipeline = resource_pool_pipelines.access(pipeline_ids[i]);
            // for (auto &set_layout : pipeline->set_layouts)
            //    vkDestroyDescriptorSetLayout(device, set_layout, nullptr);
            vkDestroyPipelineLayout(device, pipeline->pipeline_layout, nullptr);
            vkDestroyPipeline(device, pipeline->pipeline, nullptr);
            resource_pool_pipelines.release(pipeline_ids[i]);
        }
    }

    void VulkanRenderingDevice::destroy_buffers(BufferID *buffers, uint32_t count) {
        for (uint32_t i = 0; i < count; ++i) {
            VulkanBuffer *buffer = resource_pool_buffers.access(buffers[i]);
            if (buffer->buffer_ptr)
                vmaUnmapMemory(vma_allocator, buffer->allocation);

            vmaDestroyBuffer(vma_allocator, buffer->buffer, buffer->allocation);
            buffer->allocation = VK_NULL_HANDLE;
            buffer->buffer = VK_NULL_HANDLE;
            buffer->buffer_ptr = nullptr;
            buffer->size = 0;
            resource_pool_buffers.release(buffers[i]);
        }
    }

    void VulkanRenderingDevice::destroy_textures(TextureID *textures, uint32_t count) {
        for (uint32_t i = 0; i < count; ++i) {
            VulkanTexture *texture = resource_pool_textures.access(textures[i]);

            vkDestroyImageView(device, texture->image_view, nullptr);
            vmaDestroyImage(vma_allocator, texture->image, texture->allocation);
            if (texture->sampler != VK_NULL_HANDLE)
                vkDestroySampler(device, texture->sampler, nullptr);
            texture->width = texture->height = texture->depth = 0;
            texture->format = VK_FORMAT_UNDEFINED;
            texture->image_view = VK_NULL_HANDLE;
            texture->allocation = VK_NULL_HANDLE;
            texture->current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
            texture->sampler = VK_NULL_HANDLE;
            texture->mip_levels = 0;
            texture->array_layers = 0;
            texture->image = VK_NULL_HANDLE;
            texture->access_flags = 0;
            resource_pool_textures.release(textures[i]);
        }
    }

    void VulkanRenderingDevice::destroy_uniform_sets(UniformSetID *uniform_sets, uint32_t count) {
        for (uint32_t i = 0; i < count; ++i) {
            VulkanUniformSet *uniform_set = resource_pool_uniform_sets.access(uniform_sets[i]);
            vkFreeDescriptorSets(device, uniform_set->descriptor_pool, 1, &uniform_set->descriptor_set);
            uniform_set->descriptor_pool = VK_NULL_HANDLE;
            uniform_set->descriptor_set = VK_NULL_HANDLE;
            uniform_set->set_id = 0;
            resource_pool_uniform_sets.release(uniform_sets[i]);
        }
    }

    void VulkanRenderingDevice::add_bindless_texture(TextureID *textures, uint32_t texture_count) {
        std::vector<VkWriteDescriptorSet> write_set(texture_count);
        std::vector<VkDescriptorImageInfo> image_infos(texture_count);
        for (uint32_t i = 0; i < texture_count; ++i) {
            ASSERT(textures[i].is_valid());
            write_set[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_set[i].dstSet = bindless_descriptor_set;
            write_set[i].dstBinding = K_BINDLESS_TEXTURE_BINDING;
            write_set[i].descriptorCount = 1;
            write_set[i].dstArrayElement = textures[i].id;
            write_set[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

            VulkanTexture *texture = resource_pool_textures.access(textures[i]);
            image_infos[i].imageLayout = texture->current_layout;
            image_infos[i].sampler = texture->sampler;
            image_infos[i].imageView = texture->image_view;
            write_set[i].pImageInfo = &image_infos[i];
        }

        vkUpdateDescriptorSets(device, texture_count, write_set.data(), 0, nullptr);
    }

    VulkanRenderingDevice::~VulkanRenderingDevice() {
        VK_CHECK(vkDeviceWaitIdle(device));
        for (auto &fence : in_flight_fences)
            vkDestroyFence(device, fence, nullptr);

        for (auto &command_pool : command_pools)
            vkDestroyCommandPool(device, command_pool, nullptr);

        for (auto &command_buffer : command_buffers)
            vkDestroyFence(device, command_buffer->fence, nullptr);

        for (auto &semaphore : image_acquire_semaphore)
            vkDestroySemaphore(device, semaphore, nullptr);
        for (auto &semaphore : render_finished_semaphore)
            vkDestroySemaphore(device, semaphore, nullptr);
        vkDestroySwapchainKHR(device, swapchain->swapchain, nullptr);

        for (auto &image_view : swapchain->image_views)
            vkDestroyImageView(device, image_view, nullptr);

        for (auto &descriptor_pool : descriptor_pools)
            vkDestroyDescriptorPool(device, descriptor_pool, nullptr);

        for (auto &[key, val] : descriptor_set_layouts_cache)
            vkDestroyDescriptorSetLayout(device, val, nullptr);

        vkDestroyDescriptorSetLayout(device, bindless_descriptor_layout, nullptr);
        vkDestroyDescriptorPool(device, bindless_descriptor_pool, nullptr);

        swapchain = nullptr;
        vkDestroySurfaceKHR(instance, surface, nullptr);

        ASSERT_MSG(resource_pool_textures.used_indices == 0, "Texture Pool is not empty!!!");
        ASSERT_MSG(resource_pool_pipelines.used_indices == 0, "Pipeline Pool is not empty!!!");
        ASSERT_MSG(resource_pool_shaders.used_indices == 0, "Shader Pool is not empty!!!");
        ASSERT_MSG(resource_pool_buffers.used_indices == 0, "Buffer Pool is not empty!!!");

        vmaDestroyAllocator(vma_allocator);
        vkDestroyDevice(device, nullptr);
        if (debug_utils_messenger != VK_NULL_HANDLE)
            vkDestroyDebugUtilsMessengerEXT(instance, debug_utils_messenger, nullptr);
        vkDestroyInstance(instance, nullptr);
    }
} // namespace mirai
