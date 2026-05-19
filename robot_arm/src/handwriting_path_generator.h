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

        Options()
            : paperOrigin(0.0f),
              scale(0.15f),
              paperY(0.0f),
              liftHeight(0.05f),
              sampleSpacing(0.01f),
              useSpline(true),
              characterSpacing(0.035f),
              wordSpacing(0.16f)
        {
        }
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
