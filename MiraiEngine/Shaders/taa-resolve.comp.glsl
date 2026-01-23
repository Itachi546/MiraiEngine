#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D u_color;
layout(set = 0, binding = 1, rgba16f) uniform image2D u_taa_history;
layout(set = 0, binding = 2) uniform sampler2D u_depth;
layout(set = 0, binding = 3) uniform sampler2D u_velocity;

layout(push_constant) uniform PushConstant {
    float width;
    float height;
    float should_sample_motion_vector;
    float _paddding;
};

vec2 uv_nearest(ivec2 pixel, vec2 texture_size) {
    vec2 uv = floor(pixel) + .5;
    return uv / texture_size;
}

ivec2 id_nearest(vec2 uv, vec2 texture_size) {
    return ivec2(uv * texture_size);
}

void main() {
    ivec2 id = ivec2(gl_GlobalInvocationID.xy);

    vec2 image_size = vec2(width, height);
    vec2 uv = uv_nearest(id, image_size);

    vec4 src_color = textureLod(u_color, uv, 0).rgba;

    vec2 velocity = vec2(0.0f);

    if (should_sample_motion_vector > 0.5) {
        velocity = texelFetch(u_velocity, id, 0).rg;
    }

    ivec2 reprojected_id = id_nearest(uv - velocity, image_size);

    vec4 taa_history_color = imageLoad(u_taa_history, reprojected_id);

    vec3 final_color = src_color.rgb * 0.1 + taa_history_color.rgb * 0.9;

    imageStore(u_taa_history, id, vec4(final_color, 1.0f));
}
