#ifndef HERSHEY_GLYPH_LIBRARY_H
#define HERSHEY_GLYPH_LIBRARY_H

#include <map>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "handwriting_path_generator.h"
#include "waypoint.h"

struct GlyphStroke2D {
    bool closed;
    std::vector<glm::vec2> points;

    GlyphStroke2D()
        : closed(false)
    {
    }
};

struct HersheyGlyph {
    char character;
    float advance;
    glm::vec2 boundsMin;
    glm::vec2 boundsMax;
    std::vector<GlyphStroke2D> strokes;

    HersheyGlyph()
        : character('\0'),
          advance(1.0f),
          boundsMin(0.0f),
          boundsMax(0.0f)
    {
    }
};

class HersheyGlyphLibrary {
public:
    HersheyGlyphLibrary();

    bool loadFromJson(const std::string& path, std::string* errorMessage = NULL);

    bool hasGlyph(char c) const;
    const HersheyGlyph* getGlyph(char c) const;

    float getDefaultAdvance() const;
    float getDefaultSpaceAdvance() const;

    const std::string& getFontName() const;
    int getGlyphCount() const;
    std::string getSupportedCharacterSummary() const;

    std::vector<Waypoint> generateWaypointsForText(
        const std::string& text,
        const HandwritingPathGenerator::Options& options,
        int* unsupportedCharacterCount = NULL
    ) const;

private:
    static std::vector<glm::vec3> samplePolyline(
        const std::vector<glm::vec3>& controlPoints,
        float sampleSpacing,
        bool closed
    );

    std::string source_;
    std::string fontName_;
    float defaultAdvance_;
    float defaultSpaceAdvance_;
    std::map<char, HersheyGlyph> glyphs_;
};

#endif
