#include "GLTFLoader.hpp"

#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"

#include "Scene.hpp"
#include "Scene/Camera.hpp"
#include "Scene/AsyncLoader.hpp"
#include "Component.hpp"
#include "Common/FileUtils.hpp"
#include "Math/MathUtils.hpp"
#include "Math/Math.hpp"
#include "Common/dds.hpp"
#include "Engine/Timer.hpp"
#include "TextureCache.hpp"
#include "Material.hpp"
#include "Graphics/RenderingDevice.hpp"
#include "Graphics/Renderer.hpp"

#include <memory>

constexpr uint32_t SKIP_DDS_FIRST_N_LEVEL = 0;

namespace mirai {
    struct LoadState {
        Scene *scene;
        std::vector<MeshComponent> mesh_components;
        uint32_t material_base_offset;
        AsyncLoader *async_loader;
    };

    struct UserData {
        std::string base_path;
        AsyncLoader *async_loader;
        std::vector<BindlessTextureEntry> textures;
    };

    static Format get_image_format(dds::DXGI_FORMAT format) {
        switch (format) {
        case dds::DXGI_FORMAT_BC1_UNORM:
            return FORMAT_BC1_UNORM;
        case dds::DXGI_FORMAT_BC3_UNORM:
            return FORMAT_BC3_UNORM;
        case dds::DXGI_FORMAT_BC5_UNORM:
            return FORMAT_BC5_UNORM;
        case dds::DXGI_FORMAT_BC7_UNORM_SRGB:
            return FORMAT_BC7_SRGB_BLOCK;
        case dds::DXGI_FORMAT_BC7_UNORM:
            return FORMAT_BC7_UNORM_BLOCK;
        default:
            return FORMAT_UNDEFINED;
        }
    }

    FilterMode get_sampler_filter(int filter) {
        switch (filter) {
        case TINYGLTF_TEXTURE_FILTER_NEAREST:
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
            return FILTER_NEAREST;
        case TINYGLTF_TEXTURE_FILTER_LINEAR:
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
            return FILTER_LINEAR;
        default: return FILTER_LINEAR;
        }
    }

    SamplerMipmapMode get_sampler_mipmap_mode(int filter) {
        switch (filter) {
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
            return SAMPLER_MIPMAP_NEAREST;
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
            return SAMPLER_MIPMAP_NEAREST;
        default:
            return SAMPLER_MIPMAP_LINEAR;
        }
    }

    SamplerAddressMode get_sampler_address_mode(int address_mode) {
        switch (address_mode) {
        case TINYGLTF_TEXTURE_WRAP_REPEAT:
            return SAMPLER_ADDRESS_MODE_REPEAT;
        case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
            return SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
            return SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        default:
            return SAMPLER_ADDRESS_MODE_REPEAT;
        };
    }

    bool is_srgb_format(dds::DXGI_FORMAT format) {
        switch (format) {
        case dds::DXGI_FORMAT_BC7_UNORM_SRGB:
            return true;
        default:
            return false;
        }
    }

    Format get_format(int nchannel, bool is_color_texture) {
        switch (nchannel) {
        case 1: {
            ASSERT(is_color_texture == false);
            return FORMAT_R8_UNORM;
        }
        case 2: {
            ASSERT(is_color_texture == false);
            return FORMAT_R8G8_UNORM;
        }
        case 3:
        case 4:
            return is_color_texture ? FORMAT_R8G8B8A8_SRGB : FORMAT_R8G8B8A8_UNORM;
        default:
            return FORMAT_UNDEFINED;
        }
    }

    SamplerID CreateSampler(const tinygltf::Sampler *sampler) {
        SamplerDescription sampler_desc = SamplerDescription::create();
        sampler_desc.address_mode_u = sampler_desc.address_mode_v = sampler_desc.address_mode_w = SAMPLER_ADDRESS_MODE_REPEAT;
        if (sampler != nullptr) {
            sampler_desc.min_filter = get_sampler_filter(sampler->minFilter);
            sampler_desc.mag_filter = get_sampler_filter(sampler->magFilter);
            sampler_desc.address_mode_u = get_sampler_address_mode(sampler->wrapS);
            sampler_desc.address_mode_v = get_sampler_address_mode(sampler->wrapT);
            sampler_desc.mipmap_mode = get_sampler_mipmap_mode(sampler->minFilter);
        }

        return RenderingDevice::get()->create_sampler(&sampler_desc);
    }

    std::string FormatTextureURI(const std::string &uri) {
        std::string result;
        if (uri.empty()) {
            result = "gltftexture_" + std::to_string(rand()) + ".dds";
        } else {
            std::string decoded_uri;
            tinygltf::URIDecode(uri, &decoded_uri, nullptr);
            result = decoded_uri;
        }
        return result;
    }

    bool LoadEmbeddedImage(std::string filename, tinygltf::Image *image, const tinygltf::Sampler *sampler, UserData *p_user_data, bool is_color_texture = false) {
        unsigned char *data = image->image.data();
        uint32_t width = image->width;
        uint32_t height = image->height;
        uint32_t nchannel = image->component;
        ASSERT(nchannel != 3);
        ASSERT(width > 0 && height > 0);

        uint32_t mip_level = cast_u32(1 + std::log2(std::max(width, height)));
        Format format = get_format(image->component, is_color_texture);
        TextureDescription texture_desc = {
            .create_flags = 0,
            .width = width,
            .height = height,
            .depth = 1,
            .mip_levels = mip_level,
            .array_layers = 1,
            .texture_type = TEXTURE_TYPE_2D,
            .format = format,
            .usage_flags = TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_TRANSFER_DST_BIT | TEXTURE_USAGE_TRANSFER_SRC_BIT,
        };
        TextureID texture = RenderingDevice::get()->create_texture(&texture_desc, image->uri);
        TextureCache::get()->add_texture(filename, texture);

        SamplerID sampler_id = CreateSampler(sampler);
        p_user_data->textures.emplace_back(texture, sampler_id);

        TextureLoadEmbeddedTask load_task = {
            .filename = filename,
            .texture = texture,
            .width = cast_int(width),
            .height = cast_int(height),
            .n_channel = cast_int(nchannel),
        };

        load_task.data = std::move(image->image);
        p_user_data->async_loader->push({
            .task_type = TaskType::LoadTextureEmbedded,
            .data = std::move(load_task),
        });
        return true;
    }

    bool LoadExternalImage(tinygltf::Image *image, const tinygltf::Sampler *sampler, void *user_data, bool is_color_texture = false) {

        image->uri = FormatTextureURI(image->uri);
        if (TextureCache::get()->get_texture_id(image->uri).is_valid()) {
            return true;
        }

        std::string extension = utils::get_file_extension(image->uri);

        UserData *p_user_data = (UserData *)user_data;
        std::string full_path = p_user_data->base_path + image->uri;

        TextureDescription texture_desc = {
            .create_flags = 0,
            .width = 0,
            .height = 0,
            .depth = 1,
            .mip_levels = 0,
            .array_layers = 1,
            .texture_type = TEXTURE_TYPE_2D,
            .format = FORMAT_UNDEFINED,
            .usage_flags = TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_TRANSFER_DST_BIT,
        };

        bool is_dds_texture = false;
        bool force_rgba = false;
        if (extension == "dds") {
            FILE *file = fopen(full_path.c_str(), "rb");
            if (!file) {
                return false;
            }
            std::unique_ptr<FILE, int (*)(FILE *)> file_ptr(file, fclose);

            dds::Header header;
            if (fread(&header, sizeof(header), 1, file) != 1)
                return false;
            file_ptr.reset();
            file_ptr.release();

            ASSERT_MSG(header.width() > 0 && header.height() > 0, "zero width or height");
            if (!header.is_2d())
                return false;

            texture_desc.format = get_image_format(header.format());
            ASSERT_MSG(texture_desc.format != FORMAT_UNDEFINED, "Unsupported DDS Image Format");
            if (is_color_texture && !is_srgb_format(header.format())) {
                Log::Warn("Unsupported color texture format, should be srgb", full_path);
                return false;
            }

            uint32_t width = header.width();
            uint32_t height = header.height();
            uint32_t mip_count = header.mip_levels();

            if (SKIP_DDS_FIRST_N_LEVEL > 0 && mip_count > SKIP_DDS_FIRST_N_LEVEL) {
                for (int i = 0; i < SKIP_DDS_FIRST_N_LEVEL; ++i) {
                    width = width / 2;
                    height = height / 2;
                    mip_count--;
                }
            }
            texture_desc.width = width;
            texture_desc.height = height;
            texture_desc.mip_levels = mip_count;
            is_dds_texture = true;
        } else {
            int width, height, nchannel;
            if (!utils::get_image_info(full_path.c_str(), &width, &height, &nchannel)) {
                return false;
            }
            texture_desc.width = width;
            texture_desc.height = height;
            texture_desc.mip_levels = 1 + int(std::log2(std::max(width, height)));
            // Needed to generate mipmap
            texture_desc.usage_flags |= TEXTURE_USAGE_TRANSFER_SRC_BIT;
            texture_desc.format = get_format(nchannel, is_color_texture);
            if (texture_desc.format == FORMAT_UNDEFINED) {
                Log::Warn("Unsupported texture format", full_path);
                return false;
            }

            if (nchannel == 3)
                force_rgba = true;
        }
        SamplerID sampler_id = CreateSampler(sampler);
        TextureID texture = RenderingDevice::get()->create_texture(&texture_desc, image->uri);
        p_user_data->textures.emplace_back(texture, sampler_id);

        TextureCache::get()->add_texture(image->uri, texture);
        p_user_data->async_loader->push({.task_type = TaskType::LoadTextureExternal,
                                         .data = TextureLoadTask{
                                             .texture = texture,
                                             .filename = full_path,
                                             .is_dds_texture = is_dds_texture,
                                             .skip_first_n_levels = SKIP_DDS_FIRST_N_LEVEL,
                                             .force_rgba = force_rgba,
                                         }});
        return true;
    }

    static void LoadMaterials(tinygltf::Model *model, LoadState *load_state, UserData *user_data) {
        size_t material_count = model->materials.size();

        auto LoadTexture = [&](int texture_index, bool is_color_texture) {
            if (texture_index < 0)
                return K_INVALID_ID;

            const tinygltf::Texture &texture = model->textures[texture_index];
            tinygltf::Image &image = model->images[texture.source];

            const tinygltf::Sampler *sampler = nullptr;
            if (texture.sampler >= 0)
                sampler = &model->samplers[texture.sampler];

            std::string texture_name = FormatTextureURI(image.uri.size() > 0 ? image.uri : texture.name);
            TextureID texture_id = TextureCache::get()->get_texture_id(texture_name);

            if (image.image.size() > 0 && !texture_id.is_valid()) {
                LoadEmbeddedImage(texture_name, &image, sampler, user_data, is_color_texture);
            } else if (texture_name.size() > 0 && !texture_id.is_valid()) {
                std::string err, warn;
                if (!LoadExternalImage(&image, sampler, user_data, is_color_texture)) {
                    Log::Warn("Failed to load texture: ", texture_name);
                    return K_INVALID_ID;
                }
            }
            texture_id = TextureCache::get()->get_texture_id(texture_name);
            return texture_id.id;
        };

        for (uint32_t i = 0; i < material_count; ++i) {
            const tinygltf::Material *gltf_material = &model->materials[i];
            std::unique_ptr<StandardPBRMaterial> material = std::make_unique<StandardPBRMaterial>(gltf_material->name);
            const tinygltf::PbrMetallicRoughness &pbr = gltf_material->pbrMetallicRoughness;

            StandardPBRMaterial::PBRProperties &instance_data = material->instance_data;

            // instance_data.transmission = static_cast<float>(pbr.baseColorFactor[3]);
            instance_data.flags = 0;

            const std::string &alpha_mode = gltf_material->alphaMode;
            if (alpha_mode == "OPAQUE")
                instance_data.flags |= MaterialFlags::FLAG_OPAQUE;
            else if (alpha_mode == "BLEND")
                instance_data.flags |= MaterialFlags::FLAG_ALPHA_BLEND;
            else if (alpha_mode == "MASK")
                instance_data.flags |= MaterialFlags::FLAG_ALPHA_MASK;
            else
                ASSERT_MSG(0, "Unknown alpha mask");
            if (gltf_material->doubleSided) {
                instance_data.flags |= MaterialFlags::FLAG_DOUBLE_SIDED;
            }

            instance_data.alpha_cutoff = cast_float(gltf_material->alphaCutoff);

            if (gltf_material->extensions.find("KHR_materials_pbrSpecularGlossiness") != gltf_material->extensions.end()) {
                auto ext = gltf_material->extensions.find("KHR_materials_pbrSpecularGlossiness");
                instance_data.flags |= MaterialFlags::FLAG_SPECULAR_GLOSSINESS_WORKFLOW;
                if (ext->second.Has("diffuseTexture"))
                    instance_data.albedo_texture = LoadTexture(ext->second.Get("diffuseTexture").Get("index").Get<int>(), true);
                else
                    instance_data.albedo_texture = K_INVALID_ID;

                if (ext->second.Has("specularGlossinessTexture"))
                    instance_data.metallic_roughness_texture = LoadTexture(ext->second.Get("specularGlossinessTexture").Get("index").Get<int>(), true);
                else
                    instance_data.metallic_roughness_texture = K_INVALID_ID;

                if (ext->second.Has("glossinessFactor")) {
                    instance_data.roughness_factor = cast_float(ext->second.Get("glossinessFactor").Get<double>());
                } else {
                    instance_data.roughness_factor = 0.5f;
                }

                if (ext->second.Has("specularFactor")) {
                    instance_data.metallic_factor = cast_float(ext->second.Get("specularFactor").Get<double>());
                } else {
                    instance_data.metallic_factor = 0.01f;
                }

                if (ext->second.Has("diffuseFactor")) {
                    auto factor = ext->second.Get("diffuseFactor");
                    for (uint32_t d = 0; d < factor.ArrayLen(); ++d) {
                        auto val = factor.Get(d);
                        instance_data.albedo[d] = val.IsNumber() ? (float)val.Get<double>() : (float)val.Get<int>();
                    }
                }
            } else {
                // Process Textures
                instance_data.albedo_texture = LoadTexture(pbr.baseColorTexture.index, true);
                instance_data.metallic_roughness_texture = LoadTexture(pbr.metallicRoughnessTexture.index, false);
                instance_data.albedo = glm::vec4{pbr.baseColorFactor[0], pbr.baseColorFactor[1], pbr.baseColorFactor[2], pbr.baseColorFactor[3]};
                instance_data.metallic_factor = static_cast<float>(pbr.metallicFactor);
                instance_data.roughness_factor = static_cast<float>(pbr.roughnessFactor);
            }

            instance_data.emissive_factor = glm::vec3{gltf_material->emissiveFactor[0], gltf_material->emissiveFactor[1], gltf_material->emissiveFactor[2]};
            instance_data.emissive_texture = LoadTexture(gltf_material->emissiveTexture.index, true);

            const tinygltf::NormalTextureInfo &normal_texture = gltf_material->normalTexture;
            instance_data.normal_texture = LoadTexture(normal_texture.index, false);

            const tinygltf::OcclusionTextureInfo &occlusion_texture = gltf_material->occlusionTexture;
            instance_data.occlusion_texture = LoadTexture(occlusion_texture.index, false);
            load_state->scene->materials.push_back(std::move(material));
        }
    }

    static uint8_t *GetBufferPtr(const tinygltf::Model *model, const tinygltf::Accessor accessor) {
        const tinygltf::BufferView &buffer_view = model->bufferViews[accessor.bufferView];
        return (uint8_t *)(model->buffers[buffer_view.buffer].data.data() + accessor.byteOffset + buffer_view.byteOffset);
    }

    void LoadMeshes(const tinygltf::Model *model, LoadState *load_state) {
        size_t mesh_count = model->meshes.size();
        std::vector<MeshComponent> &mesh_components = load_state->mesh_components;
        mesh_components.resize(mesh_count);

        uint32_t gpu_mesh_index = static_cast<uint32_t>(load_state->scene->gpu_meshes.size());
        GpuMesh &gpu_mesh = load_state->scene->gpu_meshes.emplace_back(GpuMesh{});

        std::vector<Vertex> &vertices = gpu_mesh.vertices;
        std::vector<uint32_t> &indices = gpu_mesh.indices;

        for (uint32_t m = 0; m < mesh_count; ++m) {
            MeshComponent &mesh_component = mesh_components[m];
            mesh_component.gpu_mesh_index = gpu_mesh_index;
            const tinygltf::Mesh &gltf_mesh = model->meshes[m];

            uint32_t primitive_count = static_cast<uint32_t>(gltf_mesh.primitives.size());
            mesh_component.mesh_subsets.resize(primitive_count);
            mesh_component.aabbs.resize(primitive_count);

            for (uint32_t p = 0; p < primitive_count; ++p) {
                const auto &primitive = gltf_mesh.primitives[p];
                uint32_t vertex_offset = static_cast<uint32_t>(vertices.size());
                uint32_t index_offset = static_cast<uint32_t>(indices.size());

                auto position_attributes = primitive.attributes.find("POSITION");
                const tinygltf::Accessor position_accessor = model->accessors[position_attributes->second];
                float *positions = (float *)GetBufferPtr(model, position_accessor);

                float *normals = nullptr;
                auto normal_attributes = primitive.attributes.find("NORMAL");
                const tinygltf::Accessor normal_accessor = model->accessors[normal_attributes->second];
                if (normal_attributes != primitive.attributes.end())
                    normals = (float *)GetBufferPtr(model, normal_accessor);

                float *tangents = nullptr;
                auto tangent_attributes = primitive.attributes.find("TANGENT");
                if (tangent_attributes != primitive.attributes.end()) {
                    const tinygltf::Accessor tangent_accessor = model->accessors[tangent_attributes->second];
                    if (tangent_attributes != primitive.attributes.end())
                        tangents = (float *)GetBufferPtr(model, tangent_accessor);
                }

                float *uvs = nullptr;
                auto uv_attributes = primitive.attributes.find("TEXCOORD_0");
                if (uv_attributes != primitive.attributes.end()) {
                    const tinygltf::Accessor uv_accessor = model->accessors[uv_attributes->second];
                    if (uv_attributes != primitive.attributes.end())
                        uvs = (float *)GetBufferPtr(model, uv_accessor);
                }
                uint32_t num_position = static_cast<uint32_t>(position_accessor.count);
                AABB &aabb = mesh_component.aabbs[p];
                aabb.min = glm::vec3{FLT_MAX};
                aabb.max = glm::vec3{-FLT_MAX};

                for (uint32_t i = 0; i < num_position; ++i) {
                    Vertex &vertex = vertices.emplace_back();
                    glm::vec3 position = glm::vec3{
                        positions[i * 3],
                        positions[i * 3 + 1],
                        positions[i * 3 + 2]};
                    vertex.px = position.x;
                    vertex.py = position.y;
                    vertex.pz = position.z;

                    aabb.min = glm::min(aabb.min, position);
                    aabb.max = glm::max(aabb.max, position);

                    glm::vec3 normal;
                    if (normals != nullptr)
                        normal = {normals[i * 3], normals[i * 3 + 1], normals[i * 3 + 2]};
                    else
                        normal = {0.0f, 1.0f, 0.0f};

                    vertex.normal = utils::pack_vec3_to_u32(normal.x, normal.y, normal.z);

                    glm::vec4 tangent;
                    if (tangents != nullptr)
                        tangent = {
                            tangents[i * 4],
                            tangents[i * 4 + 1],
                            tangents[i * 4 + 2],
                            tangents[i * 4 + 3],
                        };
                    else
                        tangent = {1.0f, 0.0f, 0.0f, 1.0f};

                    vertex.tangent = utils::pack_vec3_to_u32(tangent.x, tangent.y, tangent.z);

                    glm::vec3 bitangent = glm::cross(normal, glm::vec3(tangent)) * tangent.w;
                    vertex.bitangent = utils::pack_vec3_to_u32(bitangent.x, bitangent.y, bitangent.z);

                    if (uvs != nullptr) {
                        vertex.tu = uvs[i * 2 + 0];
                        vertex.tv = uvs[i * 2 + 1];
                    }
                }

                const tinygltf::Accessor &indices_accessor = model->accessors[primitive.indices];
                uint32_t index_count = static_cast<uint32_t>(indices_accessor.count);
                if (indices_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                    uint32_t *indices_ptr = (uint32_t *)GetBufferPtr(model, indices_accessor);
                    indices.insert(indices.end(), indices_ptr, indices_ptr + index_count);
                } else if (indices_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    uint16_t *indices_ptr = (uint16_t *)GetBufferPtr(model, indices_accessor);
                    indices.insert(indices.end(), indices_ptr, indices_ptr + index_count);
                }

                MeshComponent::MeshSubset &mesh_subset = mesh_component.mesh_subsets[p];
                mesh_subset.vertex_offset = vertex_offset * sizeof(Vertex);
                mesh_subset.index_offset = index_offset * sizeof(uint32_t);
                mesh_subset.vertex_size = cast_u32((vertices.size() - vertex_offset) * sizeof(Vertex));
                mesh_subset.index_size = cast_u32((indices.size() - index_offset) * sizeof(uint32_t));
                mesh_subset.vertex_count = index_count;

                ASSERT(primitive.material >= 0);
                mesh_subset.material_index = primitive.material + load_state->material_base_offset;
            }
        }

        // @TODO May cause issue later when multiple mesh are loaded in different thread
        // Pushing to the vector may invalidates all the reference
        Renderer *renderer = Renderer::get();
        uint32_t vertex_buffer_size = static_cast<uint32_t>(vertices.size() * sizeof(Vertex));
        std::optional<BufferView> vertex_buffer_view = renderer->vertex_buffer_allocator.allocate(vertex_buffer_size);
        if (!vertex_buffer_view.has_value()) {
            Log::Fatal(0, "Failed to allocate goemetry buffer");
        }

        BufferView vertex_buffer = vertex_buffer_view.value();
        load_state->async_loader->push({.task_type = TaskType::UploadBuffer,
                                        .data = BufferCopyTask{
                                            .dst = vertex_buffer.buffer,
                                            .data = vertices.data(),
                                            .offset_in_bytes = vertex_buffer.offset,
                                            .size_in_bytes = vertex_buffer_size,
                                        }});

        uint32_t index_buffer_size = static_cast<uint32_t>(indices.size() * sizeof(uint32_t));
        std::optional<BufferView> index_buffer_view = renderer->index_buffer_allocator.allocate(index_buffer_size);
        if (!index_buffer_view.has_value()) {
            Log::Fatal("Failed to allocate goemetry buffer");
        }

        BufferView index_buffer = index_buffer_view.value();
        load_state->async_loader->push({.task_type = TaskType::UploadBuffer,
                                        .data = BufferCopyTask{
                                            .dst = index_buffer.buffer,
                                            .data = indices.data(),
                                            .offset_in_bytes = index_buffer.offset,
                                            .size_in_bytes = index_buffer_size,
                                        }});

        for (auto &mesh_component : mesh_components) {
            mesh_component.vertex_buffer = vertex_buffer;
            mesh_component.index_buffer = index_buffer;
            for (auto &mesh_subset : mesh_component.mesh_subsets) {
                mesh_subset.vertex_offset += vertex_buffer.offset;
                mesh_subset.index_offset += index_buffer.offset;
            }
        }

        UniformLayout vertex_data_layout = {
            .binding = 0,
            .binding_type = BINDING_TYPE_STORAGE_BUFFER,
            .shader_stage = SHADER_STAGE_VERTEX,
        };

        RenderingDevice *device = RenderingDevice::get();
        UniformSetID vertex_binding_set = device->create_uniform_set(&vertex_data_layout, 1, 2, "mesh_data_set");

        UniformBinding vertex_binding = {
            .resource_id = vertex_buffer.buffer,
        };

        device->update_uniform_set(vertex_binding_set, &vertex_binding, 1);

        gpu_mesh.vertex_buffer = vertex_buffer;
        gpu_mesh.vertex_buffer_size = vertex_buffer_size;

        gpu_mesh.index_buffer = index_buffer;
        gpu_mesh.index_buffer_size = index_buffer_size;
        gpu_mesh.vertex_binding_set = vertex_binding_set;
    }

    void ParseNodes(const tinygltf::Model *model, int node_index, Entity parent, LoadState *load_state) {
        const tinygltf::Node *node = &model->nodes[node_index];
        Scene *scene = load_state->scene;

        // Create parent as default entity to be passed on recursion
        // For camera, we don't create new entity
        auto &comp_manager = scene->ecs->component_manager;
        Entity entity = scene->create_entity();
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
        if (node->matrix.size() > 0) {
            glm::mat4 transformation_matrix = glm::make_mat4x4(node->matrix.data());
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(transformation_matrix, transform.scale, transform.rotation, transform.position, skew, perspective);
        }

        // HierarchyComponent
        if (!comp_manager->has_component<HierarchyComponent>(parent))
            comp_manager->add_component<HierarchyComponent>(parent);

        // Update Hierarchy
        HierarchyComponent *parent_hierarchy = comp_manager->get_component<HierarchyComponent>(parent);
        HierarchyComponent *child_hierarchy = comp_manager->get_component<HierarchyComponent>(entity);

        child_hierarchy->set_parent(parent);
        parent_hierarchy->add_children(entity);

        if (node->mesh >= 0) {
            // Add Mesh Component
            int mesh_id = node->mesh;
            if (mesh_id >= 0) {
                ASSERT(mesh_id < load_state->mesh_components.size());
                comp_manager->add_component<MeshComponent>(entity, load_state->mesh_components[mesh_id]);
            }

        } else if (node->camera >= 0) {
            const auto &camera_properties = model->cameras[node->camera];
            ASSERT(camera_properties.type == "perspective");

            Camera *camera = scene->get_camera();

            const tinygltf::PerspectiveCamera &perspective = camera_properties.perspective;
            camera->set_fov(cast_float(glm::degrees(perspective.yfov)));
            camera->set_aspect_ratio(cast_float(perspective.aspectRatio));
            camera->set_near_plane(cast_float(perspective.znear));
            camera->set_far_plane(cast_float(perspective.zfar));

            if (node->translation.size() > 0)
                camera->position = transform.position;
            if (node->rotation.size() > 0) {
                glm::fquat rotation = glm::fquat{(float)node->rotation[3], (float)node->rotation[0], (float)node->rotation[1], (float)node->rotation[2]};
                camera->rotation = glm::degrees(glm::eulerAngles(rotation));
            }
            if (node->matrix.size()) {
                camera->position = transform.position;
                camera->rotation = glm::degrees(glm::eulerAngles(transform.rotation));
                camera->rotation.y = -90.0f + camera->rotation.y;
            }
        }

        for (const auto &child : node->children)
            ParseNodes(model, child, entity, load_state);
    }

    Entity ImportModel_GLTF(const std::string &filename, Scene *scene) {
        Timer load_timer;
        AsyncLoader async_loader;
        UserData user_data = {
            .base_path = utils::get_base_path(filename),
            .async_loader = &async_loader,
        };
        std::string file_extension = utils::get_file_extension(filename);

        bool ret = false;
        std::string err, warn;

        tinygltf::TinyGLTF gltf_loader;
        gltf_loader.SetStoreOriginalJSONForExtrasAndExtensions(true);

        Log::Info("Loading Model: ", filename);
        tinygltf::Model gltf_model;
        if (file_extension == "GLB" || file_extension == "glb")
            ret = gltf_loader.LoadBinaryFromFile(&gltf_model, &err, &warn, filename);
        else
            ret = gltf_loader.LoadASCIIFromFile(&gltf_model, &err, &warn, filename);

        if (!ret) {
            Log::Warn("GLTF ERROR:: ", err);
            // Log::Error("Failed to load file: ", filename);
            return K_INVALID_ENTITY;
        }
        scene->dirty = true;

        auto &comp_manager = scene->ecs->component_manager;
        Entity root_entity = scene->entities[0];
        std::string root_entity_name = utils::get_filename(filename);
        // comp_manager->add_component<NameComponent>(root_entity, root_entity_name);
        // comp_manager->add_component<TransformComponent>(root_entity);
        // Make this entity child of scene root
        // HierarchyComponent &child_comp = comp_manager->add_component<HierarchyComponent>(root_entity);
        // child_comp.set_parent(scene->entities[0]);

        // Update scene root hierarchy component
        // HierarchyComponent *parent_comp = comp_manager->get_component<HierarchyComponent>(scene->entities[0]);
        // parent_comp->add_children(root_entity);
        // scene->add_entity(root_entity);

        LoadState load_state = {
            .scene = scene,
            .material_base_offset = static_cast<uint32_t>(scene->materials.size()),
        };

        load_state.async_loader = &async_loader;

        LoadMaterials(&gltf_model, &load_state, &user_data);
        LoadMeshes(&gltf_model, &load_state);
        for (const auto &scene : gltf_model.scenes) {
            for (const auto &node : scene.nodes)
                ParseNodes(&gltf_model, node, root_entity, &load_state);
        }
        async_loader.wait();

        Log::Info("Loaded: ", root_entity_name, "[", load_timer.elapsed_seconds(), "s]");
        Log::Info("meshes: ", load_state.mesh_components.size());

        RenderingDevice::get()->add_bindless_texture(user_data.textures.data(), static_cast<uint32_t>(user_data.textures.size()));

        return root_entity;
    }
} // namespace mirai