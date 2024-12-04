#include "HDRIConverter.hpp"
#include "Common/FileUtils.hpp"
#include "Math/MathUtils.hpp"

void HDRIConverter::set_texture(const char *filename) {
    if (texture_id.is_valid()) {
        ImGui_ImplVulkan_RemoveTexture(descriptor_set);
        device->destroy_textures(&texture_id, 1);
        descriptor_set = VK_NULL_HANDLE;
    }
    path = filename;
    
    int n_channel;
    float *data = utils::load_image_float(filename, &width, &height, &n_channel, 4);
    size_in_mb = utils::bytes_to_mb(width * height * sizeof(float) * 4);

    SamplerDescription sampler_desc = SamplerDescription::create();
    TextureDescription texture_desc = {
        .width = (uint32_t)width,
        .height = (uint32_t)height,
        .depth = 1,
        .mip_levels = 1,
        .array_layers = 1,
        .texture_type = TEXTURE_TYPE_2D,
        .format = FORMAT_R32G32B32A32_SFLOAT,
        .usage_flags = TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_TRANSFER_DST_BIT,
        .sampler_desc = &sampler_desc,
    };

    texture_id = device->create_texture(&texture_desc, "hdri_texture");
    rendering_utils::copy_texture_immediate(texture_id, data, width * height * sizeof(float) * 4);
    utils::free_image(data);

    VulkanTexture *texture = device->access_texture(texture_id);
    descriptor_set = ImGui_ImplVulkan_AddTexture(texture->sampler, texture->image_view, texture->current_layout);
}

void HDRIConverter::show_options() {
    ImGui::Begin("HDRI Texture", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    if (texture_id.is_valid()) {
        ImGui::Image((ImTextureID)(intptr_t)descriptor_set, ImVec2{1024, 512});
        ImGui::Text("Path: %s", path.c_str());
        ImGui::Text("Width: %d", width);
        ImGui::Text("Height: %d", height);
        ImGui::Text("Size: %.2fmb", size_in_mb);
        ImGui::Button("Convert To Cubemap");
        ImGui::Button("Generate Irradiance map");
        ImGui::Button("Export");
    }
    ImGui::End();
}

HDRIConverter::~HDRIConverter() {
    if (texture_id.is_valid())
        device->destroy_textures(&texture_id, 1);
}
