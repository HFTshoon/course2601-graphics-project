#include "paper_preset.h"

std::vector<PaperPreset> createDefaultPaperPresets()
{
    std::vector<PaperPreset> presets;

    PaperPreset smooth;
    smooth.name = "Smooth Paper";
    smooth.baseColor = glm::vec3(0.96f, 0.955f, 0.93f);
    smooth.albedoTexturePath = "../assets/papers/smooth/albedo.png";
    smooth.normalTexturePath = "../assets/papers/smooth/normal.png";
    smooth.roughnessTexturePath = "../assets/papers/smooth/roughness.png";
    smooth.roughness = 0.22f;
    smooth.absorbency = 0.18f;
    smooth.fiberNoise = 0.04f;
    smooth.strokeOpacityMultiplier = 1.0f;
    smooth.stampSizeMultiplier = 1.0f;
    smooth.edgeNoiseStrength = 0.04f;
    presets.push_back(smooth);

    PaperPreset rough;
    rough.name = "Rough Paper";
    rough.baseColor = glm::vec3(0.83f, 0.83f, 0.80f);
    rough.albedoTexturePath = "../assets/papers/rough/albedo.png";
    rough.normalTexturePath = "../assets/papers/rough/normal.png";
    rough.roughnessTexturePath = "../assets/papers/rough/roughness.png";
    rough.roughness = 0.82f;
    rough.absorbency = 0.68f;
    rough.fiberNoise = 0.78f;
    rough.strokeOpacityMultiplier = 0.82f;
    rough.stampSizeMultiplier = 1.12f;
    rough.edgeNoiseStrength = 0.55f;
    presets.push_back(rough);

    PaperPreset recycled;
    recycled.name = "Recycled Paper";
    recycled.baseColor = glm::vec3(0.84f, 0.76f, 0.56f);
    recycled.albedoTexturePath = "../assets/papers/recycled/albedo.png";
    recycled.normalTexturePath = "../assets/papers/recycled/normal.png";
    recycled.roughnessTexturePath = "../assets/papers/recycled/roughness.png";
    recycled.roughness = 0.55f;
    recycled.absorbency = 0.52f;
    recycled.fiberNoise = 0.46f;
    recycled.strokeOpacityMultiplier = 0.90f;
    recycled.stampSizeMultiplier = 1.05f;
    recycled.edgeNoiseStrength = 0.28f;
    presets.push_back(recycled);

    return presets;
}
