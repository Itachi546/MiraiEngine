#include "GLTFLoader.hpp"

#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"

#include "Scene.hpp"
#include "Scene/Camera.hpp"
#include "Scene/AsyncLoader.hpp"
#include "Scene/Animation.hpp"
#include "Scene/MeshData.hpp"
#include "Component.hpp"
#include "Common/FileUtils.hpp"
#include "Math/MathUtils.hpp"
#include "Math/Math.hpp"
#include "Common/dds.hpp"
#include "Common/Timer.hpp"
#include "TextureCache.hpp"
#include "Material.hpp"
#include "Graphics/RenderingDevice.hpp"
#include "Graphics/Renderer.hpp"
#include "Common/HashMap.hpp"
#include "Common/HashSet.hpp"

#include <memory>

constexpr uint32_t SKIP_DDS_FIRST_N_LEVEL = 0;

namespace mirai {

    struct TempAnimationChannel {
        Vec3Track positions;
        QuatTrack rotations;
        Vec3Track scalings;
    };

    struct TempAnimation {
        std::string name;
        float start_time;
        float end_time;
        float tick_per_seconds;
        HashMap<int, TempAnimationChannel> channels;

        bool has_node(int node_index) {
            return channels.find(node_index) != channels.end();
        }

        TempAnimationChannel *get_channel(int node_index) {
            return &channels.find(node_index)->second;
        }
    };
    struct LoadState {
        Scene *scene;
        uint32_t material_base_offset;
        uint32_t animation_player_base_offset;

        // Map of node and it's position in scene animation_clip vector
        std::vector<MeshComponent> mesh_components;
        std::vector<TempAnimation> animations;
        HashSet<int> global_joint_list;
        AsyncLoader *async_loader;
    };

    struct UserData {
        std::string base_path;
        AsyncLoader *async_loader;
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
    /*
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
    */
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
    /*
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
    */
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
        (void)sampler;
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
        // SamplerID sampler_id = CreateSampler(sampler);
        Renderer::get()->add_bindless_texture(texture);

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
        (void)sampler;

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
        // SamplerID sampler_id = CreateSampler(sampler);
        TextureID texture = RenderingDevice::get()->create_texture(&texture_desc, image->uri);
        Renderer::get()->add_bindless_texture(texture);

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

            std::string texture_name = FormatTextureURI(image.uri.size() > 0 ? image.uri : image.name);
            // Random texture name is generated if texture doesn't have name
            image.name = texture_name;

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
            std::unique_ptr<Material3D> material = std::make_unique<Material3D>(gltf_material->name);

            const tinygltf::PbrMetallicRoughness &pbr = gltf_material->pbrMetallicRoughness;

            Material3D::Properties &properties = material->properties;

            bool has_khr_transmission = false;
            auto khr_transmission_ext = gltf_material->extensions.find("KHR_materials_transmission");
            if (khr_transmission_ext != gltf_material->extensions.end()) {
                properties.transmission = cast_float(khr_transmission_ext->second.Get("transmissionFactor").Get<double>());
            }

            const std::string &alpha_mode = gltf_material->alphaMode;
            if (alpha_mode == "OPAQUE")
                material->set_alpha_mode(ALPHA_MODE_OPAQUE);
            else if (alpha_mode == "BLEND") {
                material->set_depth_write(false);
                material->set_alpha_mode(ALPHA_MODE_BLEND);
            } else if (alpha_mode == "MASK")
                material->set_alpha_mode(ALPHA_MODE_MASK);
            else
                ASSERT_MSG(0, "Unknown alpha mask");

            if (gltf_material->doubleSided) {
                material->set_cull_mode(CULL_MODE_NONE);
            }

            properties.alpha_cutoff = cast_float(gltf_material->alphaCutoff);

            auto khr_specular_glossiness_ext = gltf_material->extensions.find("KHR_materials_pbrSpecularGlossiness");
            if (khr_specular_glossiness_ext != gltf_material->extensions.end()) {
                const tinygltf::Value &specular_glossiness_ext = khr_specular_glossiness_ext->second;

                properties.flags = PBRMaterialFlags::SPECULAR_GLOSSINESS_WORKFLOW | PBRMaterialFlags::SPECULAR_GLOSSINESS_NO_GLOSSINESS_CHANNEL;

                if (khr_specular_glossiness_ext->second.Has("diffuseFactor")) {
                    auto diffuse_factor = specular_glossiness_ext.Get("diffuseFactor");
                    ASSERT(diffuse_factor.IsArray() && diffuse_factor.ArrayLen() == 4);
                    properties.albedo.x = cast_float(diffuse_factor.Get(0).GetNumberAsDouble());
                    properties.albedo.y = cast_float(diffuse_factor.Get(1).GetNumberAsDouble());
                    properties.albedo.z = cast_float(diffuse_factor.Get(2).GetNumberAsDouble());
                    properties.albedo.w = cast_float(diffuse_factor.Get(3).GetNumberAsDouble());
                }

                if (specular_glossiness_ext.Has("diffuseTexture"))
                    properties.albedo_texture_index = LoadTexture(specular_glossiness_ext.Get("diffuseTexture").Get("index").Get<int>(), true);

                if (specular_glossiness_ext.Has("specularFactor")) {
                    auto specular_factor = specular_glossiness_ext.Get("specularFactor");
                    ASSERT(specular_factor.IsArray() && specular_factor.ArrayLen() == 3);
                    properties.specular_factor.x = cast_float(specular_factor.Get(0).GetNumberAsDouble());
                    properties.specular_factor.y = cast_float(specular_factor.Get(1).GetNumberAsDouble());
                    properties.specular_factor.z = cast_float(specular_factor.Get(2).GetNumberAsDouble());
                }

                if (khr_specular_glossiness_ext->second.Has("specularGlossinessTexture"))
                    properties.pbr_texture_index = LoadTexture(specular_glossiness_ext.Get("specularGlossinessTexture").Get("index").Get<int>(), true);

                if (khr_specular_glossiness_ext->second.Has("glossinessFactor")) {
                    properties.glossiness = cast_float(specular_glossiness_ext.Get("glossinessFactor").Get<double>());
                }

            } else {
                // Process Textures
                properties.albedo_texture_index = LoadTexture(pbr.baseColorTexture.index, true);
                properties.pbr_texture_index = LoadTexture(pbr.metallicRoughnessTexture.index, false);
                properties.albedo = glm::vec4{pbr.baseColorFactor[0], pbr.baseColorFactor[1], pbr.baseColorFactor[2], pbr.baseColorFactor[3]};
                properties.metallic_factor = static_cast<float>(pbr.metallicFactor);
                properties.roughness_factor = static_cast<float>(pbr.roughnessFactor);
            }

            properties.emissive_factor = glm::vec3{gltf_material->emissiveFactor[0], gltf_material->emissiveFactor[1], gltf_material->emissiveFactor[2]};
            properties.emissive_texture_index = LoadTexture(gltf_material->emissiveTexture.index, true);

            const tinygltf::NormalTextureInfo &normal_texture = gltf_material->normalTexture;
            properties.normal_texture_index = LoadTexture(normal_texture.index, false);

            const tinygltf::OcclusionTextureInfo &occlusion_texture = gltf_material->occlusionTexture;
            properties.occlusion_texture_index = LoadTexture(occlusion_texture.index, false);

            load_state->scene->materials.push_back(std::move(material));
        }
    }

    static uint8_t *GetBufferPtr(const tinygltf::Model *model, const tinygltf::Accessor accessor) {
        const tinygltf::BufferView &buffer_view = model->bufferViews[accessor.bufferView];
        return (uint8_t *)(model->buffers[buffer_view.buffer].data.data() + accessor.byteOffset + buffer_view.byteOffset);
    }

    void LoadMeshes(const tinygltf::Model *model, LoadState *load_state) {
        Renderer *renderer = Renderer::get();
        Scene *scene = load_state->scene;

        size_t mesh_count = model->meshes.size();
        for (uint32_t m = 0; m < mesh_count; ++m) {
            const tinygltf::Mesh &gltf_mesh = model->meshes[m];
            uint32_t primitive_count = cast_u32(gltf_mesh.primitives.size());

            MeshComponent &mesh_component = load_state->mesh_components.emplace_back();
            mesh_component.primitives.resize(primitive_count);

            bool is_skinned_mesh = false;
            bool has_tangent_space = false;

            for (uint32_t p = 0; p < primitive_count; ++p) {
                const auto &gltf_primitive = gltf_mesh.primitives[p];
                const std::map<std::string, int> &attributes = gltf_primitive.attributes;

                // Parse position
                auto position_attributes = attributes.find("POSITION");
                ASSERT(position_attributes != attributes.end());
                const tinygltf::Accessor position_accessor = model->accessors[position_attributes->second];
                float *positions = (float *)GetBufferPtr(model, position_accessor);

                // Parse normal
                float *normals = nullptr;
                auto normal_attributes = attributes.find("NORMAL");
                ASSERT(normal_attributes != attributes.end());
                const tinygltf::Accessor normal_accessor = model->accessors[normal_attributes->second];
                normals = (float *)GetBufferPtr(model, normal_accessor);

                // Parse tangent
                float *tangents = nullptr;
                auto tangent_attributes = attributes.find("TANGENT");
                if (tangent_attributes != attributes.end()) {
                    const tinygltf::Accessor tangent_accessor = model->accessors[tangent_attributes->second];
                    tangents = (float *)GetBufferPtr(model, tangent_accessor);
                    has_tangent_space = true;
                }

                // Parse texture coordinates
                float *uvs = nullptr;
                auto uv_attributes = attributes.find("TEXCOORD_0");
                if (uv_attributes != attributes.end()) {
                    const tinygltf::Accessor uv_accessor = model->accessors[uv_attributes->second];
                    uvs = (float *)GetBufferPtr(model, uv_accessor);
                }

                // Parse animation data
                auto joint_attributes = attributes.find("JOINTS_0");
                std::vector<uint32_t> joints;
                if (joint_attributes != attributes.end()) {
                    is_skinned_mesh = true;

                    const tinygltf::Accessor joint_accessor = model->accessors[joint_attributes->second];
                    ASSERT(joint_accessor.type == TINYGLTF_TYPE_VEC4);
                    if (joint_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                        uint8_t *joint_ptr = (uint8_t *)GetBufferPtr(model, joint_accessor);
                        joints.insert(joints.end(), joint_ptr, joint_ptr + joint_accessor.count * 4);
                    } else if (joint_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        uint16_t *joint_ptr = (uint16_t *)GetBufferPtr(model, joint_accessor);
                        joints.insert(joints.end(), joint_ptr, joint_ptr + joint_accessor.count * 4);
                    } else {
                        ASSERT("Unknown component type for joint");
                    }
                }

                float *weights = nullptr;
                auto weights_attributes = attributes.find("WEIGHTS_0");
                if (weights_attributes != attributes.end()) {
                    const tinygltf::Accessor weights_accessor = model->accessors[weights_attributes->second];
                    ASSERT(weights_accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
                    ASSERT(weights_accessor.type == TINYGLTF_TYPE_VEC4);
                    weights = (float *)GetBufferPtr(model, weights_accessor);
                }

                // Copy local vertex data
                uint32_t vertex_stride = is_skinned_mesh ? K_VERTEX_DATA_SIZE_SKINNED : K_VERTEX_DATA_SIZE;
                uint32_t num_position = cast_u32(position_accessor.count);

                AABB local_aabb = {glm::vec3(FLT_MAX), glm::vec3(-FLT_MAX)};

                auto parse_vec3 = [](int index, float *ptr) {
                    return glm::vec3(ptr[index * 3], ptr[index * 3 + 1], ptr[index * 3 + 2]);
                };

                std::vector<uint8_t> vertices;
                vertices.reserve(num_position * vertex_stride);

                for (uint32_t i = 0; i < num_position; ++i) {
                    // We assume and allocate the total vertex data required for skinned mesh, but on copy partial data if it is not skinned
                    SkinnedVertexData vertex;
                    vertex.position = parse_vec3(i, positions);

                    local_aabb.min = glm::min(local_aabb.min, vertex.position);
                    local_aabb.max = glm::max(local_aabb.max, vertex.position);

                    glm::vec3 normal = parse_vec3(i, normals);
                    vertex.normal = utils::pack_vec3_to_u32(normal.x, normal.y, normal.z);

                    vertex.tangent = 0;
                    vertex.bitangent = 0;
                    if (tangents != nullptr) {
                        glm::vec4 tangent = {
                            tangents[i * 4],
                            tangents[i * 4 + 1],
                            tangents[i * 4 + 2],
                            tangents[i * 4 + 3],
                        };
                        vertex.tangent = utils::pack_vec3_to_u32(tangent.x, tangent.y, tangent.z);
                        glm::vec3 bitangent = glm::cross(normal, glm::vec3(tangent)) * tangent.w;
                        vertex.bitangent = utils::pack_vec3_to_u32(bitangent.x, bitangent.y, bitangent.z);
                    }

                    if (uvs != nullptr) {
                        vertex.uv = {uvs[i * 2 + 0], uvs[i * 2 + 1]};
                    }

                    if (is_skinned_mesh) {
                        vertex.joints = cast_u32(joints[i * 4]) << 24 |
                                        cast_u32(joints[i * 4 + 1]) << 16 |
                                        cast_u32(joints[i * 4 + 2]) << 8 |
                                        cast_u32(joints[i * 4 + 3]);
                        vertex.weights = {
                            weights[i * 4],
                            weights[i * 4 + 1],
                            weights[i * 4 + 2],
                            weights[i * 4 + 3],
                        };

                        // Never exactly 1.0f
                        ASSERT(vertex.weights.x + vertex.weights.y + vertex.weights.z + vertex.weights.w <= 1.001f);
                    }

                    uint8_t *vertex_bytes = reinterpret_cast<uint8_t *>(&vertex);
                    vertices.insert(vertices.end(), vertex_bytes, vertex_bytes + vertex_stride);
                }

                // Parse indices
                const tinygltf::Accessor &indices_accessor = model->accessors[gltf_primitive.indices];
                uint32_t index_count = static_cast<uint32_t>(indices_accessor.count);
                uint64_t index_data_size = index_count * sizeof(uint32_t);

                std::vector<uint8_t> indices;
                indices.resize(index_data_size);

                if (indices_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                    uint8_t *indices_ptr = (uint8_t *)GetBufferPtr(model, indices_accessor);
                    std::memcpy(indices.data(), indices_ptr, index_data_size);
                } else if (indices_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    uint16_t *indices_ptr = (uint16_t *)GetBufferPtr(model, indices_accessor);
                    uint32_t *dst = reinterpret_cast<uint32_t *>(indices.data());
                    for (uint32_t i = 0; i < index_count; ++i)
                        dst[i] = indices_ptr[i];
                }

                MeshHandle mesh_handle = cast_u32(scene->mesh_allocations.size());

                Primitive &primitive = mesh_component.primitives[p];
                primitive.mesh = mesh_handle;

                ASSERT(primitive.material >= 0);
                primitive.material = gltf_primitive.material + load_state->material_base_offset;

                // We allocate additional space for vertex data if the mesh is skinned
                uint64_t vertex_data_size = vertices.size();
                if (is_skinned_mesh)
                    vertex_data_size *= 2;

                BufferView buffer = renderer->geometry_buffer_allocator->allocate(vertex_data_size + index_data_size);

                load_state->async_loader->push({
                    .task_type = TaskType::UploadBuffer,
                    .data = BufferCopyTask{buffer.buffer, buffer.offset, std::move(vertices)},
                });

                load_state->async_loader->push({
                    .task_type = TaskType::UploadBuffer,
                    .data = BufferCopyTask{buffer.buffer, buffer.offset + vertex_data_size, std::move(indices)},
                });

                // Allocate memory in the buffer
                MeshAllocation &mesh_allocation = scene->mesh_allocations.emplace_back();
                mesh_allocation.buffer = buffer.buffer;
                mesh_allocation.blas.as = AccelerationStructureID{K_INVALID_ID};
                mesh_allocation.local_aabb = local_aabb;
                mesh_allocation.vertex_offset_bytes = buffer.offset;
                mesh_allocation.index_offset_bytes = buffer.offset + vertex_data_size;
                mesh_allocation.vertex_count = num_position;
                mesh_allocation.vertex_stride = vertex_stride;
                mesh_allocation.index_count = index_count;
                mesh_allocation.ouput_vertex_offset_bytes = is_skinned_mesh ? buffer.offset + vertex_data_size / 2 : 0;
            }
        }
    }

    InterpolationMode get_interpolation_mode(const std::string &mode) {
        if (mode == "CUBIC")
            return InterpolationMode::Cubic;
        else if (mode == "STEP")
            return InterpolationMode::Step;
        return InterpolationMode::Linear;
    }

    void LoadAnimationSampler(const tinygltf::Model *model, const tinygltf::AnimationSampler &sampler, TempAnimationChannel *channel, const std::string &target_path, float &start_time, float &end_time) {
        // timestamp array for animation
        const auto &input_accessor = model->accessors[sampler.input];
        ASSERT(input_accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && input_accessor.type == TINYGLTF_TYPE_SCALAR);

        float *timestamps_ptr = reinterpret_cast<float *>(GetBufferPtr(model, input_accessor));
        uint32_t num_timestamp = cast_u32(input_accessor.count);
        // value array for animation
        const auto &output_accessor = model->accessors[sampler.output];
        float *values_ptr = reinterpret_cast<float *>(GetBufferPtr(model, output_accessor));
        uint32_t num_value = cast_u32(output_accessor.count);

        InterpolationMode interpolation_mode = get_interpolation_mode(sampler.interpolation);
        if (output_accessor.type == TINYGLTF_TYPE_VEC3) {
            ASSERT(target_path == "scale" || target_path == "translation");
            Vec3Track &track = target_path == "scale" ? channel->scalings : channel->positions;
            ASSERT(track.values.size() == 0);
            ASSERT(num_value = num_timestamp);

            track.timestamps.assign(timestamps_ptr, timestamps_ptr + num_timestamp);
            start_time = std::min(start_time, track.timestamps.front());
            end_time = std::max(end_time, track.timestamps.back());

            track.values.resize(num_value);
            for (uint32_t i = 0; i < num_value; ++i)
                track.values[i] = glm::vec3(values_ptr[i * 3], values_ptr[i * 3 + 1], values_ptr[i * 3 + 2]);
            track.interpolation_mode = interpolation_mode;
        } else if (output_accessor.type == TINYGLTF_TYPE_VEC4) {
            ASSERT(target_path == "rotation");
            QuatTrack &track = channel->rotations;
            ASSERT(track.values.size() == 0);

            track.timestamps.assign(timestamps_ptr, timestamps_ptr + num_timestamp);
            start_time = std::min(start_time, track.timestamps.front());
            end_time = std::max(end_time, track.timestamps.back());

            track.values.resize(num_value);
            for (uint32_t i = 0; i < num_value; ++i)
                track.values[i] = glm::fquat(values_ptr[i * 4 + 3], values_ptr[i * 4], values_ptr[i * 4 + 1], values_ptr[i * 4 + 2]);
            track.interpolation_mode = interpolation_mode;
        } else {
            Log::Error("Unknown component type for animation sampler");
        }
    }

    void LoadAnimations(const tinygltf::Model *model, LoadState *load_state) {
        if (model->animations.size() == 0)
            return;

        for (uint32_t i = 0; i < model->animations.size(); ++i) {
            const auto &gltf_animation = model->animations[i];
            if (gltf_animation.channels.size() == 0)
                continue;

            const std::string &name = gltf_animation.name.size() > 0 ? gltf_animation.name : "unnamed" + std::to_string(i);
            TempAnimation *animation = &load_state->animations.emplace_back(TempAnimation{.name = name});

            float start_time = std::numeric_limits<float>::max();
            float end_time = std::numeric_limits<float>::lowest();
            /**
             * Different channelof same animation can belong to different node,
             * so in order to calculate the animation properly, we need to keep track of
             * animation start/end time globally for all the channels and update to the node.
             **/
            for (const auto &channel : gltf_animation.channels) {
                int target_node = channel.target_node;
                auto found = animation->channels.find(target_node);

                TempAnimationChannel *node_channel = nullptr;
                if (found == animation->channels.end()) {
                    animation->channels.insert(std::make_pair(target_node, TempAnimationChannel{}));
                    node_channel = &animation->channels.find(target_node)->second;
                } else {
                    node_channel = &found->second;
                }

                const std::string &target_path = channel.target_path;
                const tinygltf::AnimationSampler &sampler = gltf_animation.samplers[channel.sampler];
                LoadAnimationSampler(model, sampler, node_channel, target_path, start_time, end_time);
            }

            animation->start_time = start_time;
            animation->end_time = end_time;
        }
    }

    void ParseNodeTransform(const tinygltf::Node *node, TransformComponent *transform) {
        if (node->translation.size() > 0)
            transform->position = {(float)node->translation[0], (float)node->translation[1], (float)node->translation[2]};
        if (node->rotation.size() > 0)
            transform->rotation = {(float)node->rotation[3], (float)node->rotation[0], (float)node->rotation[1], (float)node->rotation[2]};
        if (node->scale.size() > 0)
            transform->scale = {node->scale[0], node->scale[1], node->scale[2]};
        if (node->matrix.size() > 0) {
            glm::mat4 transformation_matrix = glm::make_mat4x4(node->matrix.data());
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(transformation_matrix, transform->scale, transform->rotation, transform->position, skew, perspective);
        }
    }

    void ParseLightComponent(const tinygltf::Light *light, LightComponent *light_component) {
        if (light->type == "directional") {
            light_component->light_type = LIGHT_TYPE_DIRECTIONAL;
        } else if (light->type == "point") {
            light_component->light_type = LIGHT_TYPE_POINT;
        } else if (light->type == "spot") {
            light_component->light_type = LIGHT_TYPE_SPOT;
            light_component->inner_cone_angle = cast_float(light->spot.innerConeAngle);
            light_component->outer_cone_angle = cast_float(light->spot.outerConeAngle);
        } else {
            ASSERT_MSG(0, "Unsupported light type");
        }

        light_component->color.x = cast_float(light->color[0]);
        light_component->color.y = cast_float(light->color[1]);
        light_component->color.z = cast_float(light->color[2]);
        // @NOTE custom intensity scaling
        light_component->intensity = cast_float(light->intensity) * 0.001f;
        light_component->radius = cast_float(light->range);
        light_component->cast_shadow = false;
    }

    void LoadSkins(const tinygltf::Model *model, LoadState *load_state) {

        // List all the skeleton nodes
        for (const auto &skin : model->skins) {
            for (auto j : skin.joints) {
                load_state->global_joint_list.insert(j);
            }
        }

        for (const auto &skin : model->skins) {
            uint32_t joint_count = cast_u32(skin.joints.size());
            // We don't have skeleton information, need to reconstruct it manually
            // Lookup table between node and it's parent local index
            HashMap<int, int> joint_parent_lookup;

            std::unique_ptr<SkeletalAsset> skeletal_asset = std::make_unique<SkeletalAsset>();
            skeletal_asset->name = skin.name;

            Skeleton &skeleton = skeletal_asset->skeleton;
            skeleton.resize(joint_count);

            // ASSERT(skin.inverseBindMatrices >= 0);
            glm::mat4 *inv_bind_matrix_ptr = nullptr;

            if (skin.inverseBindMatrices >= 0) {
                const tinygltf::Accessor &bind_matrices_accessor = model->accessors[skin.inverseBindMatrices];
                ASSERT(bind_matrices_accessor.count == joint_count);
                ASSERT(bind_matrices_accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
                ASSERT(bind_matrices_accessor.type == TINYGLTF_TYPE_MAT4);
                inv_bind_matrix_ptr = (glm::mat4 *)GetBufferPtr(model, bind_matrices_accessor);
            }

            for (uint32_t j = 0; j < joint_count; ++j) {
                int joint_index = skin.joints[j];
                auto found = joint_parent_lookup.find(joint_index);
                // If we don't find any parent for this node then it must be root node
                int parent_index = -1;
                if (found == joint_parent_lookup.end())
                    joint_parent_lookup[joint_index] = parent_index;
                else
                    parent_index = found->second;

                const tinygltf::Node *node = &model->nodes[joint_index];
                ASSERT(node != nullptr);
                for (auto child : node->children) {
                    // We check the global joint list and skip the child node that is not part of skeleton
                    if (load_state->global_joint_list.find(child) != load_state->global_joint_list.end()) {
                        // Instead of storing actual parent node, we store the local index of it
                        joint_parent_lookup[child] = j;
                    }
                }

                TransformComponent transform;
                ParseNodeTransform(node, &transform);
                skeleton.add_bone(j, parent_index, node->name, transform.get_local_transform(), inv_bind_matrix_ptr == nullptr ? glm::mat4(1.0f) : inv_bind_matrix_ptr[j]);
            }
            if (inv_bind_matrix_ptr == nullptr)
                skeleton.calculate_inv_bind_transform();

            // Find all the animation clip associated with this skeleton
            // @TODO we can optimize this later
            for (auto &animation : load_state->animations) {
                float match_percent = 0.0f;
                for (auto &[key, val] : joint_parent_lookup) {
                    if (animation.has_node(key)) {
                        match_percent += 1.0f;
                    }
                }
                match_percent = match_percent / float(joint_count);
                if (match_percent > 0.49f) {
                    Log::Info("Found animation clip: ", animation.name);
                    AnimationClip &animation_clip = skeletal_asset->animation_clips.emplace_back(AnimationClip{
                        .name = animation.name,
                        .start_time = animation.start_time,
                        .end_time = animation.end_time,
                        .tick_per_seconds = 60,
                        .looping = true,
                    });
                    animation_clip.positions.resize(joint_count);
                    animation_clip.rotations.resize(joint_count);
                    animation_clip.scalings.resize(joint_count);

                    for (uint32_t j = 0; j < joint_count; ++j) {
                        uint32_t joint_index = skin.joints[j];
                        auto found = animation.channels.find(joint_index);
                        if (found == animation.channels.end())
                            continue;
                        TempAnimationChannel &channel = found->second;
                        animation_clip.positions[j] = std::move(channel.positions);
                        animation_clip.rotations[j] = std::move(channel.rotations);
                        animation_clip.scalings[j] = std::move(channel.scalings);
                    }
                }
            }
            /*
            #ifdef _DEBUG
                        if (root_nodes.size() > 1) {
                            Log::Info("Multiple root node found for skin ", skin.name, " Total root node: ", root_nodes.size());
                        }

                        if (skin.skeleton != -1 && root_nodes.size() == 1) {
                            ASSERT(skin.skeleton == root_nodes[0]);
                        }
            #endif
            */
            load_state->scene->animation_players.push_back(std::make_unique<AnimationPlayer>(skeletal_asset.get()));
            // std::unique_ptr<AnimationPlayer> &animation_player = load_state->scene->animation_players.emplace_back(skeletal_asset.get()).get();
            load_state->scene->skeletal_assets.push_back(std::move(skeletal_asset));
            Log::Info("Skin Name: ", skin.name);
        }
    }

    Entity ParseNodes(const tinygltf::Model *model, int node_index, Entity parent, LoadState *load_state, MeshType mesh_type) {
        if (load_state->global_joint_list.find(node_index) != load_state->global_joint_list.end())
            return K_INVALID_ENTITY;

        Scene *scene = load_state->scene;
        // We skip skeleton node in node hierarchy
        const tinygltf::Node *node = &model->nodes[node_index];

        // Create parent as default entity to be passed on recursion
        // For camera, we don't create new entity
        auto &comp_manager = scene->ecs->component_manager;
        std::string name = node->name.empty() ? ("Mesh" + std::to_string(node_index)) : node->name;
        Entity entity = scene->create_entity(name, parent);
        TransformComponent *transform = comp_manager->get_component<TransformComponent>(entity);
        ParseNodeTransform(node, transform);

        if (node->mesh >= 0) {
            // Add Mesh Component
            int mesh_id = node->mesh;
            if (mesh_id >= 0) {
                ASSERT(mesh_id < cast_int(load_state->mesh_components.size()));
                MeshComponent &mesh_comp = comp_manager->add_component<MeshComponent>(entity, load_state->mesh_components[mesh_id]);
                mesh_comp.mesh_type = mesh_type;
                mesh_comp.gpu_index = GPUIndexAllocator::allocate_index();
                name = model->meshes[mesh_id].name;
            }
        }

        if (node->camera >= 0) {
            const auto &camera_properties = model->cameras[node->camera];
            ASSERT(camera_properties.type == "perspective");

            Camera *camera = scene->get_camera();

            const tinygltf::PerspectiveCamera &perspective = camera_properties.perspective;
            camera->set_fov(cast_float(glm::degrees(perspective.yfov)));
            camera->set_aspect_ratio(cast_float(perspective.aspectRatio));
            camera->set_near_plane(cast_float(perspective.znear));
            camera->set_far_plane(cast_float(perspective.zfar));

            if (node->translation.size() > 0)
                camera->position = transform->position;
            if (node->rotation.size() > 0) {
                glm::fquat rotation = glm::fquat{(float)node->rotation[3], (float)node->rotation[0], (float)node->rotation[1], (float)node->rotation[2]};
                camera->rotation = glm::degrees(glm::eulerAngles(rotation));
            }
            if (node->matrix.size()) {
                camera->position = transform->position;
                camera->rotation = glm::degrees(glm::eulerAngles(transform->rotation));
                camera->rotation.y = -90.0f + camera->rotation.y;
            }
        }

        if (node->light >= 0) {
            const tinygltf::Light *light = &model->lights[node->light];
            LightComponent &light_component = comp_manager->add_component<LightComponent>(entity, LightComponent{});
            ParseLightComponent(light, &light_component);
        }

        // Either it is skeleton animation or it is a node animation
        if (node->skin >= 0) {
            uint32_t animation_player_index = load_state->animation_player_base_offset + node->skin;
            int default_animation_clip = -1;

            comp_manager->add_component<AnimatorComponent>(entity, AnimatorComponent{
                                                                       .animation_player_index = animation_player_index,
                                                                   });
        }

        // @TODO implement animation sharing
        int default_animation_clip = -1;
        std::vector<AnimationClip> animation_clips;
        for (auto &animation : load_state->animations) {
            if (animation.has_node(node_index)) {
                ASSERT(node->skin == -1);
                AnimationClip &animation_clip = animation_clips.emplace_back(AnimationClip{
                    .start_time = animation.start_time,
                    .end_time = animation.end_time,
                    .tick_per_seconds = 24,
                    .looping = true,
                });
                TempAnimationChannel &channel = animation.channels.at(node_index);
                animation_clip.positions.push_back(std::move(channel.positions));
                animation_clip.rotations.push_back(std::move(channel.rotations));
                animation_clip.scalings.push_back(std::move(channel.scalings));
            }
        }

        if (animation_clips.size() > 0) {
            NodeAnimatorComponent &component = comp_manager->add_component<NodeAnimatorComponent>(entity, NodeAnimatorComponent{
                                                                                                              .current_animation_clip = 0,
                                                                                                              .animation_clips = std::move(animation_clips),
                                                                                                          });
            mesh_type = MESH_TYPE_DYNAMIC;
        }

        for (const auto &child : node->children)
            ParseNodes(model, child, entity, load_state, mesh_type);

        return entity;
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

        // auto &comp_manager = scene->ecs->component_manager;

        //  comp_manager->add_component<NameComponent>(root_entity, root_entity_name);
        //  comp_manager->add_component<TransformComponent>(root_entity);
        //  Make this entity child of scene root
        //  HierarchyComponent &child_comp = comp_manager->add_component<HierarchyComponent>(root_entity);
        //  child_comp.set_parent(scene->entities[0]);

        // Update scene root hierarchy component
        // HierarchyComponent *parent_comp = comp_manager->get_component<HierarchyComponent>(scene->entities[0]);
        // parent_comp->add_children(root_entity);
        // scene->add_entity(root_entity);

        LoadState load_state = {
            .scene = scene,
            .material_base_offset = cast_u32(scene->materials.size()),
            .animation_player_base_offset = cast_u32(scene->animation_players.size()),
        };

        load_state.async_loader = &async_loader;

        LoadMaterials(&gltf_model, &load_state, &user_data);
        LoadMeshes(&gltf_model, &load_state);
        LoadAnimations(&gltf_model, &load_state);
        LoadSkins(&gltf_model, &load_state);

        Entity entity = K_INVALID_ID;
        Entity root_entity = scene->entities[0];
        std::string root_entity_name = utils::get_filename(filename);

        for (const auto &gltf_scene : gltf_model.scenes) {
            for (const auto &node : gltf_scene.nodes)
                entity = ParseNodes(&gltf_model, node, root_entity, &load_state, MESH_TYPE_STATIC);
        }

        async_loader.wait();

        Log::Info("Loaded: ", root_entity_name, "[", load_timer.elapsed_seconds(), "s]");
        Log::Info("meshes: ", load_state.mesh_components.size());

        return entity;
    } // namespace mirai
} // namespace mirai