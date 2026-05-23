#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 hitColor;
hitAttributeEXT vec2 baryCoord;

void main() {
    // Color using barycentric coordinates
    hitColor = vec3(1.0 - baryCoord.x - baryCoord.y, baryCoord.x, baryCoord.y);
}