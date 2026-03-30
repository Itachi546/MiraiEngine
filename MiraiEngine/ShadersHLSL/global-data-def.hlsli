#ifndef PER_FRAME_DATA_HLSLI
#define PER_FRAME_DATA_HLSLI

struct PerFrameData
{
    float4x4 P;
    float4x4 V;
    float4x4 VP;
    float4x4 invVP;

    float3 camera_position;
    float elapsed_time;

    float3 light_direction;
    float cast_shadow;

    float3 light_color;
    float light_intensity;

    float width;
    float height;
    uint irradiance_map;
    uint prefilter_map;

    uint brdf_texture_map;
    uint padding[3];
};

struct DrawData {
    uint transform_index;
    uint material_index;
    uint vertex_offset;
    uint vertex_stride;
};

#endif