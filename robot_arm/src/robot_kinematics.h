#ifndef ROBOT_KINEMATICS_H
#define ROBOT_KINEMATICS_H

#include <array>
#include <vector>

#include <glm/glm.hpp>

class RobotKinematics {
public:
    static constexpr int DOF = 7;

    RobotKinematics();

    void setJointAngle(int index, float value);
    void setJointAngles(const std::array<float, DOF>& q);

    float getJointAngle(int index) const;
    const std::array<float, DOF>& getJointAngles() const;

    void rebuildForwardKinematics();

    glm::vec3 getEndEffectorPosition() const;
    glm::mat4 getEndEffectorTransform() const;

    const std::vector<glm::vec3>& getJointWorldPositions() const;
    const std::vector<glm::mat4>& getLinkWorldTransforms() const;

    float getJointLowerLimit(int index) const;
    float getJointUpperLimit(int index) const;

private:
    struct PandaJointSpec {
        glm::vec3 xyz;
        glm::vec3 rpy;
        glm::vec3 axis;
        float lower;
        float upper;
        float initialAngle;
    };

    static std::array<PandaJointSpec, DOF> createPandaJointSpecs();
    static glm::mat4 createPandaBaseWorldTransform();
    static glm::mat4 createUrdfBasis();
    static glm::vec3 urdfVecToRender(const glm::vec3& value);
    static glm::mat4 urdfOriginToRender(const glm::vec3& xyz, const glm::vec3& rpy);

    std::array<PandaJointSpec, DOF> pandaJoints_;
    std::array<float, DOF> jointAngles_;
    glm::mat4 pandaBaseWorld_;
    glm::mat4 endEffectorTransform_;
    std::vector<glm::vec3> jointWorldPositions_;
    std::vector<glm::mat4> linkWorldTransforms_;
};

#endif
