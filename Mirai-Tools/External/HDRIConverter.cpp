#include "HDRIConverter.hpp"
#include "Mirai.hpp"

void HDRIConverter::set_texture(const char *filename) {
    if (texture_id == this->texture_id)
        return;
    ImGui_ImplVulkan_RemoveTexture(descriptor_set);
    descriptor_set = VK_NULL_HANDLE;

    unsigned char *data = ;

    VulkanTexture *texture = device->get_texture(texture);
    descriptor_set = ImGui_ImplVulkan_AddTexture(texture->sampler, texture->image_view, texture->layout);
}

void HDRIConverter::show_options() {
    if (texture_id.is_valid()) {
        ImGui::Image((ImTextureID)(intptr_t)descriptor_set, ImVec2{256, 256});
    }
}
