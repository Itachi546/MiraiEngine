#pragma once

#include "Common/CommonInclude.hpp"


#include <glm/glm.hpp>
#include <string>

struct GLFWwindow;
struct GLFWmonitor;

namespace mirai
{
    class Window
    {
      public:
        Window(const Window &window) = delete;
        Window operator=(const Window &window) = delete;

        Window(uint32_t width, uint32_t height, const std::string &title);

        static Window *get() { return Instance; }

        void set_title(const std::string &title);

        void update();

        bool is_closed();

        std::string_view get_title() const { return title; }

        float get_aspect_ratio() const { return static_cast<float>(width) / static_cast<float>(height); }

        void get_size(uint32_t *width, uint32_t *height) const
        {
            if (fullscreen)
            {
                *width = fullscreen_width;
                *height = fullscreen_height;
            }
            else
            {
                *width = this->width;
                *height = this->height;
            }
        }

        void set_fullscreen(bool fullscreen);

        void set_size(uint32_t width, uint32_t height);

        glm::vec2 get_mouse_position() const
        {
            return mouse_pos;
        }

        glm::vec2 get_mouse_delta() const
        {
            return mouse_pos_delta;
        }

        glm::vec2 get_mouse_scroll() const
        {
            return mouse_scroll;
        }

        glm::vec2 get_mouse_scroll_delta() const
        {
            return mouse_scroll_delta;
        }

        void *get_window_ptr()
        {
            return glfw_window;
        }

        void set_minimized(bool state)
        {
            this->minimized = state;
        }

        bool is_minimized() const
        {
            return this->minimized;
        }

        ~Window();

      private:
        friend void WindowSizeCallback(GLFWwindow *window, int width, int height);
        friend void WindowCursorPosCallback(GLFWwindow *window, double x, double y);
        friend void WindowScrollCallback(GLFWwindow *window, double xoffset, double yoffset);

        static Window *Instance;

        GLFWwindow *glfw_window;
        uint32_t width, height;
        uint32_t fullscreen_width, fullscreen_height;
        std::string title;
        bool fullscreen;
        bool minimized;

        glm::vec2 mouse_pos;
        glm::vec2 mouse_pos_delta;
        glm::vec2 mouse_scroll;
        glm::vec2 mouse_scroll_delta;
    };
} // namespace mirai