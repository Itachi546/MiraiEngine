#pragma once

#include "Scene/ShaderMaterial.hpp"

class FullScreenMaterial : public mirai::ShaderMaterial {
  public:
    FullScreenMaterial() : mirai::ShaderMaterial("FullScreenMaterial") {
        create_from_file(std::vector<std::string>{
            "SPIRV/main.vert.spv",
            "SPIRV/main.frag.spv",
        });
    }

  private:
};