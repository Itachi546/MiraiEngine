#version 450

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec3 vNormal;

void main() {
    vec3 n = normalize(vNormal);
    vec3 ld = normalize(vec3(-0.5, 1.0f, -0.5f));

    vec3 col = vec3(0.0f);
    float diffuse = max(dot(n, ld), 0.0f);
    col += diffuse;
    float indirect = (n.y * 0.5 + 0.5) * 0.1f;
    col += indirect;
    outColor = vec4(col, 1.0f);
}