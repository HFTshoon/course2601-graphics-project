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
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window, DirectionalLight* sun);

bool isWindowed = true;
bool isKeyboardDone[1024] = { 0 };

// setting
const unsigned int SCR_WIDTH = 1600;
const unsigned int SCR_HEIGHT = 900;
const unsigned int SHADOW_WIDTH = 2048;
const unsigned int SHADOW_HEIGHT = 2048;
const float planeSize = 15.f;
const unsigned int CSM_CASCADE_COUNT = 3;
const float CAMERA_NEAR_PLANE = 0.1f;
const float CAMERA_FAR_PLANE = 100.0f;

// camera — isometric-like: left-front-top angle looking at robot arm
Camera camera(glm::vec3(-2.38529f, 0.853243f, 2.26314f), glm::vec3(0.0f, 1.0f, 0.0f), -39.1f, -13.2f);
// Camera camera(glm::vec3(-2.5f, 2.0f, 3.5f), glm::vec3(0.0f, 1.0f, 0.0f), -45.0f, -23.0f);
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
bool imguiMode = true;  // start with mouse cursor visible for ImGui

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

    // Mouse cursor: start visible so ImGui is immediately usable (Tab toggles camera mode)
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    // ImGui setup
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

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
    Model pandaHandModel("../resources/robot_arm/franka_description/meshes/visual/hand.dae");
    Model pandaFingerModel("../resources/robot_arm/franka_description/meshes/visual/finger.dae");

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
        float lower;       // joint limit lower (rad)
        float upper;       // joint limit upper (rad)
        float initialAngle;
    };

    // Joint specs from panda_arm.xacro
    const PandaJointSpec pandaJoints[] = {
        // joint1: xyz=(0,0,0.333), rpy=(0,0,0), axis=(0,0,1), lower=-2.8973, upper=2.8973
        { glm::vec3(0.0f, 0.0f, 0.333f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), -2.8973f, 2.8973f, 0.0f },
        // joint2: xyz=(0,0,0), rpy=(-pi/2,0,0), axis=(0,0,1), lower=-1.7628, upper=1.7628
        { glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-glm::half_pi<float>(), 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), -1.7628f, 1.7628f, -glm::quarter_pi<float>() },
        // joint3: xyz=(0,-0.316,0), rpy=(pi/2,0,0), axis=(0,0,1), lower=-2.8973, upper=2.8973
        { glm::vec3(0.0f, -0.316f, 0.0f), glm::vec3(glm::half_pi<float>(), 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), -2.8973f, 2.8973f, 0.0f },
        // joint4: xyz=(0.0825,0,0), rpy=(pi/2,0,0), axis=(0,0,1), lower=-3.0718, upper=-0.0698
        { glm::vec3(0.0825f, 0.0f, 0.0f), glm::vec3(glm::half_pi<float>(), 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), -3.0718f, -0.0698f, -3.0f * glm::quarter_pi<float>() },
        // joint5: xyz=(-0.0825,0.384,0), rpy=(-pi/2,0,0), axis=(0,0,1), lower=-2.8973, upper=2.8973
        { glm::vec3(-0.0825f, 0.384f, 0.0f), glm::vec3(-glm::half_pi<float>(), 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), -2.8973f, 2.8973f, 0.0f },
        // joint6: xyz=(0,0,0), rpy=(pi/2,0,0), axis=(0,0,1), lower=-0.0175, upper=3.7525
        { glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(glm::half_pi<float>(), 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), -0.0175f, 3.7525f, glm::half_pi<float>() },
        // joint7: xyz=(0.088,0,0), rpy=(pi/2,0,0), axis=(0,0,1), lower=-2.8973, upper=2.8973
        { glm::vec3(0.088f, 0.0f, 0.0f), glm::vec3(glm::half_pi<float>(), 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), -2.8973f, 2.8973f, glm::quarter_pi<float>() }
    };

    // Gripper opening (finger_joint1 = finger_joint2, prismatic, range 0..0.04)
    float gripperOpening = 0.02f;  // half open by default

    // Dynamic joint angles (start at initial positions)
    float jointAngles[7] = {
        pandaJoints[0].initialAngle,
        pandaJoints[1].initialAngle,
        pandaJoints[2].initialAngle,
        pandaJoints[3].initialAngle,
        pandaJoints[4].initialAngle,
        pandaJoints[5].initialAngle,
        pandaJoints[6].initialAngle,
    };

    glm::mat4 pandaBaseWorld = glm::mat4(1.0f);
    pandaBaseWorld = glm::translate(pandaBaseWorld, glm::vec3(0.0f, -0.5f, 1.0f));
    pandaBaseWorld = glm::rotate(pandaBaseWorld, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    pandaBaseWorld = glm::scale(pandaBaseWorld, glm::vec3(2.0f));

    // Entities for robot arm links (rebuilt each frame)
    Entity* pandaLinkEntities[8];
    pandaLinkEntities[0] = new Entity(&pandaLink0Model, pandaBaseWorld);
    for (int i = 1; i < 8; ++i)
        pandaLinkEntities[i] = new Entity(pandaLinks[i], glm::mat4(1.0f));

    // Hand and finger entities
    Entity* handEntity      = new Entity(&pandaHandModel,   glm::mat4(1.0f));
    Entity* leftFingerEntity  = new Entity(&pandaFingerModel, glm::mat4(1.0f));
    Entity* rightFingerEntity = new Entity(&pandaFingerModel, glm::mat4(1.0f));

    scene.addEntity(pandaLinkEntities[0]);
    for (int i = 1; i < 8; ++i)
        scene.addEntity(pandaLinkEntities[i]);
    scene.addEntity(handEntity);
    scene.addEntity(leftFingerEntity);
    scene.addEntity(rightFingerEntity);

    // Helper: rebuild link transforms from current joint angles
    auto rebuildPandaTransforms = [&]() {
        glm::mat4 cur = pandaBaseWorld;
        for (int j = 0; j < 7; ++j) {
            glm::mat4 origin = urdfOriginToRender(pandaJoints[j].xyz, pandaJoints[j].rpy);
            glm::vec3 axis = glm::normalize(urdfVecToRender(pandaJoints[j].axis));
            glm::mat4 motion = glm::rotate(glm::mat4(1.0f), jointAngles[j], axis);
            cur = cur * origin * motion;
            pandaLinkEntities[j + 1]->modelMatrix = cur;
        }
        // joint8 (fixed): xyz=(0,0,0.107), rpy=(0,0,0)
        glm::mat4 joint8 = urdfOriginToRender(glm::vec3(0.0f, 0.0f, 0.107f), glm::vec3(0.0f));
        // hand_joint (fixed): xyz=(0,0,0), rpy=(0,0,-pi/4)
        glm::mat4 handJoint = urdfOriginToRender(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -glm::quarter_pi<float>()));
        glm::mat4 handWorld = cur * joint8 * handJoint;
        handEntity->modelMatrix = handWorld;

        // finger_joint1/2: xyz=(0,0,0.0584), prismatic along ±y
        glm::mat4 fingerBase = urdfOriginToRender(glm::vec3(0.0f, 0.0f, 0.0584f), glm::vec3(0.0f));
        glm::vec3 fingerAxisL = glm::normalize(urdfVecToRender(glm::vec3(0.0f, 1.0f, 0.0f)));
        glm::vec3 fingerAxisR = glm::normalize(urdfVecToRender(glm::vec3(0.0f, -1.0f, 0.0f)));
        glm::mat4 leftFingerMotion  = glm::translate(glm::mat4(1.0f),  fingerAxisL * gripperOpening);
        glm::mat4 rightFingerMotion = glm::translate(glm::mat4(1.0f),  fingerAxisR * gripperOpening);
        // rightfinger visual is rotated pi around Z in URDF
        glm::mat4 rightFingerVisual = urdfOriginToRender(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, glm::pi<float>()));
        leftFingerEntity->modelMatrix  = handWorld * fingerBase * leftFingerMotion;
        rightFingerEntity->modelMatrix = handWorld * fingerBase * rightFingerMotion * rightFingerVisual;
    };
    rebuildPandaTransforms();

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

        // ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Joint control window
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(500, 290), ImGuiCond_Once);
        ImGui::Begin("Franka Panda Joint Control");
        bool jointChanged = false;
        for (int j = 0; j < 7; ++j) {
            char label[32];
            snprintf(label, sizeof(label), "Joint %d [%.2f, %.2f]", j + 1,
                     pandaJoints[j].lower, pandaJoints[j].upper);
            if (ImGui::SliderFloat(label, &jointAngles[j], pandaJoints[j].lower, pandaJoints[j].upper))
                jointChanged = true;
        }
        if (ImGui::SliderFloat("Gripper [0.00, 0.04]", &gripperOpening, 0.0f, 0.04f))
            jointChanged = true;
        if (ImGui::Button("Reset")) {
            for (int j = 0; j < 7; ++j)
                jointAngles[j] = pandaJoints[j].initialAngle;
            gripperOpening = 0.02f;
            jointChanged = true;
        }
        ImGui::End();

        if (jointChanged)
            rebuildPandaTransforms();

        // Tab: toggle between camera mode and ImGui mode
        if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS && !isKeyboardDone[GLFW_KEY_TAB]) {
            imguiMode = !imguiMode;
            glfwSetInputMode(window, GLFW_CURSOR, imguiMode ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
            firstMouse = true;
            isKeyboardDone[GLFW_KEY_TAB] = true;
        }
        if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_RELEASE)
            isKeyboardDone[GLFW_KEY_TAB] = false;

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

        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

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

    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS && !isKeyboardDone[GLFW_KEY_P]) {
        std::cout << "Camera camera(glm::vec3("
                  << camera.Position.x << "f, "
                  << camera.Position.y << "f, "
                  << camera.Position.z << "f), glm::vec3(0.0f, 1.0f, 0.0f), "
                  << camera.Yaw << "f, "
                  << camera.Pitch << "f);" << std::endl;
        isKeyboardDone[GLFW_KEY_P] = true;
    }
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_RELEASE)
        isKeyboardDone[GLFW_KEY_P] = false;

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
    if (imguiMode) {
        // In ImGui mode: don't rotate camera
        lastX = xpos;
        lastY = ypos;
        return;
    }

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
