#pragma once

#include "Graphics/RenderingDevice.hpp"
#include <string>
#include "Common/HashMap.hpp"

namespace mirai {
    enum BindingType {
        BINDING_TYPE_SAMPLER = 0,
        BINDING_TYPE_COMBINED_IMAGE_SAMPLER = 1,
        BINDING_TYPE_SAMPLED_IMAGE = 2,
        BINDING_TYPE_STORAGE_IMAGE = 3,
        BINDING_TYPE_UNIFORM_BUFFER = 6,
        BINDING_TYPE_STORAGE_BUFFER = 7,
        BINDING_TYPE_ACCELERATION_STRUCTURE = 1000150000,
    };
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