#include "Window.hpp"
#include "Engine/Log.hpp"
#include "InputDevice.hpp"

#include <GLFW/glfw3.h>

namespace mirai {
    void WindowKeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods) {
        Input::get()->set_modifiers(mods);
        Input::get()->set_state(Key(key), action != GLFW_RELEASE);
    }

    void WindowButtonCallback(GLFWwindow *window, int button, int action, int mods) {
        Input::get()->set_modifiers(mods);
        Input::get()->set_state(Key(button), action != GLFW_RELEASE);
    }

    void WindowSizeCallback(GLFWwindow *glfw_window, int width, int height) {
        Window *window = static_cast<Window *>(glfwGetWindowUserPointer(glfw_window));
        window->width = static_cast<uint32_t>(width);
        window->height = static_cast<uint32_t>(height);
    }

    void WindowCursorPosCallback(GLFWwindow *glfw_window, double x, double y) {
    }

    void WindowScrollCallback(GLFWwindow *glfw_window, double xoffset, double yoffset) {
        Window *window = static_cast<Window *>(glfwGetWindowUserPointer(glfw_window));
        glm::vec2 current_scroll{static_cast<float>(xoffset), static_cast<float>(yoffset)};
        window->mouse_scroll_delta = current_scroll - window->mouse_scroll;
        window->mouse_scroll = current_scroll;
    }

    void WindowIconifyCallback(GLFWwindow *glfw_window, int iconified) {
        Window *window = static_cast<Window *>(glfwGetWindowUserPointer(glfw_window));
        if (iconified == GLFW_TRUE)
            window->set_minimized(true);
        else
            window->set_minimized(false);
    }

    Window *Window::Instance = nullptr;

    Window::Window(uint32_t width, uint32_t height, const std::string &title) : width(width),
                                                                                height(height),
                                                                                title(title),
                                                                                fullscreen(false),
                                                                                mouse_pos(0.0f, 0.0f),
                                                                                mouse_pos_delta(0.0f, 0.0f),
                                                                                mouse_scroll(0.0f, 0.0f),
                                                                                mouse_scroll_delta(0.0f, 0.0f),
                                                                                minimized(false) {
        ASSERT(Instance == nullptr);
        Instance = this;

        if (!glfwInit()) {
            glfwInit();
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        glfwSetErrorCallback([](int error_code, const char *description) { Log::Error("GLFWERROR:", error_code, " Description:", description); });

        Log::Info("Creating Window ...");
        glfw_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);

        ASSERT_MSG(glfw_window != nullptr, "Failed to Create Window");

        auto monitor = glfwGetPrimaryMonitor();

        glfwSetWindowUserPointer(glfw_window, this);

        glfwSetWindowCloseCallback(glfw_window, [](GLFWwindow *window) { glfwSetWindowShouldClose(window, true); });

        glfwSetKeyCallback(glfw_window, WindowKeyCallback);
        glfwSetWindowSizeCallback(glfw_window, WindowSizeCallback);
        glfwSetMouseButtonCallback(glfw_window, WindowButtonCallback);
        // glfwSetCursorPosCallback(glfw_window, WindowCursorPosCallback);
        glfwSetScrollCallback(glfw_window, WindowScrollCallback);
        glfwSetWindowIconifyCallback(glfw_window, WindowIconifyCallback);

        glfwGetWindowPos(glfw_window, &prev_x, &prev_y);

        double x, y;
        glfwGetCursorPos(glfw_window, &x, &y);
        mouse_pos = {static_cast<float>(x), static_cast<float>(y)};
    }

    void Window::set_title(const std::string &title) {
        this->title = title;
        glfwSetWindowTitle(glfw_window, title.c_str());
    }

    void Window::update() {
        glfwPollEvents();

        double x, y;
        glfwGetCursorPos(glfw_window, &x, &y);
        glm::vec2 current_pos{static_cast<float>(x), static_cast<float>(y)};
        mouse_pos_delta = current_pos - mouse_pos;
        mouse_pos = current_pos;
    }

    bool Window::is_closed() {
        return glfwWindowShouldClose(glfw_window) == 1;
    }

    void Window::set_fullscreen(bool fullscreen) {
        if (this->fullscreen == fullscreen)
            return;
        this->fullscreen = fullscreen;

        GLFWmonitor *monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode *videoMode = glfwGetVideoMode(monitor);

        if (fullscreen) {
            // Save window state
            glfwGetWindowPos(glfw_window, &prev_x, &prev_y);
            prev_width = width;
            prev_height = height;

            width = (uint32_t)videoMode->width;
            height = (uint32_t)videoMode->height;
            glfwSetWindowMonitor(glfw_window, monitor, 0, 0, width, height, videoMode->refreshRate);
            Log::Info("Enabling Fullsceen ", width, " ", height, " ", videoMode->refreshRate);
        } else {
            width = prev_width;
            height = prev_height;
            Log::Info("Disabling Fullscreen");
            glfwSetWindowMonitor(glfw_window, nullptr, static_cast<int>(prev_x), static_cast<int>(prev_y), static_cast<int>(width), static_cast<int>(height), videoMode->refreshRate);
        }
    }

    void Window::set_size(uint32_t width, uint32_t height) {
        Log::Info("Resizing Window");
        this->width = width;
        this->height = height;
        glfwSetWindowMonitor(glfw_window, nullptr, static_cast<int>(prev_x), static_cast<int>(prev_y), static_cast<int>(width), static_cast<int>(height), GLFW_DONT_CARE);
    }

    Window::~Window() {
        Log::Info("Destroying Window...");
        glfwTerminate();
    }

} // namespace mirai