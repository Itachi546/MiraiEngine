#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_ray_query : require

#define DISABLE_TEXTURE_DERIVATIVE
#include "../utils/bindless-texture.glsl"
#include "../utils/bindless-sampler.glsl"
#include "../utils/material.glsl"
#include "../utils/light.glsl"
#include "../utils/color.glsl"
#include "../pbr/pbr.glsl"
#include "../utils/sample-direction.glsl"
#include "ground-truth.glsl"

layout(location = 0) rayPayloadInEXT RayPayload p_payload;
layout(location = 1) rayPayloadEXT RayPayload p_indirect_payload;
hitAttributeEXT vec2 bary_coord;

struct MeshInstanceData {
    uint vertex_offset;
    uint index_offset;
    uint vertex_stride;
    uint material_index;
};

#define MAX_RAY_DEPTH 2

layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;

layout(set = 0, binding = 2) readonly buffer MeshInstanceBuffer {
    MeshInstanceData mesh_instances[];
};

layout(set = 0, binding = 3) readonly buffer VertexBuffer {
    uint vertices[];
};

layout(set = 0, binding = 4) readonly buffer MaterialBuffer {
    PBRMaterial materials[];
};

layout(std430, set = 0, binding = 6) readonly buffer Lights {
    Light lights[];
};

#include "../utils/vertexdata.glsl"

struct Vertex {
    vec3 position;
    vec3 normal;
    vec3 tangent;
    vec2 tex_coord;
};

Vertex fetch_interpolated_vertex(uint triangle_index, MeshInstanceData instance, vec3 bary) {
    uint index_address = instance.index_offset + triangle_index * 3;
    uint i0 = vertices[index_address + 0];
    uint i1 = vertices[index_address + 1];
    uint i2 = vertices[index_address + 2];

    uint va0 = instance.vertex_offset + i0 * instance.vertex_stride;
    uint va1 = instance.vertex_offset + i1 * instance.vertex_stride;
    uint va2 = instance.vertex_offset + i2 * instance.vertex_stride;

    Vertex result;
    result.position = bary.x * unpack_position(va0) + bary.y * unpack_position(va1) + bary.z * unpack_position(va2);
    result.normal = normalize(bary.x * unpack_normal(va0) + bary.y * unpack_normal(va1) + bary.z * unpack_normal(va2));
    result.tangent = normalize(bary.x * unpack_tangent(va0) + bary.y * unpack_tangent(va1) + bary.z * unpack_tangent(va2));
    result.tex_coord = bary.x * unpack_uv(va0) + bary.y * unpack_uv(va1) + bary.z * unpack_uv(va2);
    return result;
}

float trace_shadow(vec3 world_pos, vec3 N, vec3 light_dir) {
    rayQueryEXT ray_query;

    rayQueryInitializeEXT(
        ray_query,
        tlas,
        gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsCullBackFacingTrianglesEXT,
        0xFF,
        world_pos + N * 0.01,
        0.0,
        light_dir,
        1000.0);

    while (rayQueryProceedEXT(ray_query)) {
        // Confirm every candidate intersection immediately
        if (rayQueryGetIntersectionTypeEXT(ray_query, false) == gl_RayQueryCandidateIntersectionTriangleEXT) {
            rayQueryConfirmIntersectionEXT(ray_query);
            break; // any hit is enough — stop traversal
        }
    }

    return rayQueryGetIntersectionTypeEXT(ray_query, true) == gl_RayQueryCommittedIntersectionNoneEXT
               ? 1.0
               : 0.0;
}

vec3 evaluateBRDF(vec3 L, vec3 V, vec3 N, PBRParameter pbr) {
    vec3 H = normalize(V + L);
    float NdotL = clamp(dot(N, L), 0.001, 1.0);
    float NdotV = clamp(dot(N, V), 0.001, 1.0);
    float NdotH = clamp(dot(N, H), 0.0, 1.0);
    float LdotH = clamp(dot(L, H), 0.0, 1.0);

    vec3 F0 = mix(vec3(0.04), pbr.albedo.rgb, pbr.metallic);
    vec3 diffuse = pbr.albedo.rgb / PI;

    float D = D_GGX(NdotH, pbr.roughness);
    float G = G_Smith(NdotV, NdotL, pbr.roughness);
    vec3 F = F_Schlick(LdotH, F0);

    vec3 specular = (D * F * G) / (4.0 * NdotV * NdotL + 0.0001);
    vec3 kD = (1.0 - F) * (1.0 - pbr.metallic); // fix: use F not specular for energy conservation
    return (kD * diffuse + specular) * NdotL;
}

vec3 evaluateDirectionalLight(in Light light, in vec3 V, in vec3 N, in PBRParameter pbr, float shadow) {
    vec3 radiance = u32_to_rgba(light.color).rgb * light.intensity;
    return evaluateBRDF(light.direction, V, N, pbr) * shadow * radiance;
}

vec3 direct_lighting(Light light, vec3 V, vec3 N, PBRParameter pbr, float shadow_factor) {
    return evaluateDirectionalLight(light, V, N, pbr, shadow_factor);
}

vec3 indirect_lighting(vec3 V, vec3 N, vec3 world_pos, PBRParameter pbr) {
    // Cosine-weighted hemisphere sample — two independent floats from RNG
    vec2 xi = next_vec2(p_payload.rng);
    vec3 dir = cosine_hemisphere_sample(N, xi);

    const float tmin = 0.01;

    p_indirect_payload.L = vec3(0.0);
    p_indirect_payload.rng = p_payload.rng; // propagate RNG state
    p_indirect_payload.depth = p_payload.depth + 1;

    traceRayEXT(
        tlas,
        gl_RayFlagsOpaqueEXT | gl_RayFlagsCullBackFacingTrianglesEXT,
        0xFF,
        0,                    // sbtRecordOffset — same hit group as primary
        0,                    // sbtRecordStride
        0,                    // missIndex       — same miss shader as primary
        world_pos + N * tmin, // world-space origin, offset along normal
        0.0,
        dir,
        10000.0,
        1 // payload location 1
    );

    p_payload.rng = p_indirect_payload.rng; // bring RNG state back

    // Cosine-weighted PDF = NdotL/PI, BRDF diffuse = albedo/PI
    // estimator: BRDF * NdotL / PDF = albedo (for pure diffuse)
    vec3 kD = (1.0 - pbr.metallic) * pbr.albedo.rgb;
    return kD * p_indirect_payload.L;
}

void main() {
    MeshInstanceData instance = mesh_instances[gl_InstanceCustomIndexEXT];

    vec3 bary = vec3(1.0 - bary_coord.x - bary_coord.y, bary_coord.x, bary_coord.y);
    Vertex vertex = fetch_interpolated_vertex(gl_PrimitiveID, instance, bary);

    // Transform vertex position to world space for secondary ray origin
    vec3 world_pos = vec3(gl_ObjectToWorldEXT * vec4(vertex.position, 1.0));

    PBRMaterial material = materials[instance.material_index];
    vec2 tex_scale = vec2(material.texture_scale_x, material.texture_scale_y);
    vec2 uv = vertex.tex_coord * tex_scale;

    PBRParameter pbr;
    pbr.albedo = fetch_albedo(material, uv, 0.0);
    pbr.emissive = fetch_emissive(material, uv, 0.0);
    vec2 mr = fetch_pbr_metallic_roughness(material, pbr.albedo, uv, 0.0);
    pbr.metallic = mr.x;
    pbr.roughness = mr.y;
    pbr.ao = 1.0;

    // Build TBN and apply normal map
    vec3 sn = fetch_normal_map(material, uv, 0.0);
    vec3 N = vertex.normal;
    vec3 T = vertex.tangent;
    vec3 B = cross(N, T);
    vec3 detail_normal = normalize(sn.x * T + sn.y * B + sn.z * N);

    vec3 V = -gl_WorldRayDirectionEXT;

    vec3 light_dir = get_cone_sample(next_vec2(p_payload.rng), lights[0].direction, cos(lights[0].radius_or_height));

    float shadow_factor = trace_shadow(world_pos, N, light_dir);
    vec3 Lo = direct_lighting(lights[0], V, detail_normal, pbr, shadow_factor);
    Lo += pbr.emissive.rgb;

    // Single bounce indirect — if (depth+1 < MAX) not while
    if ((p_payload.depth + 1) < MAX_RAY_DEPTH)
        Lo += indirect_lighting(V, N, world_pos, pbr);

    p_payload.L = Lo;
}