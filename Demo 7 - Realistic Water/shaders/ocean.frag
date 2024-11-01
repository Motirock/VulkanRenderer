#version 450

layout(location = 0) in vec4 fragmentPosition;

layout(location = 0) out vec4 gPosition;
layout(location = 1) out vec4 gColor;
layout(location = 2) out vec4 gNormal;
layout(location = 3) out vec4 gBloom;

void main() {
    gPosition = fragmentPosition;
    gColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
    gNormal = vec4(0.0f);
    gBloom = vec4(0.0f);
}