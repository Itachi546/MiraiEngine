#include "Shader.hpp"

#include "spirv_reflect.h"

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

} // namespace mirai