#version 450

struct Light {
    vec3 position;
    vec3 color;
    float strength; //!= 0
};

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 projection;
    vec3 cameraPosition;
    vec3 viewDirection;
    float nearPlane;
    float farPlane;
    float time;
} ubo;

layout(binding = 1) uniform sampler2D gSamplers[3];

layout(std430, binding = 2) readonly restrict buffer LightBuffer {
    int lightCount;
    Light lights[100];
} lightBuffer;

layout(location = 0) out vec4 outColor;

layout(location = 1) in vec2 fragmentTextureCoordinates;

void main() {
    vec4 tempPosition = texture(gSamplers[0], fragmentTextureCoordinates);
    vec3 position = tempPosition.xyz;
    vec3 albedo = texture(gSamplers[1], fragmentTextureCoordinates).xyz;

    if (tempPosition.w == 0.0) {
        outColor = vec4(albedo, 1.0);
        return;
    }

    vec3 normal = texture(gSamplers[2], fragmentTextureCoordinates).xyz;
    //float Specular = texture(gAlbedoSpec, TexCoords).x;
    
    float ambient = 0.0001f;
    vec3 lighting = albedo * ambient; // hard-coded ambient component
    vec3 viewDirection = normalize(ubo.cameraPosition - position);

    for(int i = 0; i < lightBuffer.lightCount; i++) {
        vec3 lightPosition = lightBuffer.lights[i].position;
        vec3 difference = lightPosition - position;
        float distanceSquared = difference.x*difference.x+difference.y*difference.y+difference.z*difference.z;
        float strength = lightBuffer.lights[i].strength;

        //Diffuse
        vec3 lightDir = normalize(difference);
        vec3 diffuse = max(dot(normal, lightDir), 0.0) * albedo * lightBuffer.lights[i].color;
        lighting += diffuse/max(distanceSquared/(strength*strength), 0.1f)*(1.0f-ambient);
    }
    
    outColor = vec4(lighting, 1.0);
    // outColor = vec4(normal, 1.0);
}