#include "FileUtils.hpp"

#include <fstream>

namespace mirai
{
    namespace utils
    {
        std::optional<std::string> read_file_internal(const std::string &filename, std::ios::openmode mode)
        {
            std::ifstream inFile(filename, mode);
            if (!inFile)
                return {};
            return std::string{
                (std::istreambuf_iterator<char>(inFile)),
                std::istreambuf_iterator<char>()};
        }

        std::optional<std::string> read_file(const std::string &filename)
        {
            return read_file_internal(filename, std::ios::in);
        }

        std::optional<std::string> read_file_binary(const std::string &filename)
        {
            return read_file_internal(filename, std::ios::binary);
        }

    } // namespace utils
} // namespace mirai
