#pragma once

#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include "Graphics/Vulkan/VulkanRenderingDevice.hpp"

using namespace mirai;

struct HDRIConverterPass;

struct HDRIConverterUI {
    HDRIConverterUI(std::shared_ptr<HDRIConverterPass> converter_pass) : converter_pass(converter_pass) {
        device = (VulkanRenderingDevice *)RenderingDevice::get();
    }

    void set_texture(const char *filename);

    void show_options();

    ~HDRIConverterUI();

    TextureID texture_id;
    int width;
    int height;
    std::string path;
    float size_in_mb;

    std::shared_ptr<HDRIConverterPass> converter_pass;

    VulkanRenderingDevice *device;
    VkDescriptorSet descriptor_set;
};