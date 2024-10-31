#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 projection;
    mat4 reflectView;
    mat4 reflectProjection;
    mat4 refractView;
    mat4 refractProjection;
    vec3 cameraPosition;
    vec3 viewDirection;
    float nearPlane;
    float farPlane;
    float time;
    float gamma;
    float exposure;
} ubo;

layout(push_constant) uniform PushConstant {
    uint isWater;
    uint isReflect;
} pushConstant;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTextureCoordinates;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec3 fragmentPosition;
layout(location = 1) out vec2 fragmentTextureCoordinates;
layout(location = 2) out vec3 fragmentNormal;
layout(location = 3) flat out int polygonIndex;

void main() {
    if (pushConstant.isWater == 0) {
        gl_Position = ubo.projection * ubo.view * vec4(inPosition, 1.0);
    }
    else if (pushConstant.isReflect == 1) {
        gl_Position = ubo.reflectProjection * ubo.reflectView * vec4(inPosition, 1.0);    
        gl_ClipDistance[0] = fragmentPosition.z-5.0f;
    }
    else {
        gl_Position = ubo.refractProjection * ubo.refractView * vec4(inPosition, 1.0);
        gl_ClipDistance[0] = -fragmentPosition.z+5.0f;
    }
    fragmentPosition = inPosition;
    fragmentTextureCoordinates = inTextureCoordinates;
    fragmentNormal = inNormal;
    polygonIndex = gl_VertexIndex/3;
}
