#version 450

layout(binding = 1) uniform sampler2D textureSamplers[9];

layout(push_constant) uniform PushConstant {
    uint isWater;
    uint isReflect;
    uint isDay;
} pushConstant;

layout(location = 0) in vec2 fragmentTextureCoordinates;
layout(location = 1) in flat uint fragmentFaceIndex;

layout(location = 0) out vec4 gPosition;
layout(location = 1) out vec4 gColor;
layout(location = 2) out vec4 gNormal;
layout(location = 3) out vec4 gBloom;

const float SKY_BRIGHTNESS = 2.0f;

void main() {
    vec4 sampledColor = texture(textureSamplers[fragmentFaceIndex], fragmentTextureCoordinates)*pushConstant.isDay;
    gPosition = vec4(0.0f, 0.0f, 0.0f, 0.0f);
    gColor = vec4(sampledColor.xyz, 1.0f);
    gNormal = vec4(0.0f, 0.0f, 0.0f, 0.0f);
    gBloom = vec4(sampledColor.xyz*SKY_BRIGHTNESS, 0.0f);
}