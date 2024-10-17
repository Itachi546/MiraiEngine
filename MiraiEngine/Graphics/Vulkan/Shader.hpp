#include "Vulkan.hpp"
#include "Graphics/RenderingDevice.hpp"
#include "Common/ResourcePool.hpp"

#include <vector>
#include <unordered_map>

VK_DEFINE_HANDLE(VmaAllocation);
namespace mirai
{
    struct VkReflectionDescriptorBinding
    {
        std::string name;
        uint32_t binding;
        VkDescriptorType descriptor_type;
        VkShaderStageFlags shader_stage;
    };

    struct VkReflectionDescriptorSet
    {
        uint32_t set;
        std::vector<VkReflectionDescriptorBinding> bindings;
    };

    struct VulkanTexture
    {
        uint32_t width, height, depth;
        uint32_t mip_levels, array_layers;

        VkImageAspectFlags image_aspect;
        VkFormat format;
        VkImageType image_type;

        VkImage image;
        VkImageView image_view;
        VmaAllocation allocation;

        VkImageLayout current_layout;
        VkSampler sampler;
    };

    struct VulkanShader
    {
        VkShaderModule shader;
        VkShaderStageFlagBits shader_stage;
        std::vector<VkReflectionDescriptorSet> descriptor_sets;
        std::unordered_map<uint32_t, VkPushConstantRange> push_constants;
    };

    struct VulkanBindingInfo
    {
        VkWriteDescriptorSet binding;
        ID resource_id;
    };

    struct VulkanDescriptorSet
    {
        std::vector<VkDescriptorSet> descriptor_set;
        std::vector<VulkanBindingInfo> bindings;
    };

    struct VulkanBindingLookupInfo
    {
        uint32_t set_index;
        uint32_t binding_index;
    };

    struct VulkanBindings
    {
        std::vector<VulkanDescriptorSet> descriptor_sets;
        std::unordered_map<uint32_t, VulkanBindingLookupInfo> lookup_info;

        void set_resource(const std::string &name, ID resource_id);

        void update_descriptor(VkDevice device, ResourcePool<VulkanTexture> &resource_pool_textures, uint32_t frame_id, uint32_t thread_id);
    };

    struct VulkanPipeline
    {
        VkPipeline pipeline;
        VkPipelineBindPoint bind_point;
        std::vector<VkDescriptorSetLayout> set_layouts;
        VkPipelineLayout pipeline_layout;
        VulkanBindings bindings;
        std::unordered_map<uint32_t, VkPushConstantRange> push_constants;
    };

    void CreateShader(VulkanShader *shader, VkDevice device, const uint32_t *code, uint32_t code_size);

    VkDescriptorSetLayout CreateDescriptorSetLayout(VkDevice device, const std::vector<VkReflectionDescriptorBinding> &descriptor_bindings, uint32_t set, VkDescriptorSetLayoutCreateFlags flags);

    void MergePushConstants(std::unordered_map<uint32_t, VkPushConstantRange> &dst, const std::unordered_map<uint32_t, VkPushConstantRange> &src);

    void MergeShaderBindings(std::vector<VkReflectionDescriptorBinding> &dst, const std::vector<VkReflectionDescriptorBinding> src);

    void DestroyShader(VulkanShader *shader, VkDevice device);

    void CreatePipelineBindings(const std::unordered_map<uint32_t, std::vector<VkReflectionDescriptorBinding>> &descriptor_sets,
                                VkDevice device, VulkanPipeline *pipeline,
                                VkDescriptorPool descriptor_pool,
                                uint32_t total_sets,
                                VulkanBindings *out_bindings);

} // namespace mirai