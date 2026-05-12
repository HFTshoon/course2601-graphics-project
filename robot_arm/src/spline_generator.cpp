#include "spline_generator.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

std::vector<glm::vec3> SplineGenerator::generateCatmullRomSpline(
    const std::vector<glm::vec3>& controlPoints,
    const Options& options) const
{
    if (controlPoints.size() <= 2) {
        return samplePolyline(controlPoints, options.sampleSpacing, false);
    }

    const float sampleSpacing = std::max(0.001f, options.sampleSpacing);
    const int pointCount = static_cast<int>(controlPoints.size());
    const int segmentCount = options.closed ? pointCount : pointCount - 1;

    std::vector<glm::vec3> sampledPoints;
    sampledPoints.push_back(controlPoints.front());

    for (int segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex) {
        const int p1Index = segmentIndex;
        const int p2Index = options.closed
            ? (segmentIndex + 1) % pointCount
            : segmentIndex + 1;

        const int p0Index = options.closed
            ? (segmentIndex - 1 + pointCount) % pointCount
            : std::max(0, segmentIndex - 1);
        const int p3Index = options.closed
            ? (segmentIndex + 2) % pointCount
            : std::min(pointCount - 1, segmentIndex + 2);

        const glm::vec3& p0 = controlPoints[p0Index];
        const glm::vec3& p1 = controlPoints[p1Index];
        const glm::vec3& p2 = controlPoints[p2Index];
        const glm::vec3& p3 = controlPoints[p3Index];

        const float segmentLength = glm::length(p2 - p1);
        const int sampleCount = std::max(1, static_cast<int>(std::ceil(segmentLength / sampleSpacing)));

        for (int sampleIndex = 1; sampleIndex <= sampleCount; ++sampleIndex) {
            const float t = static_cast<float>(sampleIndex) / static_cast<float>(sampleCount);
            sampledPoints.push_back(sampleCatmullRom(p0, p1, p2, p3, t));
        }
    }

    return sampledPoints;
}

glm::vec3 SplineGenerator::sampleCatmullRom(
    const glm::vec3& p0,
    const glm::vec3& p1,
    const glm::vec3& p2,
    const glm::vec3& p3,
    float t)
{
    const float t2 = t * t;
    const float t3 = t2 * t;

    return 0.5f * (
        (2.0f * p1)
        + (-p0 + p2) * t
        + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
        + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
    );
}

std::vector<glm::vec3> SplineGenerator::samplePolyline(
    const std::vector<glm::vec3>& controlPoints,
    float sampleSpacing,
    bool closed)
{
    std::vector<glm::vec3> sampledPoints;
    if (controlPoints.empty()) {
        return sampledPoints;
    }

    sampledPoints.push_back(controlPoints.front());
    if (controlPoints.size() == 1) {
        return sampledPoints;
    }

    const float spacing = std::max(0.001f, sampleSpacing);
    const int pointCount = static_cast<int>(controlPoints.size());
    const int segmentCount = closed ? pointCount : pointCount - 1;

    for (int segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex) {
        const glm::vec3& start = controlPoints[segmentIndex];
        const glm::vec3& end = controlPoints[(segmentIndex + 1) % pointCount];
        const float segmentLength = glm::length(end - start);
        const int sampleCount = std::max(1, static_cast<int>(std::ceil(segmentLength / spacing)));

        for (int sampleIndex = 1; sampleIndex <= sampleCount; ++sampleIndex) {
            const float t = static_cast<float>(sampleIndex) / static_cast<float>(sampleCount);
            sampledPoints.push_back(start * (1.0f - t) + end * t);
        }
    }

    return sampledPoints;
}
