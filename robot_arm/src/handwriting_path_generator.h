#ifndef HANDWRITING_PATH_GENERATOR_H
#define HANDWRITING_PATH_GENERATOR_H

#include <vector>

#include <glm/glm.hpp>

#include "waypoint.h"

class HandwritingPathGenerator {
public:
    struct Options {
        glm::vec3 origin;
        float scale;
        float zDraw;
        float zTravel;
        float sampleSpacing;

        Options()
            : origin(0.0f),
              scale(0.15f),
              zDraw(0.0f),
              zTravel(0.05f),
              sampleSpacing(0.01f)
        {
        }
    };

    std::vector<Waypoint> generateLowercaseA(const Options& options) const;

private:
    static glm::vec3 localToWorld(const glm::vec2& localPoint, const Options& options, float heightOffset);
    static void appendStroke(
        const std::vector<glm::vec2>& localPoints,
        const Options& options,
        std::vector<Waypoint>& waypoints
    );
};

#endif
