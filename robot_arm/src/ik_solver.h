#ifndef IK_SOLVER_H
#define IK_SOLVER_H

#include <glm/glm.hpp>

#include "robot_kinematics.h"

class IKSolver {
public:
    struct Options {
        int maxIterations;
        float stepSize;
        float tolerance;
        float damping;

        Options()
            : maxIterations(20),
              stepSize(0.3f),
              tolerance(0.005f),
              damping(0.0f)
        {
        }
    };

    explicit IKSolver(const Options& options = Options());

    bool solve(RobotKinematics& robot, const glm::vec3& toolTipTarget);

    float getLastErrorNorm() const;
    int getLastIterationCount() const;

private:
    Options options_;
    float lastErrorNorm_;
    int lastIterationCount_;
};

#endif
