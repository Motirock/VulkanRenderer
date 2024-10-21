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
    float gamma;
    float exposure;
} ubo;

layout(binding = 1) uniform sampler2D gSamplers[4];

layout(std430, binding = 2) readonly restrict buffer LightBuffer {
    int lightCount;
    Light lights[100];
} lightBuffer;

layout(location = 0) out vec4 outColor;

layout(location = 1) in vec2 fragmentTextureCoordinates;

vec3 toneMap(inout vec3 color) {
    // //Exposure tone mapping
    // color = vec3(1.0) - exp(-color * ubo.exposure);
    // //Gamma correction 
    // color = pow(color, vec3(1.0 / ubo.gamma));
    return color;
}

float magnitude(in vec3 v) {
    return sqrt(dot(v, v));
}

void main() {
    outColor = texture(gSamplers[3], fragmentTextureCoordinates);
    return;

    vec4 tempPosition = texture(gSamplers[0], fragmentTextureCoordinates);
    vec3 position = tempPosition.xyz;
    vec3 albedo = texture(gSamplers[1], fragmentTextureCoordinates).xyz;

    if (tempPosition.w == 0.0) {
        vec3 hdrColor = albedo + texture(gSamplers[3], fragmentTextureCoordinates).xyz;
        outColor = vec4(toneMap(hdrColor), 1.0);
        return;
    }

    vec4 tempNormal = texture(gSamplers[2], fragmentTextureCoordinates);
    vec3 normal = tempNormal.xyz;
    float specular = tempNormal.w;
    
    float ambient = 0.001f;
    vec3 hdrColor = albedo * ambient; //Hard-coded ambient component
    vec3 viewDirection = normalize(ubo.cameraPosition - position);

    for(int i = 0; i < lightBuffer.lightCount; i++) {
        vec3 difference = lightBuffer.lights[i].position - position;
        float attenuation = lightBuffer.lights[i].strength/(difference.x*difference.x+difference.y*difference.y+difference.z*difference.z+1.0f);
        //if (attenuation < 0.01f) continue; //Useless. Fuck gpus

        //Diffuse
        vec3 lightDir = normalize(difference);
        vec3 diffuse = max(dot(normal, lightDir), 0.0) * albedo * lightBuffer.lights[i].color;

        //Specular
        vec3 reflected = reflect(-lightDir, normal);
        vec3 spec = lightBuffer.lights[i].color * specular * pow(max(0.0, dot(reflected, viewDirection)), 16.0);

        hdrColor += attenuation*(diffuse+spec)*(1.0f-ambient);
    }

    hdrColor = texture(gSamplers[3], fragmentTextureCoordinates).xyz;
  
    outColor = vec4(toneMap(hdrColor), 1.0);
}