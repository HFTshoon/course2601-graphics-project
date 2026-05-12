#include "ik_solver.h"

#include <algorithm>
#include <array>

#include <glm/geometric.hpp>

IKSolver::IKSolver(const Options& options)
    : options_(options),
      lastErrorNorm_(0.0f),
      lastIterationCount_(0)
{
}

bool IKSolver::solve(RobotKinematics& robot, const glm::vec3& target)
{
    robot.rebuildForwardKinematics();
    lastIterationCount_ = 0;

    const int maxIterations = std::max(0, options_.maxIterations);
    const float tolerance = std::max(0.0f, options_.tolerance);
    const float dampingScale = 1.0f / (1.0f + std::max(0.0f, options_.damping));

    for (int iteration = 0; iteration < maxIterations; ++iteration) {
        const glm::vec3 endEffectorPosition = robot.getEndEffectorPosition();
        const glm::vec3 error = target - endEffectorPosition;
        lastErrorNorm_ = glm::length(error);

        if (lastErrorNorm_ <= tolerance) {
            return true;
        }

        std::array<float, RobotKinematics::DOF> nextJointAngles = robot.getJointAngles();
        const std::vector<glm::vec3>& jointPositions = robot.getJointWorldPositions();
        const std::vector<glm::vec3>& jointAxes = robot.getJointWorldAxes();

        for (int jointIndex = 0; jointIndex < RobotKinematics::DOF; ++jointIndex) {
            const glm::vec3 jacobianColumn = glm::cross(
                jointAxes[jointIndex],
                endEffectorPosition - jointPositions[jointIndex]
            );
            const float deltaAngle = options_.stepSize * dampingScale * glm::dot(jacobianColumn, error);
            const float unclampedAngle = nextJointAngles[jointIndex] + deltaAngle;
            nextJointAngles[jointIndex] = std::max(
                robot.getJointLowerLimit(jointIndex),
                std::min(robot.getJointUpperLimit(jointIndex), unclampedAngle)
            );
        }

        robot.setJointAngles(nextJointAngles);
        robot.rebuildForwardKinematics();
        lastIterationCount_ = iteration + 1;
    }

    const glm::vec3 finalError = target - robot.getEndEffectorPosition();
    lastErrorNorm_ = glm::length(finalError);
    return lastErrorNorm_ <= tolerance;
}

float IKSolver::getLastErrorNorm() const
{
    return lastErrorNorm_;
}

int IKSolver::getLastIterationCount() const
{
    return lastIterationCount_;
}
