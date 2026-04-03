#pragma once

#include "Graphics/RenderingDevice.hpp"
#include <string>
#include "Common/HashMap.hpp"

namespace mirai {

    struct ShaderReflectionDescriptorBinding {
        uint32_t binding;
        uint32_t shader_stage;
        BindingType binding_type;
    };

    struct ShaderReflectionDescriptorSetInfo {
        uint32_t set;
        std::vector<ShaderReflectionDescriptorBinding> bindings;
    };

    struct ShaderReflectionPushConstant {
        uint32_t shader_stage;
        uint32_t offset;
        uint32_t size;
    };

    struct ShaderReflection {
        std::vector<ShaderReflectionDescriptorSetInfo> descriptor_infos;
        std::vector<ShaderReflectionPushConstant> push_constants;
    };
} // namespace mirai