#pragma once

#include <string>

namespace mirai {

    class Font {
      public:
        Font(const std::string &filename);

      private:
        void load_font();
        std::string filename;
    };

} // namespace mirai