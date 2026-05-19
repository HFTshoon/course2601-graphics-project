#version 330 core

out vec4 FragColor;

in vec2 TexCoord;
in vec3 WorldPosition;

uniform sampler2D brushTexture;
uniform sampler2D paperNormalMap;
uniform sampler2D paperRoughnessMap;
uniform float opacity;
uniform vec3 strokeColor;
uniform float edgeNoiseStrength;
uniform float fiberNoise;
uniform vec2 paperOriginXZ;
uniform vec2 paperSize;
uniform float paperUvScale;
uniform bool enablePaperMapModulation;
uniform bool flipNormalY;
uniform float roughnessInfluence;
uniform float normalInfluence;
uniform float paperNoiseScale;

float hash21(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float valueNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

vec2 computePaperUV(vec3 worldPosition)
{
    vec2 local = (worldPosition.xz - paperOriginXZ) / paperSize + vec2(0.5);
    return local * paperUvScale;
}

void main()
{
    vec4 tex = texture(brushTexture, TexCoord);
    float edgeMask = 1.0 - smoothstep(0.22, 0.86, tex.a);
    float edgeNoise = valueNoise(WorldPosition.xz * 95.0);
    float fiber = valueNoise(vec2(WorldPosition.x * 28.0, WorldPosition.z * 170.0));
    float edgeVariation = mix(1.0, 0.52 + 0.74 * edgeNoise, edgeNoiseStrength * edgeMask);
    float fiberVariation = mix(1.0, 0.78 + 0.32 * fiber, fiberNoise);
    float paperVariation = 1.0;
    if (enablePaperMapModulation) {
        vec2 paperUv = computePaperUV(WorldPosition);
        float roughness = texture(paperRoughnessMap, paperUv).r;
        float roughNoise = valueNoise(paperUv * paperNoiseScale);
        float roughVariation = mix(1.0, 0.48 + 0.92 * roughNoise, roughnessInfluence * roughness);

        vec3 paperNormal = texture(paperNormalMap, paperUv).rgb * 2.0 - 1.0;
        if (flipNormalY) {
            paperNormal.g = -paperNormal.g;
        }
        float slope = clamp(length(paperNormal.xy), 0.0, 1.0);
        float normalDropout = clamp(1.0 - normalInfluence * slope, 0.0, 1.0);
        paperVariation = roughVariation * normalDropout;
    }

    float alpha = tex.a * opacity * edgeVariation * fiberVariation * paperVariation;
    if (alpha < 0.01) {
        discard;
    }
    FragColor = vec4(strokeColor, alpha);
}
