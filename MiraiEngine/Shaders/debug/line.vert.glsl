#version 460
#extension GL_GOOGLE_include_directive : enable
#include "../utils/color.glsl"
#include "../utils/per-frame-data.glsl"
struct Vertex {
    float sx, sy, sz;
    uint color;
};

layout(binding = 0) readonly uniform PerFrameBindings {
    PerFrameData per_frame_data;
};

layout(std430, binding = 1) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(location = 0) out vec3 vColor;
layout(location = 1) out vec2 velocity;

void main() {
    Vertex vertex = vertices[gl_VertexIndex];
    vec4 position = vec4(vertex.sx, vertex.sy, vertex.sz, 1.0f);
    vec4 current_clip_pos = per_frame_data.VP * position;
    vec4 prev_clip_pos = per_frame_data.prev_VP * position;
    gl_Position = current_clip_pos;

    vColor = u32_to_rgba(vertex.color).rgb;
    velocity = get_pixel_velocity(current_clip_pos, prev_clip_pos, per_frame_data.current_frame_jitter, per_frame_data.prev_frame_jitter);
}