#pragma once
#ifndef ROBOT_ARM_MODEL_H
#define ROBOT_ARM_MODEL_H

#include "scene.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

class RobotArmModel {
public:
    void addToScene(Scene& scene)
    {
        for (size_t i = 0; i < linkEntities.size(); ++i) {
            scene.addEntity(linkEntities[i].get());
        }
    }

private:
    struct JointDesc {
        std::string name;
        std::string parent;
        std::string child;
        std::string type;
        glm::vec3 xyz;
        glm::vec3 rpy;
        glm::vec3 axis;
    };

    struct LinkVisual {
        std::string meshFile;
        glm::mat4 visualOffset;
    };

    std::map<std::string, LinkVisual> links;
    std::unordered_map<std::string, JointDesc> childToJoint;
    std::unordered_map<std::string, glm::mat4> linkFrameCache;
    std::unordered_map<std::string, float> jointPositions;
    std::vector<std::unique_ptr<Model>> linkModels;
    std::vector<std::unique_ptr<Entity>> linkEntities;

    static std::unordered_map<std::string, float> defaultJointPositions()
    {
        return {
            std::make_pair("panda_joint1", 0.0f),
            std::make_pair("panda_joint2", -3.14159265358979323846f / 4.0f),
            std::make_pair("panda_joint3", 0.0f),
            std::make_pair("panda_joint4", -3.0f * 3.14159265358979323846f / 4.0f),
            std::make_pair("panda_joint5", 0.0f),
            std::make_pair("panda_joint6", 3.14159265358979323846f / 2.0f),
            std::make_pair("panda_joint7", 3.14159265358979323846f / 4.0f),
            std::make_pair("panda_finger_joint1", 0.0f),
            std::make_pair("panda_finger_joint2", 0.0f)
        };
    }

    static std::string readTextFile(const std::string& filePath)
    {
        std::ifstream file(filePath.c_str());
        if (!file.is_open()) {
            std::cout << "WARN::ROBOT_ARM:: failed to open " << filePath << std::endl;
            return "";
        }

        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    static std::string trim(const std::string& s)
    {
        const std::string whitespace = " \t\n\r";
        const size_t begin = s.find_first_not_of(whitespace);
        if (begin == std::string::npos) return "";
        const size_t end = s.find_last_not_of(whitespace);
        return s.substr(begin, end - begin + 1);
    }

    static std::string normalizeVarTokens(std::string s)
    {
        const std::vector<std::pair<std::string, std::string>> replacements = {
            std::make_pair("${arm_id}", "panda"),
            std::make_pair("${ns}", "panda"),
            std::make_pair("$(arg arm_id)", "panda")
        };

        for (size_t i = 0; i < replacements.size(); ++i) {
            const std::string& from = replacements[i].first;
            const std::string& to = replacements[i].second;
            size_t pos = 0;
            while ((pos = s.find(from, pos)) != std::string::npos) {
                s.replace(pos, from.size(), to);
                pos += to.size();
            }
        }
        return s;
    }

    static std::string extractAttribute(const std::string& text, const std::string& attrName)
    {
        std::regex attrRegex(attrName + "\\s*=\\s*\"([^\"]*)\"");
        std::smatch match;
        if (std::regex_search(text, match, attrRegex)) {
            return match[1].str();
        }
        return "";
    }

    static double parseScalarExpr(std::string token)
    {
        token = trim(token);
        if (token.empty()) return 0.0;

        if (token.size() >= 3 && token[0] == '$' && token[1] == '{' && token[token.size() - 1] == '}') {
            token = token.substr(2, token.size() - 3);
        }

        token = trim(token);
        token.erase(std::remove(token.begin(), token.end(), ' '), token.end());
        if (token.empty()) return 0.0;

        size_t pos = 0;
        while ((pos = token.find("pi", pos)) != std::string::npos) {
            token.replace(pos, 2, "3.14159265358979323846");
            pos += 19;
        }

        std::vector<double> values;
        std::vector<char> ops;
        std::string cur;

        auto toDoubleSafe = [](const std::string& s, bool* ok) -> double {
            try {
                size_t processed = 0;
                double v = std::stod(s, &processed);
                if (processed != s.size()) {
                    *ok = false;
                    return 0.0;
                }
                *ok = true;
                return v;
            } catch (...) {
                *ok = false;
                return 0.0;
            }
        };

        for (size_t i = 0; i < token.size(); ++i) {
            const char c = token[i];
            if ((c == '*' || c == '/') && !cur.empty()) {
                bool ok = false;
                double value = toDoubleSafe(cur, &ok);
                if (!ok) return 0.0;
                values.push_back(value);
                ops.push_back(c);
                cur.clear();
            } else {
                cur.push_back(c);
            }
        }
        if (!cur.empty()) {
            bool ok = false;
            double value = toDoubleSafe(cur, &ok);
            if (!ok) return 0.0;
            values.push_back(value);
        }
        if (values.empty()) return 0.0;

        double result = values[0];
        for (size_t i = 0; i < ops.size() && (i + 1) < values.size(); ++i) {
            if (ops[i] == '*') result *= values[i + 1];
            else result /= values[i + 1];
        }
        return result;
    }

    static glm::vec3 parseVec3(const std::string& value)
    {
        glm::vec3 out(0.0f);
        std::stringstream ss(normalizeVarTokens(value));
        std::string x, y, z;
        ss >> x >> y >> z;
        if (!x.empty()) out.x = static_cast<float>(parseScalarExpr(x));
        if (!y.empty()) out.y = static_cast<float>(parseScalarExpr(y));
        if (!z.empty()) out.z = static_cast<float>(parseScalarExpr(z));
        return out;
    }

    static glm::mat4 makeOriginTransform(const glm::vec3& xyz, const glm::vec3& rpy)
    {
        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::translate(transform, xyz);
        transform = glm::rotate(transform, rpy.z, glm::vec3(0.0f, 0.0f, 1.0f));
        transform = glm::rotate(transform, rpy.y, glm::vec3(0.0f, 1.0f, 0.0f));
        transform = glm::rotate(transform, rpy.x, glm::vec3(1.0f, 0.0f, 0.0f));
        return transform;
    }

    void parseXacroFile(const std::string& filePath)
    {
        const std::string text = readTextFile(filePath);
        if (text.empty()) return;

        std::regex linkRegex("<link\\s+name=\"([^\"]+)\">([\\s\\S]*?)</link>");
        std::sregex_iterator linkIt(text.begin(), text.end(), linkRegex);
        std::sregex_iterator linkEnd;
        for (; linkIt != linkEnd; ++linkIt) {
            std::string linkName = normalizeVarTokens((*linkIt)[1].str());
            std::string body = (*linkIt)[2].str();

            std::smatch meshMatch;
            std::regex meshRegex("<mesh[^>]*filename=\"[^\"]*meshes/visual/([^\"]+)\"");
            if (!std::regex_search(body, meshMatch, meshRegex)) {
                continue;
            }

            LinkVisual lv;
            lv.meshFile = meshMatch[1].str();
            lv.visualOffset = glm::mat4(1.0f);

            std::smatch visualMatch;
            std::regex visualRegex("<visual>([\\s\\S]*?)</visual>");
            if (std::regex_search(body, visualMatch, visualRegex)) {
                const std::string visualBody = visualMatch[1].str();
                const std::string xyzAttr = extractAttribute(visualBody, "xyz");
                const std::string rpyAttr = extractAttribute(visualBody, "rpy");
                if (!xyzAttr.empty() || !rpyAttr.empty()) {
                    lv.visualOffset = makeOriginTransform(parseVec3(xyzAttr), parseVec3(rpyAttr));
                }
            }

            links[linkName] = lv;
        }

        std::regex jointRegex("<joint\\s+name=\"([^\"]+)\"\\s+type=\"([^\"]+)\">([\\s\\S]*?)</joint>");
        std::sregex_iterator jointIt(text.begin(), text.end(), jointRegex);
        std::sregex_iterator jointEnd;
        for (; jointIt != jointEnd; ++jointIt) {
            const std::string jointName = normalizeVarTokens((*jointIt)[1].str());
            const std::string jointType = (*jointIt)[2].str();
            const std::string body = (*jointIt)[3].str();

            const std::string parent = normalizeVarTokens(extractAttribute(body, "parent\\s+link"));
            const std::string child = normalizeVarTokens(extractAttribute(body, "child\\s+link"));
            if (parent.empty() || child.empty()) {
                continue;
            }

            JointDesc joint;
            joint.name = jointName;
            joint.parent = parent;
            joint.child = child;
            joint.type = jointType;
            joint.xyz = parseVec3(extractAttribute(body, "xyz"));
            joint.rpy = parseVec3(extractAttribute(body, "rpy"));
            joint.axis = parseVec3(extractAttribute(body, "axis\\s+xyz"));
            if (glm::length(joint.axis) < 1e-6f) {
                joint.axis = glm::vec3(0.0f, 0.0f, 1.0f);
            }

            childToJoint[child] = joint;
        }
    }

    void parseHandAttachment(const std::string& simFilePath)
    {
        const std::string text = readTextFile(simFilePath);
        if (text.empty()) return;

        std::smatch handMatch;
        std::regex handTag("<xacro:hand[^>]*/>");
        if (!std::regex_search(text, handMatch, handTag)) {
            return;
        }

        std::string handInvoke = handMatch[0].str();
        const std::string connectedTo = normalizeVarTokens(extractAttribute(handInvoke, "connected_to"));
        if (connectedTo.empty()) return;

        JointDesc handJoint;
        handJoint.parent = connectedTo;
        handJoint.child = "panda_hand";
        handJoint.type = "fixed";
        handJoint.xyz = parseVec3(extractAttribute(handInvoke, "xyz"));
        handJoint.rpy = parseVec3(extractAttribute(handInvoke, "rpy"));
        handJoint.axis = glm::vec3(0.0f, 0.0f, 1.0f);
        childToJoint[handJoint.child] = handJoint;
    }

    float getJointPosition(const JointDesc& joint) const
    {
        std::unordered_map<std::string, float>::const_iterator it = jointPositions.find(joint.name);
        if (it != jointPositions.end()) {
            return it->second;
        }
        return 0.0f;
    }

    glm::mat4 computeLinkFrame(const std::string& linkName)
    {
        std::unordered_map<std::string, glm::mat4>::iterator cacheIt = linkFrameCache.find(linkName);
        if (cacheIt != linkFrameCache.end()) {
            return cacheIt->second;
        }

        std::unordered_map<std::string, JointDesc>::const_iterator jt = childToJoint.find(linkName);
        if (jt == childToJoint.end()) {
            glm::mat4 base = glm::mat4(1.0f);
            linkFrameCache[linkName] = base;
            return base;
        }

        const JointDesc& joint = jt->second;
        glm::mat4 parentFrame = computeLinkFrame(joint.parent);
        glm::mat4 local = makeOriginTransform(joint.xyz, joint.rpy);

        if (joint.type == "revolute" || joint.type == "continuous") {
            const float q = getJointPosition(joint);
            local = local * glm::rotate(glm::mat4(1.0f), q, glm::normalize(joint.axis));
        } else if (joint.type == "prismatic") {
            const float d = getJointPosition(joint);
            local = local * glm::translate(glm::mat4(1.0f), glm::normalize(joint.axis) * d);
        }

        glm::mat4 frame = parentFrame * local;

        linkFrameCache[linkName] = frame;
        return frame;
    }

public:
    RobotArmModel(const std::string& frankaDescriptionRoot, const glm::mat4& worldTransform)
        : jointPositions(defaultJointPositions())
    {
        const std::string robotsCommonDir = frankaDescriptionRoot + "/robots/common";
        const std::string robotsSimDir = frankaDescriptionRoot + "/robots/sim";
        const std::string visualMeshDir = frankaDescriptionRoot + "/meshes/visual";

        parseXacroFile(robotsCommonDir + "/panda_arm.xacro");
        parseXacroFile(robotsCommonDir + "/hand.xacro");
        parseHandAttachment(robotsSimDir + "/panda_arm_sim.urdf.xacro");

        for (std::map<std::string, LinkVisual>::const_iterator it = links.begin(); it != links.end(); ++it) {
            const std::string& linkName = it->first;
            const LinkVisual& linkVisual = it->second;
            if (linkVisual.meshFile.empty()) {
                continue;
            }

            std::unique_ptr<Model> model(new Model(visualMeshDir + "/" + linkVisual.meshFile));
            model->ignoreShadow = false;

            glm::mat4 linkFrame = computeLinkFrame(linkName);
            glm::mat4 modelMatrix = worldTransform * linkFrame * linkVisual.visualOffset;

            std::unique_ptr<Entity> entity(new Entity(model.get(), modelMatrix));

            linkModels.push_back(std::move(model));
            linkEntities.push_back(std::move(entity));
        }
    }
};

#endif