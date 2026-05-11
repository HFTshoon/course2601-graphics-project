#define GLM_ENABLE_EXPERIMENTAL
#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
#define MAX(X,Y) ((X) > (Y) ? (X) : (Y))
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "shader.h"
#include "opengl_utils.h"
#include "geometry_primitives.h"
#include <iostream>
#include <vector>
#include "camera.h"
#include "texture.h"
#include "texture_cube.h"
#include "model.h"
#include "mesh.h"
#include "scene.h"
#include "math_utils.h"
#include "light.h"
#include <algorithm>
#include <array>
#include <limits>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window, DirectionalLight* sun);

bool isWindowed = true;
bool isKeyboardDone[1024] = { 0 };

// setting
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
const unsigned int SHADOW_WIDTH = 2048;
const unsigned int SHADOW_HEIGHT = 2048;
const float planeSize = 15.f;
const unsigned int CSM_CASCADE_COUNT = 3;
const float CAMERA_NEAR_PLANE = 0.1f;
const float CAMERA_FAR_PLANE = 100.0f;

// camera
Camera camera(glm::vec3(0.0f, 0.5f, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;	// time between current frame and last frame
float lastFrame = 0.0f;

bool useNormalMap = true;
bool useShadow = true;
bool useLighting = true;
bool useCSM = true;

struct CascadeShadowMap {
    unsigned int fbo;
    unsigned int texture;
};

CascadeShadowMap createCascadeShadowMap(int width, int height)
{
    CascadeShadowMap shadowMap = {};
    glGenFramebuffers(1, &shadowMap.fbo);
    glGenTextures(1, &shadowMap.texture);
    glBindTexture(GL_TEXTURE_2D, shadowMap.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, shadowMap.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap.texture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return shadowMap;
}

std::vector<glm::vec4> getFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view)
{
    const glm::mat4 inv = glm::inverse(proj * view);
    std::vector<glm::vec4> corners;
    corners.reserve(8);

    for (unsigned int x = 0; x < 2; ++x) {
        for (unsigned int y = 0; y < 2; ++y) {
            for (unsigned int z = 0; z < 2; ++z) {
                glm::vec4 point = inv * glm::vec4(
                    2.0f * x - 1.0f,
                    2.0f * y - 1.0f,
                    2.0f * z - 1.0f,
                    1.0f
                );
                corners.push_back(point / point.w);
            }
        }
    }
    return corners;
}

glm::mat4 getCascadeLightSpaceMatrix(
    const Camera& activeCamera,
    const glm::vec3& lightDir,
    float fovDegrees,
    float aspect,
    float cascadeNear,
    float cascadeFar)
{
    const float tanHalfFov = tan(glm::radians(fovDegrees) * 0.5f);
    const float farHalfHeight = tanHalfFov * cascadeFar;
    const float farHalfWidth = farHalfHeight * aspect;
    const float depthHalfSpan = 0.5f * (cascadeFar - cascadeNear);
    float radius = sqrt(farHalfWidth * farHalfWidth + farHalfHeight * farHalfHeight + depthHalfSpan * depthHalfSpan);
    radius = std::ceil(radius * 16.0f) / 16.0f + 6.0f;

    const float splitMid = 0.5f * (cascadeNear + cascadeFar);
    glm::vec3 center = activeCamera.Position + activeCamera.Front * splitMid;

    glm::vec3 up = glm::abs(glm::dot(lightDir, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.99f
        ? glm::vec3(0.0f, 0.0f, 1.0f)
        : glm::vec3(0.0f, 1.0f, 0.0f);
    const float lightDistance = cascadeFar + radius;
    glm::vec3 lightPos = center - lightDir * lightDistance;
    glm::mat4 lightView = glm::lookAt(lightPos, center, up);
    glm::mat4 lightProjection = glm::ortho(-radius, radius, -radius, radius, 0.1f, lightDistance * 2.0f + radius);
    return lightProjection * lightView;
}

int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // uncomment this statement to fix compilation on OS X
#endif

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);

    // build and compile our shader program
    // ------------------------------------
    Shader lightingShader("../shaders/shader_lighting.vs", "../shaders/shader_lighting.fs"); // you can name your shader files however you like
    Shader shadowShader("../shaders/shadow.vs", "../shaders/shadow.fs");
    Shader skyboxShader("../shaders/shader_skybox.vs", "../shaders/shader_skybox.fs");

    Model groundModel("../resources/plane.obj");
    groundModel.diffuse = new Texture("../resources/rock_ground.png");
    groundModel.ignoreShadow = true;

    // Add entities to scene.
    Scene scene;
    glm::mat4 planeWorldTransform = glm::mat4(1.0f);
    planeWorldTransform = glm::scale(planeWorldTransform, glm::vec3(planeSize));
    planeWorldTransform = glm::translate(glm::vec3(0.0f, -0.5f, 0.0f)) * planeWorldTransform;
    scene.addEntity(new Entity(&groundModel, planeWorldTransform));

    const char* pandaLinkPaths[] = {
        "../resources/robot_arm/franka_description/meshes/visual/link0.dae",
        "../resources/robot_arm/franka_description/meshes/visual/link1.dae",
        "../resources/robot_arm/franka_description/meshes/visual/link2.dae",
        "../resources/robot_arm/franka_description/meshes/visual/link3.dae",
        "../resources/robot_arm/franka_description/meshes/visual/link4.dae",
        "../resources/robot_arm/franka_description/meshes/visual/link5.dae",
        "../resources/robot_arm/franka_description/meshes/visual/link6.dae",
        "../resources/robot_arm/franka_description/meshes/visual/link7.dae"
    };

    Model pandaLink0Model(pandaLinkPaths[0]);
    Model pandaLink1Model(pandaLinkPaths[1]);
    Model pandaLink2Model(pandaLinkPaths[2]);
    Model pandaLink3Model(pandaLinkPaths[3]);
    Model pandaLink4Model(pandaLinkPaths[4]);
    Model pandaLink5Model(pandaLinkPaths[5]);
    Model pandaLink6Model(pandaLinkPaths[6]);
    Model pandaLink7Model(pandaLinkPaths[7]);

    Model* pandaLinks[] = {
        &pandaLink0Model,
        &pandaLink1Model,
        &pandaLink2Model,
        &pandaLink3Model,
        &pandaLink4Model,
        &pandaLink5Model,
        &pandaLink6Model,
        &pandaLink7Model
    };

    for (int i = 0; i < 8; ++i) {
        std::cout << "[DEBUG] link" << i
                  << " vertices=" << pandaLinks[i]->mesh.vertices.size()
                  << ", indices=" << pandaLinks[i]->mesh.indices.size() << std::endl;
    }

    auto urdfBasis = []() {
        glm::mat4 basis(1.0f);
        basis[0] = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
        basis[1] = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
        basis[2] = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
        basis[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        return basis;
    }();

    auto urdfVecToRender = [&](const glm::vec3& value) {
        return glm::vec3(urdfBasis * glm::vec4(value, 0.0f));
    };

    auto urdfOriginToRender = [&](const glm::vec3& xyz, const glm::vec3& rpy) {
        glm::mat4 urdfRotation(1.0f);
        urdfRotation = glm::rotate(urdfRotation, rpy.z, glm::vec3(0.0f, 0.0f, 1.0f));
        urdfRotation = glm::rotate(urdfRotation, rpy.y, glm::vec3(0.0f, 1.0f, 0.0f));
        urdfRotation = glm::rotate(urdfRotation, rpy.x, glm::vec3(1.0f, 0.0f, 0.0f));

        glm::mat4 renderRotation = urdfBasis * urdfRotation * glm::transpose(urdfBasis);
        glm::mat4 transform(1.0f);
        transform = glm::translate(transform, urdfVecToRender(xyz));
        return transform * renderRotation;
    };

    struct PandaJointSpec {
        glm::vec3 xyz;
        glm::vec3 rpy;
        glm::vec3 axis;
        float initialAngle;
    };

    const PandaJointSpec pandaJoints[] = {
        { glm::vec3(0.0f, 0.0f, 0.333f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), 0.0f },
        { glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-glm::half_pi<float>(), 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), -glm::quarter_pi<float>() },
        { glm::vec3(0.0f, -0.316f, 0.0f), glm::vec3(glm::half_pi<float>(), 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), 0.0f },
        { glm::vec3(0.0825f, 0.0f, 0.0f), glm::vec3(glm::half_pi<float>(), 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), -3.0f * glm::quarter_pi<float>() },
        { glm::vec3(-0.0825f, 0.384f, 0.0f), glm::vec3(-glm::half_pi<float>(), 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), 0.0f },
        { glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(glm::half_pi<float>(), 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::half_pi<float>() },
        { glm::vec3(0.088f, 0.0f, 0.0f), glm::vec3(glm::half_pi<float>(), 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::quarter_pi<float>() }
    };

    glm::mat4 pandaBaseWorld = glm::mat4(1.0f);
    pandaBaseWorld = glm::translate(pandaBaseWorld, glm::vec3(0.0f, -0.5f, 1.0f));
    pandaBaseWorld = glm::rotate(pandaBaseWorld, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    pandaBaseWorld = glm::scale(pandaBaseWorld, glm::vec3(8.0f));
    scene.addEntity(new Entity(&pandaLink0Model, pandaBaseWorld));

    glm::mat4 currentLinkWorld = pandaBaseWorld;
    for (int jointIndex = 0; jointIndex < 7; ++jointIndex) {
        glm::mat4 jointOrigin = urdfOriginToRender(pandaJoints[jointIndex].xyz, pandaJoints[jointIndex].rpy);
        glm::vec3 jointAxis = glm::normalize(urdfVecToRender(pandaJoints[jointIndex].axis));
        glm::mat4 jointMotion = glm::rotate(glm::mat4(1.0f), pandaJoints[jointIndex].initialAngle, jointAxis);

        currentLinkWorld = currentLinkWorld * jointOrigin * jointMotion;
        scene.addEntity(new Entity(pandaLinks[jointIndex + 1], currentLinkWorld));
    }

    // define depth textures for cascaded shadow mapping
    std::array<CascadeShadowMap, CSM_CASCADE_COUNT> cascadeShadowMaps = {
        createCascadeShadowMap(SHADOW_WIDTH, SHADOW_HEIGHT),
        createCascadeShadowMap(SHADOW_WIDTH, SHADOW_HEIGHT),
        createCascadeShadowMap(SHADOW_WIDTH, SHADOW_HEIGHT)
    };

    // skybox
    std::vector<std::string> faces
    {
        "../resources/skybox/right.jpg",
        "../resources/skybox/left.jpg",
        "../resources/skybox/top.jpg",
        "../resources/skybox/bottom.jpg",
        "../resources/skybox/front.jpg",
        "../resources/skybox/back.jpg"
    };
    CubemapTexture skyboxTexture = CubemapTexture(faces);
    unsigned int VAOskybox, VBOskybox;
    getPositionVAO(skybox_positions, sizeof(skybox_positions), VAOskybox, VBOskybox);

    shadowShader.use();    

    lightingShader.use();
    lightingShader.setInt("material.diffuseSampler", 0);
    lightingShader.setInt("material.specularSampler", 1);
    lightingShader.setInt("material.normalSampler", 2);
    lightingShader.setInt("depthMapSampler0", 3);
    lightingShader.setInt("depthMapSampler1", 4);
    lightingShader.setInt("depthMapSampler2", 5);
    lightingShader.setFloat("material.shininess", 64.f);    // set shininess to constant value.

    skyboxShader.use();
    skyboxShader.setInt("skyboxTexture1", 0);


    DirectionalLight sun(30.0f, 30.0f, glm::vec3(0.8f));

    float oldTime = 0;
    while (!glfwWindowShouldClose(window))// render loop
    {
        float currentTime = glfwGetTime();
        float dt = currentTime - oldTime;
        deltaTime = dt;
        oldTime = currentTime;

        // input
        processInput(window, &sun);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // TODO : 
        // (1) render shadow map!
            // framebuffer: shadow frame buffer(depth.depthMapFBO)
            // shader : shadow.fs/vs
        // (2) render objects in the scene!
            // framebuffer : default frame buffer(0)
            // shader : shader_lighting.fs/vs
        // Iterate using map<Model*, vector<Entity*>>::iterator it = scene.entities.begin()

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, CAMERA_NEAR_PLANE, CAMERA_FAR_PLANE);
        glm::mat4 view = camera.GetViewMatrix();
        std::array<float, CSM_CASCADE_COUNT> cascadeFarPlanes = { 12.0f, 36.0f, CAMERA_FAR_PLANE };
        std::array<glm::mat4, CSM_CASCADE_COUNT> lightSpaceMatrices;
        const unsigned int activeCascadeCount = useCSM ? CSM_CASCADE_COUNT : 1;

        float cascadeNear = CAMERA_NEAR_PLANE;
        for (unsigned int cascade = 0; cascade < activeCascadeCount; ++cascade) {
            lightSpaceMatrices[cascade] = getCascadeLightSpaceMatrix(
                camera,
                sun.lightDir,
                camera.Zoom,
                (float)SCR_WIDTH / (float)SCR_HEIGHT,
                cascadeNear,
                cascadeFarPlanes[cascade]
            );
            cascadeNear = cascadeFarPlanes[cascade];
        }
        if (!useCSM) {
            lightSpaceMatrices[1] = lightSpaceMatrices[0];
            lightSpaceMatrices[2] = lightSpaceMatrices[0];
            cascadeFarPlanes[0] = CAMERA_FAR_PLANE;
            cascadeFarPlanes[1] = CAMERA_FAR_PLANE;
            cascadeFarPlanes[2] = CAMERA_FAR_PLANE;
        }

        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        shadowShader.use();
        for (unsigned int cascade = 0; cascade < activeCascadeCount; ++cascade) {
            glBindFramebuffer(GL_FRAMEBUFFER, cascadeShadowMaps[cascade].fbo);
            glClear(GL_DEPTH_BUFFER_BIT);
            shadowShader.setMat4("lightSpaceMatrix", lightSpaceMatrices[cascade]);

            for (auto it = scene.entities.begin(); it != scene.entities.end(); it++) {
                Model* model = it->first;
                if (model->ignoreShadow) continue;

                for (Entity* entity : it->second) {
                    shadowShader.setMat4("world", entity->getModelMatrix());

                    model->bind();
                    glDrawElements(GL_TRIANGLES, model->mesh.indices.size(), GL_UNSIGNED_INT, 0);
                    glBindVertexArray(0);
                }
            }
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

        lightingShader.use();
        lightingShader.setMat4("projection", projection);
        lightingShader.setMat4("view", view);
        lightingShader.setVec3("viewPos", camera.Position);
        lightingShader.setVec3("light.dir", sun.lightDir);
        lightingShader.setVec3("light.color", sun.lightColor);
        lightingShader.setFloat("useLighting", useLighting);
        lightingShader.setFloat("useShadow", useShadow);
        for (unsigned int cascade = 0; cascade < CSM_CASCADE_COUNT; ++cascade) {
            lightingShader.setMat4("lightSpaceMatrices[" + std::to_string(cascade) + "]", lightSpaceMatrices[cascade]);
            lightingShader.setFloat("cascadePlaneDistances[" + std::to_string(cascade) + "]", cascadeFarPlanes[cascade]);
        }
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, cascadeShadowMaps[0].texture);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, useCSM ? cascadeShadowMaps[1].texture : cascadeShadowMaps[0].texture);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, useCSM ? cascadeShadowMaps[2].texture : cascadeShadowMaps[0].texture);

        // iterate with map<Model*, vector<Entity*>>::iterator it = scene.entities.begin()
        for (auto it = scene.entities.begin(); it != scene.entities.end(); it++) {
            Model* model = it->first;
            std::vector<Entity*> entities = it->second;

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, model->diffuse ? model->diffuse->ID : Model::getFallbackWhiteTextureID());
            if (model->specular) {
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, model->specular->ID);
                lightingShader.setFloat("useSpecularMap", 1.0f);
            } else {
                lightingShader.setFloat("useSpecularMap", 0.0f);
            }

            if (model->normal) {
                glActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_2D, model->normal->ID);
                lightingShader.setFloat("useNormalMap", useNormalMap ? 1.0f : 0.0f);
            } else {
                lightingShader.setFloat("useNormalMap", 0.0f);
            }

            for (Entity* entity : entities) {
                lightingShader.setMat4("world", entity->getModelMatrix());

                model->bind();
                glDrawElements(GL_TRIANGLES, model->mesh.indices.size(), GL_UNSIGNED_INT, 0);
                glBindVertexArray(0);
            }
        }

        // use skybox Shader
        skyboxShader.use();
        glDepthFunc(GL_LEQUAL);
        view = glm::mat4(glm::mat3(camera.GetViewMatrix()));
        skyboxShader.setMat4("view", view);
        skyboxShader.setMat4("projection", projection);

        // render a skybox
        glBindVertexArray(VAOskybox);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexture.textureID);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        glBindVertexArray(0);
        glDepthFunc(GL_LESS);

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow* window, DirectionalLight* sun)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);


    float t = 20.0f * deltaTime;
    
    // TODO : 
    // Arrow key : increase, decrease sun's azimuth, elevation with amount of t.
    // key 1 : toggle using normal map
    // key 2 : toggle using shadow
    // key 3 : toggle using whole lighting
    // key 4 : toggle cascaded shadow mapping
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS){
        sun->processKeyboard(0, t);
        std::cout << "Azimuth: " << sun->azimuth << ", Elevation: " << sun->elevation << std::endl;
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS){
        sun->processKeyboard(0, -t);
        std::cout << "Azimuth: " << sun->azimuth << ", Elevation: " << sun->elevation << std::endl;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS){
        sun->processKeyboard(-t, 0);
        std::cout << "Azimuth: " << sun->azimuth << ", Elevation: " << sun->elevation << std::endl;
    }
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS){
        sun->processKeyboard(t, 0);
        std::cout << "Azimuth: " << sun->azimuth << ", Elevation: " << sun->elevation << std::endl;
    }

    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS && !isKeyboardDone[GLFW_KEY_1]) {
        useNormalMap = !useNormalMap;
        std::cout << "Use Normal Map: " << (useNormalMap ? "ON" : "OFF") << std::endl;
        isKeyboardDone[GLFW_KEY_1] = true;
    }
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_RELEASE) {
        isKeyboardDone[GLFW_KEY_1] = false;
    }

    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS && !isKeyboardDone[GLFW_KEY_2]) {
        useShadow = !useShadow;
        std::cout << "Use Shadow: " << (useShadow ? "ON" : "OFF") << std::endl;
        isKeyboardDone[GLFW_KEY_2] = true;
    }
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_RELEASE) {
        isKeyboardDone[GLFW_KEY_2] = false;
    }

    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS && !isKeyboardDone[GLFW_KEY_3]) {
        useLighting = !useLighting;
        std::cout << "Use Lighting: " << (useLighting ? "ON" : "OFF") << std::endl;
        isKeyboardDone[GLFW_KEY_3] = true;
    }
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_RELEASE) {
        isKeyboardDone[GLFW_KEY_3] = false;
    }

    if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS && !isKeyboardDone[GLFW_KEY_4]) {
        useCSM = !useCSM;
        std::cout << "Shadow Mode: " << (useCSM ? "CSM (3 cascades)" : "Single shadow map") << std::endl;
        isKeyboardDone[GLFW_KEY_4] = true;
    }
    if (glfwGetKey(window, GLFW_KEY_4) == GLFW_RELEASE) {
        isKeyboardDone[GLFW_KEY_4] = false;
    }
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}


// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(yoffset);
}
