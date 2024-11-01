#version 450

const float PI = 3.14159265359;

struct Light {
    vec3 position;
    vec3 color;
    float strength; //!= 0
};

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

layout(binding = 1) uniform sampler2D gSamplers[6];

layout(std430, binding = 2) readonly restrict buffer LightBuffer {
    int lightCount;
    Light lights[2000];
} lightBuffer;

layout(push_constant) uniform PushConstant {
    uint isDay;
} pushConstant;

layout(location = 0) out vec4 outColor;

layout(location = 1) in vec2 fragmentTextureCoordinates;

vec3 toneMap(inout vec3 color) {
    // //Exposure tone mapping
    // color = vec3(1.0) - exp(-color * ubo.exposure);
    // //Gamma correction 
    // color = pow(color, vec3(1.0 / ubo.gamma));
    return color;
}

//The following math is from https://learnopengl.com/PBR/Lighting {
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
//}

vec2 projectReflectTextureCoordinate(vec3 data) {
    vec4 clipSpacePosition = ubo.projection * ubo.view * vec4(data.xyz, 1.0f);
    vec3 ndcSpacePosition = clipSpacePosition.xyz / clipSpacePosition.w;
    return fragmentTextureCoordinates;
}

vec3 getFinalColor(int samplerStartingIndex, in vec2 textureCoordinates) {
    vec4 tempPosition = texture(gSamplers[samplerStartingIndex+0], textureCoordinates);
    vec3 position = tempPosition.xyz;
    vec4 tempColor = texture(gSamplers[samplerStartingIndex+1], textureCoordinates);
    vec3 albedo = tempColor.xyz;

    if (tempColor.w != 0.0) {
        vec3 hdrColor = albedo;
        return hdrColor;
    }

    vec4 tempNormal = texture(gSamplers[samplerStartingIndex+2], textureCoordinates);
    vec3 normal = tempNormal.xyz;
    float roughness = tempNormal.w;
    
    float ambient = 0.005f;
    vec3 hdrColor = albedo * ambient; //Hard-coded ambient component
    vec3 viewDirection = normalize(ubo.cameraPosition - position);

    vec3 V = normalize(ubo.cameraPosition-position);
    vec3 N = normal;
    float metallic = 0.0f;
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    //Lights
    for(int i = 0; i < lightBuffer.lightCount; i++) {
        vec3 L = normalize(lightBuffer.lights[i].position - position);
        vec3 H = normalize(viewDirection + L);
        float distance = length(lightBuffer.lights[i].position - position);
        float attenuation = lightBuffer.lights[i].strength / (distance * distance);
        vec3 radiance = lightBuffer.lights[i].color * attenuation;

        //The following math is from https://learnopengl.com/PBR/Lightings {
        float NDF = DistributionGGX(N, H, roughness);   
        float G   = GeometrySmith(N, V, L, roughness);      
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
           
        vec3 numerator    = NDF * G * F; 
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;	  

        float NdotL = max(dot(N, L), 0.0);        
        //}

        hdrColor += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    //Sun
    if (pushConstant.isDay != 0) {
        vec3 sunDirection = normalize(vec3(sin(ubo.time), cos(ubo.time), 1.0f));
        vec3 sunColor = vec3(1.0, 1.0, 0.8);
        float sunStrength = 1.0;

        vec3 L = normalize(sunDirection);
        vec3 H = normalize(viewDirection + L);
        float NdotL = max(dot(N, L), 0.0);
        float distance = 1.0;
        float attenuation = sunStrength / (distance * distance);
        vec3 radiance = sunColor * attenuation;

        float NDF = DistributionGGX(N, H, roughness);   
        float G   = GeometrySmith(N, V, L, roughness);      
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
            
        vec3 numerator    = NDF * G * F; 
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;
                
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;	  

        hdrColor += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    return hdrColor;
}

float magnitude(vec2 v) {
    return sqrt(v.x*v.x + v.y*v.y);
}

void main() {
    vec4 tempPosition = texture(gSamplers[0], fragmentTextureCoordinates);
    vec4 tempColor = texture(gSamplers[1], fragmentTextureCoordinates);

    if (tempColor.w != 0.0f || tempPosition.z > 5.0f) {
        vec3 finalColor = getFinalColor(0, fragmentTextureCoordinates);
        outColor = vec4(toneMap(finalColor), 1.0);
        return;
    }
    else {
        vec2 TEMP = fragmentTextureCoordinates * vec2(1.0f, -1.0f);
        vec3 finalColor = getFinalColor(3, TEMP);
        outColor = vec4(toneMap(finalColor)*vec3(0.0f, 0.8f, 1.0f), 1.0);
    }
    // if (fragmentTextureCoordinates.x < 0.5f) {
    //     vec3 finalColor = getFinalColor(0, fragmentTextureCoordinates);
    //     outColor = vec4(toneMap(finalColor), 1.0);
    // }
    // else {
    //     vec2 TEMP = fragmentTextureCoordinates * vec2(1.0f, -1.0f);
    //     vec3 finalColor = getFinalColor(3, TEMP);
    //     outColor = vec4(toneMap(finalColor)*vec3(0.0f, 0.8f, 1.0f), 1.0);
    // }
}