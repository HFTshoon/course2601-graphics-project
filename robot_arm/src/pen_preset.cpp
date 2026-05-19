#include "pen_preset.h"

std::vector<PenPreset> createDefaultPenPresets()
{
    const std::string pencilBrushPath = "../assets/brushes/chalk.png";
    const std::string ballpointBrushPath = "../assets/brushes/round.png";
    const std::string markerBrushPath = "../assets/brushes/blob.png";

    std::vector<PenPreset> presets;
    PenPreset pencil;
    pencil.name = "Pencil";
    pencil.brushTexturePath = pencilBrushPath;
    pencil.color = glm::vec3(0.18f, 0.18f, 0.17f);
    pencil.brushSize = 0.014f;
    pencil.opacity = 0.42f;
    pencil.stampSpacing = 0.0040f;
    presets.push_back(pencil);

    PenPreset ballpoint;
    ballpoint.name = "Ballpoint Pen";
    ballpoint.brushTexturePath = ballpointBrushPath;
    ballpoint.color = glm::vec3(0.01f, 0.018f, 0.05f);
    ballpoint.brushSize = 0.009f;
    ballpoint.opacity = 0.95f;
    ballpoint.stampSpacing = 0.0035f;
    presets.push_back(ballpoint);

    PenPreset marker;
    marker.name = "Marker";
    marker.brushTexturePath = markerBrushPath;
    marker.color = glm::vec3(0.0f, 0.0f, 0.0f);
    marker.brushSize = 0.045f;
    marker.opacity = 0.90f;
    marker.stampSpacing = 0.017f;
    presets.push_back(marker);

    return presets;
}
