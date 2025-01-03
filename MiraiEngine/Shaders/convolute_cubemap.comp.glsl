#version 460

#extension GL_GOOGLE_include_directive : enable

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform samplerCube u_cubemap;
layout(set = 0, binding = 1, rgba16f) uniform imageCube u_irradiance_map;

#include "utils/cubemap.glsl"

layout(push_constant) uniform PushConstants {
    vec2 irradiance_map_dims;
    vec2 cubemap_dims;
};

const int K_MAX_SAMPLES = 512;
const float PI = 3.141593;
const float PI2 = 6.283185;
const float PIH = 1.570796;

vec3 convolute(vec3 direction) {
    float dP = PI2 / float(K_MAX_SAMPLES);
    float dT = PIH / float(K_MAX_SAMPLES);

    // Orthonormal basis
    vec3 normal = direction;
    vec3 tangent = cross(vec3(0.0, 1.0, 0.0), normal);
    vec3 bitangent = cross(normal, tangent);

    int sample_count = 0;
    vec3 irradiance = vec3(0.0f);
    for (float phi = 0; phi <= PI2; phi += dP) {
        float cos_phi = cos(phi);
        float sin_phi = 1 - cos_phi * cos_phi;
        for (float theta = 0; theta <= PIH; theta += dT) {
            float sin_theta = sin(theta);
            float cos_theta = 1 - sin_theta * sin_theta;
            vec3 sphere_coord = vec3(cos_phi * sin_theta, sin_phi * sin_theta, cos_theta);

            vec3 sample_dir = sphere_coord.x * tangent + sphere_coord.y * bitangent + sphere_coord.z * normal;
            irradiance += texture(u_cubemap, normalize(sample_dir)).rgb;
            sample_count++;
        }
    }

    return irradiance / float(sample_count);
}

void main() {
    ivec3 uv = ivec3(gl_GlobalInvocationID.xyz);
    vec3 direction = normalize(uv_to_xyz(uv, irradiance_map_dims));

    vec3 irradiance = convolute(direction);
    imageStore(u_irradiance_map, uv, vec4(irradiance, 1.0f));
}