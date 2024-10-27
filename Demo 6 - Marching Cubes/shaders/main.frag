#version 450

layout(binding = 1) uniform sampler2D texSampler[3];

layout(location = 0) in vec3 fragmentPosition;
layout(location = 1) in vec3 fragmentColor;
layout(location = 2) in vec2 fragmentTextureCoordinates;
layout(location = 3) in vec3 fragmentNormal;

layout(location = 0) out vec4 gPosition;
layout(location = 1) out vec4 gColor;
layout(location = 2) out vec4 gNormal;
layout(location = 3) out vec4 gExtra;

void main() {
    gPosition = vec4(fragmentPosition, 1.0);
    vec4 tempColor = texture(texSampler[0], fragmentTextureCoordinates);
    if (tempColor.w <= 0.01f)
        discard;
    gColor = vec4(tempColor.xyz, 1.0f);
    gNormal = vec4(fragmentNormal, 0.1f);
    gExtra = vec4(0.0f, 0.0f, 0.0f, 0.0f);
}
