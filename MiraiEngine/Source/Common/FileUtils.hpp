#pragma once

#include <string>
#include <optional>
#include <memory>

namespace mirai {
namespace utils {
    std::optional<std::string> read_file(const std::string &filename);
    std::optional<std::string> read_file_binary(const std::string &filename);
    std::string get_file_extension(const std::string &filename);
    std::string get_filename(const std::string &filename);
    std::string trim_file_extension(const std::string &filename);
    std::string replace_file_extension(const std::string &filename, const std::string &new_extension);
    std::string get_base_path(const std::string &path);

    std::unique_ptr<unsigned char, void (*)(void *)> load_from_file(FILE *file, int *width, int *height, int *n_channel, int req_channel);
    std::unique_ptr<unsigned char, void (*)(void *)> load_image(const char *filename, int *width, int *height, int *n_channel, int req_channel = 0);
    std::unique_ptr<uint16_t, void (*)(void *)> load_image16(const char *filename, int *width, int *height, int *n_channel, int req_channel = 0);
    std::unique_ptr<float, void (*)(void *)> load_image_float(const char *filename, int *width, int *height, int *n_channel, int req_channel = 0);
}
} // namespace mirai::utils