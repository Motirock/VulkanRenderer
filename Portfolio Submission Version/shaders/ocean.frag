#version 450

layout(binding = 1) uniform sampler2D gSamplers[10];

layout(location = 0) in vec4 fragmentPosition;
layout(location = 1) in vec2 fragmentDudv1Offset;
layout(location = 2) in vec2 fragmentDudv2Offset;

layout(location = 0) out vec4 gPosition;
layout(location = 1) out vec4 gColor;
layout(location = 2) out vec4 gNormal;
layout(location = 3) out vec4 gBloom;

const float frequency = 0.1f;

void main() {
    gPosition = fragmentPosition;
    vec2 dudv1 = texture(gSamplers[9], frequency*(fragmentPosition.xy+fragmentDudv1Offset)).xy*0.8f;
    vec2 dudv2 = texture(gSamplers[9], frequency*(fragmentPosition.xy+fragmentDudv2Offset)).xy*0.2f;
    gColor = vec4(dudv1+dudv2, 0.0f, 0.0f);
    gNormal = vec4(0.0f);
    gBloom = vec4(0.0f);
}