#pragma once

#include "Mirai.hpp"
#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include "VulkanRenderingDevice.hpp"

using namespace mirai;

struct HDRIConverter {
    HDRIConverter() {
        device = (VulkanRenderingDevice *)RenderingDevice::get();
    }

    void set_texture(const char *filename);

    void show_options();

    TextureID texture_id;
    VulkanRenderingDevice *device;
    VkDescriptorSet descriptor_set;
};