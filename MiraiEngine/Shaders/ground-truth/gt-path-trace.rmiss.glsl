#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

layout(location = 0) rayPayloadInEXT vec4 hit_color;

#include "../utils/bindless-texture.glsl"
#include "../utils/bindless-sampler.glsl"

layout(push_constant) uniform PushConstant {
    mat4 invP;
    mat4 invV;
    vec3 camera_position;
    uint skybox_texture_index;
};

void main() {
    vec3 ray_dir = -normalize(gl_WorldRayDirectionEXT);
    vec4 sky_color = sample_texture_cube(skybox_texture_index, u_samplers[SAMPLER_LINEAR_CLAMP], ray_dir);
    hit_color = vec4(sky_color.rgb, 1.0f);
}