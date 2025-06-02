#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

#extension GL_GOOGLE_include_directive : enable

layout(set = 0, binding = 0, r16f) uniform image2D u_output_texture;
layout(set = 0, binding = 1) uniform sampler2D u_input_texture;
#define PI 3.141592

layout(push_constant) uniform BlurPushConstants {
    float width;
    float height;
    float direction;
    float blur_radius;
    float sample_count;
};

float guassian(float x) {
    float denom = 2.0 * sample_count * sample_count;
    float v2 = x * x;
    return exp(-v2 / denom) / sqrt(PI * denom);
}

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    if (coord.x >= width || coord.y >= height)
        return;

    vec2 inv_res = 1.0f / vec2(width, height);
    vec2 uv = vec2(coord) * inv_res;

    vec2 direction = direction == 0 ? vec2(1.0f, 0.0f) : vec2(0.0f, 1.0f);
    direction = direction * inv_res;

    float col = 0.0f;
    for (float i = -sample_count; i <= sample_count; ++i) {
        vec2 offset = direction * i * blur_radius;
        float weight = guassian(i);
        col += weight * texture(u_input_texture, uv + offset).r;
    }
    imageStore(u_output_texture, coord, vec4(col));
}
