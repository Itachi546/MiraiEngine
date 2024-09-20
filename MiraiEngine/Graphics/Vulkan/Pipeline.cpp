#include "Pipeline.hpp"

#include "spirv_reflect.h"

namespace mirai
{
    static void parse_shader_reflection(VulkanShader *shader, const uint32_t *code, uint32_t code_size_in_bytes)
    {
        SpvReflectShaderModule module;
        SpvReflectResult result = spvReflectCreateShaderModule(code_size_in_bytes, code, &module);
        ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

        shader->shader_stage = VkShaderStageFlagBits(module.shader_stage);
        Log::Debug("Shader Stage: ", module.shader_stage);
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

    void DestroyShader(VulkanShader *shader, VkDevice device)
    {
        vkDestroyShaderModule(device, shader->shader, nullptr);
        shader->shader_stage = VkShaderStageFlagBits(0);
    }

    void CreateGraphicsPipeline(VulkanPipeline *pipeline, VkDevice device)
    {
    }
} // namespace mirai