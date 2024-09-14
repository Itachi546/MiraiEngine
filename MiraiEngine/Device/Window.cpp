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
            glfwInit();
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        glfwSetErrorCallback([](int error_code, const char *description)
                             { Log::Error("GLFWERROR:", error_code, " Description:", description); });

        Log::Info("Creating Window ...");
        glfw_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);

        if (glfw_window == nullptr)
            ASSERT_MSG("Failed to Create Window");

        auto monitor = glfwGetPrimaryMonitor();
        auto videoMode = glfwGetVideoMode(monitor);
        fullscreen_width = videoMode->width;
        fullscreen_height = videoMode->height;

        glfwSetWindowCloseCallback(glfw_window, [](GLFWwindow *window)
                                   { glfwSetWindowShouldClose(window, true); });
        Instance = this;
    }

    void Window::set_title(const std::string &title)
    {
        this->title = title;
        glfwSetWindowTitle(glfw_window, title.c_str());
    }

    void Window::update()
    {
        glfwPollEvents();
    }

    bool Window::is_closed()
    {
        return glfwWindowShouldClose(glfw_window) == 1;
    }

    void Window::set_fullscreen(bool fullscreen)
    {
        if (this->fullscreen == fullscreen)
            return;
        this->fullscreen = fullscreen;

        GLFWmonitor *monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode *videoMode = glfwGetVideoMode(monitor);
        fullscreen_width = videoMode->width;
        fullscreen_height = videoMode->height;
        if (fullscreen)
        {
            glfwSetWindowMonitor(glfw_window, monitor, 0, 0, fullscreen_width, fullscreen_height, GLFW_DONT_CARE);
            Log::Info("Enabling Fullsceen");
        }
        else
        {
            Log::Info("Disabling Fullscreen");
            int xpos = (fullscreen_width - width) / 2;
            int ypos = (fullscreen_height - height) / 2;
            glfwSetWindowMonitor(glfw_window, nullptr, xpos, ypos, width, height, GLFW_DONT_CARE);
        }
    }

    void Window::set_size(int width, int height)
    {
        Log::Info("Resizing Window");
        int xpos = (fullscreen_width - width) / 2;
        int ypos = (fullscreen_height - height) / 2;
        glfwSetWindowMonitor(glfw_window, nullptr, xpos, ypos, width, height, GLFW_DONT_CARE);
    }

    Window::~Window()
    {
        Log::Info("Destroying Window...");
        glfwTerminate();
    }

} // namespace mirai