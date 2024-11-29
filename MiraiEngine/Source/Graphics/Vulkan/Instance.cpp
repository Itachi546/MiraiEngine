#include "Instance.hpp"

#include <vector>
#include <string.h>

namespace mirai {
    VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsMessengerCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT object_type, uint64_t object, size_t location, int message_code, const char *p_layer_prefix, const char *p_message, void *p_user_data) {
        const char *type = (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) ? "ERROR" : (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) ? "WARNING"
                                                                                                                         : "INFO";
        char message[4096];
        snprintf(message, std::size(message), "[%s]::%s", type, p_message);

        std::cout << message << std::endl;

        if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
            assert(!"Validation error encountered!");

        return VK_FALSE;
    }

    static bool is_validation_layers_available(const std::vector<const char *> &requested_layers) {
        uint32_t instance_layer_count;
        VK_CHECK(vkEnumerateInstanceLayerProperties(&instance_layer_count, nullptr));

        std::vector<VkLayerProperties> supported_layers(instance_layer_count);
        VK_CHECK(vkEnumerateInstanceLayerProperties(&instance_layer_count, supported_layers.data()));

        for (auto &requested : requested_layers) {
            bool available = false;
            for (auto &layer : supported_layers) {
                if (strcmp(layer.layerName, requested) == 0) {
                    available = true;
                    break;
                }
            }

            if (!available) {
                Log::Error("Failed to find instance layer: " + std::string(requested));
                return false;
            }
        }
        return true;
    }

    static bool is_instance_extensions_available(const std::vector<const char *> &requested_extensions) {
        uint32_t extension_count = 0;
        VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr));
        std::vector<VkExtensionProperties> supported_extensions(extension_count);
        VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, supported_extensions.data()));

        for (auto &requested : requested_extensions) {
            bool available = false;
            for (auto &supported : supported_extensions) {
                if (strcmp(requested, supported.extensionName) == 0) {
                    available = true;
                    break;
                }
            }

            if (!available) {
                Log::Error("Failed to find instance extension: " + std::string(requested));
                return false;
            }
        }

        return true;
    }

    VkInstance CreateInstance(const std::vector<const char *> &validation_layers, const std::vector<const char *> &instance_extensions) {
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

#if ENABLE_VALIDATION
        VkValidationFeatureEnableEXT enable_validation_features[] = {
            VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
        };
        VkValidationFeaturesEXT validation_features = {VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
        validation_features.enabledValidationFeatureCount = static_cast<uint32_t>(std::size(enable_validation_features));
        validation_features.pEnabledValidationFeatures = enable_validation_features;
        create_info.pNext = &validation_features;
#endif

        VkInstance instance = VK_NULL_HANDLE;
        VK_CHECK(vkCreateInstance(&create_info, nullptr, &instance));

        return instance;
    }

    VkDebugReportCallbackEXT RegisterDebugCallback(VkInstance instance) {
        if (!vkCreateDebugReportCallbackEXT)
            return nullptr;

        VkDebugReportCallbackCreateInfoEXT create_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
            .flags = VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT,
            .pfnCallback = DebugUtilsMessengerCallback,
            .pUserData = nullptr,
        };

        VkDebugReportCallbackEXT debug_report_callback_ext = VK_NULL_HANDLE;
        VK_CHECK(vkCreateDebugReportCallbackEXT(instance, &create_info, nullptr, &debug_report_callback_ext));
        return debug_report_callback_ext;
    }
} // namespace mirai