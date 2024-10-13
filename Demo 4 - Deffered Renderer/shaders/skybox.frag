#version 450

layout(binding = 2) uniform sampler2D skyboxSamplers[6];

layout(location = 0) in vec2 fragmentTextureCoordinates;
layout(location = 1) in flat uint fragmentFaceIndex;

layout(location = 0) out vec4 gPosition;
layout(location = 1) out vec4 gColor;
layout(location = 2) out vec4 gNormal;

void main() {
    vec4 sampledColor = texture(skyboxSamplers[fragmentFaceIndex], fragmentTextureCoordinates);
    gPosition = vec4(0.0);
    gColor = sampledColor;
    gNormal = vec4(0.0, 0.0, 0.0, 1.0);
}