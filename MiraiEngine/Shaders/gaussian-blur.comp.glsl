#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

#extension GL_GOOGLE_include_directive : enable

layout(set = 0, binding = 0, r16f) uniform image2D u_output_texture;
layout(set = 0, binding = 1) uniform sampler2D u_input_texture;

layout(push_constant) uniform BlurPushConstants {
    float width;
    float height;
    float direction;
    float sigma;
    float sample_count;
};

#define SQRT_2PI 2.50662827463

float gaussianDistribution(float x) {
    float x2 = x * x;
    float denom = 1.0 / (sigma * SQRT_2PI);
    return exp(-x2 / (2.0 * sigma * sigma)) * denom;
}

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    if (coord.x >= width || coord.y >= height)
        return;

    vec2 inv_res = 1.0f / vec2(width, height);
    vec2 uv = vec2(coord) * inv_res;

    vec2 direction = direction == 0 ? vec2(1.0f, 0.0f) : vec2(0.0f, 1.0f);
    direction = direction * inv_res;

    bool enable_hardware_filtering = true;
    float col = texture(u_input_texture, uv).r * gaussianDistribution(0);
    // https://lisyarus.github.io/blog/posts/compute-blur.html#section-separable
    if (enable_hardware_filtering) {
        for (float i = 1; i <= sample_count; i += 2) {
            float w0 = gaussianDistribution(i);
            float w1 = gaussianDistribution(i + 1);
            float w = w0 + w1;
            float t = w1 / w;

            vec2 offset = direction * (i + t);
            col += w * texture(u_input_texture, uv + offset).r;
            col += w * texture(u_input_texture, uv - offset).r;
        }
    } else {
        for (float i = 1; i <= sample_count; ++i) {
            vec2 offset = direction * i;
            float weight = gaussianDistribution(i);
            col += weight * texture(u_input_texture, uv + offset).r;
            col += weight * texture(u_input_texture, uv - offset).r;
        }
    }
    imageStore(u_output_texture, coord, vec4(col));
}
