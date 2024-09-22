#pragma once

#include "RenderingDevice.hpp"

namespace mirai
{
    class Material
    {
      public:
        Material(const std::string &name);

        void create_from_file(std::vector<std::string> &shader_files);

        template <typename T>
        void set_uniform(const std::string &name, const T &value, std::size_t size = 0)
        {
        }

        void set_resource(const std::string &name, void *resource)
        {
        }

        void bind(CommandBuffer *command_buffer)
        {
            // uniform_object->bind(command_buffer);
            // uniform_set->bind(command_buffer);
        }

      private:
        std::string name;
        PipelineID pipeline;

        struct UniformObject
        {
        };

        struct UniformSet
        {
        };

        UniformObject uniform_object;
        UniformSet uniform_set;
    };
} // namespace mirai