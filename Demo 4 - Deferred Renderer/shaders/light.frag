#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragmentPosition;
layout(location = 1) in vec3 fragmentColor;
layout(location = 2) in vec2 fragmentTextureCoordinates;
layout(location = 3) in vec3 fragmentNormal;

layout(location = 0) out vec4 gPosition; //a is for to ignore
layout(location = 1) out vec4 gColor; //a is for bloom
layout(location = 2) out vec4 gNormal; //a is for water?

void main() {
    gPosition = vec4(0.0f);
    gColor = vec4(fragmentColor, 1.0);
    gNormal = vec4(0.0f, 0.0f, 0.0f, 1.0f);
}
