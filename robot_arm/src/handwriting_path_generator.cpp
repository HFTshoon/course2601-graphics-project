#include "handwriting_path_generator.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include "spline_generator.h"

std::vector<Waypoint> HandwritingPathGenerator::generateLowercaseA(const Options& options) const
{
    std::vector<Waypoint> waypoints;

    std::vector<glm::vec2> loopStroke;
    const glm::vec2 loopCenter(-0.12f, -0.05f);
    const float radiusX = 0.45f;
    const float radiusY = 0.55f;
    const int loopControlCount = 12;
    for (int i = 0; i < loopControlCount; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(loopControlCount);
        const float angle = glm::two_pi<float>() * t;
        loopStroke.push_back(glm::vec2(
            loopCenter.x + radiusX * std::cos(angle),
            loopCenter.y + radiusY * std::sin(angle)
        ));
    }
    appendStroke(loopStroke, options, waypoints, true);

    std::vector<glm::vec2> stemStroke;
    stemStroke.push_back(glm::vec2(0.34f, 0.50f));
    stemStroke.push_back(glm::vec2(0.42f, 0.18f));
    stemStroke.push_back(glm::vec2(0.39f, -0.34f));
    stemStroke.push_back(glm::vec2(0.44f, -0.58f));
    stemStroke.push_back(glm::vec2(0.62f, -0.68f));
    appendStroke(stemStroke, options, waypoints, false);

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
    std::vector<Waypoint>& waypoints,
    bool closed)
{
    if (localPoints.empty()) {
        return;
    }

    const float sampleSpacing = std::max(0.001f, options.sampleSpacing);

    std::vector<glm::vec3> drawControlPoints;
    drawControlPoints.reserve(localPoints.size());
    for (size_t i = 0; i < localPoints.size(); ++i) {
        drawControlPoints.push_back(localToWorld(localPoints[i], options, options.zDraw));
    }

    std::vector<glm::vec3> drawPoints;
    if (options.useSpline) {
        SplineGenerator splineGenerator;
        SplineGenerator::Options splineOptions;
        splineOptions.sampleSpacing = sampleSpacing;
        splineOptions.closed = closed;
        drawPoints = splineGenerator.generateCatmullRomSpline(drawControlPoints, splineOptions);
    } else {
        drawPoints = samplePolyline(drawControlPoints, sampleSpacing, closed);
    }

    if (drawPoints.empty()) {
        return;
    }

    const glm::vec3 firstDraw = drawPoints.front();
    const glm::vec3 firstTravel(firstDraw.x, options.origin.y + options.zTravel, firstDraw.z);
    if (!waypoints.empty()) {
        const glm::vec3 lastPosition = waypoints.back().position;
        waypoints.push_back(Waypoint(
            glm::vec3(lastPosition.x, options.origin.y + options.zTravel, lastPosition.z),
            false
        ));
    }
    waypoints.push_back(Waypoint(firstTravel, false));
    waypoints.push_back(Waypoint(firstDraw, true));

    for (size_t i = 1; i < drawPoints.size(); ++i) {
        waypoints.push_back(Waypoint(drawPoints[i], true));
    }
}

std::vector<glm::vec3> HandwritingPathGenerator::samplePolyline(
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
