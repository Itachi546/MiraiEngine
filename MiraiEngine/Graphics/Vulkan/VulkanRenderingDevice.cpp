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

namespace mirai
{
    void VulkanRenderingDevice::set_debug_marker_object_name(VkObjectType objectType, uint64_t handle, const char *objectName)
    {
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
                                                                           RenderingDevice(enable_validation)
    {
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

        if (enable_validation)
        {
            instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            instance_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
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
        for (uint32_t i = 0; i < swapchain->image_count; ++i)
        {
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

        for (uint32_t i = 0; i < buffer_counts; ++i)
        {
            uint32_t command_pool_index = i / K_NUM_COMMAND_BUFFER_PER_THREAD;
            VkCommandPool command_pool = command_pools[command_pool_index];

            command_buffer_allocate_info.commandPool = command_pool;

            auto &command_buffer = command_buffers.emplace_back(std::make_unique<CommandBuffer>());
            VK_CHECK(vkAllocateCommandBuffers(device, &command_buffer_allocate_info, &command_buffer->command_buffer));
        }

        for (uint32_t i = 0; i < K_MAX_FRAME_IN_FLIGHTS; ++i)
        {
            std::string index = std::to_string(i);
            image_acquire_semaphore[i] = create_semaphore("image_acquire_semaphore" + index);
            render_finished_semaphore[i] = create_semaphore("render_finished_semaphore" + index);
            in_flight_fences[i] = create_fence("in_flight_fence" + index, true);
        }

        VkDescriptorPoolSize poolSizes[] = {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 32},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 32},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 32},
        };
        uint32_t maxSets = 288;
        VkDescriptorPoolCreateInfo descriptor_pool_create_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags = 0,
            .maxSets = maxSets,
            .poolSizeCount = 3,
            .pPoolSizes = poolSizes,
        };

        VK_CHECK(vkCreateDescriptorPool(device, &descriptor_pool_create_info, nullptr, &descriptor_pool));
    }

    VkSemaphore VulkanRenderingDevice::create_semaphore(const std::string &name)
    {
        VkSemaphoreCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };

        VkSemaphore semaphore = VK_NULL_HANDLE;
        VK_CHECK(vkCreateSemaphore(device, &create_info, nullptr, &semaphore));

        set_debug_marker_object_name(VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)semaphore, name.c_str());
        return semaphore;
    }

    VmaAllocator VulkanRenderingDevice::create_allocator()
    {
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

    VkFence VulkanRenderingDevice::create_fence(const std::string &name, bool signalled)
    {
        VkFenceCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = signalled ? VK_FENCE_CREATE_SIGNALED_BIT : VkFenceCreateFlags{0},
        };
        VkFence fence = VK_NULL_HANDLE;
        VK_CHECK(vkCreateFence(device, &create_info, nullptr, &fence));
        set_debug_marker_object_name(VK_OBJECT_TYPE_FENCE, (uint64_t)fence, name.c_str());
        return fence;
    }

    VkSampler VulkanRenderingDevice::create_sampler(SamplerDescription *desc)
    {
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

    ShaderID VulkanRenderingDevice::create_shader(uint32_t *code, uint32_t code_size_in_bytes, const std::string &debug_name)
    {
        uint32_t shader_id = resource_pool_shaders.obtain();
        VulkanShader *shader = resource_pool_shaders.access(shader_id);
        CreateShader(shader, device, code, code_size_in_bytes);
        set_debug_marker_object_name(VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)shader->shader, debug_name.c_str());
        return ShaderID{shader_id};
    }

    PipelineID VulkanRenderingDevice::create_graphics_pipeline(PipelineDescription *pipeline_description, const std::string &debug_name)
    {
        std::vector<VkPipelineShaderStageCreateInfo> shader_stage_create_infos(pipeline_description->shader_count);
        std::unordered_map<uint32_t, std::vector<VkReflectionDescriptorBinding>> descriptor_sets_map;
        std::unordered_map<uint32_t, VkPushConstantRange> push_constants_map;
        for (uint32_t i = 0; i < pipeline_description->shader_count; ++i)
        {
            VulkanShader *shader = resource_pool_shaders.access(pipeline_description->shaders[i]);
            shader_stage_create_infos[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shader_stage_create_infos[i].module = shader->shader;
            shader_stage_create_infos[i].stage = shader->shader_stage;
            shader_stage_create_infos[i].pName = "main";

            if (shader->descriptor_sets.size() > 0)
            {
                for (auto &set : shader->descriptor_sets)
                {
                    auto found = descriptor_sets_map.find(set.set);
                    if (found != descriptor_sets_map.end())
                    {
                        // Merge the bindings if the binding index is same
                        MergeShaderBindings(found->second, set.bindings);
                    }
                    else
                        descriptor_sets_map.insert(std::make_pair(set.set, set.bindings));
                }
            }

            MergePushConstants(push_constants_map, shader->push_constants);
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
            .minDepthBounds = pipeline_description->depth_state->min_depth_bounds,
            .maxDepthBounds = pipeline_description->depth_state->max_depth_bounds,
        };

        uint32_t attachment_count = pipeline_description->color_attachment_count;

        std::vector<VkPipelineColorBlendAttachmentState> attachment_blend_states(attachment_count);
        std::vector<VkFormat> color_attachment_formats(attachment_count);

        for (uint32_t i = 0; i < attachment_count; ++i)
        {
            attachment_blend_states[i].blendEnable = pipeline_description->blend_state->enable;
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
        pipeline->push_constants = push_constants_map;

        for (const auto &[key, val] : descriptor_sets_map)
            pipeline->set_layouts.push_back(CreateDescriptorSetLayout(device, val, key, 0));

        std::vector<VkPushConstantRange> push_constants;
        for (const auto &entry : push_constants_map)
            push_constants.push_back(entry.second);

        VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = static_cast<uint32_t>(pipeline->set_layouts.size()),
            .pSetLayouts = pipeline->set_layouts.data(),
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

        CreatePipelineBindings(descriptor_sets_map, device, pipeline, descriptor_pool, K_MAX_FRAME_IN_FLIGHTS * K_NUM_COMMAND_BUFFER_PER_THREAD, &pipeline->bindings);

        return PipelineID{pipeline_id};
    }
    /*
    VkDescriptorSet VulkanRenderingDevice::create_descriptor_set(const std::unordered_map<uint32_t, std::vector<VkReflectionDescriptorBinding>> &descriptor_sets, const std::string &name)
    {
        /*
        std::vector<VkWriteDescriptorSet> writeSets(uniformCount);

        // @TODO replace with custom allocator
        std::vector<VkDescriptorImageInfo> imageInfos;
        std::vector<VkDescriptorBufferInfo> bufferInfos;
        imageInfos.reserve(16), bufferInfos.reserve(16);

        for (uint32_t i = 0; i < uniformCount; ++i)
        {
            BoundUniform *uniform = uniforms + i;
            writeSets[i] = {};
            writeSets[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeSets[i].dstBinding = uniform->binding;
            writeSets[i].descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
            writeSets[i].descriptorCount = 1;

            switch (uniform->bindingType)
            {
            case BINDING_TYPE_IMAGE:
            {
                VulkanTexture *texture = _textures.Access(uniform->resourceID.id);
                VkDescriptorImageInfo &imageInfo = imageInfos.emplace_back(VkDescriptorImageInfo{});
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                imageInfo.imageView = texture->imageView;

                writeSets[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                writeSets[i].pImageInfo = &imageInfo;
            }
            break;
            case BINDING_TYPE_UNIFORM_BUFFER:
            {
                VulkanBuffer *buffer = _buffers.Access(uniform->resourceID.id);
                VkDescriptorBufferInfo &bufferInfo = bufferInfos.emplace_back(VkDescriptorBufferInfo{});
                bufferInfo.buffer = buffer->buffer;
                bufferInfo.offset = uniform->offset;
                bufferInfo.range = uniform->range;
                writeSets[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                writeSets[i].pBufferInfo = &bufferInfo;
            }
            break;
            case BINDING_TYPE_STORAGE_BUFFER:
            {
                VulkanBuffer *buffer = _buffers.Access(uniform->resourceID.id);
                VkDescriptorBufferInfo &bufferInfo = bufferInfos.emplace_back(VkDescriptorBufferInfo{});
                bufferInfo.buffer = buffer->buffer;
                bufferInfo.offset = uniform->offset;
                bufferInfo.range = uniform->range;
                writeSets[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                writeSets[i].pBufferInfo = &bufferInfo;
            }
            break;
            default:
                assert(0 && "Undefined Binding Type");
                break;
            }
        }

        VulkanPipeline *vkPipeline = _pipeline.Access(pipeline.id);
        VkDescriptorSetAllocateInfo allocateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = _descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &vkPipeline->setLayout[set],
        };

        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        vkAllocateDescriptorSets(device, &allocateInfo, &descriptorSet);
        SetDebugMarkerObjectName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)descriptorSet, name.c_str());

        for (auto &writeSet : writeSets)
            writeSet.dstSet = descriptorSet;

        vkUpdateDescriptorSets(device, uniformCount, writeSets.data(), 0, nullptr);

        uint64_t uniformSetID = _uniformSets.Obtain();
        VulkanUniformSet *uniformSet = _uniformSets.Access(uniformSetID);
        uniformSet->descriptorPool = _descriptorPool;
        uniformSet->descriptorSet = descriptorSet;
        uniformSet->set = set;

        return UniformSetID{uniformSetID};
    }
        */

    TextureID VulkanRenderingDevice::create_texture(TextureDescription *texture_description, const std::string &debug_name)
    {
        uint32_t textureID = resource_pool_textures.obtain();
        VulkanTexture *texture = resource_pool_textures.access(textureID);
        texture->width = texture_description->width;
        texture->height = texture_description->height;
        texture->depth = texture_description->depth;
        texture->mip_levels = texture_description->mip_levels;
        texture->array_layers = texture_description->array_layers;
        texture->image_type = VkImageType(texture_description->texture_type);
        texture->current_layout = VK_IMAGE_LAYOUT_UNDEFINED;

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
        if (texture_description->usage_flags & TEXTURE_USAGE_DEPTH_ATTACHMENT_BIT)
        {
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

    void VulkanRenderingDevice::new_frame()
    {
        vkWaitForFences(device, 1, &in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);
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
        if (resized)
        {
            swapchain->width = surface_caps.currentExtent.width;
            swapchain->height = surface_caps.currentExtent.height;
            ResizeSwapchain(swapchain.get(), physical_device, device, surface, vsync);
            // @TODO Handle swapchain resize
        }

        uint32_t &current_image_index = swapchain->current_image_index;
        VK_CHECK(vkAcquireNextImageKHR(device, swapchain->swapchain, UINT64_MAX, image_acquire_semaphore[current_frame], VK_NULL_HANDLE, &current_image_index));
    }

    CommandBuffer *VulkanRenderingDevice::get_command_buffer(uint32_t thread_id)
    {
        ASSERT_MSG(thread_id < K_NUM_THREAD, "ThreadID exceed the number of threads");
        uint32_t index = current_frame * K_NUM_THREAD * K_NUM_COMMAND_BUFFER_PER_THREAD + thread_id;
        return command_buffers[index].get();
    }

    void VulkanRenderingDevice::pipeline_set_resources(const std::string &name, PipelineID pipeline_id, ID resource_id)
    {
        VulkanPipeline *pipeline = resource_pool_pipelines.access(pipeline_id);
        pipeline->bindings.set_resource(name, resource_id);
    }
    
    void VulkanRenderingDevice::wait()
    {
        VK_CHECK(vkDeviceWaitIdle(device));
    }

    void VulkanRenderingDevice::present()
    {
        std::vector<VkCommandBufferSubmitInfo> command_buffer_submit_infos(queued_command_buffer.size());

        VkImageLayout current_layout = swapchain->get_current_image_layout();
        if (current_layout != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
        {

            VkImageMemoryBarrier present_barrier = CreateImageMemoryBarrier(swapchain->get_current_image(),
                                                                            VK_IMAGE_ASPECT_COLOR_BIT,
                                                                            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                                                            0,
                                                                            current_layout,
                                                                            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
            vkCmdPipelineBarrier(queued_command_buffer[0]->command_buffer,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                 VK_DEPENDENCY_BY_REGION_BIT,
                                 0, nullptr,
                                 0, nullptr,
                                 1, &present_barrier);
            swapchain->set_current_image_layout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        }

        for (uint32_t i = 0; i < queued_command_buffer.size(); ++i)
        {
            VK_CHECK(vkEndCommandBuffer(queued_command_buffer[i]->command_buffer));
            command_buffer_submit_infos[i].sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
            command_buffer_submit_infos[i].commandBuffer = queued_command_buffer[i]->command_buffer;
        }

        VkSemaphoreSubmitInfo semaphore_wait_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = image_acquire_semaphore[current_frame],
            .stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
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

    void VulkanRenderingDevice::destroy_shaders(ShaderID *shader_ids, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            VulkanShader *shader = resource_pool_shaders.access(shader_ids[i]);
            DestroyShader(shader, device);
            resource_pool_shaders.release(shader_ids[i]);
        }
    }

    void VulkanRenderingDevice::destroy_pipeline(PipelineID *pipeline_ids, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            VulkanPipeline *pipeline = resource_pool_pipelines.access(pipeline_ids[i]);
            for (auto &set_layout : pipeline->set_layouts)
                vkDestroyDescriptorSetLayout(device, set_layout, nullptr);
            vkDestroyPipelineLayout(device, pipeline->pipeline_layout, nullptr);
            vkDestroyPipeline(device, pipeline->pipeline, nullptr);
            pipeline->bindings.descriptor_sets.clear();
            pipeline->bindings.lookup_info.clear();
            resource_pool_pipelines.release(pipeline_ids[i]);
        }
    }

    void VulkanRenderingDevice::destroy_texture(TextureID *textures, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
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
            resource_pool_textures.release(textures[i]);
        }
    }

    VulkanRenderingDevice::~VulkanRenderingDevice()
    {
        VK_CHECK(vkDeviceWaitIdle(device));
        for (auto &fence : in_flight_fences)
            vkDestroyFence(device, fence, nullptr);

        for (auto &command_pool : command_pools)
            vkDestroyCommandPool(device, command_pool, nullptr);

        for (auto &semaphore : image_acquire_semaphore)
            vkDestroySemaphore(device, semaphore, nullptr);
        for (auto &semaphore : render_finished_semaphore)
            vkDestroySemaphore(device, semaphore, nullptr);
        vkDestroySwapchainKHR(device, swapchain->swapchain, nullptr);
        for (auto &image_view : swapchain->image_views)
        {
            vkDestroyImageView(device, image_view, nullptr);
        }

        vkDestroyDescriptorPool(device, descriptor_pool, nullptr);

        swapchain = nullptr;
        vkDestroySurfaceKHR(instance, surface, nullptr);

        vmaDestroyAllocator(vma_allocator);
        vkDestroyDevice(device, nullptr);
        if (debug_utils_messenger != VK_NULL_HANDLE)
            vkDestroyDebugUtilsMessengerEXT(instance, debug_utils_messenger, nullptr);
        vkDestroyInstance(instance, nullptr);
    }
} // namespace mirai
