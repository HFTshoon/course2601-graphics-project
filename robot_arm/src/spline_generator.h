#ifndef SPLINE_GENERATOR_H
#define SPLINE_GENERATOR_H

#include <vector>

#include <glm/glm.hpp>

class SplineGenerator {
public:
    struct Options {
        float sampleSpacing;
        bool closed;

        Options()
            : sampleSpacing(0.01f),
              closed(false)
        {
        }
    };

    std::vector<glm::vec3> generateCatmullRomSpline(
        const std::vector<glm::vec3>& controlPoints,
        const Options& options
    ) const;

private:
    static glm::vec3 sampleCatmullRom(
        const glm::vec3& p0,
        const glm::vec3& p1,
        const glm::vec3& p2,
        const glm::vec3& p3,
        float t
    );

    static std::vector<glm::vec3> samplePolyline(
        const std::vector<glm::vec3>& controlPoints,
        float sampleSpacing,
        bool closed
    );
};

#endif
