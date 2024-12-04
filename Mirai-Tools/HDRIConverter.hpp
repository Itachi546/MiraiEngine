#pragma once

#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include "Graphics/Vulkan/VulkanRenderingDevice.hpp"

using namespace mirai;

struct HDRIConverter {
    HDRIConverter() {
        device = (VulkanRenderingDevice *)RenderingDevice::get();
    }

    void set_texture(const char *filename);

    void show_options();

    ~HDRIConverter();

    TextureID texture_id;
    int width;
    int height;
    std::string path;
    float size_in_mb;
    std::thread load_thread;

    VulkanRenderingDevice *device;
    VkDescriptorSet descriptor_set;
};