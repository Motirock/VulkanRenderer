#version 450

layout(binding = 1) uniform sampler2D textureSamplers[9];

layout(binding = 2) readonly buffer PolygonInfoBuffer {
    mat3 TBNs[2000000];
} polygonInfoBuffer;

layout(location = 0) in vec3 fragmentPosition;
layout(location = 1) in vec3 fragmentColor;
layout(location = 2) in vec2 fragmentTextureCoordinates;
layout(location = 3) in vec3 fragmentNormal;
layout(location = 4) flat in int polygonIndex;

layout(location = 0) out vec4 gPosition;
layout(location = 1) out vec4 gColor;
layout(location = 2) out vec4 gNormal;
layout(location = 3) out vec4 gExtra;

//From https://learnopengl.com/PBR/Lighting : {
vec3 getNormalFromMap() {
    vec3 tangentNormal = vec3(0.0f, 0.0f, 1.0f) * 2.0 - 1.0;;//texture(textureSamplers[7], fragmentTextureCoordinates).xyz * 2.0 - 1.0;

    //return normalize(polygonInfoBuffer.TBNs[polygonIndex] * tangentNormal);
    return polygonInfoBuffer.TBNs[polygonIndex][1];
}
//}

void main() {
    gPosition = vec4(fragmentPosition, 1.0);
    vec4 tempColor = texture(textureSamplers[6], fragmentTextureCoordinates);
    if (tempColor.w <= 0.01f)
        discard;
    gColor = vec4(tempColor.xyz, 1.0f);
    gNormal = vec4(getNormalFromMap(), texture(textureSamplers[8], fragmentTextureCoordinates).x);
    //gNormal = vec4(polygonInfoBuffer.TBNs[polygonIndex][2], 1.0f);
    gExtra = vec4(0.0f, 0.0f, 0.0f, 0.0f);
}
