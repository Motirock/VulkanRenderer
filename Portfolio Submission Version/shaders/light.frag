#version 450

layout(location = 0) in vec3 fragmentPosition;
layout(location = 1) in vec3 fragmentColor;
layout(location = 2) in float fragmentStrength;

layout(location = 0) out vec4 gPosition; //w is for to ignore
layout(location = 1) out vec4 gColor; //w is for water
layout(location = 2) out vec4 gNormal; //w for roughness
layout(location = 3) out vec4 gBloom; //bloom (xyz). W ???

void main() {
    gPosition = vec4(0.0f, 0.0f, 0.0f, 0.0f);
    gColor = vec4(fragmentColor, 1.0f);
    gNormal = vec4(0.0f, 0.0f, 0.0f, 0.0f);
    gBloom = vec4(fragmentColor*fragmentStrength, 0.0f);
}
