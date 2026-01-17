#version 460

#define BLUR 1

#extension GL_GOOGLE_include_directive : require

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(push_constant) uniform block {
    float width;
    float height;
    float direction;
    float znear;
};

layout(binding = 0) uniform writeonly image2D u_output_texture;
layout(binding = 1) uniform sampler2D u_shadow_texture;
layout(binding = 2) uniform sampler2D u_depth_texture;

float sample_depth_texture(ivec2 uv) {
    return 1.0f - texelFetch(u_depth_texture, uv, 0).r;
}

void main() {
    uvec2 pos = gl_GlobalInvocationID.xy;

#if BLUR
    float shadow = texelFetch(u_shadow_texture, ivec2(pos), 0).r;
    float accumw = 1;

    float depth = znear / sample_depth_texture(ivec2(pos));

    ivec2 offsetMask = -ivec2(direction, 1.0f - direction);

    const int KERNEL = 10;

    for (int sign = -1; sign <= 1; sign += 2) {
        ivec2 uvnext = ivec2(pos) + (ivec2(sign) & offsetMask);
        float dnext = znear / sample_depth_texture(uvnext);
        float dgrad = abs(depth - dnext) < 0.1 ? dnext - depth : 0;

        for (int i = 1; i <= KERNEL; ++i) {
            ivec2 uvoff = ivec2(pos) + (ivec2(i * sign) & offsetMask);

            float gw = exp2(-i * i / 10);
            float dv = znear / sample_depth_texture(uvoff);
            float dw = exp2(-abs(dv - (depth + dgrad * i)) * 20);
            float fw = gw * dw;

            shadow += texelFetch(u_shadow_texture, uvoff, 0).r * fw;
            accumw += fw;
        }
    }

    shadow /= accumw;
#else
    float shadow = texelFetch(u_shadow_texture, ivec2(pos), 0).r;
#endif
    imageStore(u_output_texture, ivec2(pos), vec4(shadow, 0, 0, 0));
}