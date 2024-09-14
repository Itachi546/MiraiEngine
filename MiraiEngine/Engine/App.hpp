#pragma once

#include <string>

#include "Common/CommonInclude.hpp"

namespace mirai
{
    class App
    {
      public:
        explicit App(const std::string &name)
            : name(name)
        {
        }

        virtual void start() = 0;

        virtual void update() = 0;

        void set_name(const std::string &name) { this->name = name; }
        std::string get_name() const { return name; }

        virtual ~App() = default;

      private:
        bool started = false;
        std::string name;
    };
} // namespace mirai