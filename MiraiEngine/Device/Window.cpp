#include "Window.hpp"
#include "Engine/Log.hpp"
#include "InputDevice.hpp"

namespace mirai
{
    static void WindowKeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
    {
        Input::get()->set_modifiers(mods);
        Input::get()->set_state(Key(key), action != GLFW_RELEASE);
    }

    static void WindowButtonCallback(GLFWwindow *window, int button, int action, int mods)
    {
        Input::get()->set_modifiers(mods);
        Input::get()->set_state(Key(button), action != GLFW_RELEASE);
    }

    static void WindowSizeCallback(GLFWwindow *glfw_window, int width, int height)
    {
        Window *window = static_cast<Window *>(glfwGetWindowUserPointer(glfw_window));
        window->width = width;
        window->height = height;
    }

    static void WindowCursorPosCallback(GLFWwindow *glfw_window, double x, double y)
    {
        Window *window = static_cast<Window *>(glfwGetWindowUserPointer(glfw_window));
        glm::vec2 current_pos{static_cast<float>(x), static_cast<float>(y)};
        window->mouse_pos_delta = current_pos - window->mouse_pos;
        window->mouse_pos = current_pos;
    }

    static void WindowScrollCallback(GLFWwindow *glfw_window, double xoffset, double yoffset)
    {
        Window *window = static_cast<Window *>(glfwGetWindowUserPointer(glfw_window));
        glm::vec2 current_scroll{static_cast<float>(xoffset), static_cast<float>(yoffset)};
        window->mouse_scroll_delta = current_scroll - window->mouse_scroll;
        window->mouse_scroll = current_scroll;
    }

    Window *Window::Instance = nullptr;

    Window::Window(int width, int height, const std::string &title) : width(width),
                                                                      height(height),
                                                                      title(title),
                                                                      fullscreen(false),
                                                                      mouse_pos(0.0f, 0.0f),
                                                                      mouse_pos_delta(0.0f, 0.0f),
                                                                      mouse_scroll(0.0f, 0.0f),
                                                                      mouse_scroll_delta(0.0f, 0.0f)
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

        glfwSetWindowUserPointer(glfw_window, this);

        glfwSetWindowCloseCallback(glfw_window, [](GLFWwindow *window)
                                   { glfwSetWindowShouldClose(window, true); });

        glfwSetKeyCallback(glfw_window, WindowKeyCallback);
        glfwSetWindowSizeCallback(glfw_window, WindowSizeCallback);
        glfwSetMouseButtonCallback(glfw_window, WindowButtonCallback);
        glfwSetCursorPosCallback(glfw_window, WindowCursorPosCallback);
        glfwSetScrollCallback(glfw_window, WindowScrollCallback);

        double x, y;
        glfwGetCursorPos(glfw_window, &x, &y);
        mouse_pos = {static_cast<float>(x), static_cast<float>(y)};

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