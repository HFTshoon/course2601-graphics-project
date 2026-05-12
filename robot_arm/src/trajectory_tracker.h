#ifndef TRAJECTORY_TRACKER_H
#define TRAJECTORY_TRACKER_H

#include <vector>

#include <glm/glm.hpp>

#include "ik_solver.h"
#include "robot_kinematics.h"
#include "waypoint.h"

class TrajectoryTracker {
public:
    TrajectoryTracker();

    void setWaypoints(const std::vector<Waypoint>& waypoints);
    void reset(const glm::vec3& currentToolTipPosition);

    void update(float dt, RobotKinematics& robot, IKSolver& ikSolver);

    const Waypoint& getCurrentWaypoint() const;
    int getCurrentWaypointIndex() const;
    int getWaypointCount() const;

    bool isPlaying() const;
    bool isFinished() const;

    void play(const glm::vec3& currentToolTipPosition);
    void pause();

    void setPlaybackSpeed(float speed);
    float getPlaybackSpeed() const;

    void setWaypointReachThreshold(float threshold);
    float getWaypointReachThreshold() const;

    glm::vec3 getInterpolatedTargetPosition() const;
    float getDistanceToCurrentWaypoint() const;
    float getToolTipDistanceToCurrentWaypoint() const;
    float getEndEffectorDistanceToCurrentWaypoint() const;

private:
    bool hasCurrentWaypoint() const;
    void refreshDistances(const RobotKinematics& robot);
    void moveToNextWaypoint();

    std::vector<Waypoint> waypoints_;
    int currentWaypointIndex_;
    bool playing_;
    bool finished_;
    glm::vec3 interpolatedTargetPosition_;
    float playbackSpeed_;
    float waypointReachThreshold_;
    float distanceToCurrentWaypoint_;
    float toolTipDistanceToCurrentWaypoint_;
};

#endif
