#include "trajectory_tracker.h"

#include <algorithm>
#include <cassert>

#include <glm/geometric.hpp>

TrajectoryTracker::TrajectoryTracker()
    : currentWaypointIndex_(0),
      playing_(false),
      finished_(false),
      interpolatedTargetPosition_(0.0f),
      playbackSpeed_(0.20f),
      waypointReachThreshold_(0.025f),
      distanceToCurrentWaypoint_(0.0f),
      toolTipDistanceToCurrentWaypoint_(0.0f)
{
}

void TrajectoryTracker::setWaypoints(const std::vector<Waypoint>& waypoints)
{
    waypoints_ = waypoints;
    currentWaypointIndex_ = 0;
    playing_ = false;
    finished_ = waypoints_.empty();
    distanceToCurrentWaypoint_ = 0.0f;
    toolTipDistanceToCurrentWaypoint_ = 0.0f;
}

void TrajectoryTracker::reset(const glm::vec3& currentToolTipPosition)
{
    currentWaypointIndex_ = 0;
    playing_ = false;
    finished_ = waypoints_.empty();
    interpolatedTargetPosition_ = currentToolTipPosition;
    distanceToCurrentWaypoint_ = hasCurrentWaypoint()
        ? glm::length(waypoints_[currentWaypointIndex_].position - interpolatedTargetPosition_)
        : 0.0f;
    toolTipDistanceToCurrentWaypoint_ = distanceToCurrentWaypoint_;
}

void TrajectoryTracker::update(float dt, RobotKinematics& robot, IKSolver& ikSolver)
{
    if (!hasCurrentWaypoint()) {
        playing_ = false;
        finished_ = true;
        distanceToCurrentWaypoint_ = 0.0f;
        toolTipDistanceToCurrentWaypoint_ = 0.0f;
        return;
    }

    if (!playing_ || finished_) {
        refreshDistances(robot);
        return;
    }

    const Waypoint& currentWaypoint = waypoints_[currentWaypointIndex_];
    const glm::vec3 toWaypoint = currentWaypoint.position - interpolatedTargetPosition_;
    const float distanceToTarget = glm::length(toWaypoint);
    const float clampedDt = std::max(0.0f, dt);
    const float step = std::max(0.0f, playbackSpeed_) * clampedDt;

    if (distanceToTarget > 1e-6f) {
        if (step >= distanceToTarget) {
            interpolatedTargetPosition_ = currentWaypoint.position;
        } else if (step > 0.0f) {
            interpolatedTargetPosition_ += (toWaypoint / distanceToTarget) * step;
        }
    }

    ikSolver.solve(robot, interpolatedTargetPosition_);
    refreshDistances(robot);

    const bool targetReached = distanceToCurrentWaypoint_ <= waypointReachThreshold_;
    const bool toolTipReached = toolTipDistanceToCurrentWaypoint_ <= waypointReachThreshold_;
    if (targetReached && toolTipReached) {
        moveToNextWaypoint();
        refreshDistances(robot);
    }
}

const Waypoint& TrajectoryTracker::getCurrentWaypoint() const
{
    assert(hasCurrentWaypoint());
    return waypoints_[currentWaypointIndex_];
}

int TrajectoryTracker::getCurrentWaypointIndex() const
{
    return currentWaypointIndex_;
}

int TrajectoryTracker::getWaypointCount() const
{
    return static_cast<int>(waypoints_.size());
}

bool TrajectoryTracker::isPlaying() const
{
    return playing_;
}

bool TrajectoryTracker::isFinished() const
{
    return finished_;
}

void TrajectoryTracker::play(const glm::vec3& currentToolTipPosition)
{
    if (!hasCurrentWaypoint() || finished_) {
        return;
    }

    if (!playing_) {
        interpolatedTargetPosition_ = currentToolTipPosition;
    }
    playing_ = true;
}

void TrajectoryTracker::pause()
{
    playing_ = false;
}

void TrajectoryTracker::setPlaybackSpeed(float speed)
{
    playbackSpeed_ = std::max(0.0f, speed);
}

float TrajectoryTracker::getPlaybackSpeed() const
{
    return playbackSpeed_;
}

void TrajectoryTracker::setWaypointReachThreshold(float threshold)
{
    waypointReachThreshold_ = std::max(0.0f, threshold);
}

float TrajectoryTracker::getWaypointReachThreshold() const
{
    return waypointReachThreshold_;
}

glm::vec3 TrajectoryTracker::getInterpolatedTargetPosition() const
{
    return interpolatedTargetPosition_;
}

float TrajectoryTracker::getDistanceToCurrentWaypoint() const
{
    return distanceToCurrentWaypoint_;
}

float TrajectoryTracker::getToolTipDistanceToCurrentWaypoint() const
{
    return toolTipDistanceToCurrentWaypoint_;
}

float TrajectoryTracker::getEndEffectorDistanceToCurrentWaypoint() const
{
    return toolTipDistanceToCurrentWaypoint_;
}

bool TrajectoryTracker::hasCurrentWaypoint() const
{
    return currentWaypointIndex_ >= 0
        && currentWaypointIndex_ < static_cast<int>(waypoints_.size());
}

void TrajectoryTracker::refreshDistances(const RobotKinematics& robot)
{
    if (!hasCurrentWaypoint()) {
        distanceToCurrentWaypoint_ = 0.0f;
        toolTipDistanceToCurrentWaypoint_ = 0.0f;
        return;
    }

    const glm::vec3 waypointPosition = waypoints_[currentWaypointIndex_].position;
    distanceToCurrentWaypoint_ = glm::length(waypointPosition - interpolatedTargetPosition_);
    toolTipDistanceToCurrentWaypoint_ = glm::length(
        waypointPosition - robot.getToolTipPosition()
    );
}

void TrajectoryTracker::moveToNextWaypoint()
{
    ++currentWaypointIndex_;
    if (!hasCurrentWaypoint()) {
        playing_ = false;
        finished_ = true;
    }
}
