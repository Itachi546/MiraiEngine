#version 450

#extension GL_GOOGLE_include_directive : enable
#include "utils/raycast.glsl"

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 fragColor;

layout(push_constant) uniform PushConstants {
    mat4 invP;
    mat4 invV;
};

const vec4 sky_top_color = vec4(0.385, 0.454, 0.55, 1.0);
const vec4 sky_horizon_color = vec4(0.646, 0.656, 0.67, 1.0);
const vec3 sun_color = vec3(1.0, .8, .55);
vec4 ground_bottom_color = vec4(0.2, 0.169, 0.133, 1.0);
vec4 ground_horizon_color = vec4(0.646, 0.656, 0.67, 1.0);

#define PI 3.141592

void main() {
    vec3 rd = generate_camera_ray(uv, invP, invV);
    float v_angle = acos(clamp(rd.y, -1.0, 1.0));
    float c = (1.0 - v_angle / (PI * 0.5));
    vec3 sky = mix(sky_horizon_color.rgb, sky_top_color.rgb, clamp(1.0 - pow(1.0 - c, 6.66), 0.0, 1.0));
    /*
    vec3 light_dir = normalize(vec3(0.3, 0.1f, 0.3f));
    float sun_angle = acos(dot(light_dir, rd));
    if (sun_angle < 0.015)
        sky = vec3(1.);
    else
        sky += pow(max(dot(light_dir, rd), 0.0), 30.0) * 0.4 * sun_color;
    */
    c = (v_angle - (PI * 0.5)) / (PI * 0.5);
    vec3 ground = mix(ground_horizon_color.rgb, ground_bottom_color.rgb, clamp(1.0 - pow(1.0 - c, 6.66), 0.0, 1.0));

    vec3 col = mix(ground, sky, step(0, rd.y));
    fragColor = vec4(col, 1.0f);
}