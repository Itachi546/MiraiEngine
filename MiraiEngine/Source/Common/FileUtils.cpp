#include "FileUtils.hpp"

#include <fstream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace mirai {
namespace utils {
    std::optional<std::string> read_file_internal(const std::string &filename, std::ios::openmode mode) {
        std::ifstream inFile(filename, mode);
        if (!inFile)
            return {};
        return std::string{
            (std::istreambuf_iterator<char>(inFile)),
            std::istreambuf_iterator<char>()};
    }

    std::optional<std::string> read_file(const std::string &filename) {
        return read_file_internal(filename, std::ios::in);
    }

    std::optional<std::string> read_file_binary(const std::string &filename) {
        return read_file_internal(filename, std::ios::binary);
    }

    std::string get_file_extension(const std::string &filename) {
        size_t index = filename.find_last_of('.');
        if (index != std::string::npos) {
            return filename.substr(index + 1);
        }
        return "";
    }

    std::string trim_file_extension(const std::string &filename) {
        size_t index = filename.find_last_of('.');
        if (index == std::string::npos)
            return filename;
        return filename.substr(0, index - 1);
    }

    std::string get_base_path(const std::string &path) {
        size_t index = path.find_last_of('/');
        if (index == std::string::npos)
            return path;
        return path.substr(0, index + 1);
    }

    bool get_image_info(const char *filename, int *width, int *height, int *n_channel) {
        return stbi_info(filename, width, height, n_channel) == 1;
    }

    std::unique_ptr<unsigned char, void (*)(void *)> load_from_file(FILE *file, int *width, int *height, int *n_channel, int req_channel) {
        return std::unique_ptr<unsigned char, void (*)(void *)>(
            stbi_load_from_file(file, width, height, n_channel, req_channel),
            stbi_image_free);
    }

    std::unique_ptr<unsigned char, void (*)(void *)> load_image(const char *filename, int *width, int *height, int *n_channel, int req_channel) {
        return std::unique_ptr<unsigned char, void (*)(void *)>(
            stbi_load(filename, width, height, n_channel, req_channel),
            stbi_image_free);
    }

    std::unique_ptr<uint16_t, void (*)(void *)> load_image16(const char *filename, int *width, int *height, int *n_channel, int req_channel) {
        return std::unique_ptr<uint16_t, void (*)(void *)>(
            stbi_load_16(filename, width, height, n_channel, req_channel),
            stbi_image_free);
    }

    std::unique_ptr<float, void (*)(void *)> load_image_float(const char *filename, int *width, int *height, int *n_channel, int req_channel) {
        stbi_set_flip_vertically_on_load(true);
        float *data = stbi_loadf(filename, width, height, n_channel, req_channel);
        stbi_set_flip_vertically_on_load(false);
        return std::unique_ptr<float, void (*)(void *)>(
            data,
            stbi_image_free);
    }

    std::string replace_file_extension(const std::string &filename, const std::string &new_extension) {
        size_t index = filename.find_last_of('.');
        if (index == std::string::npos) {
            return filename.substr(0, index) + "." + new_extension;
        }
        return filename.substr(0, index + 1) + new_extension;
    }

    std::string get_filename(const std::string &filename) {
        size_t index = filename.find_last_of('/');
        if (index == std::string::npos)
            return filename;
        return filename.substr(index + 1);
    }

}
} // namespace mirai::utils
