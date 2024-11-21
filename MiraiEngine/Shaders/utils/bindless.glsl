#ifndef BINDLESS_GLSL
#define BINDLESS_GLSL

#extension GL_EXT_nonuniform_qualifier : enable

const uint K_INVALID_TEXTURE = 0xffffffff;

layout(set = 1, binding = 0) uniform sampler2D u_bindless_texture[];

#define sample_texture(index, uv) texture(u_bindless_texture[nonuniformEXT(index)], uv)

#endif