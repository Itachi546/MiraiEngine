#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_ray_query : enable

#define ENABLE_RT_SHADOW 1
#include "utils/deferred_lighting.glsl"