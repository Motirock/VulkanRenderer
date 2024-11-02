#version 450 core

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 projection;
    mat4 reflectView;
    mat4 refractView;
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
    uint isDay;
} pushConstant; 

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec2 fragmentTextureCoordinates;
layout(location = 1) out flat uint fragmentFaceIndex;

void main() {
    //Determine which face of the cube the vertex is on
    fragmentFaceIndex = gl_VertexIndex / 6;
    //Determine the texture coordinates of the vertex
    if (fragmentFaceIndex == 0) {
        fragmentTextureCoordinates = inPosition.yz*vec2(-0.5f, -0.5f)+vec2(0.5f, 0.5f);
    } else if (fragmentFaceIndex == 1) {
        fragmentTextureCoordinates = inPosition.yz*vec2(0.5f, -0.5f)+vec2(0.5f, 0.5f);
    } else if (fragmentFaceIndex == 2) {
        fragmentTextureCoordinates = inPosition.xz*vec2(0.5f, -0.5f)+vec2(0.5f, 0.5f);
    } else if (fragmentFaceIndex == 3) {
        fragmentTextureCoordinates = inPosition.xz*vec2(-0.5f, -0.5f)+vec2(0.5f, 0.5f);
    } else if (fragmentFaceIndex == 4) {
        fragmentTextureCoordinates = inPosition.yx*vec2(-0.5f, 0.5f)+vec2(0.5f, 0.5f);
    } else if (fragmentFaceIndex == 5) {
        fragmentTextureCoordinates = inPosition.yx*vec2(-0.5f, -0.5f)+vec2(0.5f, 0.5f);
    }

    //Calculate the position of the vertex in clip space
    if (pushConstant.isWater == 0) {
        gl_Position = ubo.projection * ubo.view * vec4(inPosition*ubo.farPlane/sqrt(3)+ubo.cameraPosition, 1.0);
    }
    else if (pushConstant.isReflect == 1) {
        gl_Position = ubo.projection * ubo.reflectView * vec4(inPosition*ubo.farPlane/sqrt(3)+ubo.cameraPosition, 1.0);
    }
    else {
        gl_Position = ubo.projection * ubo.refractView * vec4(inPosition*ubo.farPlane/sqrt(3)+ubo.cameraPosition, 1.0);
    }
}  