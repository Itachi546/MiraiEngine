#include "VulkanRenderingDevice.hpp"

#include "Instance.hpp"
#include "Device.hpp"
#include "Swapchain.hpp"
#include "CommandBuffer.hpp"
#include "VulkanUtils.hpp"
#include "Common/Hash.hpp"
#include "Math/MathUtils.hpp"

#define VMA_IMPLEMENTATION
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vma/vk_mem_alloc.h>

#include <unordered_map>
#include <algorithm>

namespace mirai {
    void VulkanRenderingDevice::set_debug_marker_object_name(VkObjectType objectType, uint64_t handle, const char *objectName) {
#if ENABLE_VALIDATION && ENABLE_DEBUG_LABELS

        VkDebugUtilsObjectNameInfoEXT name_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .objectType = objectType,
            .objectHandle = handle,
            .pObjectName = objectName,
        };

        VK_CHECK(vkSetDebugUtilsObjectNameEXT(device, &name_info));
#endif
    }

    void VulkanRenderingDevice::initialize_bindless_descriptor() {
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

    VulkanRenderingDevice::VulkanRenderingDevice() : resource_pool_pipelines(128, "Pipeline"),
                                                     resource_pool_shaders(32, "Shader"),
                                                     resource_pool_textures(1024, "Texture"),
                                                     resource_pool_buffers(256, "Buffer"),
                                                     resource_pool_uniform_sets(256, "UniformSet"),
                                                     resource_pool_queries(32, "Query") {
        requested_instance_extensions = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME,
            VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
#ifdef MIRAI_PLATFORM_WINDOW
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#endif
        };

#if ENABLE_VALIDATION
        requested_validation_layers = {
            "VK_LAYER_KHRONOS_validation",
            "VK_LAYER_KHRONOS_synchronization2",
        };
        requested_instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

        instance = CreateInstance(requested_validation_layers, requested_instance_extensions);
        volkLoadInstance(instance);

#if ENABLE_VALIDATION
        debug_report_callback = RegisterDebugCallback(instance);
#else
        debug_report_callback = VK_NULL_HANDLE;
#endif
        std::vector<PhysicalDeviceInfo> physical_device_infos;
        EnumeratePhysicalDevices(instance, physical_device_infos);

        // Compulsary extension required to run the engine
        requested_device_extensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        };

        const std::vector<const char *> raytracing_extensions = {
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_RAY_QUERY_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        };

        uint32_t max_score = 0;
        uint32_t max_score_index = 0;
        for (uint32_t i = 0; i < physical_device_infos.size(); ++i) {
            const PhysicalDeviceInfo &physical_device_info = physical_device_infos[i];
            all_vendor_infos.push_back(physical_device_info.vendor_info);

            uint32_t score = 0;
            // Skip integrated GPU for now
            if (physical_device_info.vendor_info.device_type == DeviceType::DEVICE_TYPE_INTEGRATED_GPU) {
                continue;
            }

            // All compulsary extensions must be available
            if (!IsExtensionsAvailable(physical_device_info.supported_extensions, requested_device_extensions))
                continue;

            score += cast_u32(requested_device_extensions.size());

            if (IsExtensionsAvailable(physical_device_info.supported_extensions, raytracing_extensions)) {
                has_rt_support = true;
                score += 3;
            }

            if (score > max_score) {
                max_score_index = i;
                max_score = score;
            }
        }

        if (has_rt_support)
            requested_device_extensions.insert(requested_device_extensions.end(), raytracing_extensions.begin(), raytracing_extensions.end());

        Log::Info("VULKAN::SELECTED DEVICE:: ", all_vendor_infos[max_score_index].name);
        physical_device = physical_device_infos[max_score_index].physical_device;

        physical_device_properties = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
        vkGetPhysicalDeviceProperties2(physical_device, &physical_device_properties);

        GetDeviceQueueFamilies(physical_device, queue_family_indices);

        uint32_t graphics_queue = queue_family_indices[QUEUE_TYPE_GRAPHICS];
        if (!PhysicalDeviceSupportPresentation(instance, physical_device, graphics_queue))
            Log::Fatal("VULKAN::Selected Physical Device Doesn't Support Presentation!!!");

        device = CreateDevice(instance, physical_device, queue_family_indices, requested_device_extensions, has_rt_support);

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
        CreateSwapchain(swapchain.get(), physical_device, device, surface, K_MAX_FRAME_IN_FLIGHTS, vsync);
        for (uint32_t i = 0; i < swapchain->images.size(); ++i) {
            std::string image_name = "swapchain_image_" + std::to_string(i);
            set_debug_marker_object_name(VK_OBJECT_TYPE_IMAGE, (uint64_t)swapchain->images[i], image_name.c_str());

            std::string image_view_name = "swapchain_image_view" + std::to_string(i);
            set_debug_marker_object_name(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)swapchain->image_views[i], image_view_name.c_str());
        }

        // Initialize CommandPool and CommandBuffer
        uint32_t swapchain_image_count = cast_u32(swapchain->images.size());
        uint32_t pool_counts = swapchain_image_count * K_NUM_THREAD;

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
            set_debug_marker_object_name(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)command_buffer->command_buffer, "pooled_command_buffer");
            command_buffer->fence = create_fence("command_buffer_fence");
        }

        image_acquire_semaphore.resize(swapchain_image_count);
        render_finished_semaphore.resize(swapchain_image_count);
        in_flight_fences.resize(swapchain_image_count);
        for (uint32_t i = 0; i < swapchain_image_count; ++i) {
            std::string index = std::to_string(i);
            image_acquire_semaphore[i] = create_semaphore("image_acquire_semaphore" + index);
            render_finished_semaphore[i] = create_semaphore("render_finished_semaphore" + index);
            in_flight_fences[i] = create_fence("in_flight_fence" + index, true);
        }

        descriptor_pools.push_back(create_descriptor_pool(0));
        initialize_bindless_descriptor();
    }

    VkDescriptorPool VulkanRenderingDevice::create_descriptor_pool(VkDescriptorPoolCreateFlags create_flags, VkDescriptorPoolSize *pools, uint32_t pool_count, uint32_t max_sets) {
        VkDescriptorPoolSize default_pools[] = {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 512},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 32},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 32},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 32},
            {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 32},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 32}};

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
            .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice = physical_device,
            .device = device,
            .pVulkanFunctions = &vulkan_functions,
            .instance = instance,
            .vulkanApiVersion = VULKAN_API_VERSION,
        };

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

    SamplerID VulkanRenderingDevice::create_sampler(SamplerDescription *desc) {
        uint64_t hash = CalculateSamplerHash(desc);
        if (sampler_caches.find(hash) != sampler_caches.end())
            return hash;

        VkSamplerCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = nullptr,
            .magFilter = VkFilter(desc->mag_filter),
            .minFilter = VkFilter(desc->min_filter),
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
        sampler_caches.insert(std::make_pair(hash, sampler));
        return SamplerID{hash};
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
            .depthCompareOp = VkCompareOp(pipeline_description->depth_state->compare_op),
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

        uint32_t set_layouts_count = (pipeline->support_bindless_texture ? 1 : 0) + static_cast<uint32_t>(descriptor_sets_map.size());
        std::vector<VkDescriptorSetLayout> set_layouts(set_layouts_count);
        if (pipeline->support_bindless_texture)
            set_layouts[K_BINDLESS_TEXTURE_SET] = bindless_descriptor_layout;

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
                set_layouts[key] = set_layout;
                descriptor_set_layouts_cache.insert(std::make_pair(hash, set_layout));
            } else {
                set_layouts[key] = found->second;
            }
        }

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

    PipelineID VulkanRenderingDevice::create_compute_pipeline(ShaderID shader_id, const std::string &debug_name) {
        VulkanShader *shader = resource_pool_shaders.access(shader_id);
        ASSERT(shader->shader_stage = VK_SHADER_STAGE_COMPUTE_BIT);
        uint32_t pipeline_id = resource_pool_pipelines.obtain();
        VulkanPipeline *pipeline = resource_pool_pipelines.access(pipeline_id);

        pipeline->bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;
        pipeline->support_bindless_texture = false;

        std::vector<VkDescriptorSetLayout> set_layouts(shader->descriptor_sets.size());
        for (const auto &descriptor_set : shader->descriptor_sets) {
            uint32_t set = descriptor_set.set;
            uint64_t hash = GetDescriptorSetLayoutHash(descriptor_set.bindings, set);
            auto found = descriptor_set_layouts_cache.find(hash);

            if (found == descriptor_set_layouts_cache.end()) {

                uint32_t binding_count = static_cast<uint32_t>(descriptor_set.bindings.size());
                std::vector<VkDescriptorSetLayoutBinding> bindings(binding_count);
                for (uint32_t b = 0; b < descriptor_set.bindings.size(); ++b) {
                    bindings[b].binding = descriptor_set.bindings[b].binding;
                    bindings[b].descriptorCount = 1;
                    bindings[b].descriptorType = descriptor_set.bindings[b].descriptor_type;
                    bindings[b].stageFlags = descriptor_set.bindings[b].shader_stage;
                }
                VkDescriptorSetLayout set_layout = CreateDescriptorSetLayout(device, bindings.data(), binding_count, 0, nullptr);
                set_layouts[set] = set_layout;
                descriptor_set_layouts_cache.insert(std::make_pair(hash, set_layout));
            } else {
                set_layouts[set] = found->second;
            }
        }

        std::vector<VkPushConstantRange> push_constants;
        for (auto &entry : shader->push_constants)
            push_constants.push_back(entry.second);

        VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = static_cast<uint32_t>(set_layouts.size()),
            .pSetLayouts = set_layouts.data(),
            .pushConstantRangeCount = static_cast<uint32_t>(push_constants.size()),
            .pPushConstantRanges = push_constants.data(),
        };

        VK_CHECK(vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &pipeline->pipeline_layout));

        VkComputePipelineCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = shader->shader_stage,
                .module = shader->shader,
                .pName = "main",
            },
            .layout = pipeline->pipeline_layout,
        };

        VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &create_info, nullptr, &pipeline->pipeline));
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
        VkWriteDescriptorSetAccelerationStructureKHR acceleration_structure_write_info = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};

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

                uint64_t mip_level = binding.texture_info.mip_levels;
                ASSERT(mip_level <= texture->image_views.size());
                image_info.imageView = texture->image_views[mip_level];

                write_sets[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                write_sets[i].pImageInfo = &image_info;
            } break;
            case BINDING_TYPE_UNIFORM_BUFFER: {
                VulkanBuffer *buffer = resource_pool_buffers.access(binding.resource_id);
                VkDescriptorBufferInfo &buffer_info = buffer_infos.emplace_back(VkDescriptorBufferInfo{});
                buffer_info.buffer = buffer->buffer;
                buffer_info.offset = binding.buffer_info.offset;
                buffer_info.range = binding.buffer_info.range;
                write_sets[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                write_sets[i].pBufferInfo = &buffer_info;
            } break;
            case BINDING_TYPE_STORAGE_BUFFER: {
                VulkanBuffer *buffer = resource_pool_buffers.access(binding.resource_id);
                VkDescriptorBufferInfo &buffer_info = buffer_infos.emplace_back(VkDescriptorBufferInfo{});
                buffer_info.buffer = buffer->buffer;
                buffer_info.offset = binding.buffer_info.offset;
                buffer_info.range = binding.buffer_info.range;
                write_sets[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                write_sets[i].pBufferInfo = &buffer_info;
            } break;
            case BINDING_TYPE_COMBINED_IMAGE_SAMPLER: {
                VulkanTexture *texture = resource_pool_textures.access(binding.resource_id);
                VkDescriptorImageInfo &image_info = image_infos.emplace_back(VkDescriptorImageInfo{});
                image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                uint64_t mip_level = binding.texture_info.mip_levels;
                ASSERT(mip_level <= texture->image_views.size());
                image_info.imageView = texture->image_views[mip_level];

                auto found = sampler_caches.find(binding.texture_info.sampler);
                if (found != sampler_caches.end())
                    image_info.sampler = found->second;

                write_sets[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                write_sets[i].pImageInfo = &image_info;
            } break;
            case BINDING_TYPE_ACCELERATION_STRUCTURE: {
                if (!has_rt_support) {
                    ASSERT("Raytracing is not supported");
                    continue;
                }
                acceleration_structure_write_info.accelerationStructureCount = 1;
                acceleration_structure_write_info.pAccelerationStructures = &acceleration_structure.tlas;
                write_sets[i].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
                write_sets[i].pNext = &acceleration_structure_write_info;
                break;
            }
            default:
                ASSERT(0 && "Undefined Binding Type");
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
            // This is a staging buffer
            allocation_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
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

        if (MEMORY_ALLOCATION_TYPE_CPU) {
            VkMemoryPropertyFlags memory_properties;
            vmaGetAllocationMemoryProperties(vma_allocator, allocation, &memory_properties);
            ASSERT((memory_properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            ASSERT((memory_properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        }

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

    void VulkanRenderingDevice::unmap_buffer(BufferID buffer) {
        VulkanBuffer *vk_buffer = resource_pool_buffers.access(buffer.id);
        if (vk_buffer->buffer_ptr != nullptr) {
            vmaUnmapMemory(vma_allocator, vk_buffer->allocation);
            vk_buffer->buffer_ptr = nullptr;
        }
    }

    QueryID VulkanRenderingDevice::create_query(uint32_t query_count) {
        VkQueryPoolCreateInfo query_pool_create_info = {
            .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            .queryType = VK_QUERY_TYPE_TIMESTAMP,
            .queryCount = query_count,
        };
        VkQueryPool query_pool = VK_NULL_HANDLE;
        VK_CHECK(vkCreateQueryPool(device, &query_pool_create_info, nullptr, &query_pool));

        uint32_t id = resource_pool_queries.obtain();
        VulkanQuery *query = resource_pool_queries.access(id);
        query->type = VK_QUERY_TYPE_TIMESTAMP;
        query->query_pool = query_pool;
        return QueryID{id};
    }

    void VulkanRenderingDevice::query(CommandBuffer *command_buffer, QueryID query, uint32_t query_index) {
        VulkanQuery *vk_query = resource_pool_queries.access(query);
        ASSERT(vk_query->type == VK_QUERY_TYPE_TIMESTAMP);
        vkCmdWriteTimestamp(command_buffer->command_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, vk_query->query_pool, query_index);
    }

    void VulkanRenderingDevice::resolve_query(QueryID query, uint64_t *resolve_output, uint32_t start, uint32_t count) {
        VulkanQuery *vk_query = resource_pool_queries.access(query);
        VK_CHECK(vkGetQueryPoolResults(device, vk_query->query_pool, start, count, sizeof(uint64_t) * count, resolve_output, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));
    }

    void VulkanRenderingDevice::reset_query(CommandBuffer *command_buffer, QueryID query, uint32_t start, uint32_t count) {
        VulkanQuery *vk_query = resource_pool_queries.access(query);
        vkCmdResetQueryPool(command_buffer->command_buffer, vk_query->query_pool, start, count);
    }

    float VulkanRenderingDevice::get_timestamp_period() {
        return physical_device_properties.properties.limits.timestampPeriod;
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
        VkImageViewType image_view_type = VK_IMAGE_VIEW_TYPE_2D;
        VkImageType image_type = VK_IMAGE_TYPE_2D;

        uint32_t array_layers = texture_description->array_layers;
        switch (texture_description->texture_type) {
        case TEXTURE_TYPE_1D:
            image_view_type = array_layers > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
            image_type = VK_IMAGE_TYPE_1D;
            break;
        case TEXTURE_TYPE_2D:
            image_view_type = array_layers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
            break;
        case TEXTURE_TYPE_3D:
            ASSERT(array_layers == 1);
            image_view_type = VK_IMAGE_VIEW_TYPE_3D;
            image_type = VK_IMAGE_TYPE_3D;
            break;
        case TEXTURE_TYPE_CUBE:
            image_view_type = VK_IMAGE_VIEW_TYPE_CUBE;
            image_type = VK_IMAGE_TYPE_2D;
            break;
        }

        uint32_t textureID = resource_pool_textures.obtain();
        VulkanTexture *texture = resource_pool_textures.access(textureID);
        texture->create_flags = texture_description->create_flags;
        texture->width = texture_description->width;
        texture->height = texture_description->height;
        texture->depth = texture_description->depth;
        texture->mip_levels = texture_description->mip_levels;
        texture->array_layers = texture_description->array_layers;
        texture->image_type = image_type;
        texture->image_view_type = image_view_type;
        texture->current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        texture->access_flags = 0;
        texture->stage_mask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;

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
            .imageType = image_type,
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

        if (image_view_type == VK_IMAGE_VIEW_TYPE_CUBE || image_view_type == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
            create_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

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
            .viewType = image_view_type,
            .format = texture->format,
            .components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A},
            .subresourceRange = {
                .aspectMask = texture->image_aspect,
                .levelCount = texture->mip_levels,
                .layerCount = texture->array_layers,
            },
        };

        uint32_t image_view_count = (texture_description->create_flags & TEXTURE_CREATION_FLAG_IMAGE_VIEW_PER_MIP) > 0 ? texture->mip_levels : 1;
        texture->image_views.resize(image_view_count);
        for (uint32_t i = 0; i < image_view_count; ++i) {
            imageViewCreateInfo.subresourceRange.baseMipLevel = i;
            imageViewCreateInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
            VK_CHECK(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &texture->image_views[i]));
            set_debug_marker_object_name(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)texture->image_views[i], (debug_name + "_image_view" + std::to_string(i)).c_str());
        }
        return TextureID{textureID};
    }

    void VulkanRenderingDevice::generate_mipmap(CommandBuffer *command_buffer, TextureID texture_id, PipelineStage src_pipeline_stage) {
        VulkanTexture *texture = resource_pool_textures.access(texture_id);
        ASSERT(texture->mip_levels > 1);

        uint32_t width = texture->width;
        uint32_t height = texture->height;
        // @TODO handle 3D texture
        uint32_t layer_count = texture->image_view_type == VK_IMAGE_VIEW_TYPE_CUBE ? 6 : 1;
        VkImageBlit2 blit_region = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
            .srcSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseArrayLayer = 0,
                .layerCount = layer_count,
            },
            .dstSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseArrayLayer = 0,
                .layerCount = layer_count,
            },
        };

        VkBlitImageInfo2 blit_image_info = {
            .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
            .pNext = nullptr,
            .srcImage = texture->image,
            .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .dstImage = texture->image,
            .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .regionCount = 1,
            .pRegions = &blit_region,
            .filter = VK_FILTER_LINEAR,
        };

        // TRANSFER SRC BARRIER
        VkImageMemoryBarrier2 barriers[2];
        barriers[0] = CreateImageMemoryBarrier2(texture->image,
                                                VkPipelineStageFlagBits2(src_pipeline_stage),
                                                texture->access_flags,
                                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                VK_ACCESS_TRANSFER_READ_BIT,
                                                texture->current_layout,
                                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                                VK_IMAGE_ASPECT_COLOR_BIT);
        // TRANSFER DST BARRIER
        barriers[1] = CreateImageMemoryBarrier2(texture->image,
                                                VkPipelineStageFlagBits2(src_pipeline_stage),
                                                texture->access_flags,
                                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                VK_ACCESS_TRANSFER_WRITE_BIT,
                                                texture->current_layout,
                                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                VK_IMAGE_ASPECT_COLOR_BIT);
        barriers[0].subresourceRange.levelCount = 1;
        barriers[1].subresourceRange.levelCount = 1;

        VkDependencyInfo dependency_info = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
            .memoryBarrierCount = 0,
            .bufferMemoryBarrierCount = 0,
            .imageMemoryBarrierCount = 2,
            .pImageMemoryBarriers = barriers,
        };

        for (uint32_t i = 0; i < texture->mip_levels - 1; ++i) {
            // Image memory barrier
            barriers[0].subresourceRange.baseMipLevel = i;
            barriers[1].subresourceRange.baseMipLevel = i + 1;
            vkCmdPipelineBarrier2(command_buffer->command_buffer, &dependency_info);

            blit_region.srcOffsets[0] = {0, 0, 0};
            blit_region.srcOffsets[1] = {cast_int(width), cast_int(height), 1};
            blit_region.srcSubresource.mipLevel = i;

            uint32_t mip_width = width > 1 ? width / 2 : 1;
            uint32_t mip_height = height > 1 ? height / 2 : 1;

            blit_region.dstOffsets[0] = {0, 0, 0};
            blit_region.dstOffsets[1] = {cast_int(mip_width), cast_int(mip_height), 1};
            blit_region.dstSubresource.mipLevel = i + 1;

            vkCmdBlitImage2(command_buffer->command_buffer, &blit_image_info);

            width = mip_width;
            height = mip_height;
            if (i == 0) {
                barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barriers[0].srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
                barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            }
        }

        // Convert last mip level to transfer src optimal
        barriers[0].subresourceRange.baseMipLevel = texture->mip_levels - 1;
        dependency_info.imageMemoryBarrierCount = 1;
        vkCmdPipelineBarrier2(command_buffer->command_buffer, &dependency_info);

        texture->current_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    } // namespace mirai

    void VulkanRenderingDevice::new_frame() {
        VK_CHECK(vkWaitForFences(device, 1, &in_flight_fences[current_frame], VK_TRUE, UINT64_MAX));
        vkResetFences(device, 1, &in_flight_fences[current_frame]);

        VkSurfaceCapabilitiesKHR surface_caps = {};
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_caps));

        uint32_t width = swapchain->width;
        uint32_t height = swapchain->height;

        bool resized = (width != surface_caps.currentExtent.width) || (height != surface_caps.currentExtent.height);
        if (resized) {
            swapchain->width = surface_caps.currentExtent.width;
            swapchain->height = surface_caps.currentExtent.height;
            ResizeSwapchain(swapchain.get(), physical_device, device, surface, surface_caps, K_MAX_FRAME_IN_FLIGHTS, vsync);
        }
        VK_CHECK(vkAcquireNextImageKHR(device, swapchain->swapchain, UINT64_MAX, image_acquire_semaphore[current_frame], VK_NULL_HANDLE, &swapchain->current_image_index));
        // Reset command pool
        uint32_t command_pool_begin = current_frame * K_NUM_THREAD;
        for (uint32_t i = command_pool_begin; i < K_NUM_THREAD; ++i)
            vkResetCommandPool(device, command_pools[i], 0);
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
        VkResult result = vkDeviceWaitIdle(device);
        VK_CHECK(result);
    }

    void VulkanRenderingDevice::present() {

        VkImageLayout current_layout = swapchain->get_current_image_layout();
        if (current_layout != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {

            VkImageMemoryBarrier2 present_barrier = CreateImageMemoryBarrier2(swapchain->get_current_image(),
                                                                              VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                                              VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                                                              VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                                                                              0,
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

        std::vector<VkCommandBuffer> submit_command_buffers(queued_command_buffer.size());
        for (uint32_t i = 0; i < queued_command_buffer.size(); ++i) {
            VK_CHECK(vkEndCommandBuffer(queued_command_buffer[i]->command_buffer));
            submit_command_buffers[i] = queued_command_buffer[i]->command_buffer;
        }
        queued_command_buffer.clear();

        VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT};

        VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &image_acquire_semaphore[current_frame],
            .pWaitDstStageMask = wait_stages,
            .commandBufferCount = static_cast<uint32_t>(submit_command_buffers.size()),
            .pCommandBuffers = submit_command_buffers.data(),
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &render_finished_semaphore[swapchain->current_image_index],
        };

        VK_CHECK(vkQueueSubmit(device_queues[QUEUE_TYPE_GRAPHICS], 1, &submit_info, in_flight_fences[current_frame]));

        VkPresentInfoKHR present_info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &render_finished_semaphore[swapchain->current_image_index],
            .swapchainCount = 1,
            .pSwapchains = &swapchain->swapchain,
            .pImageIndices = &swapchain->current_image_index,
        };

        VK_CHECK(vkQueuePresentKHR(device_queues[QUEUE_TYPE_GRAPHICS], &present_info));
        current_frame = (current_frame + 1) % cast_u32(swapchain->images.size());
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
            vkDestroyPipelineLayout(device, pipeline->pipeline_layout, nullptr);
            vkDestroyPipeline(device, pipeline->pipeline, nullptr);
            resource_pool_pipelines.release(pipeline_ids[i]);
        }
    }

    void VulkanRenderingDevice::destroy_buffers(BufferID *buffers, uint32_t count) {
        for (uint32_t i = 0; i < count; ++i) {
            BufferID bufferId = buffers[i];
            VulkanBuffer *buffer = resource_pool_buffers.access(bufferId);
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

    void VulkanRenderingDevice::destroy_queries(QueryID *queries, uint32_t count) {
        for (uint32_t i = 0; i < count; ++i) {
            VulkanQuery *query = resource_pool_queries.access(queries[i]);
            vkDestroyQueryPool(device, query->query_pool, nullptr);
            query->query_pool = VK_NULL_HANDLE;
            resource_pool_queries.release(queries[i]);
        }
    }

    void VulkanRenderingDevice::destroy_textures(TextureID *textures, uint32_t count) {
        for (uint32_t i = 0; i < count; ++i) {
            VulkanTexture *texture = resource_pool_textures.access(textures[i]);

            for (auto image_view : texture->image_views)
                vkDestroyImageView(device, image_view, nullptr);

            vmaDestroyImage(vma_allocator, texture->image, texture->allocation);
            texture->width = texture->height = texture->depth = 0;
            texture->format = VK_FORMAT_UNDEFINED;
            texture->image_views.clear();
            texture->allocation = VK_NULL_HANDLE;
            texture->current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
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

    void VulkanRenderingDevice::begin_debug_utils_label(CommandBuffer *command_buffer, const char *name, float *colors) {
#if ENABLE_VALIDATION && ENABLE_DEBUG_LABELS
        VkDebugUtilsLabelEXT label_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
            .pLabelName = name,
        };

        if (colors == nullptr) {
            utils::string_hash_to_colors(name, label_info.color);
        } else {
            label_info.color[0] = colors[0];
            label_info.color[1] = colors[1];
            label_info.color[2] = colors[2];
        }
        label_info.color[3] = 1.0f;
        vkCmdBeginDebugUtilsLabelEXT(command_buffer->command_buffer, &label_info);
#endif
    }

    void VulkanRenderingDevice::end_debug_utils_label(CommandBuffer *command_buffer) {
#if ENABLE_VALIDATION && ENABLE_DEBUG_LABELS
        vkCmdEndDebugUtilsLabelEXT(command_buffer->command_buffer);
#endif
    }

    void VulkanRenderingDevice::add_bindless_texture(BindlessTextureEntry *textures, uint32_t texture_count) {
        std::vector<VkWriteDescriptorSet> write_set(texture_count);
        std::vector<VkDescriptorImageInfo> image_infos(texture_count);
        for (uint32_t i = 0; i < texture_count; ++i) {
            ASSERT(textures[i].texture.is_valid());
            write_set[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_set[i].dstSet = bindless_descriptor_set;
            write_set[i].dstBinding = K_BINDLESS_TEXTURE_BINDING;
            write_set[i].descriptorCount = 1;
            write_set[i].dstArrayElement = textures[i].texture.id;
            write_set[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

            VulkanTexture *texture = resource_pool_textures.access(textures[i].texture.id);
            image_infos[i].imageLayout = texture->current_layout;

            auto found = sampler_caches.find(textures[i].sampler);
            ASSERT(found != sampler_caches.end());
            image_infos[i].sampler = found->second;

            image_infos[i].imageView = texture->image_views[0];
            write_set[i].pImageInfo = &image_infos[i];
        }

        vkUpdateDescriptorSets(device, texture_count, write_set.data(), 0, nullptr);
    }

    void VulkanRenderingDevice::create_acceleration_structure_geometry_info(const AccelerationStructureBufferInfo &vertex_buffer, const AccelerationStructureBufferInfo &index_buffer, VkAccelerationStructureGeometryKHR &geometry) {
        VulkanBuffer *vb = resource_pool_buffers.access(vertex_buffer.buffer);
        VulkanBuffer *ib = resource_pool_buffers.access(index_buffer.buffer);

        uint32_t num_vertices = vertex_buffer.count;
        VkDeviceAddress vertex_address = GetBufferDeviceAddress(device, vb->buffer) + vertex_buffer.offset;
        VkDeviceAddress index_address = GetBufferDeviceAddress(device, ib->buffer) + index_buffer.offset;

        geometry = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
            .geometry = {
                .triangles = {
                    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                    .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                    .vertexData = {.deviceAddress = vertex_address},
                    .vertexStride = vertex_buffer.stride,
                    .maxVertex = num_vertices - 1,
                    .indexType = VK_INDEX_TYPE_UINT32,
                    .indexData = {.deviceAddress = index_address},
                    .transformData = {},
                },
            },
            // @TODO maybe need to change this later for transparent
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
        };
    }

    void VulkanRenderingDevice::create_blas(const AccelerationStructureMeshInfo *meshes, uint32_t mesh_count, std::vector<VkAccelerationStructureKHR> &out_blas, BufferID &out_blas_buffer_id, std::vector<VkDeviceSize> &out_compacted_offsets, std::vector<VkDeviceSize> &out_compacted_size) {
        // For each model we create a blas structure and cache it
        std::vector<VkAccelerationStructureGeometryKHR> geometries(mesh_count);
        std::vector<VkAccelerationStructureBuildGeometryInfoKHR> build_infos(mesh_count);
        std::vector<VkAccelerationStructureBuildRangeInfoKHR> build_ranges(mesh_count);
        std::vector<VkDeviceSize> acceleration_sizes(mesh_count);
        std::vector<VkDeviceSize> scratch_sizes(mesh_count);

        out_blas.resize(mesh_count);
        out_compacted_offsets.resize(mesh_count);
        out_compacted_size.resize(mesh_count);

        const size_t k_alignment = 256;
        uint32_t total_primitives = 0;
        VkDeviceSize blas_buffer_size = 0;
        VkDeviceSize scratch_buffer_size = 0;

        // Determine memory needed for scratch buffer and blas
        for (uint32_t i = 0; i < mesh_count; ++i) {
            create_acceleration_structure_geometry_info(meshes[i].vertex_buffer, meshes[i].index_buffer, geometries[i]);

            build_infos[i].sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
            build_infos[i].pNext = nullptr;
            build_infos[i].type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            build_infos[i].flags = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
            build_infos[i].mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
            build_infos[i].geometryCount = 1;
            build_infos[i].pGeometries = &geometries[i];
            uint32_t max_primitives = meshes[i].index_buffer.count / 3;

            build_ranges[i].primitiveCount = max_primitives;

            VkAccelerationStructureBuildSizesInfoKHR size_info = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
            vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build_infos[i], &max_primitives, &size_info);

            // Just use the memory for now, this is not the final data
            out_compacted_offsets[i] = blas_buffer_size;
            acceleration_sizes[i] = size_info.accelerationStructureSize;
            scratch_sizes[i] = size_info.buildScratchSize;

            total_primitives += max_primitives;
            blas_buffer_size += align_memory(size_info.accelerationStructureSize, k_alignment);
            scratch_buffer_size = std::max(scratch_buffer_size, size_info.buildScratchSize);
        }

        Log::Info("Scratch Buffer Size: ", utils::bytes_to_mb(scratch_buffer_size), " mb");
        Log::Info("BLAS Buffer Size: ", utils::bytes_to_mb(blas_buffer_size), " mb");

        // Allocate memory for scratch buffer and blas
        BufferDescription buffer_desc = {
            .size = cast_u32(scratch_buffer_size),
            .usage_flags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .allocation_type = MEMORY_ALLOCATION_TYPE_GPU,
        };
        BufferID scratch_buffer_id = create_buffer(&buffer_desc, "blas_scratch_buffer");
        VulkanBuffer *scratch_buffer = resource_pool_buffers.access(scratch_buffer_id);
        VkDeviceAddress scratch_buffer_address = GetBufferDeviceAddress(device, scratch_buffer->buffer);

        buffer_desc.size = cast_u32(blas_buffer_size);
        buffer_desc.usage_flags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        out_blas_buffer_id = create_buffer(&buffer_desc, "build_blas_buffer");

        VulkanBuffer *blas_buffer = resource_pool_buffers.access(out_blas_buffer_id);
        VkDeviceAddress blas_buffer_address = GetBufferDeviceAddress(device, blas_buffer->buffer);

        // Create acceleration structures
        for (uint32_t i = 0; i < mesh_count; ++i) {
            VkAccelerationStructureCreateInfoKHR acceleration_create_info = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
                .buffer = blas_buffer->buffer,
                .offset = out_compacted_offsets[i],
                .size = acceleration_sizes[i],
                .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            };
            VK_CHECK(vkCreateAccelerationStructureKHR(device, &acceleration_create_info, nullptr, &out_blas[i]));
        }

        // Create query for determining actual size of acceleration structure
        VkQueryPool query_pool = 0;
        VkQueryPoolCreateInfo blas_query_create_info = {
            .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            .queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
            .queryCount = mesh_count,
        };
        VK_CHECK(vkCreateQueryPool(device, &blas_query_create_info, nullptr, &query_pool));

        CommandBuffer *command_buffer = get_command_buffer();
        command_buffer->begin();

        VkBufferMemoryBarrier2 scratch_buffer_barrier = CreateBufferMemoryBarrier2(scratch_buffer->buffer,
                                                                                   VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                                                                                   VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR);
        VkDependencyInfo dependency_info = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
            .memoryBarrierCount = 0,
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers = &scratch_buffer_barrier,
            .imageMemoryBarrierCount = 0,
        };

        std::vector<const VkAccelerationStructureBuildRangeInfoKHR *> build_range_ptrs(mesh_count);
        for (uint32_t start = 0; start < mesh_count;) {
            size_t scratch_offset = 0;
            size_t i = start;
            while (i < mesh_count && scratch_offset + scratch_sizes[i] <= scratch_buffer_size) {
                build_infos[i].scratchData.deviceAddress = scratch_buffer_address + scratch_offset;
                build_infos[i].dstAccelerationStructure = out_blas[i];

                build_range_ptrs[i] = &build_ranges[i];
                scratch_offset += scratch_sizes[i];
                ++i;
            }
            ASSERT(i > start);
            vkCmdBuildAccelerationStructuresKHR(command_buffer->command_buffer, uint32_t(i - start), &build_infos[start], &build_range_ptrs[start]);
            start = cast_u32(i);
            vkCmdPipelineBarrier2(command_buffer->command_buffer, &dependency_info);
        }

        vkCmdResetQueryPool(command_buffer->command_buffer, query_pool, 0, mesh_count);

        vkCmdWriteAccelerationStructuresPropertiesKHR(command_buffer->command_buffer, cast_u32(out_blas.size()), out_blas.data(), VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, query_pool, 0);

        submit_command_buffer_immediate(command_buffer);

        command_buffer->wait();

        VK_CHECK(vkGetQueryPoolResults(device, query_pool, 0, cast_u32(out_blas.size()), out_blas.size() * sizeof(uint64_t), out_compacted_size.data(), sizeof(uint64_t), VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_64_BIT));

        uint64_t total_compacted_size = 0;
        for (uint32_t i = 0; i < mesh_count; ++i) {
            out_compacted_offsets[i] = total_compacted_size;
            out_compacted_size[i] = align_memory(out_compacted_size[i], 256);
            total_compacted_size += out_compacted_size[i];
        }
        Log::Info("Compacted Size: ", utils::bytes_to_mb(total_compacted_size), " mb");

        destroy_buffers(&scratch_buffer_id, 1);
        vkDestroyQueryPool(device, query_pool, nullptr);
    }

    void VulkanRenderingDevice::create_tlas(BufferID instance_buffer_id, uint32_t primitive_count, VkAccelerationStructureKHR &tlas, BufferID &tlas_buffer_id) {
        // @TODO implement
        VulkanBuffer *instance_buffer = resource_pool_buffers.access(instance_buffer_id);
        VkDeviceAddress instance_buffer_device_ptr = GetBufferDeviceAddress(device, instance_buffer->buffer);

        VkAccelerationStructureGeometryKHR geometry = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
        geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        geometry.geometry.instances.data.deviceAddress = instance_buffer_device_ptr;

        VkAccelerationStructureBuildGeometryInfoKHR build_info = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
        build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        build_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
        build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        build_info.geometryCount = 1;
        build_info.pGeometries = &geometry;

        VkAccelerationStructureBuildSizesInfoKHR size_info = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
        vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build_info, &primitive_count, &size_info);
        Log::Info("TLAS Scratch Buffer Size: ", utils::bytes_to_mb(size_info.buildScratchSize), " mb");
        Log::Info("TLAS Buffer Size: ", utils::bytes_to_mb(size_info.accelerationStructureSize), " mb");

        // Allocate memory for scratch buffer and blas
        BufferDescription buffer_desc = {
            .size = cast_u32(size_info.buildScratchSize),
            .usage_flags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .allocation_type = MEMORY_ALLOCATION_TYPE_GPU,
        };

        BufferID scratch_buffer_id = create_buffer(&buffer_desc, "blas_scratch_buffer");
        VulkanBuffer *scratch_buffer = resource_pool_buffers.access(scratch_buffer_id);
        VkDeviceAddress scratch_buffer_address = GetBufferDeviceAddress(device, scratch_buffer->buffer);

        buffer_desc.size = cast_u32(size_info.accelerationStructureSize);
        buffer_desc.usage_flags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        tlas_buffer_id = create_buffer(&buffer_desc, "blas_buffer");
        VulkanBuffer *tlas_buffer = resource_pool_buffers.access(tlas_buffer_id);

        // Create Acceleration Structure
        VkAccelerationStructureCreateInfoKHR create_info = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        create_info.buffer = tlas_buffer->buffer;
        create_info.size = size_info.accelerationStructureSize;
        create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        VK_CHECK(vkCreateAccelerationStructureKHR(device, &create_info, nullptr, &tlas));

        // Build TLAS
        CommandBuffer *command_buffer = get_command_buffer();
        command_buffer->begin();

        build_info.dstAccelerationStructure = tlas;
        build_info.srcAccelerationStructure = tlas;
        build_info.scratchData.deviceAddress = scratch_buffer_address;

        VkAccelerationStructureBuildRangeInfoKHR build_range = {};
        build_range.primitiveCount = primitive_count;
        const VkAccelerationStructureBuildRangeInfoKHR *build_range_ptr = &build_range;

        vkCmdBuildAccelerationStructuresKHR(command_buffer->command_buffer, 1, &build_info, &build_range_ptr);

        VkBufferMemoryBarrier2 buffer_barrier = CreateBufferMemoryBarrier2(tlas_buffer->buffer,
                                                                           VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                                                                           VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR);
        VkDependencyInfo dependency_info = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
            .memoryBarrierCount = 0,
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers = &buffer_barrier,
            .imageMemoryBarrierCount = 0,
        };

        vkCmdPipelineBarrier2(command_buffer->command_buffer, &dependency_info);

        submit_command_buffer_immediate(command_buffer);
        command_buffer->wait();

        destroy_buffers(&scratch_buffer_id, 1);
    }

    void VulkanRenderingDevice::create_acceleration_structure(const AccelerationStructureMeshInfo *meshes, uint32_t mesh_count) {
        // Create bottom level acceleration structure
        std::vector<VkDeviceAddress> blas_compacted_offsets;
        std::vector<VkDeviceAddress> blas_compacted_sizes;
        std::vector<VkAccelerationStructureKHR> build_blas;
        BufferID build_blas_buffer;
        create_blas(meshes, mesh_count, build_blas, build_blas_buffer, blas_compacted_offsets, blas_compacted_sizes);

        // Compact BLAS
        uint32_t num_blas = cast_u32(build_blas.size());
        acceleration_structure.blas.resize(num_blas);

        VkDeviceSize compacted_total_size = 0;
        for (auto &size : blas_compacted_sizes)
            compacted_total_size += size;

        BufferDescription buffer_desc = {
            .size = cast_u32(compacted_total_size),
            .usage_flags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
            .allocation_type = MEMORY_ALLOCATION_TYPE_GPU,
        };
        acceleration_structure.blas_buffer = create_buffer(&buffer_desc, "blas_buffer");
        VulkanBuffer *blas_buffer = resource_pool_buffers.access(acceleration_structure.blas_buffer);

        CommandBuffer *cb = get_command_buffer(0);
        cb->begin();

        for (uint32_t i = 0; i < mesh_count; ++i) {
            VkAccelerationStructureCreateInfoKHR acceleration_create_info = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
                .buffer = blas_buffer->buffer,
                .offset = blas_compacted_offsets[i],
                .size = blas_compacted_sizes[i],
                .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            };
            VK_CHECK(vkCreateAccelerationStructureKHR(device, &acceleration_create_info, nullptr, &acceleration_structure.blas[i]));

            VkCopyAccelerationStructureInfoKHR copy_info = {
                .sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
                .src = build_blas[i],
                .dst = acceleration_structure.blas[i],
                .mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR,
            };
            vkCmdCopyAccelerationStructureKHR(cb->command_buffer, &copy_info);
        }
        submit_command_buffer_immediate(cb);
        cb->wait();

        // Cleanup build resources
        for (auto &acceleration_structure : build_blas)
            vkDestroyAccelerationStructureKHR(device, acceleration_structure, nullptr);
        destroy_buffers(&build_blas_buffer, 1);

        // Allocate TLAS instance buffer
        buffer_desc.size = sizeof(VkAccelerationStructureInstanceKHR) * mesh_count;
        buffer_desc.usage_flags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        buffer_desc.allocation_type = MEMORY_ALLOCATION_TYPE_CPU;

        acceleration_structure.tlas_instance_buffer = create_buffer(&buffer_desc, "tlas_instance_buffer");
        void *instance_buffer_ptr = map_buffer(acceleration_structure.tlas_instance_buffer);

        // Populate TLAS Instance Buffer
        VulkanBuffer *buffer = resource_pool_buffers.access(acceleration_structure.blas_buffer);
        VkDeviceAddress blas_device_address = GetBufferDeviceAddress(device, buffer->buffer);
        for (uint32_t i = 0; i < mesh_count; ++i) {
            VkAccelerationStructureInstanceKHR instance_info{};
            std::memcpy(&instance_info.transform.matrix[0][0], &meshes[i].transform[0][0], sizeof(float) * 12);
            instance_info.instanceCustomIndex = 0;
            instance_info.mask = 0xFF;
            instance_info.instanceShaderBindingTableRecordOffset = 0;
            instance_info.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            instance_info.accelerationStructureReference = blas_device_address + blas_compacted_offsets[i];
            std::memcpy(reinterpret_cast<VkAccelerationStructureInstanceKHR *>(instance_buffer_ptr) + i, &instance_info, sizeof(VkAccelerationStructureInstanceKHR));
        }

        create_tlas(acceleration_structure.tlas_instance_buffer, mesh_count, acceleration_structure.tlas, acceleration_structure.tlas_buffer);
    }

    uint32_t VulkanRenderingDevice::get_swapchain_image_count() {
        return cast_u32(swapchain->images.size());
    }

    VulkanRenderingDevice::~VulkanRenderingDevice() {
        VK_CHECK(vkDeviceWaitIdle(device));
        for (auto &fence : in_flight_fences)
            vkDestroyFence(device, fence, nullptr);

        for (auto &command_buffer : command_buffers) {
            vkDestroyFence(device, command_buffer->fence, nullptr);
            // vkFreeCommandBuffers(device, command_buffer->command_pool, 1, &command_buffer->command_buffer);
        }

        if (has_rt_support && acceleration_structure.blas_buffer) {
            BufferID buffers[] = {acceleration_structure.blas_buffer, acceleration_structure.tlas_buffer, acceleration_structure.tlas_instance_buffer};
            for (BufferID buffer : buffers) {
                if (buffer.is_valid())
                    destroy_buffers(&buffer, 1);
            }

            for (auto &blas : acceleration_structure.blas)
                vkDestroyAccelerationStructureKHR(device, blas, nullptr);

            if (acceleration_structure.tlas != VK_NULL_HANDLE)
                vkDestroyAccelerationStructureKHR(device, acceleration_structure.tlas, nullptr);
        }

        for (auto &command_pool : command_pools)
            vkDestroyCommandPool(device, command_pool, nullptr);

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

        for (auto &[key, val] : sampler_caches)
            vkDestroySampler(device, val, nullptr);

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
        if (debug_report_callback != VK_NULL_HANDLE)
            vkDestroyDebugReportCallbackEXT(instance, debug_report_callback, nullptr);

        vkDestroyInstance(instance, nullptr);
    }
} // namespace mirai
