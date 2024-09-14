#include "Window.hpp"
#include "Engine/Log.hpp"

namespace mirai
{
    Window *Window::Instance = nullptr;

    Window::Window(int width, int height, const std::string &title) : width(width),
                                                                      height(height),
                                                                      title(title),
                                                                      fullscreen(false)
    {
        if (!glfwInit())
        {
            Log::Info("Initializing GLFW ...");
            glfwInit();
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        Log::Info("Creating GLFW Window ...");
        glfwWindow = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);

        if (glfwWindow == nullptr)
            ASSERT_MSG("Failed to create window");

        glfwMakeContextCurrent(glfwWindow);

        auto monitor = glfwGetPrimaryMonitor();
        auto videoMode = glfwGetVideoMode(monitor);
        fullscreenWidth = videoMode->width;
        fullscreenHeight = videoMode->height;

        Instance = this;
    }

    void Window::set_title(const std::string &title)
    {
        this->title = title;
        glfwSetWindowTitle(glfwWindow, title.c_str());
    }

    void Window::set_fullscreen(bool fullscreen)
    {
        if (this->fullscreen == fullscreen)
            return;
        this->fullscreen = fullscreen;

        GLFWmonitor *monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode *videoMode = glfwGetVideoMode(monitor);
        fullscreenWidth = videoMode->width;
        fullscreenHeight = videoMode->height;
        if (fullscreen)
        {
            glfwSetWindowMonitor(glfwWindow, monitor, 0, 0, fullscreenWidth, fullscreenHeight, GLFW_DONT_CARE);
            Log::Info("Enabling Fullsceen");
        }
        else
        {
            Log::Info("Disabling Fullscreen");
            int xpos = (fullscreenWidth - width) / 2;
            int ypos = (fullscreenHeight - height) / 2;
            glfwSetWindowMonitor(glfwWindow, nullptr, xpos, ypos, width, height, GLFW_DONT_CARE);
        }
    }

    void Window::set_size(int width, int height)
    {
        Log::Info("Resizing Window");
        int xpos = (fullscreenWidth - width) / 2;
        int ypos = (fullscreenHeight - height) / 2;
        glfwSetWindowMonitor(glfwWindow, nullptr, xpos, ypos, width, height, GLFW_DONT_CARE);
    }

    Window::~Window()
    {
        glfwTerminate();
    }
} // namespace mirai