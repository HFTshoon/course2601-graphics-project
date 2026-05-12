#include "handwriting_path_generator.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

std::vector<Waypoint> HandwritingPathGenerator::generateLowercaseA(const Options& options) const
{
    std::vector<Waypoint> waypoints;

    std::vector<glm::vec2> loopStroke;
    const glm::vec2 loopCenter(-0.12f, -0.05f);
    const float radiusX = 0.45f;
    const float radiusY = 0.55f;
    const int loopSegmentCount = 48;
    for (int i = 0; i <= loopSegmentCount; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(loopSegmentCount);
        const float angle = glm::two_pi<float>() * t;
        loopStroke.push_back(glm::vec2(
            loopCenter.x + radiusX * std::cos(angle),
            loopCenter.y + radiusY * std::sin(angle)
        ));
    }
    appendStroke(loopStroke, options, waypoints);

    std::vector<glm::vec2> stemStroke;
    stemStroke.push_back(glm::vec2(0.34f, 0.48f));
    stemStroke.push_back(glm::vec2(0.40f, 0.12f));
    stemStroke.push_back(glm::vec2(0.38f, -0.50f));
    stemStroke.push_back(glm::vec2(0.58f, -0.64f));
    appendStroke(stemStroke, options, waypoints);

    if (!waypoints.empty()) {
        const glm::vec3 lastPosition = waypoints.back().position;
        waypoints.push_back(Waypoint(
            glm::vec3(lastPosition.x, options.origin.y + options.zTravel, lastPosition.z),
            false
        ));
    }

    return waypoints;
}

glm::vec3 HandwritingPathGenerator::localToWorld(
    const glm::vec2& localPoint,
    const Options& options,
    float heightOffset)
{
    return glm::vec3(
        options.origin.x + options.scale * localPoint.x,
        options.origin.y + heightOffset,
        options.origin.z + options.scale * localPoint.y
    );
}

void HandwritingPathGenerator::appendStroke(
    const std::vector<glm::vec2>& localPoints,
    const Options& options,
    std::vector<Waypoint>& waypoints)
{
    if (localPoints.empty()) {
        return;
    }

    const float sampleSpacing = std::max(0.001f, options.sampleSpacing);
    const glm::vec3 firstTravel = localToWorld(localPoints.front(), options, options.zTravel);
    const glm::vec3 firstDraw = localToWorld(localPoints.front(), options, options.zDraw);
    if (!waypoints.empty()) {
        const glm::vec3 lastPosition = waypoints.back().position;
        waypoints.push_back(Waypoint(
            glm::vec3(lastPosition.x, options.origin.y + options.zTravel, lastPosition.z),
            false
        ));
    }
    waypoints.push_back(Waypoint(firstTravel, false));
    waypoints.push_back(Waypoint(firstDraw, true));

    glm::vec3 previousDrawPoint = firstDraw;
    for (size_t i = 1; i < localPoints.size(); ++i) {
        const glm::vec3 segmentEnd = localToWorld(localPoints[i], options, options.zDraw);
        const float segmentLength = glm::length(segmentEnd - previousDrawPoint);
        const int sampleCount = std::max(1, static_cast<int>(std::ceil(segmentLength / sampleSpacing)));

        for (int sampleIndex = 1; sampleIndex <= sampleCount; ++sampleIndex) {
            const float t = static_cast<float>(sampleIndex) / static_cast<float>(sampleCount);
            const glm::vec3 sample = previousDrawPoint * (1.0f - t) + segmentEnd * t;
            waypoints.push_back(Waypoint(sample, true));
        }

        previousDrawPoint = segmentEnd;
    }
}
