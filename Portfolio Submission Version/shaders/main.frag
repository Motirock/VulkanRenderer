#version 450

layout(binding = 1) uniform sampler2D textureSamplers[10];

struct TBN {
    vec3 T;
    vec3 B;
    vec3 N;
};

layout(binding = 2) readonly buffer PolygonInfoBuffer {
    TBN TBNs[30000];
} polygonInfoBuffer;

layout(location = 0) in vec3 fragmentPosition;
layout(location = 1) in vec2 fragmentTextureCoordinates;
layout(location = 2) in vec3 fragmentNormal;
layout(location = 3) flat in int polygonIndex;

layout(location = 0) out vec4 gPosition;
layout(location = 1) out vec4 gColor;
layout(location = 2) out vec4 gNormal;
layout(location = 3) out vec4 gBloom;

vec3 getNormalFromMap() {
    vec3 tangentNormal = normalize(texture(textureSamplers[7], fragmentTextureCoordinates).xyz);//texture(textureSamplers[7], fragmentTextureCoordinates).xyz * 2.0 - 1.0;

    vec3 T = normalize(polygonInfoBuffer.TBNs[polygonIndex].T);
    vec3 B = normalize(polygonInfoBuffer.TBNs[polygonIndex].B);
    vec3 N = normalize(polygonInfoBuffer.TBNs[polygonIndex].N);

    return normalize(vec3(  T.x*tangentNormal.x+B.x*tangentNormal.y+N.x*tangentNormal.z,
                            T.y*tangentNormal.x+B.y*tangentNormal.y+N.y*tangentNormal.z,
                            T.z*tangentNormal.x+B.z*tangentNormal.y+N.z*tangentNormal.z));
}


void main() {
    gPosition = vec4(fragmentPosition, 0.0f);
    vec4 tempColor = texture(textureSamplers[6], fragmentTextureCoordinates);
    if (tempColor.w <= 0.01f)
        discard;
    gColor = vec4(tempColor.xyz, 0.0f);
    gNormal = vec4(getNormalFromMap(), texture(textureSamplers[8], fragmentTextureCoordinates).x);
    gBloom = vec4(0.0f, 0.0f, 0.0f, 0.0f);
}
