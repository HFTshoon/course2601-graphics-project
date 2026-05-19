#version 330 core

out vec4 FragColor;

in vec3 WorldPosition;

uniform sampler2D albedoMap;
uniform sampler2D normalMap;
uniform sampler2D roughnessMap;

uniform vec2 paperOriginXZ;
uniform vec2 paperSize;
uniform float paperUvScale;
uniform bool usePaperMapShading;
uniform bool flipNormalY;
uniform float ambientStrength;
uniform float specularStrength;
uniform float scalarRoughness;
uniform vec3 viewPos;
uniform vec3 lightDir;
uniform vec3 lightColor;

vec2 computePaperUV(vec3 worldPosition)
{
    vec2 local = (worldPosition.xz - paperOriginXZ) / paperSize + vec2(0.5);
    return local * paperUvScale;
}

void main()
{
    vec2 uv = computePaperUV(WorldPosition);
    vec3 albedo = texture(albedoMap, uv).rgb;

    vec3 worldNormal = vec3(0.0, 1.0, 0.0);
    float roughness = scalarRoughness;

    if (usePaperMapShading) {
        vec3 tangentNormal = texture(normalMap, uv).rgb * 2.0 - 1.0;
        if (flipNormalY) {
            tangentNormal.g = -tangentNormal.g;
        }

        vec3 T = vec3(1.0, 0.0, 0.0);
        vec3 B = vec3(0.0, 0.0, 1.0);
        vec3 N = vec3(0.0, 1.0, 0.0);
        mat3 TBN = mat3(T, B, N);
        worldNormal = normalize(TBN * tangentNormal);
        roughness = clamp(texture(roughnessMap, uv).r, 0.02, 1.0);
    }

    vec3 L = normalize(-lightDir);
    vec3 V = normalize(viewPos - WorldPosition);
    vec3 H = normalize(L + V);

    float diff = max(dot(worldNormal, L), 0.0);
    float shininess = mix(128.0, 8.0, roughness);
    float spec = pow(max(dot(worldNormal, H), 0.0), shininess) * (1.0 - roughness);

    vec3 ambient = albedo * lightColor * ambientStrength;
    vec3 diffuse = albedo * lightColor * diff;
    vec3 specular = lightColor * specularStrength * spec;

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
