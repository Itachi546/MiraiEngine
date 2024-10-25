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

        std::string get_file_extension(const std::string &filename)
        {
            size_t index = filename.find_last_of('.');
            if (index != std::string::npos)
            {
                return filename.substr(index + 1);
            }
            return "";
        }

        std::string trim_file_extension(const std::string &filename)
        {
            size_t index = filename.find_last_of('.');
            if (index == std::string::npos)
                return filename;
            return filename.substr(0, index - 1);
        }

        std::string get_filename(const std::string &filename)
        {
            size_t index = filename.find_last_of('/');
            if (index == std::string::npos)
                return filename;
            return filename.substr(index + 1);
        }

    } // namespace utils
} // namespace mirai
