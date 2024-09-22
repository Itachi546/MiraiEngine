#pragma once

#include "Graphics/Material.hpp"

class FullScreenMaterial : public mirai::Material
{
  public:
    FullScreenMaterial() : mirai::Material("FullScreenMaterial")
    {
        create_from_file(std::vector<std::string>{
            "SPIRV/main.vert.spv",
            "SPIRV/main.frag.spv",
        });
    }

  private:
};