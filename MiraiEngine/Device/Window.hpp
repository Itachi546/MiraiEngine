#pragma once

#include <GLFW/glfw3.h>
#include <string>

#include "Common/CommonInclude.hpp"

struct GLFWwindow;
struct GLFWmonitor;

namespace mirai
{
    class Window
    {
      public:
        Window(const Window &window) = delete;
        Window operator=(const Window &window) = delete;

        Window(int width, int height, const std::string &title);

        static Window *get() { return Instance; }

        void set_title(const std::string &title);

        void update();

        bool is_closed();

        std::string_view get_title() const { return title; }

        float get_aspect_ratio() const { return static_cast<float>(width) / static_cast<float>(height); }

        void get_size(int *width, int *height) const
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

        void set_size(int width, int height);

        ~Window();

      private:
        static Window *Instance;

        GLFWwindow *glfw_window;
        int width, height;
        int fullscreen_width, fullscreen_height;
        std::string title;
        bool fullscreen;
    };
} // namespace mirai