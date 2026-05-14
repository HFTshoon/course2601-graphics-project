#include "pen_preset.h"

std::vector<PenPreset> createDefaultPenPresets()
{
    const std::string basicBrushPath = "../assets/brushes/basic_circle.png";

    std::vector<PenPreset> presets;
    PenPreset pencil;
    pencil.name = "Pencil";
    pencil.brushTexturePath = basicBrushPath;
    pencil.color = glm::vec3(0.18f, 0.18f, 0.17f);
    pencil.brushSize = 0.012f;
    pencil.opacity = 0.38f;
    pencil.stampSpacing = 0.0045f;
    presets.push_back(pencil);

    PenPreset ballpoint;
    ballpoint.name = "Ballpoint Pen";
    ballpoint.brushTexturePath = basicBrushPath;
    ballpoint.color = glm::vec3(0.01f, 0.018f, 0.05f);
    ballpoint.brushSize = 0.010f;
    ballpoint.opacity = 0.92f;
    ballpoint.stampSpacing = 0.0040f;
    presets.push_back(ballpoint);

    PenPreset marker;
    marker.name = "Marker";
    marker.brushTexturePath = basicBrushPath;
    marker.color = glm::vec3(0.0f, 0.0f, 0.0f);
    marker.brushSize = 0.040f;
    marker.opacity = 0.88f;
    marker.stampSpacing = 0.016f;
    presets.push_back(marker);

    return presets;
}
