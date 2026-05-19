#ifndef WAYPOINT_H
#define WAYPOINT_H

#include <glm/glm.hpp>

struct Waypoint {
    glm::vec3 position;
    bool penDown = false;

    Waypoint()
        : position(0.0f)
    {
    }

    Waypoint(const glm::vec3& positionValue, bool penDownValue)
        : position(positionValue),
          penDown(penDownValue)
    {
    }
};

#endif
