#version 450

layout(binding = 1) uniform sampler2D textureSamplers[9];

layout(location = 0) in vec3 fragmentPosition;
layout(location = 1) in vec3 fragmentColor;
layout(location = 2) in vec2 fragmentTextureCoordinates;
layout(location = 3) in vec3 fragmentNormal;

layout(location = 0) out vec4 gPosition;
layout(location = 1) out vec4 gColor;
layout(location = 2) out vec4 gNormal;
layout(location = 3) out vec4 gExtra;

vec3 getNormalFromMap() {
    vec3 tangentNormal = texture(textureSamplers[7], fragmentTextureCoordinates).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(fragmentPosition);
    vec3 Q2  = dFdy(fragmentPosition);
    vec2 st1 = dFdx(fragmentTextureCoordinates);
    vec2 st2 = dFdy(fragmentTextureCoordinates);

    vec3 N  = fragmentNormal;
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

void main() {
    gPosition = vec4(fragmentPosition, 1.0);
    vec4 tempColor = texture(textureSamplers[6], fragmentTextureCoordinates);
    if (tempColor.w <= 0.01f)
        discard;
    gColor = vec4(tempColor.xyz, 1.0f);
    gNormal = vec4(getNormalFromMap(), textureSamplers[8]);
    gExtra = vec4(0.0f, 0.0f, 0.0f, 0.0f);
}
