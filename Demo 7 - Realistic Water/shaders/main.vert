#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 projection;
    vec3 cameraPosition;
    vec3 viewDirection;
    float nearPlane;
    float farPlane;
    float time;
    float gamma;
    float exposure;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTextureCoordinates;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec3 fragmentPosition;
layout(location = 1) out vec2 fragmentTextureCoordinates;
layout(location = 2) out vec3 fragmentNormal;
layout(location = 3) flat out int polygonIndex;

void main() {
    gl_Position = ubo.projection * ubo.view * vec4(inPosition, 1.0);
    fragmentPosition = inPosition;
    fragmentTextureCoordinates = inTextureCoordinates;
    fragmentNormal = inNormal;
    polygonIndex = gl_VertexIndex/3;
}