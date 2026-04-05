#include "VulkanShader.hpp"

#include "spirv_reflect.h"
#include "Common/Hash.hpp"

#include <algorithm>

namespace mirai {
    static void parse_shader_reflection(VulkanShader *shader, const uint32_t *code, uint32_t code_size_in_bytes) {
        SpvReflectShaderModule reflection;
        SpvReflectResult result = spvReflectCreateShaderModule(code_size_in_bytes, code, &reflection);
        ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

        for (uint32_t s = 0; s < reflection.descriptor_set_count; ++s) {
            SpvReflectDescriptorSet &descriptor_set = reflection.descriptor_sets[s];

            ShaderReflectionDescriptorSetInfo vk_set;
            vk_set.set = descriptor_set.set;
            for (uint32_t b = 0; b < descriptor_set.binding_count; ++b) {
                SpvReflectDescriptorBinding *binding = descriptor_set.bindings[b];

                ShaderReflectionDescriptorBinding &vk_binding = vk_set.bindings.emplace_back(ShaderReflectionDescriptorBinding{});
                vk_binding.binding = binding->binding;
                vk_binding.binding_type = BindingType(binding->descriptor_type);
                vk_binding.shader_stage = ShaderStage(reflection.shader_stage);
            }
            if (vk_set.bindings.size() > 0) {
                std::sort(vk_set.bindings.begin(), vk_set.bindings.end(), [](const ShaderReflectionDescriptorBinding &lhs, const ShaderReflectionDescriptorBinding &rhs) {
                    return lhs.binding < rhs.binding;
                });
                shader->descriptor_sets_info.push_back(std::move(vk_set));
            }
        }

        if (shader->descriptor_sets_info.size() > 0) {
            std::sort(shader->descriptor_sets_info.begin(), shader->descriptor_sets_info.end(), [](const ShaderReflectionDescriptorSetInfo &lhs, const ShaderReflectionDescriptorSetInfo &rhs) {
                return lhs.set < rhs.set;
            });
        }

        shader->push_constants_info.resize(reflection.push_constant_block_count);
        for (uint32_t p = 0; p < reflection.push_constant_block_count; ++p) {
            SpvReflectBlockVariable &push_constant = reflection.push_constant_blocks[p];
            shader->push_constants_info[p] = {
                .shader_stage = cast_u32(ShaderStage(reflection.shader_stage)),
                .offset = push_constant.offset,
                .size = push_constant.size,
            };
        }
        shader->shader_stage = VkShaderStageFlagBits(reflection.shader_stage);
    }

    void CreateShader(VulkanShader *shader, VkDevice device, const uint32_t *code, uint32_t code_size_in_bytes) {
        parse_shader_reflection(shader, code, code_size_in_bytes);

        VkShaderModuleCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = code_size_in_bytes,
            .pCode = code,
        };
        VK_CHECK(vkCreateShaderModule(device, &create_info, nullptr, &shader->shader));
    }

    void MergeShaderBindings(std::vector<ShaderReflectionDescriptorBinding> &dst, const std::vector<ShaderReflectionDescriptorBinding> &src) {
        for (const auto &binding : src) {
            auto found = std::find_if(dst.begin(), dst.end(), [&binding](const ShaderReflectionDescriptorBinding &entry) { return binding.binding == entry.binding; });
            if (found != dst.end())
                found->shader_stage |= binding.shader_stage;
            else
                dst.push_back(binding);
        }
    }

    void MergePushConstants(HashMap<uint32_t, ShaderReflectionPushConstant> &dst, const HashMap<uint32_t, ShaderReflectionPushConstant> &src) {
        for (const auto &[key, val] : src) {
            auto found = dst.find(key);
            if (found != dst.end())
                found->second.shader_stage |= val.shader_stage;
            else
                dst.insert(std::make_pair(key, val));
        }
    }

    void DestroyShader(VulkanShader *shader, VkDevice device) {
        vkDestroyShaderModule(device, shader->shader, nullptr);
        shader->shader_stage = VkShaderStageFlagBits(0);
        shader->descriptor_sets_info.clear();
        shader->push_constants_info.clear();
        shader->shader = VK_NULL_HANDLE;
    }
} // namespace mirai