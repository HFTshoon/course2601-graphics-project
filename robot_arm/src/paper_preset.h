#ifndef PAPER_PRESET_H
#define PAPER_PRESET_H

#include <string>
#include <vector>

#include <glm/glm.hpp>

struct PaperPreset {
    std::string name;
    glm::vec3 baseColor;

    std::string albedoTexturePath;
    std::string normalTexturePath;
    std::string roughnessTexturePath;

    float roughness;
    float absorbency;
    float fiberNoise;

    float strokeOpacityMultiplier;
    float stampSizeMultiplier;
    float edgeNoiseStrength;
};

std::vector<PaperPreset> createDefaultPaperPresets();

#endif
