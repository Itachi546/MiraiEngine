#pragma once

#include "Graphics/RenderingDevice.hpp"
#include <string>
#include "Common/HashMap.hpp"

namespace mirai {

    struct ShaderReflectionDescriptorBinding {
        std::string name;
        uint32_t binding;
        uint32_t shader_stage;
        BindingType binding_type;
    };

    struct ShaderReflectionDescriptorSetInfo {
        uint32_t set;
        std::vector<ShaderReflectionDescriptorBinding> bindings;
    };

    struct ShaderReflectionPushConstantMember {
        uint32_t offset;
        uint32_t size;
        uint32_t padded_size;
    };

    struct ShaderReflectionPushConstant {
        std::string name;
        uint32_t shader_stage;
        uint32_t offset;
        uint32_t size;
        HashMap<std::string, ShaderReflectionPushConstantMember> field_info;
    };

    struct ShaderReflection {
        std::vector<ShaderReflectionDescriptorSetInfo> descriptor_infos;
        std::vector<ShaderReflectionPushConstant> push_constants;
    };
} // namespace mirai