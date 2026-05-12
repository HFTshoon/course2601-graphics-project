#include "robot_kinematics.h"

#include <cassert>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

RobotKinematics::RobotKinematics()
    : pandaJoints_(createPandaJointSpecs()),
      jointAngles_(),
      pandaBaseWorld_(createPandaBaseWorldTransform()),
      endEffectorTransform_(glm::mat4(1.0f))
{
    for (int i = 0; i < DOF; ++i) {
        jointAngles_[i] = pandaJoints_[i].initialAngle;
    }

    rebuildForwardKinematics();
}

void RobotKinematics::setJointAngle(int index, float value)
{
    assert(index >= 0 && index < DOF);
    jointAngles_[index] = value;
}

void RobotKinematics::setJointAngles(const std::array<float, DOF>& q)
{
    jointAngles_ = q;
}

float RobotKinematics::getJointAngle(int index) const
{
    assert(index >= 0 && index < DOF);
    return jointAngles_[index];
}

const std::array<float, RobotKinematics::DOF>& RobotKinematics::getJointAngles() const
{
    return jointAngles_;
}

void RobotKinematics::rebuildForwardKinematics()
{
    jointWorldPositions_.clear();
    jointWorldPositions_.reserve(DOF);

    linkWorldTransforms_.clear();
    linkWorldTransforms_.reserve(DOF + 1);
    linkWorldTransforms_.push_back(pandaBaseWorld_);

    glm::mat4 cur = pandaBaseWorld_;
    for (int jointIndex = 0; jointIndex < DOF; ++jointIndex) {
        const PandaJointSpec& joint = pandaJoints_[jointIndex];
        const glm::mat4 origin = urdfOriginToRender(joint.xyz, joint.rpy);
        const glm::vec3 axis = glm::normalize(urdfVecToRender(joint.axis));
        const glm::mat4 motion = glm::rotate(glm::mat4(1.0f), jointAngles_[jointIndex], axis);

        const glm::mat4 jointWorld = cur * origin;
        jointWorldPositions_.push_back(glm::vec3(jointWorld[3]));

        cur = cur * origin * motion;
        linkWorldTransforms_.push_back(cur);
    }

    const glm::mat4 joint8 = urdfOriginToRender(
        glm::vec3(0.0f, 0.0f, 0.107f),
        glm::vec3(0.0f)
    );
    const glm::mat4 handJoint = urdfOriginToRender(
        glm::vec3(0.0f),
        glm::vec3(0.0f, 0.0f, -glm::quarter_pi<float>())
    );

    endEffectorTransform_ = cur * joint8 * handJoint;
}

glm::vec3 RobotKinematics::getEndEffectorPosition() const
{
    return glm::vec3(endEffectorTransform_[3]);
}

glm::mat4 RobotKinematics::getEndEffectorTransform() const
{
    return endEffectorTransform_;
}

const std::vector<glm::vec3>& RobotKinematics::getJointWorldPositions() const
{
    return jointWorldPositions_;
}

const std::vector<glm::mat4>& RobotKinematics::getLinkWorldTransforms() const
{
    return linkWorldTransforms_;
}

float RobotKinematics::getJointLowerLimit(int index) const
{
    assert(index >= 0 && index < DOF);
    return pandaJoints_[index].lower;
}

float RobotKinematics::getJointUpperLimit(int index) const
{
    assert(index >= 0 && index < DOF);
    return pandaJoints_[index].upper;
}

std::array<RobotKinematics::PandaJointSpec, RobotKinematics::DOF>
RobotKinematics::createPandaJointSpecs()
{
    std::array<PandaJointSpec, DOF> joints = {{
        { glm::vec3(0.0f, 0.0f, 0.333f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), -2.8973f, 2.8973f, 0.0f },
        { glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-glm::half_pi<float>(), 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), -1.7628f, 1.7628f, -glm::quarter_pi<float>() },
        { glm::vec3(0.0f, -0.316f, 0.0f), glm::vec3(glm::half_pi<float>(), 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), -2.8973f, 2.8973f, 0.0f },
        { glm::vec3(0.0825f, 0.0f, 0.0f), glm::vec3(glm::half_pi<float>(), 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), -3.0718f, -0.0698f, -3.0f * glm::quarter_pi<float>() },
        { glm::vec3(-0.0825f, 0.384f, 0.0f), glm::vec3(-glm::half_pi<float>(), 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), -2.8973f, 2.8973f, 0.0f },
        { glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(glm::half_pi<float>(), 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), -0.0175f, 3.7525f, glm::half_pi<float>() },
        { glm::vec3(0.088f, 0.0f, 0.0f), glm::vec3(glm::half_pi<float>(), 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), -2.8973f, 2.8973f, glm::quarter_pi<float>() }
    }};

    return joints;
}

glm::mat4 RobotKinematics::createPandaBaseWorldTransform()
{
    glm::mat4 pandaBaseWorld = glm::mat4(1.0f);
    pandaBaseWorld = glm::translate(pandaBaseWorld, glm::vec3(0.0f, -0.5f, 1.0f));
    pandaBaseWorld = glm::rotate(pandaBaseWorld, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    pandaBaseWorld = glm::scale(pandaBaseWorld, glm::vec3(2.0f));
    return pandaBaseWorld;
}

glm::mat4 RobotKinematics::createUrdfBasis()
{
    glm::mat4 basis(1.0f);
    basis[0] = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
    basis[1] = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
    basis[2] = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
    basis[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    return basis;
}

glm::vec3 RobotKinematics::urdfVecToRender(const glm::vec3& value)
{
    const glm::mat4 basis = createUrdfBasis();
    return glm::vec3(basis * glm::vec4(value, 0.0f));
}

glm::mat4 RobotKinematics::urdfOriginToRender(const glm::vec3& xyz, const glm::vec3& rpy)
{
    const glm::mat4 basis = createUrdfBasis();

    glm::mat4 urdfRotation(1.0f);
    urdfRotation = glm::rotate(urdfRotation, rpy.z, glm::vec3(0.0f, 0.0f, 1.0f));
    urdfRotation = glm::rotate(urdfRotation, rpy.y, glm::vec3(0.0f, 1.0f, 0.0f));
    urdfRotation = glm::rotate(urdfRotation, rpy.x, glm::vec3(1.0f, 0.0f, 0.0f));

    const glm::mat4 renderRotation = basis * urdfRotation * glm::transpose(basis);
    glm::mat4 transform(1.0f);
    transform = glm::translate(transform, urdfVecToRender(xyz));
    return transform * renderRotation;
}
