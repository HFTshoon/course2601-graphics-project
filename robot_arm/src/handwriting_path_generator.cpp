#include "handwriting_path_generator.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include "spline_generator.h"

void HandwritingPathGenerator::Options::setTextYawDegrees(float degrees)
{
    textYawDegrees = degrees;
    const float theta = degrees * glm::pi<float>() / 180.0f;
    paperRight = glm::normalize(glm::vec3(std::cos(theta), 0.0f, std::sin(theta)));
    paperUp = glm::normalize(glm::vec3(-std::sin(theta), 0.0f, std::cos(theta)));
    paperNormal = glm::vec3(0.0f, 1.0f, 0.0f);
}

glm::vec3 HandwritingPathGenerator::Options::toWorld(
    float localX,
    float localY,
    float heightAbovePaper) const
{
    glm::vec3 surfaceOrigin = paperOrigin;
    surfaceOrigin.y = paperY;
    return surfaceOrigin
        + scale * localX * paperRight
        + scale * localY * paperUp
        + heightAbovePaper * paperNormal;
}

glm::vec3 HandwritingPathGenerator::Options::waypointPosition(
    float localX,
    float localY,
    bool penDown) const
{
    return toWorld(localX, localY, penDown ? 0.0f : liftHeight);
}

std::vector<Waypoint> HandwritingPathGenerator::generateLowercaseA(const Options& options) const
{
    std::vector<Waypoint> waypoints;
    GlyphDefinition glyphDefinition;
    if (buildGlyphDefinition('a', glyphDefinition)) {
        appendGlyph(glyphDefinition, options, waypoints);
    }

    if (!waypoints.empty()) {
        const glm::vec3 lastPosition = waypoints.back().position;
        waypoints.push_back(Waypoint(
            glm::vec3(lastPosition.x, options.paperY + options.liftHeight, lastPosition.z),
            false
        ));
    }

    return waypoints;
}

std::vector<Waypoint> HandwritingPathGenerator::generateText(
    const std::string& text,
    const Options& options) const
{
    std::vector<Waypoint> waypoints;
    lastUnsupportedCharacterCount_ = 0;

    float cursorX = 0.0f;
    for (size_t characterIndex = 0; characterIndex < text.size(); ++characterIndex) {
        const unsigned char rawCharacter = static_cast<unsigned char>(text[characterIndex]);
        const char character = static_cast<char>(std::tolower(rawCharacter));

        if (character == ' ') {
            cursorX += options.wordSpacing;
            continue;
        }

        GlyphDefinition glyphDefinition;
        if (!buildGlyphDefinition(character, glyphDefinition)) {
            ++lastUnsupportedCharacterCount_;
            continue;
        }

        Options glyphOptions = options;
        glyphOptions.paperOrigin = options.paperOrigin + cursorX * options.paperRight;
        appendGlyph(glyphDefinition, glyphOptions, waypoints);
        cursorX += options.scale * glyphDefinition.advance + options.characterSpacing;
    }

    if (!waypoints.empty()) {
        const glm::vec3 lastPosition = waypoints.back().position;
        waypoints.push_back(Waypoint(
            glm::vec3(lastPosition.x, options.paperY + options.liftHeight, lastPosition.z),
            false
        ));
    }

    return waypoints;
}

int HandwritingPathGenerator::getLastUnsupportedCharacterCount() const
{
    return lastUnsupportedCharacterCount_;
}

bool HandwritingPathGenerator::buildGlyphDefinition(
    char character,
    GlyphDefinition& glyphDefinition)
{
    glyphDefinition.strokes.clear();
    glyphDefinition.advance = 1.15f;

    if (character == 'a') {
        GlyphStroke loopStroke;
        loopStroke.closed = true;
        const glm::vec2 loopCenter(-0.12f, -0.05f);
        const float radiusX = 0.45f;
        const float radiusY = 0.55f;
        const int loopControlCount = 12;
        for (int i = 0; i < loopControlCount; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(loopControlCount);
            const float angle = glm::two_pi<float>() * t;
            loopStroke.points.push_back(glm::vec2(
                loopCenter.x + radiusX * std::cos(angle),
                loopCenter.y + radiusY * std::sin(angle)
            ));
        }
        glyphDefinition.strokes.push_back(loopStroke);

        GlyphStroke stemStroke;
        stemStroke.closed = false;
        stemStroke.points.push_back(glm::vec2(0.34f, 0.50f));
        stemStroke.points.push_back(glm::vec2(0.42f, 0.18f));
        stemStroke.points.push_back(glm::vec2(0.39f, -0.34f));
        stemStroke.points.push_back(glm::vec2(0.44f, -0.58f));
        stemStroke.points.push_back(glm::vec2(0.62f, -0.68f));
        glyphDefinition.strokes.push_back(stemStroke);
        glyphDefinition.advance = 1.18f;
        return true;
    }

    if (character == 'b') {
        GlyphStroke stemStroke;
        stemStroke.closed = false;
        stemStroke.points.push_back(glm::vec2(-0.42f, 0.72f));
        stemStroke.points.push_back(glm::vec2(-0.42f, 0.28f));
        stemStroke.points.push_back(glm::vec2(-0.42f, -0.22f));
        stemStroke.points.push_back(glm::vec2(-0.42f, -0.70f));
        glyphDefinition.strokes.push_back(stemStroke);

        GlyphStroke bowlStroke;
        bowlStroke.closed = true;
        const glm::vec2 bowlCenter(-0.05f, -0.16f);
        const float bowlRadiusX = 0.40f;
        const float bowlRadiusY = 0.48f;
        const int bowlControlCount = 12;
        for (int i = 0; i < bowlControlCount; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(bowlControlCount);
            const float angle = glm::two_pi<float>() * t;
            bowlStroke.points.push_back(glm::vec2(
                bowlCenter.x + bowlRadiusX * std::cos(angle),
                bowlCenter.y + bowlRadiusY * std::sin(angle)
            ));
        }
        glyphDefinition.strokes.push_back(bowlStroke);
        glyphDefinition.advance = 1.12f;
        return true;
    }

    if (character == 'c') {
        GlyphStroke arcStroke;
        arcStroke.closed = false;
        const glm::vec2 arcCenter(0.02f, -0.06f);
        const float radiusX = 0.48f;
        const float radiusY = 0.58f;
        const int arcControlCount = 12;
        const float startAngle = glm::radians(46.0f);
        const float endAngle = glm::radians(314.0f);
        for (int i = 0; i < arcControlCount; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(arcControlCount - 1);
            const float angle = startAngle + (endAngle - startAngle) * t;
            arcStroke.points.push_back(glm::vec2(
                arcCenter.x + radiusX * std::cos(angle),
                arcCenter.y + radiusY * std::sin(angle)
            ));
        }
        glyphDefinition.strokes.push_back(arcStroke);
        glyphDefinition.advance = 1.08f;
        return true;
    }

    return false;
}

void HandwritingPathGenerator::appendGlyph(
    const GlyphDefinition& glyphDefinition,
    const Options& options,
    std::vector<Waypoint>& waypoints)
{
    for (size_t strokeIndex = 0; strokeIndex < glyphDefinition.strokes.size(); ++strokeIndex) {
        appendStroke(
            glyphDefinition.strokes[strokeIndex].points,
            options,
            waypoints,
            glyphDefinition.strokes[strokeIndex].closed
        );
    }
}

glm::vec3 HandwritingPathGenerator::localToWorld(
    const glm::vec2& localPoint,
    const Options& options,
    bool penDown)
{
    return options.waypointPosition(localPoint.x, localPoint.y, penDown);
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
        drawControlPoints.push_back(localToWorld(localPoints[i], options, true));
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
    const glm::vec3 firstTravel(firstDraw.x, options.paperY + options.liftHeight, firstDraw.z);
    if (!waypoints.empty()) {
        const glm::vec3 lastPosition = waypoints.back().position;
        waypoints.push_back(Waypoint(
            glm::vec3(lastPosition.x, options.paperY + options.liftHeight, lastPosition.z),
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
