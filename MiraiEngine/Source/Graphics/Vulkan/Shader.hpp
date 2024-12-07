#include "Vulkan.hpp"
#include "Graphics/RenderingDevice.hpp"
#include "Common/ResourcePool.hpp"

#include <vector>
#include <unordered_map>

VK_DEFINE_HANDLE(VmaAllocation);
namespace mirai {
    struct VkReflectionDescriptorBinding {
        uint32_t binding;
        VkDescriptorType descriptor_type;
        VkShaderStageFlags shader_stage;
    };

    struct VkReflectionDescriptorSet {
        uint32_t set;
        std::vector<VkReflectionDescriptorBinding> bindings;
    };

    struct VulkanTexture {
        uint32_t width, height, depth;
        uint32_t mip_levels, array_layers;

        VkImageAspectFlags image_aspect;
        VkFormat format;
        VkImageType image_type;

        VkImage image;
        VkImageView image_view;
        VmaAllocation allocation;

        VkSampler sampler;
        VkImageLayout current_layout;
        VkAccessFlags2 access_flags;
        VkPipelineStageFlags2 stage_mask;
    };

    struct VulkanQuery {
        VkQueryPool query_pool;
        VkQueryType type;
    };

    struct VulkanBuffer {
        VkBuffer buffer;
        VmaAllocation allocation;
        uint32_t size;
        void *buffer_ptr;
    };

    struct VulkanShader {
        VkShaderModule shader;
        VkShaderStageFlagBits shader_stage;
        std::vector<VkReflectionDescriptorSet> descriptor_sets;
        std::unordered_map<uint32_t, VkPushConstantRange> push_constants;
        bool support_bindless_texture;
    };

    struct VulkanBindingInfo {
        VkWriteDescriptorSet binding;
        ID resource_id;
    };

    struct VulkanUniformSet {
        std::vector<UniformLayout> uniform_layout;
        uint32_t set_id;
        VkDescriptorSet descriptor_set;
        VkDescriptorPool descriptor_pool;
    };

    /*
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
    */
    /*
    struct VulkanBindings
    {
        std::vector<VulkanDescriptorSet> descriptor_sets;
        std::unordered_map<uint32_t, VulkanBindingLookupInfo> lookup_info;
    };
    */

    struct VulkanPipeline {
        VkPipeline pipeline;
        VkPipelineBindPoint bind_point;
        VkPipelineLayout pipeline_layout;
        bool support_bindless_texture;
    };

    void CreateShader(VulkanShader *shader, VkDevice device, const uint32_t *code, uint32_t code_size);

    VkDescriptorSetLayout CreateDescriptorSetLayout(VkDevice device, VkDescriptorSetLayoutBinding *bindings, uint32_t binding_count, VkDescriptorSetLayoutCreateFlags flags, void *p_next);

    uint64_t GetDescriptorSetLayoutHash(const std::vector<VkReflectionDescriptorBinding> &bindings, uint32_t set);
    uint64_t GetDescriptorSetLayoutHash(UniformLayout *uniforms, uint32_t count, uint32_t set);

    void MergePushConstants(std::unordered_map<uint32_t, VkPushConstantRange> &dst, const std::unordered_map<uint32_t, VkPushConstantRange> &src);

    void MergeShaderBindings(std::vector<VkReflectionDescriptorBinding> &dst, const std::vector<VkReflectionDescriptorBinding> src);

    void DestroyShader(VulkanShader *shader, VkDevice device);

    /*
    void CreatePipelineBindings(const std::unordered_map<uint32_t, std::vector<VkReflectionDescriptorBinding>> &descriptor_sets,
                                VkDevice device, VulkanPipeline *pipeline,
                                VkDescriptorPool descriptor_pool,
                                uint32_t total_sets,
                                VulkanBindings *out_bindings);
    */

} // namespace mirai