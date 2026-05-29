#version 460

#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "ddgi.glsl"

#define DISABLE_TEXTURE_DERIVATIVE
#include "../utils/bindless-texture.glsl"
#include "../utils/bindless-sampler.glsl"
#include "../utils/material.glsl"
#include "../utils/light.glsl"
#include "../utils/color.glsl"
#include "../pbr/pbr-lighting.glsl"

layout(set = 0, binding = 4) readonly buffer VertexBuffer {
    uint vertices[];
};

#include "../utils/vertexdata.glsl"
#include "../utils/rt-common.glsl"

layout(set = 0, binding = 5) readonly buffer MeshInstanceBuffer {
    MeshInstanceData mesh_instances[];
};

layout(set = 0, binding = 6) readonly buffer MaterialBuffer {
    PBRMaterial materials[];
};

layout(set = 0, binding = 7) readonly buffer LightBuffer {
    Light lights[];
};

layout(location = 0) rayPayloadInEXT DDGIRayPayload p_payload;

hitAttributeEXT vec2 bary_coord;

vec3 direct_lighting(Light light, vec3 V, vec3 N, PBRParameter pbr, float shadow_factor) {
    return evaluateDirectionalLight(light, V, N, pbr, shadow_factor);
}

void main() {
    MeshInstanceData instance = mesh_instances[gl_InstanceCustomIndexEXT];
    vec3 bary_coord = vec3(1.0f - bary_coord.x - bary_coord.y, bary_coord.x, bary_coord.y);

    Vertex vertex = fetch_interpolated_vertex(gl_PrimitiveID, instance, bary_coord);
    vec3 world_pos = vec3(gl_ObjectToWorldEXT * vec4(vertex.position, 1.0));

    PBRMaterial material = materials[instance.material_index];
    vec2 tex_scale = vec2(material.texture_scale_x, material.texture_scale_y);
    vec2 uv = vertex.tex_coord * tex_scale;

    PBRParameter pbr;
    pbr.albedo = fetch_albedo(material, uv, 0.0);
    pbr.emissive = fetch_emissive(material, uv, 0.0);
    vec2 mr = fetch_pbr_metallic_roughness(material, uv, 0.0);
    pbr.metallic = mr.x;
    pbr.roughness = mr.y;
    pbr.ao = 1.0;

    // Build TBN and apply normal map
    vec3 sn = fetch_normal_map(material, uv, 0.0);
    mat3 normal_matrix = transpose(inverse(mat3(gl_ObjectToWorldEXT)));
    vec3 N = normalize(normal_matrix * vertex.normal);
    vec3 T = normalize(normal_matrix * vertex.tangent);
    vec3 B = cross(N, T);
    vec3 detail_normal = normalize(sn.x * T + sn.y * B + sn.z * N);

    vec3 V = -gl_WorldRayDirectionEXT;

    // @TODO fix this later, testing only directional light for now
    Light light = lights[0];

    // vec3 Lo = vec3(N * 0.5 + 0.5);
    vec3 Lo = (direct_lighting(light, V, detail_normal, pbr, 1.0f) + pbr.emissive.rgb) * p_payload.T;

    p_payload.L = Lo;
    p_payload.hit_distance = gl_RayTminEXT + gl_HitTEXT;
}