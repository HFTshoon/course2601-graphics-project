#version 330 core
out vec4 FragColor;

struct Material {
    sampler2D diffuseSampler;
    sampler2D specularSampler;
    sampler2D normalSampler;
    float shininess;
}; 

struct Light {
    vec3 dir;
    vec3 color; // this is I_d (I_s = I_d, I_a = 0.3 * I_d)
};

uniform vec3 viewPos;
uniform Material material;
uniform Light light;
uniform mat4 view;
uniform mat4 lightSpaceMatrices[3];
uniform float cascadePlaneDistances[3];
uniform sampler2D depthMapSampler0;
uniform sampler2D depthMapSampler1;
uniform sampler2D depthMapSampler2;

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;
in mat3 TBN;

uniform float useNormalMap;
uniform float useSpecularMap;
uniform float useShadow;
uniform float useLighting;

float sampleShadowPCF(sampler2D depthMap, vec4 fragPosLightSpace, float currentDepth, float bias, float radiusScale)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z <= 0.0 || projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 0.0;
    }

    vec2 poissonDisk[16] = vec2[](
        vec2(-0.94201624, -0.39906216),
        vec2(0.94558609, -0.76890725),
        vec2(-0.094184101, -0.92938870),
        vec2(0.34495938, 0.29387760),
        vec2(-0.91588581, 0.45771432),
        vec2(-0.81544232, -0.87912464),
        vec2(-0.38277543, 0.27676845),
        vec2(0.97484398, 0.75648379),
        vec2(0.44323325, -0.97511554),
        vec2(0.53742981, -0.47373420),
        vec2(-0.26496911, -0.41893023),
        vec2(0.79197514, 0.19090188),
        vec2(-0.24188840, 0.99706507),
        vec2(-0.81409955, 0.91437590),
        vec2(0.19984126, 0.78641367),
        vec2(0.14383161, -0.14100790)
    );

    vec2 texelSize = 1.0 / textureSize(depthMap, 0);
    float shadowSum = 0.0;
    float random = fract(sin(dot(TexCoord.xy, vec2(68.60, 29.13))) * 4849.27446216);

    for (int i = 0; i < 16; ++i) {
        vec2 offset = poissonDisk[i] * texelSize * radiusScale;
        offset += (random - 0.5) * texelSize;
        float sampleDepth = texture(depthMap, projCoords.xy + offset).r;
        shadowSum += currentDepth - bias > sampleDepth ? 1.0 : 0.0;
    }

    return shadowSum / 16.0;
}

void main()
{
	vec3 color = texture(material.diffuseSampler, TexCoord).rgb;

    // on-off by key 3 (useLighting). 
    // if useLighting is 0, return diffuse value without considering any lighting.(DO NOT CHANGE)
	if (useLighting < 0.5f){
        FragColor = vec4(color, 1.0); 
        return; 
    }

    
    /////////////////////
    // ambient
    vec3 ambient = 0.3f * light.color * color;

    /////////////////////
    // diffuse
    vec3 norm;
	if(useNormalMap > 0.5f)
	{
        vec3 tangentNormal = texture(material.normalSampler, TexCoord).rgb * 2.0 - 1.0;
        norm = normalize(TBN * tangentNormal);
	}
    else
    {
        norm = normalize(Normal);
    }

	vec3 lightDir = normalize(-light.dir);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = light.color * color * diff;

    /////////////////////
    // shadow
    // on-off by key 2 (useShadow).
    // calculate shadow
    // if useShadow is 0, do not consider shadow.
    // if useShadow is 1, consider shadow.
    float shadow = 0.0;
    if(useShadow > 0.5f)
    {
        float viewDepth = abs((view * vec4(FragPos, 1.0)).z);
        int cascadeIndex = 0;
        if (viewDepth > cascadePlaneDistances[0]) {
            cascadeIndex = 1;
        }
        if (viewDepth > cascadePlaneDistances[1]) {
            cascadeIndex = 2;
        }

        vec4 fragPosLightSpace = lightSpaceMatrices[cascadeIndex] * vec4(FragPos, 1.0);
        vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
        float currentDepth = projCoords.z * 0.5 + 0.5;
        float ndotl = max(dot(norm, lightDir), 0.0);
        float baseBias = cascadeIndex == 0 ? 0.00003 : (cascadeIndex == 1 ? 0.00006 : 0.00012);
        float bias = max(baseBias, baseBias * (1.0 - ndotl) * 4.0);
        float radiusScale = cascadeIndex == 0 ? 1.5 : (cascadeIndex == 1 ? 2.0 : 2.5);

        if (cascadeIndex == 0) {
            shadow = sampleShadowPCF(depthMapSampler0, fragPosLightSpace, currentDepth, bias, radiusScale);
        } else if (cascadeIndex == 1) {
            shadow = sampleShadowPCF(depthMapSampler1, fragPosLightSpace, currentDepth, bias, radiusScale);
        } else {
            shadow = sampleShadowPCF(depthMapSampler2, fragPosLightSpace, currentDepth, bias, radiusScale);
        }
    }

    /////////////////////
    // specular
    vec3 specular = vec3(0.0f);
	if(useSpecularMap > 0.5f && diff > 0.0)
	{
        //use only red channel of specularSampler as a reflectance coefficient(k_s).
        float k_s = texture(material.specularSampler, TexCoord).r;
        vec3 viewDir = normalize(viewPos - FragPos);
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
        specular = light.color * k_s * spec;
	}

    // vec3 result = ambient + diffuse + specular;
    // result *= (1.0 - shadow);
    vec3 result = ambient + (1.0 - shadow) * (diffuse + specular);
    FragColor = vec4(result, 1.0);
}
