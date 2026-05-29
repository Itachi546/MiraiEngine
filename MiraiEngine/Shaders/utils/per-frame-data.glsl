#ifndef PER_FRAME_DATA_GLSL
#define PER_FRAME_DATA_GLSL

struct PerFrameData {
    mat4 P;
    mat4 V;
    mat4 VP;
    mat4 invVP;
    mat4 prev_VP;

    vec2 current_frame_jitter;
    vec2 prev_frame_jitter;

    vec3 camera_position;
    float elapsed_time;

    float width;
    float height;
    uint irradiance_map;
    uint prefilter_map;

    uint prefilter_mip_count;
    uint brdf_texture_map;
    uint skybox_texture_index;
    uint padding;
};

vec2 ndc_to_uv(vec2 uv) {
    return vec2(uv.x * 0.5 + 0.5, 0.5 - 0.5 * uv.y);
}

vec2 get_pixel_velocity(vec4 current_clip_pos, vec4 prev_clip_pos, vec2 current_frame_jitter, vec2 prev_frame_jitter) {
    vec2 current_ndc_pos = current_clip_pos.xy / current_clip_pos.w;
    vec2 prev_ndc_pos = prev_clip_pos.xy / prev_clip_pos.w;
    return ndc_to_uv(current_ndc_pos + current_frame_jitter) - ndc_to_uv(prev_ndc_pos + prev_frame_jitter);
}

#endif