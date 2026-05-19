#ifndef PEN_PRESET_H
#define PEN_PRESET_H

#include <string>
#include <vector>

#include <glm/glm.hpp>

struct PenPreset {
    std::string name;
    std::string brushTexturePath;
    glm::vec3 color;
    float brushSize;
    float opacity;
    float stampSpacing;
};

std::vector<PenPreset> createDefaultPenPresets();

#endif
