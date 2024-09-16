#include "Renderer.hpp"

#include "Vulkan/VulkanRenderingDevice.hpp"

namespace mirai
{
    Renderer *Renderer::Instance = nullptr;

    Renderer::Renderer()
    {
        device = std::make_unique<VulkanRenderingDevice>();
        Instance = this;
    }

    Renderer::~Renderer()
    {
    }
} // namespace mirai