#include "Instance.hpp"

#include <vector>

namespace mirai
{
    VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                               VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                                               const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                               void *pUserData)
    {

        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        {
            Log::Error(pCallbackData->pMessage);
        }
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        {
            Log::Warn(pCallbackData->pMessage);
        }
        else
        {
            Log::Debug(pCallbackData->pMessage);
        }
        return VK_FALSE;
    }

    static bool is_validation_layers_available(const std::vector<const char *> &requested_layers)
    {
        uint32_t instance_layer_count;
        VK_CHECK(vkEnumerateInstanceLayerProperties(&instance_layer_count, nullptr));

        std::vector<VkLayerProperties> supported_layers(instance_layer_count);
        VK_CHECK(vkEnumerateInstanceLayerProperties(&instance_layer_count, supported_layers.data()));

        for (auto &requested : requested_layers)
        {
            bool available = false;
            for (auto &layer : supported_layers)
            {
                if (strcmp(layer.layerName, requested) == 0)
                {
                    available = true;
                    break;
                }
            }

            if (!available)
            {
                Log::Error("Failed to find instance layer: " + std::string(requested));
                return false;
            }
        }
        return true;
    }

    static bool is_instance_extensions_available(const std::vector<const char *> &requested_extensions)
    {
        uint32_t extension_count = 0;
        VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr));
        std::vector<VkExtensionProperties> supported_extensions(extension_count);
        VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, supported_extensions.data()));

        for (auto &requested : requested_extensions)
        {
            bool available = false;
            for (auto &supported : supported_extensions)
            {
                if (std::strcmp(requested, supported.extensionName) == 0)
                {
                    available = true;
                    break;
                }
            }

            if (!available)
            {
                Log::Error("Failed to find instance extension: " + std::string(requested));
                return false;
            }
        }

        return true;
    }

    VkInstance CreateInstance(const std::vector<const char *> &validation_layers, const std::vector<const char *> &instance_extensions, bool enable_validation)
    {
        VK_CHECK(volkInitialize());

        if (!is_instance_extensions_available(instance_extensions))
            Log::Fatal("Vulkan::Failed to find all the required device extensions...");
        if (!is_validation_layers_available(validation_layers))
            Log::Fatal("Vulkan::Failed to find all the required validation layers...");

        VkApplicationInfo app_info = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "MiraiEngine",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = VULKAN_API_VERSION,
        };

        VkInstanceCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = nullptr,
            .pApplicationInfo = &app_info,
            .enabledLayerCount = static_cast<uint32_t>(validation_layers.size()),
            .ppEnabledLayerNames = validation_layers.data(),
            .enabledExtensionCount = static_cast<uint32_t>(instance_extensions.size()),
            .ppEnabledExtensionNames = instance_extensions.data(),
        };

        VkDebugUtilsMessengerCreateInfoEXT debug_utils_create_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        if (enable_validation)
        {
            debug_utils_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
            debug_utils_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
            debug_utils_create_info.pfnUserCallback = DebugUtilsMessengerCallback;
        }
        create_info.pNext = &debug_utils_create_info;

        VkInstance instance = VK_NULL_HANDLE;
        VK_CHECK(vkCreateInstance(&create_info, nullptr, &instance));

        return instance;
    }

    VkDebugUtilsMessengerEXT CreateDebugUtilMessanger(VkInstance instance)
    {
        VkDebugUtilsMessengerCreateInfoEXT debug_utils_create_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        debug_utils_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        debug_utils_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
        debug_utils_create_info.pfnUserCallback = DebugUtilsMessengerCallback;

        VkDebugUtilsMessengerEXT debug_utils_messenger;
        VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance, &debug_utils_create_info, nullptr, &debug_utils_messenger));
        return debug_utils_messenger;
    }
} // namespace mirai