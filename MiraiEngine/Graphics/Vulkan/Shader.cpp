#include "Shader.hpp"

#include "spirv_reflect.h"
#include "Common/Hash.hpp"

namespace mirai
{
    static void parse_shader_reflection(VulkanShader *shader, const uint32_t *code, uint32_t code_size_in_bytes)
    {
        SpvReflectShaderModule reflection;
        SpvReflectResult result = spvReflectCreateShaderModule(code_size_in_bytes, code, &reflection);
        ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

        shader->descriptor_sets.resize(reflection.descriptor_set_count);
        for (uint32_t s = 0; s < reflection.descriptor_set_count; ++s)
        {
            SpvReflectDescriptorSet &descriptor_set = reflection.descriptor_sets[s];

            VkReflectionDescriptorSet &vk_set = shader->descriptor_sets[s];
            vk_set.set = descriptor_set.set;
            vk_set.bindings.resize(descriptor_set.binding_count);

            for (uint32_t b = 0; b < descriptor_set.binding_count; ++b)
            {
                SpvReflectDescriptorBinding *binding = descriptor_set.bindings[b];
                VkReflectionDescriptorBinding &vk_binding = vk_set.bindings[b];
                vk_binding.name = binding->name;
                vk_binding.binding = binding->binding;
                vk_binding.descriptor_type = VkDescriptorType(binding->descriptor_type);
                vk_binding.shader_stage = VkShaderStageFlagBits(reflection.shader_stage);
            }
        }

        shader->push_constants.resize(reflection.push_constant_block_count);
        for (uint32_t p = 0; p < reflection.push_constant_block_count; ++p)
        {
            SpvReflectBlockVariable &push_constant = reflection.push_constant_blocks[p];

            VkPushConstantRange &vk_push_constant = shader->push_constants[p];
            vk_push_constant.offset = push_constant.offset;
            vk_push_constant.size = push_constant.size;
            vk_push_constant.stageFlags = VkShaderStageFlagBits(reflection.shader_stage);
        }

        shader->shader_stage = VkShaderStageFlagBits(reflection.shader_stage);
    }

    void CreateShader(VulkanShader *shader, VkDevice device, const uint32_t *code, uint32_t code_size_in_bytes)
    {
        parse_shader_reflection(shader, code, code_size_in_bytes);

        VkShaderModuleCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = code_size_in_bytes,
            .pCode = code,
        };
        VK_CHECK(vkCreateShaderModule(device, &create_info, nullptr, &shader->shader));
    }

    void MergeShaderBindings(std::vector<VkReflectionDescriptorBinding> &dst, const std::vector<VkReflectionDescriptorBinding> src)
    {
        for (const auto &binding : src)
        {
            auto found = std::find_if(dst.begin(), dst.end(), [&binding](const VkReflectionDescriptorBinding &entry)
                                      { return binding.binding == entry.binding; });
            if (found != dst.end())
                found->shader_stage |= binding.shader_stage;
            else
                dst.push_back(binding);
        }
    }

    void MergePushConstants(std::vector<VkPushConstantRange> &dst, const std::vector<VkPushConstantRange> &src)
    {
        for (const auto &push_constant : src)
        {
            auto found = std::find_if(dst.begin(), dst.end(), [&push_constant](const VkPushConstantRange &entry)
                                      { return push_constant.size == entry.size && push_constant.offset == entry.offset; });
            if (found != dst.end())
                found->stageFlags |= push_constant.stageFlags;
            else
                dst.push_back(push_constant);
        }
    }

    VkDescriptorSetLayout CreateDescriptorSetLayout(VkDevice device, const std::vector<VkReflectionDescriptorBinding> &descriptor_bindings, uint32_t set, VkDescriptorSetLayoutCreateFlags flags)
    {
        uint32_t binding_count = static_cast<uint32_t>(descriptor_bindings.size());
        std::vector<VkDescriptorSetLayoutBinding> bindings(binding_count);
        for (uint32_t i = 0; i < binding_count; ++i)
        {
            bindings[i].binding = descriptor_bindings[i].binding;
            bindings[i].descriptorCount = 1;
            bindings[i].descriptorType = descriptor_bindings[i].descriptor_type;
            bindings[i].stageFlags = descriptor_bindings[i].shader_stage;
        }

        VkDescriptorSetLayoutCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .flags = flags,
            .bindingCount = binding_count,
            .pBindings = bindings.data(),
        };

        VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
        VK_CHECK(vkCreateDescriptorSetLayout(device, &create_info, nullptr, &set_layout));
        return set_layout;
    }

    void DestroyShader(VulkanShader *shader, VkDevice device)
    {
        vkDestroyShaderModule(device, shader->shader, nullptr);
        shader->shader_stage = VkShaderStageFlagBits(0);
        shader->descriptor_sets.clear();
        shader->push_constants.clear();
        shader->shader = VK_NULL_HANDLE;
    }

    void CreatePipelineBindings(const std::unordered_map<uint32_t, std::vector<VkReflectionDescriptorBinding>> &descriptor_sets,
                                VkDevice device,
                                VulkanPipeline *pipeline,
                                VkDescriptorPool descriptor_pool,
                                uint32_t total_sets,
                                VulkanBindings *out_bindings)
    {
        for (auto [set, bindings] : descriptor_sets)
        {
            VkDescriptorSetAllocateInfo allocate_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = descriptor_pool,
                .descriptorSetCount = 1,
                .pSetLayouts = &pipeline->set_layouts[set],
            };

            VulkanDescriptorSet &descriptor_set = out_bindings->descriptor_sets.emplace_back(VulkanDescriptorSet{});
            for (uint32_t i = 0; i < total_sets; ++i)
            {
                VkDescriptorSet vk_set = VK_NULL_HANDLE;
                vkAllocateDescriptorSets(device, &allocate_info, &vk_set);
                descriptor_set.descriptor_set.push_back(vk_set);
            }

            for (auto &binding : bindings)
            {
                VkWriteDescriptorSet write_set = {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstBinding = binding.binding,
                    .descriptorCount = 1,
                    .descriptorType = binding.descriptor_type,
                };

                descriptor_set.bindings.push_back(VulkanBindingInfo{
                    .binding = write_set,
                    .resource_id = K_INVALID_ID,
                });

                VulkanBindingLookupInfo lookup_info = {
                    .set_index = static_cast<uint32_t>(out_bindings->descriptor_sets.size() - 1),
                    .binding_index = static_cast<uint32_t>(descriptor_set.bindings.size() - 1),
                };
                out_bindings->lookup_info.insert(std::make_pair(utils::djb2_hash_string(binding.name), std::move(lookup_info)));
            }
        }
    }

    void VulkanBindings::set_resource(const std::string &name, ID id)
    {
        auto found = lookup_info.find(utils::djb2_hash_string(name));
        if (found == lookup_info.end())
        {
            Log::Warn("ResourceName::", name, " couldn't be found");
            return;
        }

        auto [set_index, binding_index] = found->second;

        descriptor_sets[set_index].bindings[binding_index].resource_id = id;

        dirty = true;
    }

    void VulkanBindings::update_descriptor(VkDevice device, ResourcePool<VulkanTexture> &resource_pool_textures, uint32_t frame_id, uint32_t thread_id)
    {
        if (dirty && descriptor_sets.size() > 0)
        {
            std::vector<VkDescriptorImageInfo> image_infos;
            std::vector<VkDescriptorBufferInfo> buffer_infos;
            for (auto &descriptor_set : descriptor_sets)
            {
                std::vector<VkWriteDescriptorSet> write_sets;
                for (auto &binding : descriptor_set.bindings)
                {
                    VkWriteDescriptorSet &write_set = write_sets.emplace_back(binding.binding);
                    write_set.dstSet = descriptor_set.descriptor_set[frame_id * thread_id];
                    if (write_set.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || write_set.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                    {
                        VulkanTexture *texture = resource_pool_textures.access(binding.resource_id);
                        VkDescriptorImageInfo &image_info = image_infos.emplace_back(VkDescriptorImageInfo{
                            .sampler = texture->sampler,
                            .imageView = texture->image_view,
                            .imageLayout = texture->current_layout,
                        });
                        write_set.pImageInfo = &image_info;
                    }
                    else if (write_set.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                    {
                        ASSERT_MSG(0, "TODO::");
                    }

                    vkUpdateDescriptorSets(device, static_cast<uint32_t>(write_sets.size()), write_sets.data(), 0, nullptr);
                }
            }
            dirty = false;
        }
    }
} // namespace mirai