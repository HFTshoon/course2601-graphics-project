#ifndef HERSHEY_PATH_LOADER_H
#define HERSHEY_PATH_LOADER_H

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "handwriting_path_generator.h"
#include "waypoint.h"

struct Stroke2D {
    bool closed;
    std::vector<glm::vec2> points;

    Stroke2D()
        : closed(false)
    {
    }
};

class HersheyPathLoader {
public:
    struct LoadedPath {
        std::string source;
        std::string text;
        std::string font;
        std::vector<Stroke2D> strokes;
    };

    bool loadFromJson(
        const std::string& path,
        LoadedPath& outPath,
        std::string* errorMessage = NULL
    ) const;

    std::vector<Waypoint> generateWaypoints(
        const LoadedPath& path,
        const HandwritingPathGenerator::Options& options
    ) const;

private:
    static std::vector<glm::vec3> samplePolyline(
        const std::vector<glm::vec3>& controlPoints,
        float sampleSpacing,
        bool closed
    );
};

#endif
