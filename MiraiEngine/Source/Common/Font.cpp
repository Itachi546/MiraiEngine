#include "Font.hpp"

namespace mirai {
    Font::Font(const std::string &filename) : filename(filename) {
        load_font();
    }

    void Font::load_font() {
    }
} // namespace mirai