#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D u_color;
layout(set = 0, binding = 1, rgba16f) uniform image2D u_taa_history;
layout(set = 0, binding = 2) uniform sampler2D u_depth;
layout(set = 0, binding = 3) uniform sampler2D u_velocity;

layout(push_constant) uniform PushConstant {
    float inv_width;
    float inv_height;
    float _paddding[2];
};

void main() {
    ivec2 id = ivec2(gl_GlobalInvocationID.xy);

    vec2 uv = id * vec2(inv_width, inv_height);

    vec4 src_color = texture(u_color, uv).rgba;
    vec4 taa_history_color = imageLoad(u_taa_history, id);

    vec3 final_color = src_color.rgb * 0.1 + taa_history_color.rgb * 0.9;

    imageStore(u_taa_history, id, vec4(final_color, 1.0f));
}
