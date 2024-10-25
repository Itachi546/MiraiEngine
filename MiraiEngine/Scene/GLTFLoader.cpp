#include "GLTFLoader.hpp"

// #define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>
#include "Scene.hpp"
#include "Component.hpp"
#include "Common/FileUtils.hpp"
#include "Common/MathUtils.hpp"

#include <glm/glm.hpp>
namespace mirai
{
    struct LoadState
    {
        Scene *scene;
        std::vector<MaterialComponent> materials;
        std::vector<MeshComponent> mesh_components;
    };

    static void LoadMaterials(const tinygltf::Model *model, std::vector<MaterialComponent> &materials)
    {
        size_t material_count = model->materials.size();
        materials.resize(material_count);

        auto LoadTexture = [](uint32_t texture_index, Colorspace color_space)
        {
            if (texture_index == 0)
                return K_INVALID_ID;
            // @TODO need to implement
            return 0u;
        };

        for (uint32_t i = 0; i < material_count; ++i)
        {
            const tinygltf::Material *gltf_material = &model->materials[i];
            std::string name = gltf_material->name;
            materials[i].name = name.size() > 0 ? std::move(name) : "Unnamed" + std::to_string(i);

            const tinygltf::PbrMetallicRoughness &pbr = gltf_material->pbrMetallicRoughness;
            materials[i].albedo = glm::vec4{pbr.baseColorFactor[0], pbr.baseColorFactor[1], pbr.baseColorFactor[2], pbr.baseColorFactor[3]};
            materials[i].emission = glm::vec4{gltf_material->emissiveFactor[0], gltf_material->emissiveFactor[1], gltf_material->emissiveFactor[2], 1.0f};
            materials[i].metallic_factor = static_cast<float>(pbr.metallicFactor);
            materials[i].roughness_factor = static_cast<float>(pbr.roughnessFactor);
            materials[i].transmission = static_cast<float>(pbr.baseColorFactor[3]);
            materials[i].receive_shadow = true;
            materials[i].cast_shadow = true;

            // Process Textures
            LoadTexture(materials[i].albedo_texture, COLOR_SPACE_SRGB);
            LoadTexture(materials[i].metallic_roughness_texture, COLOR_SPACE_LINEAR);
            LoadTexture(materials[i].normal_texture, COLOR_SPACE_LINEAR);
            LoadTexture(materials[i].occlusion_texture, COLOR_SPACE_LINEAR);
        }
    }

    static uint8_t *GetBufferPtr(const tinygltf::Model *model, const tinygltf::Accessor accessor)
    {
        const tinygltf::BufferView &buffer_view = model->bufferViews[accessor.bufferView];
        return (uint8_t *)(model->buffers[buffer_view.buffer].data.data() + accessor.byteOffset + buffer_view.byteOffset);
    }

    void LoadMeshes(const tinygltf::Model *model, MeshDataComponent *meshdata_component, LoadState *load_state)
    {
        size_t mesh_count = model->meshes.size();
        std::vector<Vertex> &vertices = meshdata_component->vertices;
        std::vector<uint32_t> &indices = meshdata_component->indices;

        std::vector<MeshComponent> &out_mesh_components = load_state->mesh_components;
        for (uint32_t i = 0; i < model->meshes.size(); ++i)
        {
            const tinygltf::Mesh &gltf_mesh = model->meshes[i];
            MeshComponent &mesh_component = out_mesh_components.emplace_back(MeshComponent{});
            mesh_component.mesh_data_comp_index = 0;

            for (const auto &primitive : gltf_mesh.primitives)
            {
                uint32_t vertex_offset = static_cast<uint32_t>(vertices.size() * sizeof(Vertex));
                uint32_t index_offset = static_cast<uint32_t>(indices.size() * sizeof(uint32_t));

                auto position_attributes = primitive.attributes.find("POSITION");
                const tinygltf::Accessor position_accessor = model->accessors[position_attributes->second];
                float *positions = (float *)GetBufferPtr(model, position_accessor);

                float *normals = nullptr;
                auto normal_attributes = primitive.attributes.find("NORMAL");
                if (normal_attributes != primitive.attributes.end())
                    normals = (float *)GetBufferPtr(model, model->accessors[normal_attributes->second]);

                float *tangents = nullptr;
                auto tangent_attributes = primitive.attributes.find("TANGENT");
                if (tangent_attributes != primitive.attributes.end())
                    tangents = (float *)GetBufferPtr(model, model->accessors[tangent_attributes->second]);

                float *uvs = nullptr;
                auto uv_attributes = primitive.attributes.find("TEXCOORD_0");
                if (uv_attributes != primitive.attributes.end())
                    uvs = (float *)GetBufferPtr(model, model->accessors[uv_attributes->second]);

                uint32_t num_position = static_cast<uint32_t>(position_accessor.count);
                for (uint32_t i = 0; i < num_position; ++i)
                {
                    Vertex &vertex = vertices.emplace_back();
                    vertex.position.x = positions[i * 3];
                    vertex.position.y = positions[i * 3 + 1];
                    vertex.position.z = positions[i * 3 + 2];

                    glm::vec3 normal;
                    if (normals != nullptr)
                        normal = {normals[i * 3], normals[i * 3 + 1], normals[i * 3 + 2]};
                    else
                        normal = {0.0f, 1.0f, 0.0f};

                    normal = glm::normalize(normal);
                    vertex.normal = utils::pack_vec3_to_u32(normal.x, normal.y, normal.z);

                    glm::vec3 tangent;
                    if (tangents != nullptr)
                        tangent = {tangents[i * 3], tangents[i * 3 + 1], tangents[i * 3 + 2]};
                    else
                        tangent = {1.0f, 0.0f, 0.0f};

                    tangent = glm::normalize(tangent);
                    vertex.tangent = utils::pack_vec3_to_u32(tangent.x, tangent.y, tangent.z);

                    glm::vec3 bitangent = glm::cross(normal, tangent);
                    vertex.bitangent = utils::pack_vec3_to_u32(bitangent.x, bitangent.y, bitangent.z);
                }

                const tinygltf::Accessor &indices_accessor = model->accessors[primitive.indices];
                uint32_t index_count = static_cast<uint32_t>(indices_accessor.count);
                if (indices_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                {
                    uint32_t *indices_ptr = (uint32_t *)GetBufferPtr(model, indices_accessor);
                    indices.insert(indices.end(), indices_ptr, indices_ptr + index_count);
                }
                else if (indices_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                {
                    uint16_t *indices_ptr = (uint16_t *)GetBufferPtr(model, indices_accessor);
                    indices.insert(indices.end(), indices_ptr, indices_ptr + index_count);
                }

                MeshComponent::MeshSubset &mesh_subset = mesh_component.mesh_subsets.emplace_back();
                mesh_subset.vertex_buffer = {
                    .offset = vertex_offset,
                    .size = (num_position / 3) * sizeof(Vertex),
                };
                mesh_subset.index_buffer = {
                    .offset = index_offset,
                    .size = index_count * sizeof(uint32_t),
                };
                mesh_subset.vertex_count = index_count;
            }
        }
    } // namespace mirai

    void ParseNodes(const tinygltf::Model *model, int node_index, Entity parent, LoadState *load_state)
    {
        Entity entity = ecs::create_entity();
        const tinygltf::Node *node = &model->nodes[node_index];
        Scene *scene = load_state->scene;
        ComponentManager *comp_manager = scene->get_component_manager();

        // NameComponent
        std::string name = node->name.empty() ? ("Mesh" + std::to_string(node_index)) : node->name;
        comp_manager->add_component<NameComponent>(entity, name);
        comp_manager->add_component<HierarchyComponent>(entity);

        // TransformComponent
        TransformComponent &transform = comp_manager->add_component<TransformComponent>(entity);
        if (node->translation.size() > 0)
            transform.position = {(float)node->translation[0], (float)node->translation[1], (float)node->translation[2]};
        if (node->rotation.size() > 0)
            transform.rotation = {(float)node->rotation[3], (float)node->rotation[0], (float)node->rotation[1], (float)node->rotation[2]};
        if (node->scale.size() > 0)
            transform.scale = {node->scale[0], node->scale[1], node->scale[2]};

        // HierarchyComponent
        if (!comp_manager->has_component<HierarchyComponent>(parent))
            comp_manager->add_component<HierarchyComponent>(parent);

        // Update Hierarchy
        HierarchyComponent *parent_hierarchy = comp_manager->get_component<HierarchyComponent>(parent);
        HierarchyComponent *child_hierarchy = comp_manager->get_component<HierarchyComponent>(entity);

        child_hierarchy->set_parent(parent);
        parent_hierarchy->add_children(entity);

        // Add Mesh Component
        uint32_t mesh_id = node->mesh;
        if (mesh_id >= 0)
        {
            ASSERT(mesh_id < load_state->mesh_components.size());
            comp_manager->add_component<MeshComponent>(entity, load_state->mesh_components[mesh_id]);
        }

        for (const auto &child : node->children)
            ParseNodes(model, child, entity, load_state);
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

        if (!ret)
        {
            Log::Warn("GLTF ERROR:: ", err);
            Log::Error("Failed to load file: ", filename);
            return K_INVALID_ENTITY;
        }

        ComponentManager *comp_manager = scene->get_component_manager();

        Entity root_entity = ecs::create_entity();
        comp_manager->add_component<NameComponent>(root_entity, utils::get_filename(filename));
        comp_manager->add_component<TransformComponent>(root_entity);
        scene->add_entity(root_entity);

        LoadState load_state = {.scene = scene};
        LoadMaterials(&gltf_model, load_state.materials);

        {
            MeshDataComponent &meshdata_component = comp_manager->add_component<MeshDataComponent>(root_entity);
            LoadMeshes(&gltf_model, &meshdata_component, &load_state);
            meshdata_component.prepare_render_data();
        }

        for (uint32_t i = 0; i < gltf_model.nodes.size(); ++i)
            ParseNodes(&gltf_model, i, root_entity, &load_state);

        return root_entity;
    }
} // namespace mirai