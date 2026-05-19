#ifndef HANDWRITING_PATH_GENERATOR_H
#define HANDWRITING_PATH_GENERATOR_H

#include <string>
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
        float characterSpacing;
        float wordSpacing;
        float textYawDegrees;
        glm::vec3 paperRight;
        glm::vec3 paperUp;
        glm::vec3 paperNormal;

        Options()
            : paperOrigin(0.0f),
              scale(0.15f),
              paperY(0.0f),
              liftHeight(0.05f),
              sampleSpacing(0.01f),
              useSpline(true),
              characterSpacing(0.035f),
              wordSpacing(0.16f),
              textYawDegrees(90.0f),
              paperRight(0.0f, 0.0f, 1.0f),
              paperUp(-1.0f, 0.0f, 0.0f),
              paperNormal(0.0f, 1.0f, 0.0f)
        {
        }

        void setTextYawDegrees(float degrees);
        glm::vec3 toWorld(float localX, float localY, float heightAbovePaper) const;
        glm::vec3 waypointPosition(float localX, float localY, bool penDown) const;
    };

    std::vector<Waypoint> generateLowercaseA(const Options& options) const;
    std::vector<Waypoint> generateText(const std::string& text, const Options& options) const;
    int getLastUnsupportedCharacterCount() const;

private:
    struct GlyphStroke {
        std::vector<glm::vec2> points;
        bool closed;
    };

    struct GlyphDefinition {
        std::vector<GlyphStroke> strokes;
        float advance;
    };

    mutable int lastUnsupportedCharacterCount_ = 0;

    static bool buildGlyphDefinition(char character, GlyphDefinition& glyphDefinition);
    static void appendGlyph(
        const GlyphDefinition& glyphDefinition,
        const Options& options,
        std::vector<Waypoint>& waypoints
    );
    static glm::vec3 localToWorld(const glm::vec2& localPoint, const Options& options, bool penDown);
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
