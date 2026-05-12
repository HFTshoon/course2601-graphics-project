#ifndef HANDWRITING_PATH_GENERATOR_H
#define HANDWRITING_PATH_GENERATOR_H

#include <vector>

#include <glm/glm.hpp>

#include "waypoint.h"

class HandwritingPathGenerator {
public:
    struct Options {
        glm::vec3 paperOrigin;
        float scale;
        float paperY;
        float liftHeight;
        float sampleSpacing;
        bool useSpline;

        Options()
            : paperOrigin(0.0f),
              scale(0.15f),
              paperY(0.0f),
              liftHeight(0.05f),
              sampleSpacing(0.01f),
              useSpline(true)
        {
        }
    };

    std::vector<Waypoint> generateLowercaseA(const Options& options) const;

private:
    static glm::vec3 localToWorld(const glm::vec2& localPoint, const Options& options, float worldY);
    static void appendStroke(
        const std::vector<glm::vec2>& localPoints,
        const Options& options,
        std::vector<Waypoint>& waypoints,
        bool closed
    );
    static std::vector<glm::vec3> samplePolyline(
        const std::vector<glm::vec3>& controlPoints,
        float sampleSpacing,
        bool closed
    );
};

#endif
