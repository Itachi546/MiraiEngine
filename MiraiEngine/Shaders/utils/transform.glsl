#ifndef TRANSFORMATION_GLSL
#define TRANSFORMATION_GLSL

// @TODO Decompose the component instead of doing all the multiplication
// https://gamedev.stackexchange.com/questions/108856/fast-position-reconstruction-from-depth
vec3 ndc_pos_to_world_pos(vec3 clip_pos, mat4 invVP) {
    vec4 world_pos = invVP * vec4(clip_pos, 1.0f);
    return world_pos.xyz / world_pos.w;
}

vec3 ndc_pos_to_view_pos(vec3 clip_pos, mat4 invP) {
    vec4 view_pos = invP * vec4(clip_pos, 1.0f);
    return view_pos.xyz / view_pos.w;
}

float linearize_depth(float d, float near, float far) {
    return (near * far) / (far - d * (far - near));
}

/*
    Reconstruct view position from view space depth and uv (0-1)
*/
vec3 get_view_pos_from_uv_depth(vec2 uv, float depth, float tanh_fov, float aspect_ratio) {
    uv = vec2(uv.x * 2.0f - 1.0f, 1.0f - 2.0f * uv.y);
    vec3 ray = vec3(uv.x * (aspect_ratio * tanh_fov),
                    uv.y * tanh_fov,
                    -1.0);

    return ray * depth;
}

vec2 octwarp(vec2 v) {
    vec2 w = 1.0 - abs(v.yx);
    if (v.x < 0.0)
        w.x = -w.x;
    if (v.y < 0.0)
        w.y = -w.y;
    return w;
}

vec2 octahedral_encode(vec3 n) {
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    n.xy = n.z >= 0.0 ? n.xy : octwarp(n.xy);
    n.xy = n.xy * 0.5 + 0.5;
    return n.xy;
}

vec3 octahedral_decode(vec2 f) {
    f = f * 2.0 - 1.0;

    // https://twitter.com/Stubbesaurus/status/937994790553227264
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = max(-n.z, 0.0);

    n.x += n.x >= 0.0 ? -t : t;
    n.y += n.y >= 0.0f ? -t : t;
    return normalize(n);
}

vec2 pack_float(float v) {
    uint bits = floatBitsToUint(v);
    uint high = (bits >> 16u) & 0xffffu;
    uint low = bits & 0xffffu;
    return unpackHalf2x16(high << 16 | low);
}

float unpack_float(vec2 packed) {
    uint bits = packHalf2x16(packed);
    return uintBitsToFloat(bits);
}

#endif