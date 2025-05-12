#include "ShaderManager.hpp"
#include "ShaderMaterial.hpp"
#include "Common/Hash.hpp"

namespace mirai {
    ShaderManager* ShaderManager::Instance = nullptr;
    ShaderManager::ShaderManager() {
        Instance = this;
    }

    ShaderMaterial* ShaderManager::load(const std::string &name, const std::vector<std::string> &shaders, const ShaderMaterialProperties &props) {
        uint32_t hash = utils::djb2_hash_string(name);
        auto found = shader_material_cache.find(hash);
        if (found != shader_material_cache.end())
            return found->second.get();

        std::shared_ptr<ShaderMaterial> material = std::make_shared<ShaderMaterial>(name);
        material->create_from_file(shaders, props);
        shader_material_cache.insert(std::make_pair(hash, material));
        return material.get();
    }
    /*
    ShaderMaterial* ShaderManager::load(const std::string &name, std::shared_ptr<ShaderMaterial> material) {
        uint32_t hash = utils::djb2_hash_string(name);
        auto found = shader_material_cache.find(hash);
        if (found != shader_material_cache.end())
            return found->second;
        shader_material_cache.insert(std::make_pair(hash, material));
        return material.get();
    }
    */
    ShaderMaterial* ShaderManager::get_shader(const std::string &name) {
        uint32_t hash = utils::djb2_hash_string(name);
        auto found = shader_material_cache.find(hash);
        if (found != shader_material_cache.end())
            return found->second.get();
        return nullptr;
    }

    ShaderManager::~ShaderManager() {
        shader_material_cache.clear();
    }

} // namespace mirai