#include "Vulkan.hpp"
#include "../RenderingDevice.hpp"

#include <vector>

namespace mirai
{
    struct VkReflectionDescriptorBinding
    {
        uint32_t binding;
        VkDescriptorType descriptor_type;
        VkShaderStageFlags shader_stage;
    };

    struct VkReflectionDescriptorSet
    {
        uint32_t set;
        std::vector<VkReflectionDescriptorBinding> bindings;
    };

    struct VulkanShader
    {
        VkShaderModule shader;
        VkShaderStageFlagBits shader_stage;
        std::vector<VkReflectionDescriptorSet> descriptor_sets;
        std::vector<VkPushConstantRange> push_constants;
    };

    void CreateShader(VulkanShader *shader, VkDevice device, const uint32_t *code, uint32_t code_size);

    VkDescriptorSetLayout CreateDescriptorSetLayout(VkDevice device, const std::vector<VkReflectionDescriptorBinding> &descriptor_bindings, uint32_t set, VkDescriptorSetLayoutCreateFlags flags);

    void MergePushConstants(std::vector<VkPushConstantRange> &dst, const std::vector<VkPushConstantRange> &src);

    void MergeShaderBindings(std::vector<VkReflectionDescriptorBinding> &dst, const std::vector<VkReflectionDescriptorBinding> src);

    void DestroyShader(VulkanShader *shader, VkDevice device);
} // namespace mirai