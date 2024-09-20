#pragma once

#include <string>
#include <optional>

namespace mirai
{
    namespace utils
    {
        std::optional<std::string> read_file(const std::string &filename);
        std::optional<std::string> read_file_binary(const std::string &filename);
    }
} // namespace mirai