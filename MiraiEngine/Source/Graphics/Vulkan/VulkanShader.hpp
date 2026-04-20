#include "Vulkan.hpp"
#include "Graphics/RenderingDevice.hpp"
#include "Common/ResourcePool.hpp"
#include "Common/ShaderReflect.hpp"
#include "Common/HashMap.hpp"
#include <vector>

VK_DEFINE_HANDLE(VmaAllocation);
namespace mirai {
    struct VulkanTexture {
        uint32_t create_flags;
        uint32_t width, height, depth;
        uint32_t mip_levels, array_layers;

        VkImageAspectFlags image_aspect;
        VkFormat format;
        VkImageType image_type;
        VkImageViewType image_view_type;

        VkImage image;
        std::vector<VkImageView> image_views;
        VmaAllocation allocation;

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
        VkDeviceSize device_address;

        VkAccessFlags2 access_flags;
        VkPipelineStageFlags2 stage_mask;
    };

    struct VulkanShader {
        VkShaderModule shader;
        VkShaderStageFlagBits shader_stage;
        std::vector<ShaderReflectionDescriptorSetInfo> descriptor_sets_info;
        std::vector<ShaderReflectionPushConstant> push_constants_info;
    };

    struct VulkanBindingInfo {
        VkWriteDescriptorSet binding;
        ID resource_id;
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
        HashMap<uint32_t, VulkanBindingLookupInfo> lookup_info;
    };
    */

    struct VulkanPipeline {
        VkPipeline pipeline;
        VkPipelineBindPoint bind_point;
        bool support_bindless_texture;
    };

    void CreateShader(VulkanShader *shader, VkDevice device, const uint32_t *code, uint32_t code_size);

    void MergePushConstants(HashMap<uint32_t, ShaderReflectionPushConstant> &dst, const HashMap<uint32_t, ShaderReflectionPushConstant> &src);

    void MergeShaderBindings(std::vector<ShaderReflectionDescriptorBinding> &dst, const std::vector<ShaderReflectionDescriptorBinding> &src);

    void DestroyShader(VulkanShader *shader, VkDevice device);

} // namespace mirai