#include "hershey_path_loader.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include <glm/geometric.hpp>

#include "spline_generator.h"

namespace {

void setError(std::string* errorMessage, const std::string& message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

void skipWhitespace(const std::string& text, size_t& pos)
{
    while (pos < text.size()
        && (text[pos] == ' ' || text[pos] == '\n' || text[pos] == '\r' || text[pos] == '\t')) {
        ++pos;
    }
}

bool findKey(const std::string& text, const std::string& key, size_t start, size_t& valueStart)
{
    const std::string quotedKey = "\"" + key + "\"";
    const size_t keyPos = text.find(quotedKey, start);
    if (keyPos == std::string::npos) {
        return false;
    }

    size_t colonPos = text.find(':', keyPos + quotedKey.size());
    if (colonPos == std::string::npos) {
        return false;
    }

    valueStart = colonPos + 1;
    skipWhitespace(text, valueStart);
    return valueStart < text.size();
}

bool parseJsonStringAt(const std::string& text, size_t& pos, std::string& value)
{
    skipWhitespace(text, pos);
    if (pos >= text.size() || text[pos] != '"') {
        return false;
    }

    ++pos;
    value.clear();
    bool escaped = false;
    while (pos < text.size()) {
        const char c = text[pos++];
        if (escaped) {
            if (c == 'n') {
                value.push_back('\n');
            } else if (c == 't') {
                value.push_back('\t');
            } else {
                value.push_back(c);
            }
            escaped = false;
            continue;
        }

        if (c == '\\') {
            escaped = true;
        } else if (c == '"') {
            return true;
        } else {
            value.push_back(c);
        }
    }

    return false;
}

std::string parseOptionalString(const std::string& text, const std::string& key)
{
    size_t valueStart = 0;
    if (!findKey(text, key, 0, valueStart)) {
        return "";
    }

    std::string value;
    if (!parseJsonStringAt(text, valueStart, value)) {
        return "";
    }
    return value;
}

bool parseBoolFromObject(const std::string& objectText, const std::string& key, bool defaultValue)
{
    size_t valueStart = 0;
    if (!findKey(objectText, key, 0, valueStart)) {
        return defaultValue;
    }

    if (objectText.compare(valueStart, 4, "true") == 0) {
        return true;
    }
    if (objectText.compare(valueStart, 5, "false") == 0) {
        return false;
    }
    return defaultValue;
}

bool findMatchingBracket(
    const std::string& text,
    size_t openPos,
    char openChar,
    char closeChar,
    size_t& closePos)
{
    int depth = 0;
    bool inString = false;
    bool escaped = false;

    for (size_t pos = openPos; pos < text.size(); ++pos) {
        const char c = text[pos];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                inString = false;
            }
            continue;
        }

        if (c == '"') {
            inString = true;
        } else if (c == openChar) {
            ++depth;
        } else if (c == closeChar) {
            --depth;
            if (depth == 0) {
                closePos = pos;
                return true;
            }
        }
    }

    return false;
}

bool parseNumberAt(const std::string& text, size_t& pos, float& value)
{
    skipWhitespace(text, pos);
    if (pos >= text.size()) {
        return false;
    }

    char* endPtr = NULL;
    const char* startPtr = text.c_str() + pos;
    const double parsed = std::strtod(startPtr, &endPtr);
    if (endPtr == startPtr) {
        return false;
    }

    value = static_cast<float>(parsed);
    pos = static_cast<size_t>(endPtr - text.c_str());
    return true;
}

bool parsePointArray(const std::string& text, size_t& pos, glm::vec2& point)
{
    skipWhitespace(text, pos);
    if (pos >= text.size() || text[pos] != '[') {
        return false;
    }
    ++pos;

    float x = 0.0f;
    float y = 0.0f;
    if (!parseNumberAt(text, pos, x)) {
        return false;
    }
    skipWhitespace(text, pos);
    if (pos >= text.size() || text[pos] != ',') {
        return false;
    }
    ++pos;
    if (!parseNumberAt(text, pos, y)) {
        return false;
    }
    skipWhitespace(text, pos);
    if (pos >= text.size() || text[pos] != ']') {
        return false;
    }
    ++pos;

    point = glm::vec2(x, y);
    return true;
}

bool parsePoints(const std::string& objectText, std::vector<glm::vec2>& points)
{
    size_t pointsValueStart = 0;
    if (!findKey(objectText, "points", 0, pointsValueStart)) {
        return false;
    }
    if (pointsValueStart >= objectText.size() || objectText[pointsValueStart] != '[') {
        return false;
    }

    size_t arrayEnd = std::string::npos;
    if (!findMatchingBracket(objectText, pointsValueStart, '[', ']', arrayEnd)) {
        return false;
    }

    size_t pos = pointsValueStart + 1;
    while (pos < arrayEnd) {
        skipWhitespace(objectText, pos);
        if (pos >= arrayEnd) {
            break;
        }
        if (objectText[pos] == ',') {
            ++pos;
            continue;
        }
        glm::vec2 point(0.0f);
        if (!parsePointArray(objectText, pos, point)) {
            return false;
        }
        points.push_back(point);
    }

    return !points.empty();
}

} // namespace

bool HersheyPathLoader::loadFromJson(
    const std::string& path,
    LoadedPath& outPath,
    std::string* errorMessage) const
{
    std::ifstream file(path.c_str());
    if (!file.is_open()) {
        setError(errorMessage, "Failed to open Hershey JSON: " + path);
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string contents = buffer.str();

    outPath = LoadedPath();
    outPath.source = parseOptionalString(contents, "source");
    outPath.text = parseOptionalString(contents, "text");
    outPath.font = parseOptionalString(contents, "font");

    size_t strokesValueStart = 0;
    if (!findKey(contents, "strokes", 0, strokesValueStart)
        || strokesValueStart >= contents.size()
        || contents[strokesValueStart] != '[') {
        setError(errorMessage, "Hershey JSON does not contain a valid strokes array.");
        return false;
    }

    size_t strokesArrayEnd = std::string::npos;
    if (!findMatchingBracket(contents, strokesValueStart, '[', ']', strokesArrayEnd)) {
        setError(errorMessage, "Failed to parse strokes array.");
        return false;
    }

    size_t pos = strokesValueStart + 1;
    while (pos < strokesArrayEnd) {
        skipWhitespace(contents, pos);
        if (pos >= strokesArrayEnd) {
            break;
        }
        if (contents[pos] == ',') {
            ++pos;
            continue;
        }
        if (contents[pos] != '{') {
            setError(errorMessage, "Expected stroke object in strokes array.");
            return false;
        }

        size_t objectEnd = std::string::npos;
        if (!findMatchingBracket(contents, pos, '{', '}', objectEnd)) {
            setError(errorMessage, "Failed to parse stroke object.");
            return false;
        }

        const std::string strokeObject = contents.substr(pos, objectEnd - pos + 1);
        Stroke2D stroke;
        stroke.closed = parseBoolFromObject(strokeObject, "closed", false);
        if (!parsePoints(strokeObject, stroke.points)) {
            setError(errorMessage, "Failed to parse stroke points.");
            return false;
        }
        if (stroke.points.size() >= 2) {
            outPath.strokes.push_back(stroke);
        }
        pos = objectEnd + 1;
    }

    if (outPath.strokes.empty()) {
        setError(errorMessage, "Hershey JSON loaded, but it contains no drawable strokes.");
        return false;
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

std::vector<Waypoint> HersheyPathLoader::generateWaypoints(
    const LoadedPath& path,
    const HandwritingPathGenerator::Options& options) const
{
    std::vector<Waypoint> waypoints;
    const float sampleSpacing = std::max(0.001f, options.sampleSpacing);

    for (size_t strokeIndex = 0; strokeIndex < path.strokes.size(); ++strokeIndex) {
        const Stroke2D& stroke = path.strokes[strokeIndex];
        if (stroke.points.empty()) {
            continue;
        }

        std::vector<glm::vec3> controlPoints;
        controlPoints.reserve(stroke.points.size());
        for (size_t pointIndex = 0; pointIndex < stroke.points.size(); ++pointIndex) {
            const glm::vec2& point = stroke.points[pointIndex];
            controlPoints.push_back(options.waypointPosition(point.x, point.y, true));
        }

        std::vector<glm::vec3> drawPoints;
        if (options.useSpline) {
            SplineGenerator splineGenerator;
            SplineGenerator::Options splineOptions;
            splineOptions.sampleSpacing = sampleSpacing;
            splineOptions.closed = stroke.closed;
            drawPoints = splineGenerator.generateCatmullRomSpline(controlPoints, splineOptions);
        } else {
            drawPoints = samplePolyline(controlPoints, sampleSpacing, stroke.closed);
        }

        if (drawPoints.empty()) {
            continue;
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

        for (size_t pointIndex = 1; pointIndex < drawPoints.size(); ++pointIndex) {
            waypoints.push_back(Waypoint(drawPoints[pointIndex], true));
        }
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

std::vector<glm::vec3> HersheyPathLoader::samplePolyline(
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
