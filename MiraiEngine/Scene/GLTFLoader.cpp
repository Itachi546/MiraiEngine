#include "GLTFLoader.hpp"

#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h> 
#include "Scene.hpp"
#include "Component.hpp"
#include "Common/FileUtils.hpp"

namespace mirai {

    static void LoadMaterials(tinygltf::Model* model, std::vector<MaterialComponent>& materials) {

    }

    Entity ImportModel_GLTF(const std::string &filename, Scene *scene)
    {
        std::string file_extension = utils::get_file_extension(filename);

        bool ret = false;
        std::string err, warn;
        tinygltf::TinyGLTF gltf_loader;
        tinygltf::Model gltf_model;
        if (file_extension == "GLB" || file_extension == "glb")
            ret = gltf_loader.LoadBinaryFromFile(&gltf_model, &err, &warn, filename);
        else
            ret = gltf_loader.LoadASCIIFromFile(&gltf_model, &err, &warn, filename);
        
        if(!ret) {
            Log::Error("Failed to load file: ", filename);
            Log::Error("GLTF ERROR:: ", err);
            return INVALID_ENTITY;
        }

        ComponentManager* comp_manager = scene->get_component_manager();
        Entity root_entity = ecs::create_entity();
        comp_manager->add_component<NameComponent>(root_entity, utils::trim_file_extension(filename));
        scene->add_entity(root_entity); 

        std::vector<MaterialComponent> materials;
        LoadMaterials(&gltf_model, materials);

    }
} // namespace mirai