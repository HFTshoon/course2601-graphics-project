#include "hershey_glyph_library.h"

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

bool findKey(const std::string& text, const std::string& key, size_t start, size_t& valueStart)
{
    const std::string quotedKey = "\"" + key + "\"";
    const size_t keyPos = text.find(quotedKey, start);
    if (keyPos == std::string::npos) {
        return false;
    }

    const size_t colonPos = text.find(':', keyPos + quotedKey.size());
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

float parseOptionalNumber(const std::string& text, const std::string& key, float defaultValue)
{
    size_t valueStart = 0;
    if (!findKey(text, key, 0, valueStart)) {
        return defaultValue;
    }

    float value = defaultValue;
    if (!parseNumberAt(text, valueStart, value)) {
        return defaultValue;
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

bool parseVec2FromObject(const std::string& objectText, const std::string& key, glm::vec2& value)
{
    size_t valueStart = 0;
    if (!findKey(objectText, key, 0, valueStart)) {
        return false;
    }
    return parsePointArray(objectText, valueStart, value);
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

    return true;
}

bool parseStrokeObjects(const std::string& glyphObject, std::vector<GlyphStroke2D>& strokes)
{
    size_t strokesValueStart = 0;
    if (!findKey(glyphObject, "strokes", 0, strokesValueStart)) {
        return false;
    }
    if (strokesValueStart >= glyphObject.size() || glyphObject[strokesValueStart] != '[') {
        return false;
    }

    size_t strokesArrayEnd = std::string::npos;
    if (!findMatchingBracket(glyphObject, strokesValueStart, '[', ']', strokesArrayEnd)) {
        return false;
    }

    size_t pos = strokesValueStart + 1;
    while (pos < strokesArrayEnd) {
        skipWhitespace(glyphObject, pos);
        if (pos >= strokesArrayEnd) {
            break;
        }
        if (glyphObject[pos] == ',') {
            ++pos;
            continue;
        }
        if (glyphObject[pos] != '{') {
            return false;
        }

        size_t strokeObjectEnd = std::string::npos;
        if (!findMatchingBracket(glyphObject, pos, '{', '}', strokeObjectEnd)) {
            return false;
        }

        const std::string strokeObject = glyphObject.substr(pos, strokeObjectEnd - pos + 1);
        GlyphStroke2D stroke;
        stroke.closed = parseBoolFromObject(strokeObject, "closed", false);
        if (!parsePoints(strokeObject, stroke.points)) {
            return false;
        }
        if (stroke.points.size() >= 2) {
            strokes.push_back(stroke);
        }
        pos = strokeObjectEnd + 1;
    }

    return true;
}

} // namespace

HersheyGlyphLibrary::HersheyGlyphLibrary()
    : source_(),
      fontName_(),
      defaultAdvance_(1.0f),
      defaultSpaceAdvance_(0.6f),
      glyphs_()
{
}

bool HersheyGlyphLibrary::loadFromJson(const std::string& path, std::string* errorMessage)
{
    std::ifstream file(path.c_str());
    if (!file.is_open()) {
        setError(errorMessage, "Failed to open Hershey glyph library: " + path);
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string contents = buffer.str();

    source_ = parseOptionalString(contents, "source");
    fontName_ = parseOptionalString(contents, "font");
    defaultAdvance_ = parseOptionalNumber(contents, "default_advance", 1.0f);
    defaultSpaceAdvance_ = parseOptionalNumber(contents, "default_space_advance", 0.6f);
    glyphs_.clear();

    size_t glyphsValueStart = 0;
    if (!findKey(contents, "glyphs", 0, glyphsValueStart)
        || glyphsValueStart >= contents.size()
        || contents[glyphsValueStart] != '{') {
        setError(errorMessage, "Glyph library does not contain a valid glyphs object.");
        return false;
    }

    size_t glyphsObjectEnd = std::string::npos;
    if (!findMatchingBracket(contents, glyphsValueStart, '{', '}', glyphsObjectEnd)) {
        setError(errorMessage, "Failed to parse glyphs object.");
        return false;
    }

    size_t pos = glyphsValueStart + 1;
    while (pos < glyphsObjectEnd) {
        skipWhitespace(contents, pos);
        if (pos >= glyphsObjectEnd) {
            break;
        }
        if (contents[pos] == ',') {
            ++pos;
            continue;
        }

        std::string glyphKey;
        if (!parseJsonStringAt(contents, pos, glyphKey)) {
            setError(errorMessage, "Failed to parse glyph key.");
            return false;
        }
        skipWhitespace(contents, pos);
        if (pos >= glyphsObjectEnd || contents[pos] != ':') {
            setError(errorMessage, "Expected ':' after glyph key.");
            return false;
        }
        ++pos;
        skipWhitespace(contents, pos);
        if (pos >= glyphsObjectEnd || contents[pos] != '{') {
            setError(errorMessage, "Expected glyph object.");
            return false;
        }

        size_t glyphObjectEnd = std::string::npos;
        if (!findMatchingBracket(contents, pos, '{', '}', glyphObjectEnd)) {
            setError(errorMessage, "Failed to parse glyph object.");
            return false;
        }

        const std::string glyphObject = contents.substr(pos, glyphObjectEnd - pos + 1);
        if (!glyphKey.empty()) {
            HersheyGlyph glyph;
            glyph.character = glyphKey[0];
            glyph.advance = parseOptionalNumber(glyphObject, "advance", defaultAdvance_);

            size_t boundsStart = 0;
            if (findKey(glyphObject, "bounds", 0, boundsStart)
                && boundsStart < glyphObject.size()
                && glyphObject[boundsStart] == '{') {
                size_t boundsEnd = std::string::npos;
                if (findMatchingBracket(glyphObject, boundsStart, '{', '}', boundsEnd)) {
                    const std::string boundsObject = glyphObject.substr(boundsStart, boundsEnd - boundsStart + 1);
                    parseVec2FromObject(boundsObject, "min", glyph.boundsMin);
                    parseVec2FromObject(boundsObject, "max", glyph.boundsMax);
                }
            }

            if (!parseStrokeObjects(glyphObject, glyph.strokes)) {
                setError(errorMessage, "Failed to parse strokes for glyph: " + glyphKey);
                return false;
            }
            glyphs_[glyph.character] = glyph;
        }

        pos = glyphObjectEnd + 1;
    }

    if (glyphs_.empty()) {
        setError(errorMessage, "Glyph library loaded, but it contains no glyphs.");
        return false;
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

bool HersheyGlyphLibrary::hasGlyph(char c) const
{
    return glyphs_.find(c) != glyphs_.end();
}

const HersheyGlyph* HersheyGlyphLibrary::getGlyph(char c) const
{
    std::map<char, HersheyGlyph>::const_iterator it = glyphs_.find(c);
    if (it == glyphs_.end()) {
        return NULL;
    }
    return &it->second;
}

float HersheyGlyphLibrary::getDefaultAdvance() const
{
    return defaultAdvance_;
}

float HersheyGlyphLibrary::getDefaultSpaceAdvance() const
{
    return defaultSpaceAdvance_;
}

const std::string& HersheyGlyphLibrary::getSourceName() const
{
    return source_;
}

const std::string& HersheyGlyphLibrary::getFontName() const
{
    return fontName_;
}

int HersheyGlyphLibrary::getGlyphCount() const
{
    return static_cast<int>(glyphs_.size());
}

bool HersheyGlyphLibrary::isLikelyFallbackLibrary() const
{
    return source_.find("fallback") != std::string::npos || source_ != "hershey-fonts";
}

std::string HersheyGlyphLibrary::getSupportedCharacterSummary() const
{
    std::string result;
    for (std::map<char, HersheyGlyph>::const_iterator it = glyphs_.begin(); it != glyphs_.end(); ++it) {
        if (it->first == ' ') {
            continue;
        }
        result.push_back(it->first);
    }
    if (hasGlyph(' ')) {
        result += " space";
    }
    return result;
}

std::vector<Waypoint> HersheyGlyphLibrary::generateWaypointsForText(
    const std::string& text,
    const HandwritingPathGenerator::Options& options,
    int* unsupportedCharacterCount,
    std::string* unsupportedCharacters) const
{
    std::vector<Waypoint> waypoints;
    int unsupportedCount = 0;
    std::string unsupported;
    float cursorXLocal = 0.0f;
    const float inverseScale = options.scale > 0.0001f ? 1.0f / options.scale : 1.0f;
    const float characterSpacingLocal = options.characterSpacing * inverseScale;
    const float wordSpacingLocal = options.wordSpacing * inverseScale;
    const float sampleSpacing = std::max(0.001f, options.sampleSpacing);

    for (size_t characterIndex = 0; characterIndex < text.size(); ++characterIndex) {
        const char c = text[characterIndex];
        if (c == ' ') {
            const HersheyGlyph* spaceGlyph = getGlyph(' ');
            cursorXLocal += (spaceGlyph ? spaceGlyph->advance : defaultSpaceAdvance_) + wordSpacingLocal;
            continue;
        }

        const HersheyGlyph* glyph = getGlyph(c);
        if (!glyph) {
            ++unsupportedCount;
            if (unsupported.find(c) == std::string::npos) {
                if (!unsupported.empty()) {
                    unsupported += ", ";
                }
                unsupported.push_back(c);
            }
            continue;
        }

        for (size_t strokeIndex = 0; strokeIndex < glyph->strokes.size(); ++strokeIndex) {
            const GlyphStroke2D& stroke = glyph->strokes[strokeIndex];
            if (stroke.points.empty()) {
                continue;
            }

            std::vector<glm::vec2> strokePoints = stroke.points;
            bool effectiveClosed = stroke.closed;
            if (strokePoints.size() >= 4
                && glm::length(strokePoints.front() - strokePoints.back()) <= 1e-5f) {
                effectiveClosed = true;
                strokePoints.pop_back();
            }
            if (strokePoints.size() < 2) {
                continue;
            }

            std::vector<glm::vec3> controlPoints;
            controlPoints.reserve(strokePoints.size());
            for (size_t pointIndex = 0; pointIndex < strokePoints.size(); ++pointIndex) {
                const glm::vec2& point = strokePoints[pointIndex];
                controlPoints.push_back(glm::vec3(
                    options.paperOrigin.x + options.scale * (cursorXLocal + point.x),
                    options.paperY,
                    options.paperOrigin.z + options.scale * point.y
                ));
            }

            std::vector<glm::vec3> drawPoints;
            if (options.useSpline && controlPoints.size() >= 4) {
                SplineGenerator splineGenerator;
                SplineGenerator::Options splineOptions;
                splineOptions.sampleSpacing = sampleSpacing;
                splineOptions.closed = effectiveClosed;
                drawPoints = splineGenerator.generateCatmullRomSpline(controlPoints, splineOptions);
            } else {
                drawPoints = samplePolyline(controlPoints, sampleSpacing, effectiveClosed);
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

        cursorXLocal += glyph->advance + characterSpacingLocal;
    }

    if (!waypoints.empty()) {
        const glm::vec3 lastPosition = waypoints.back().position;
        waypoints.push_back(Waypoint(
            glm::vec3(lastPosition.x, options.paperY + options.liftHeight, lastPosition.z),
            false
        ));
    }

    if (unsupportedCharacterCount) {
        *unsupportedCharacterCount = unsupportedCount;
    }
    if (unsupportedCharacters) {
        *unsupportedCharacters = unsupported;
    }
    return waypoints;
}

std::vector<glm::vec3> HersheyGlyphLibrary::samplePolyline(
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
