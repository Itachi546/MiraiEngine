#include "Vulkan.hpp"
#include "../RenderingDevice.hpp"

namespace mirai
{
    struct VulkanPipeline
    {
    };

    struct VulkanShader
    {
        VkShaderModule shader;
        VkShaderStageFlagBits shader_stage;
    };

    void CreateShader(VulkanShader *shader, VkDevice device, const uint32_t *code, uint32_t code_size);

    void DestroyShader(VulkanShader *shader, VkDevice device);

    void CreateGraphicsPipeline(VulkanPipeline *pipeline, VkDevice device);

} // namespace mirai