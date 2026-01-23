#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D u_color;
layout(set = 0, binding = 1, rgba16f) uniform image2D u_taa_history;

layout(push_constant) uniform PushConstant {
    float inv_width;
    float inv_height;
    float _paddding[2];
};

void main() {
    ivec2 id = ivec2(gl_GlobalInvocationID.xy);

    vec2 uv = id * vec2(inv_width, inv_height);

    vec4 src_color = texture(u_color, uv);

    imageStore(u_taa_history, id, src_color);
}
