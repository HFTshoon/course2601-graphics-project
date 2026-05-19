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
#include "robot_kinematics.h"
#include "ik_solver.h"
#include "trajectory_tracker.h"
#include "waypoint.h"
#include "handwriting_path_generator.h"
#include "stroke_renderer.h"
#include "pen_preset.h"
#include "paper_preset.h"
#include "hershey_path_loader.h"
#include "hershey_glyph_library.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
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
const unsigned int SCR_WIDTH = 3200;
const unsigned int SCR_HEIGHT = 1800;
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
    Model paperModel("../resources/plane.obj");
    paperModel.ignoreShadow = true;

    float floorY = 0.0f;
    float paperThicknessOffset = 0.001f;
    bool lockPaperToFloor = true;

    // Add entities to scene.
    Scene scene;
    auto makeGroundPlaneTransform = [&]() {
        glm::mat4 planeWorldTransform = glm::mat4(1.0f);
        planeWorldTransform = glm::scale(planeWorldTransform, glm::vec3(planeSize));
        planeWorldTransform = glm::translate(glm::vec3(0.0f, floorY, 0.0f)) * planeWorldTransform;
        return planeWorldTransform;
    };
    Entity* groundEntity = new Entity(&groundModel, makeGroundPlaneTransform());
    scene.addEntity(groundEntity);
    auto updateGroundPlane = [&]() {
        groundEntity->modelMatrix = makeGroundPlaneTransform();
    };

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

    // Gripper opening (finger_joint1 = finger_joint2, prismatic, range 0..0.04)
    float gripperOpening = 0.02f;  // half open by default
    RobotKinematics robotKinematics;
    IKSolver ikSolver;
    bool enableIK = false;
    glm::vec3 ikTarget = robotKinematics.getToolTipPosition();
    bool enableWaypointPlayback = false;
    const glm::vec3 initialToolTipPosition = robotKinematics.getToolTipPosition();
    std::vector<Waypoint> testWaypoints;
    testWaypoints.push_back(Waypoint(initialToolTipPosition, false));
    testWaypoints.push_back(Waypoint(initialToolTipPosition + glm::vec3(0.10f, 0.00f, 0.00f), false));
    testWaypoints.push_back(Waypoint(initialToolTipPosition + glm::vec3(0.10f, -0.06f, 0.05f), true));
    testWaypoints.push_back(Waypoint(initialToolTipPosition + glm::vec3(-0.02f, -0.06f, 0.08f), true));
    testWaypoints.push_back(Waypoint(initialToolTipPosition + glm::vec3(-0.02f, 0.04f, 0.08f), false));
    HandwritingPathGenerator handwritingGenerator;
    HandwritingPathGenerator::Options handwritingOptions;
    handwritingOptions.paperOrigin = glm::vec3(
        initialToolTipPosition.x,
        0.0f,
        initialToolTipPosition.z
    );
    handwritingOptions.scale = 0.15f;
    handwritingOptions.paperY = floorY + paperThicknessOffset;
    handwritingOptions.liftHeight = 0.05f;
    handwritingOptions.sampleSpacing = 0.01f;
    handwritingOptions.useSpline = true;
    int pathMode = 4;
    const char* pathModeItems[] = {
        "Test Waypoints",
        "Lowercase a",
        "Text Input Fallback",
        "Hershey JSON",
        "Hershey Glyph Library Text"
    };
    char handwritingTextInput[128] = "abc";
    char hersheyJsonPath[256] = "../assets/paths/example_hershey_text.json";
    char glyphLibraryPath[256] = "../assets/fonts/hershey_futural_glyphs.json";
    char glyphLibraryTextInput[128] = "hello";
    int unsupportedCharacterCount = 0;
    HersheyPathLoader hersheyPathLoader;
    HersheyPathLoader::LoadedPath loadedHersheyPath;
    bool hasLoadedHersheyPath = false;
    std::string hersheyLoadError;
    int loadedHersheyWaypointCount = 0;
    HersheyGlyphLibrary hersheyGlyphLibrary;
    bool hasLoadedGlyphLibrary = false;
    std::string glyphLibraryLoadError;
    std::string loadedGlyphLibraryPath;
    std::string glyphLibraryUnsupportedCharacters;
    int glyphLibraryUnsupportedCount = 0;
    int glyphLibraryWaypointCount = 0;
    const int glyphLibraryWaypointWarningThreshold = 5000;
    const size_t glyphLibraryTextLengthWarningThreshold = 64;
    std::vector<Waypoint> activeWaypoints = testWaypoints;
    TrajectoryTracker trajectoryTracker;
    trajectoryTracker.setWaypoints(activeWaypoints);
    trajectoryTracker.reset(initialToolTipPosition);
    StrokeRenderer strokeRenderer;
    bool enableStrokeRendering = false;
    std::vector<PenPreset> penPresets = createDefaultPenPresets();
    std::vector<const char*> penPresetNames;
    for (size_t presetIndex = 0; presetIndex < penPresets.size(); ++presetIndex) {
        penPresetNames.push_back(penPresets[presetIndex].name.c_str());
    }
    int selectedPenPreset = penPresets.size() > 1 ? 1 : 0;
    struct BrushAsset {
        std::string fileName;
        std::string texturePath;
        int width = 0;
        int height = 0;
        int channels = 0;
        bool exists = false;
    };
    auto brushFileNameFromPath = [](const std::string& path) {
        const size_t slash = path.find_last_of("/\\");
        if (slash == std::string::npos) {
            return path;
        }
        return path.substr(slash + 1);
    };
    const char* brushFileNames[] = {
        "basic_circle.png",
        "blob.png",
        "chalk.png",
        "dot.png",
        "oil.png",
        "rakchalk.png",
        "rock.png",
        "round.png",
        "square.png"
    };
    std::vector<BrushAsset> brushAssets;
    std::vector<const char*> brushAssetNames;
    for (size_t brushIndex = 0; brushIndex < sizeof(brushFileNames) / sizeof(brushFileNames[0]); ++brushIndex) {
        BrushAsset asset;
        asset.fileName = brushFileNames[brushIndex];
        asset.texturePath = "../assets/brushes/" + asset.fileName;
        asset.exists = stbi_info(
            asset.texturePath.c_str(),
            &asset.width,
            &asset.height,
            &asset.channels
        ) != 0;
        brushAssets.push_back(asset);
    }
    for (size_t brushIndex = 0; brushIndex < brushAssets.size(); ++brushIndex) {
        brushAssetNames.push_back(brushAssets[brushIndex].fileName.c_str());
    }
    auto findBrushAssetIndexForPath = [&](const std::string& texturePath) {
        const std::string fileName = brushFileNameFromPath(texturePath);
        for (size_t brushIndex = 0; brushIndex < brushAssets.size(); ++brushIndex) {
            if (brushAssets[brushIndex].texturePath == texturePath ||
                brushAssets[brushIndex].fileName == fileName) {
                return static_cast<int>(brushIndex);
            }
        }
        return 0;
    };
    int selectedBrushImage = !penPresets.empty()
        ? findBrushAssetIndexForPath(penPresets[selectedPenPreset].brushTexturePath)
        : 0;
    bool brushImageOverrideActive = false;
    auto applyPenPreset = [&](const PenPreset& preset, int brushIndex) {
        std::string brushTexturePath = preset.brushTexturePath;
        if (brushIndex >= 0 && brushIndex < static_cast<int>(brushAssets.size())) {
            brushTexturePath = brushAssets[brushIndex].texturePath;
        }
        strokeRenderer.setBrushTexturePath(brushTexturePath);
        strokeRenderer.setStrokeColor(preset.color);
        strokeRenderer.setBrushSize(preset.brushSize);
        strokeRenderer.setOpacity(preset.opacity);
        strokeRenderer.setStampSpacing(preset.stampSpacing);
    };
    if (!penPresets.empty()) {
        applyPenPreset(penPresets[selectedPenPreset], selectedBrushImage);
    }
    std::vector<PaperPreset> paperPresets = createDefaultPaperPresets();
    std::vector<const char*> paperPresetNames;
    struct PaperTextureBinding {
        Texture* texture = NULL;
        std::string loadedAlbedoPath;
        std::string fallbackAlbedoPath;
        bool structuredAlbedoExists = false;
        bool fallbackAlbedoExists = false;
        bool normalExists = false;
        bool roughnessExists = false;
        bool usingStructuredAlbedo = false;
        bool usingFallbackAlbedo = false;
        bool usingBaseColor = false;
    };
    auto fileExists = [](const std::string& path) {
        std::ifstream file(path.c_str(), std::ios::binary);
        return file.good();
    };
    auto createSolidColorTexture = [](const glm::vec3& color) {
        Texture* texture = new Texture();
        texture->width = 1;
        texture->height = 1;
        texture->channels = 4;
        const unsigned char pixel[4] = {
            static_cast<unsigned char>(std::max(0.0f, std::min(1.0f, color.r)) * 255.0f),
            static_cast<unsigned char>(std::max(0.0f, std::min(1.0f, color.g)) * 255.0f),
            static_cast<unsigned char>(std::max(0.0f, std::min(1.0f, color.b)) * 255.0f),
            255
        };
        glGenTextures(1, &texture->ID);
        glBindTexture(GL_TEXTURE_2D, texture->ID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        return texture;
    };
    auto getPlaceholderPaperPath = [](const std::string& presetName) {
        if (presetName == "Rough Paper") {
            return std::string("../assets/papers/rough_paper.png");
        }
        if (presetName == "Recycled Paper") {
            return std::string("../assets/papers/recycled_paper.png");
        }
        return std::string("../assets/papers/smooth_paper.png");
    };
    auto createPaperTextureBinding = [&](const PaperPreset& preset) {
        PaperTextureBinding binding;
        binding.fallbackAlbedoPath = getPlaceholderPaperPath(preset.name);
        binding.structuredAlbedoExists = fileExists(preset.albedoTexturePath);
        binding.fallbackAlbedoExists = fileExists(binding.fallbackAlbedoPath);
        binding.normalExists = fileExists(preset.normalTexturePath);
        binding.roughnessExists = fileExists(preset.roughnessTexturePath);

        if (binding.structuredAlbedoExists) {
            binding.texture = new Texture(preset.albedoTexturePath.c_str());
            binding.loadedAlbedoPath = preset.albedoTexturePath;
            binding.usingStructuredAlbedo = true;
        } else if (binding.fallbackAlbedoExists) {
            binding.texture = new Texture(binding.fallbackAlbedoPath.c_str());
            binding.loadedAlbedoPath = binding.fallbackAlbedoPath;
            binding.usingFallbackAlbedo = true;
        } else {
            binding.texture = createSolidColorTexture(preset.baseColor);
            binding.loadedAlbedoPath = "baseColor fallback";
            binding.usingBaseColor = true;
        }

        return binding;
    };
    std::vector<PaperTextureBinding> paperTextureBindings;
    for (size_t presetIndex = 0; presetIndex < paperPresets.size(); ++presetIndex) {
        paperPresetNames.push_back(paperPresets[presetIndex].name.c_str());
        paperTextureBindings.push_back(createPaperTextureBinding(paperPresets[presetIndex]));
    }
    int selectedPaperPreset = 0;
    PaperPreset activePaperPreset = paperPresets.empty() ? PaperPreset() : paperPresets[selectedPaperPreset];
    auto applyPaperInfluence = [&]() {
        const float effectiveOpacityMultiplier =
            activePaperPreset.strokeOpacityMultiplier * (1.0f - activePaperPreset.absorbency * 0.05f);
        const float effectiveStampSizeMultiplier =
            activePaperPreset.stampSizeMultiplier * (1.0f + activePaperPreset.absorbency * 0.04f);
        const float effectiveEdgeNoiseStrength =
            std::min(1.0f, activePaperPreset.edgeNoiseStrength + activePaperPreset.roughness * 0.08f);
        strokeRenderer.setPaperInfluence(
            effectiveOpacityMultiplier,
            effectiveStampSizeMultiplier,
            effectiveEdgeNoiseStrength,
            activePaperPreset.fiberNoise
        );
    };
    auto applyPaperPreset = [&](int presetIndex) {
        if (presetIndex < 0 || presetIndex >= static_cast<int>(paperPresets.size())) {
            return;
        }
        selectedPaperPreset = presetIndex;
        activePaperPreset = paperPresets[selectedPaperPreset];
        if (selectedPaperPreset < static_cast<int>(paperTextureBindings.size())) {
            paperModel.diffuse = paperTextureBindings[selectedPaperPreset].texture;
        }
        applyPaperInfluence();
    };
    if (!paperPresets.empty()) {
        applyPaperPreset(selectedPaperPreset);
    }
    const char* strokeRenderModeItems[] = { "Line Strip", "Image Brush Stamp" };
    strokeRenderer.setRenderMode(StrokeRenderMode::ImageBrushStamp);
    Entity* paperEntity = new Entity(&paperModel, glm::mat4(1.0f));
    scene.addEntity(paperEntity);
    auto syncPaperYToFloor = [&]() {
        if (lockPaperToFloor) {
            handwritingOptions.paperY = floorY + paperThicknessOffset;
        }
    };
    auto updatePaperPlane = [&]() {
        syncPaperYToFloor();
        paperEntity->modelMatrix =
            glm::translate(glm::vec3(
                handwritingOptions.paperOrigin.x,
                handwritingOptions.paperY,
                handwritingOptions.paperOrigin.z
            ))
            * glm::scale(glm::vec3(0.75f, 1.0f, 0.55f));
        strokeRenderer.setPaperY(handwritingOptions.paperY);
    };
    auto getTrajectoryPenDown = [&]() {
        return enableWaypointPlayback
            && !trajectoryTracker.isFinished()
            && trajectoryTracker.getWaypointCount() > 0
            && trajectoryTracker.getCurrentWaypointIndex() < trajectoryTracker.getWaypointCount()
            && trajectoryTracker.getCurrentWaypoint().penDown;
    };
    auto generateGlyphLibraryTextPath = [&]() {
        glyphLibraryUnsupportedCount = 0;
        glyphLibraryUnsupportedCharacters.clear();
        glyphLibraryWaypointCount = 0;

        if (!hasLoadedGlyphLibrary) {
            activeWaypoints.clear();
            glyphLibraryLoadError = "Load a glyph library before generating text.";
            return false;
        }
        if (std::strlen(glyphLibraryTextInput) == 0) {
            activeWaypoints.clear();
            glyphLibraryLoadError = "Glyph library text is empty.";
            return false;
        }

        activeWaypoints = hersheyGlyphLibrary.generateWaypointsForText(
            glyphLibraryTextInput,
            handwritingOptions,
            &glyphLibraryUnsupportedCount,
            &glyphLibraryUnsupportedCharacters
        );
        glyphLibraryWaypointCount = static_cast<int>(activeWaypoints.size());
        glyphLibraryLoadError.clear();
        return true;
    };
    auto loadGlyphLibraryFromPath = [&]() {
        std::string loadError;
        if (hersheyGlyphLibrary.loadFromJson(glyphLibraryPath, &loadError)) {
            hasLoadedGlyphLibrary = true;
            loadedGlyphLibraryPath = glyphLibraryPath;
            glyphLibraryLoadError.clear();
            return true;
        }

        hasLoadedGlyphLibrary = false;
        loadedGlyphLibraryPath.clear();
        glyphLibraryLoadError = loadError;
        glyphLibraryUnsupportedCount = 0;
        glyphLibraryUnsupportedCharacters.clear();
        glyphLibraryWaypointCount = 0;
        return false;
    };
    auto reloadActivePath = [&]() {
        syncPaperYToFloor();
        if (pathMode == 0) {
            activeWaypoints = testWaypoints;
            unsupportedCharacterCount = 0;
            loadedHersheyWaypointCount = 0;
            glyphLibraryWaypointCount = 0;
            glyphLibraryUnsupportedCount = 0;
        } else if (pathMode == 1) {
            activeWaypoints = handwritingGenerator.generateLowercaseA(handwritingOptions);
            unsupportedCharacterCount = 0;
            loadedHersheyWaypointCount = 0;
            glyphLibraryWaypointCount = 0;
            glyphLibraryUnsupportedCount = 0;
        } else if (pathMode == 2) {
            activeWaypoints = handwritingGenerator.generateText(handwritingTextInput, handwritingOptions);
            unsupportedCharacterCount = handwritingGenerator.getLastUnsupportedCharacterCount();
            loadedHersheyWaypointCount = 0;
            glyphLibraryWaypointCount = 0;
            glyphLibraryUnsupportedCount = 0;
        } else if (pathMode == 3) {
            unsupportedCharacterCount = 0;
            glyphLibraryWaypointCount = 0;
            glyphLibraryUnsupportedCount = 0;
            if (hasLoadedHersheyPath) {
                activeWaypoints = hersheyPathLoader.generateWaypoints(loadedHersheyPath, handwritingOptions);
                loadedHersheyWaypointCount = static_cast<int>(activeWaypoints.size());
            } else {
                activeWaypoints.clear();
                loadedHersheyWaypointCount = 0;
                if (hersheyLoadError.empty()) {
                    hersheyLoadError = "No Hershey JSON loaded yet.";
                }
            }
        } else {
            unsupportedCharacterCount = 0;
            loadedHersheyWaypointCount = 0;
            if (!generateGlyphLibraryTextPath() && glyphLibraryLoadError.empty()) {
                glyphLibraryLoadError = "No Hershey glyph library loaded yet.";
            }
        }
        trajectoryTracker.setWaypoints(activeWaypoints);
        trajectoryTracker.reset(robotKinematics.getToolTipPosition());
        enableWaypointPlayback = false;
    };
    updatePaperPlane();
    loadGlyphLibraryFromPath();
    reloadActivePath();
    const std::array<float, RobotKinematics::DOF> initialJointAngles = robotKinematics.getJointAngles();
    const std::vector<glm::mat4>& initialLinkTransforms = robotKinematics.getLinkWorldTransforms();

    // Entities for robot arm links (rebuilt each frame)
    Entity* pandaLinkEntities[8];
    pandaLinkEntities[0] = new Entity(&pandaLink0Model, initialLinkTransforms[0]);
    for (int i = 1; i < 8; ++i)
        pandaLinkEntities[i] = new Entity(pandaLinks[i], initialLinkTransforms[i]);

    // Hand and finger entities
    Entity* handEntity      = new Entity(&pandaHandModel,   robotKinematics.getEndEffectorTransform());
    Entity* leftFingerEntity  = new Entity(&pandaFingerModel, glm::mat4(1.0f));
    Entity* rightFingerEntity = new Entity(&pandaFingerModel, glm::mat4(1.0f));

    scene.addEntity(pandaLinkEntities[0]);
    for (int i = 1; i < 8; ++i)
        scene.addEntity(pandaLinkEntities[i]);
    scene.addEntity(handEntity);
    scene.addEntity(leftFingerEntity);
    scene.addEntity(rightFingerEntity);

    // Helper: copy kinematics output into the existing render entities.
    auto applyPandaTransforms = [&]() {
        const std::vector<glm::mat4>& linkTransforms = robotKinematics.getLinkWorldTransforms();
        for (int linkIndex = 0; linkIndex < 8; ++linkIndex) {
            pandaLinkEntities[linkIndex]->modelMatrix = linkTransforms[linkIndex];
        }

        const glm::mat4 handWorld = robotKinematics.getEndEffectorTransform();
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
    applyPandaTransforms();

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
        if (lockPaperToFloor && handwritingOptions.paperY != floorY + paperThicknessOffset) {
            updatePaperPlane();
        }

        // Joint control window
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(560, 860), ImGuiCond_Once);
        ImGui::Begin("Franka Panda Joint Control");
        bool jointChanged = false;
        for (int j = 0; j < RobotKinematics::DOF; ++j) {
            char label[32];
            snprintf(label, sizeof(label), "Joint %d [%.2f, %.2f]", j + 1,
                     robotKinematics.getJointLowerLimit(j), robotKinematics.getJointUpperLimit(j));
            float jointAngle = robotKinematics.getJointAngle(j);
            if (ImGui::SliderFloat(label, &jointAngle, robotKinematics.getJointLowerLimit(j), robotKinematics.getJointUpperLimit(j))) {
                robotKinematics.setJointAngle(j, jointAngle);
                jointChanged = true;
            }
        }
        if (ImGui::SliderFloat("Gripper [0.00, 0.04]", &gripperOpening, 0.0f, 0.04f))
            jointChanged = true;
        if (ImGui::Button("Reset")) {
            robotKinematics.setJointAngles(initialJointAngles);
            gripperOpening = 0.02f;
            jointChanged = true;
        }

        if (jointChanged) {
            robotKinematics.rebuildForwardKinematics();
            applyPandaTransforms();
        }

        ImGui::Separator();
        ImGui::Checkbox("Enable IK", &enableIK);
        ImGui::SliderFloat("Target X", &ikTarget.x, -3.0f, 3.0f);
        ImGui::SliderFloat("Target Y", &ikTarget.y, -3.0f, 3.0f);
        ImGui::SliderFloat("Target Z", &ikTarget.z, -3.0f, 3.0f);
        if (ImGui::Button("Reset Target")) {
            ikTarget = robotKinematics.getToolTipPosition();
        }

        ImGui::Separator();
        if (ImGui::Combo("Path Mode", &pathMode, pathModeItems, 5)) {
            reloadActivePath();
        }
        if (ImGui::Checkbox("Use Spline", &handwritingOptions.useSpline)) {
            reloadActivePath();
        }
        if (ImGui::SliderFloat("Glyph Scale", &handwritingOptions.scale, 0.02f, 0.30f)) {
            reloadActivePath();
        }
        if (ImGui::SliderFloat("Sample Spacing", &handwritingOptions.sampleSpacing, 0.005f, 0.030f)) {
            reloadActivePath();
        }
        if (pathMode == 2) {
            ImGui::InputText("Text Input", handwritingTextInput, sizeof(handwritingTextInput));
        }
        if (pathMode == 2 || pathMode == 4) {
            if (ImGui::SliderFloat("Character Spacing", &handwritingOptions.characterSpacing, 0.0f, 0.12f)) {
                reloadActivePath();
            }
            if (ImGui::SliderFloat("Word Spacing", &handwritingOptions.wordSpacing, 0.03f, 0.40f)) {
                reloadActivePath();
            }
        }
        if (pathMode == 2) {
            if (ImGui::Button("Generate Text Path")) {
                reloadActivePath();
            }
            ImGui::Text("Current Text: %s", handwritingTextInput);
            ImGui::Text("Supported Glyphs: a, b, c, space");
            ImGui::Text("Unsupported Character Count: %d", unsupportedCharacterCount);
        }
        if (pathMode == 3) {
            ImGui::InputText("Hershey JSON Path", hersheyJsonPath, sizeof(hersheyJsonPath));
            if (ImGui::Button("Load Hershey JSON")) {
                HersheyPathLoader::LoadedPath loadedPath;
                std::string loadError;
                if (hersheyPathLoader.loadFromJson(hersheyJsonPath, loadedPath, &loadError)) {
                    loadedHersheyPath = loadedPath;
                    hasLoadedHersheyPath = true;
                    hersheyLoadError.clear();
                    activeWaypoints = hersheyPathLoader.generateWaypoints(loadedHersheyPath, handwritingOptions);
                    loadedHersheyWaypointCount = static_cast<int>(activeWaypoints.size());
                    trajectoryTracker.setWaypoints(activeWaypoints);
                    trajectoryTracker.reset(robotKinematics.getToolTipPosition());
                    enableWaypointPlayback = false;
                } else {
                    hasLoadedHersheyPath = false;
                    hersheyLoadError = loadError;
                    loadedHersheyWaypointCount = 0;
                }
            }
            ImGui::Text("Current Loaded Text: %s", hasLoadedHersheyPath ? loadedHersheyPath.text.c_str() : "none");
            ImGui::Text("Current Loaded Font: %s", hasLoadedHersheyPath ? loadedHersheyPath.font.c_str() : "none");
            ImGui::Text("Loaded Stroke Count: %d", hasLoadedHersheyPath ? static_cast<int>(loadedHersheyPath.strokes.size()) : 0);
            ImGui::Text("Loaded Hershey Waypoint Count: %d", loadedHersheyWaypointCount);
            if (!hersheyLoadError.empty()) {
                ImGui::Text("JSON Load Error: %s", hersheyLoadError.c_str());
            }
            ImGui::Text("Command hint:");
            ImGui::TextWrapped("cd robot_arm && python scripts/generate_hershey_path.py --text \"hello\" --font futural --output assets/paths/hershey_text.json");
        }
        if (pathMode == 4) {
            ImGui::InputText("Glyph Library Path", glyphLibraryPath, sizeof(glyphLibraryPath));
            if (ImGui::Button("Load Glyph Library")) {
                if (loadGlyphLibraryFromPath() && generateGlyphLibraryTextPath()) {
                    trajectoryTracker.setWaypoints(activeWaypoints);
                    trajectoryTracker.reset(robotKinematics.getToolTipPosition());
                    enableWaypointPlayback = false;
                }
            }
            ImGui::InputText("Glyph Library Text", glyphLibraryTextInput, sizeof(glyphLibraryTextInput));
            if (ImGui::Button("Generate Glyph Library Text Path")) {
                if (generateGlyphLibraryTextPath()) {
                    trajectoryTracker.setWaypoints(activeWaypoints);
                    trajectoryTracker.reset(robotKinematics.getToolTipPosition());
                    enableWaypointPlayback = false;
                }
            }
            if (ImGui::Button("Reset Glyph Demo")) {
                std::snprintf(glyphLibraryPath, sizeof(glyphLibraryPath), "%s", "../assets/fonts/hershey_futural_glyphs.json");
                std::snprintf(glyphLibraryTextInput, sizeof(glyphLibraryTextInput), "%s", "hello");
                handwritingOptions.useSpline = true;
                handwritingOptions.scale = 0.15f;
                handwritingOptions.sampleSpacing = 0.01f;
                handwritingOptions.characterSpacing = 0.035f;
                handwritingOptions.wordSpacing = 0.16f;
                strokeRenderer.setRenderMode(StrokeRenderMode::ImageBrushStamp);
                if (loadGlyphLibraryFromPath() && generateGlyphLibraryTextPath()) {
                    trajectoryTracker.setWaypoints(activeWaypoints);
                    trajectoryTracker.reset(robotKinematics.getToolTipPosition());
                    enableWaypointPlayback = false;
                }
            }
            ImGui::Text("Loaded Glyph Library Path: %s", loadedGlyphLibraryPath.empty() ? "none" : loadedGlyphLibraryPath.c_str());
            ImGui::Text("Glyph Library Source: %s", hasLoadedGlyphLibrary ? hersheyGlyphLibrary.getSourceName().c_str() : "none");
            ImGui::Text("Current Glyph Library Font: %s", hasLoadedGlyphLibrary ? hersheyGlyphLibrary.getFontName().c_str() : "none");
            ImGui::Text("Loaded Glyph Count: %d", hasLoadedGlyphLibrary ? hersheyGlyphLibrary.getGlyphCount() : 0);
            ImGui::Text(
                "Glyph Library Type: %s",
                hasLoadedGlyphLibrary
                    ? (hersheyGlyphLibrary.isLikelyFallbackLibrary() ? "fallback / sample" : "real Hershey-Fonts")
                    : "not loaded"
            );
            if (hasLoadedGlyphLibrary && hersheyGlyphLibrary.isLikelyFallbackLibrary()) {
                ImGui::TextWrapped("Warning: loaded glyph library appears to be fallback data. Run the Python script in the conda environment with Hershey-Fonts installed.");
            }
            if (hasLoadedGlyphLibrary) {
                ImGui::TextWrapped("Supported Characters: %s", hersheyGlyphLibrary.getSupportedCharacterSummary().c_str());
            } else {
                ImGui::Text("Supported Characters: none");
            }
            ImGui::Text("Glyph Library Unsupported Characters: %d", glyphLibraryUnsupportedCount);
            if (!glyphLibraryUnsupportedCharacters.empty()) {
                ImGui::Text("Unsupported: %s", glyphLibraryUnsupportedCharacters.c_str());
            }
            ImGui::Text("Glyph Library Waypoint Count: %d", glyphLibraryWaypointCount);
            if (std::strlen(glyphLibraryTextInput) > glyphLibraryTextLengthWarningThreshold) {
                ImGui::TextWrapped("Warning: long text may generate many waypoints and slow playback.");
            }
            if (glyphLibraryWaypointCount > glyphLibraryWaypointWarningThreshold) {
                ImGui::TextWrapped("Warning: generated waypoint count is high. Consider increasing Sample Spacing or shortening the text.");
            }
            if (!glyphLibraryLoadError.empty()) {
                ImGui::Text("Glyph Library Error: %s", glyphLibraryLoadError.c_str());
            }
            ImGui::Text("Glyph library command hint:");
            ImGui::TextWrapped("cd robot_arm && python scripts/generate_hershey_glyph_library.py --font futural --output assets/fonts/hershey_futural_glyphs.json");
        }
        if (ImGui::Checkbox("Lock Paper To Floor", &lockPaperToFloor)) {
            updatePaperPlane();
            reloadActivePath();
        }
        if (ImGui::SliderFloat("Floor Y", &floorY, -1.0f, 1.0f)) {
            updateGroundPlane();
            updatePaperPlane();
            reloadActivePath();
        }
        if (ImGui::SliderFloat("Paper Thickness Offset", &paperThicknessOffset, 0.0f, 0.02f)) {
            updatePaperPlane();
            reloadActivePath();
        }
        if (lockPaperToFloor) {
            ImGui::Text("Paper Y locked to floor");
        } else if (ImGui::SliderFloat("Paper Y", &handwritingOptions.paperY, -1.0f, 2.0f)) {
            updatePaperPlane();
            reloadActivePath();
        }
        if (ImGui::SliderFloat("Lift Height", &handwritingOptions.liftHeight, 0.01f, 0.15f)) {
            reloadActivePath();
        }
        if (ImGui::SliderFloat("Paper Origin X", &handwritingOptions.paperOrigin.x, -1.5f, 1.5f)) {
            updatePaperPlane();
            reloadActivePath();
        }
        if (ImGui::SliderFloat("Paper Origin Z", &handwritingOptions.paperOrigin.z, -1.5f, 1.5f)) {
            updatePaperPlane();
            reloadActivePath();
        }
        if (ImGui::Button("Reset Paper Origin Near Tool Tip")) {
            const glm::vec3 toolTipPosition = robotKinematics.getToolTipPosition();
            handwritingOptions.paperOrigin.x = toolTipPosition.x;
            handwritingOptions.paperOrigin.z = toolTipPosition.z;
            updatePaperPlane();
            reloadActivePath();
        }
        if (ImGui::Button("Generate / Reload Path")) {
            updatePaperPlane();
            reloadActivePath();
        }
        ImGui::Text("Current Path Mode: %s", pathModeItems[pathMode]);
        ImGui::Text("Use Spline: %s", handwritingOptions.useSpline ? "true" : "false");
        ImGui::Text("Generated Waypoint Count: %d", trajectoryTracker.getWaypointCount());
        if (pathMode == 2) {
            ImGui::Text("Text Path Unsupported Characters: %d", unsupportedCharacterCount);
        }
        if (pathMode == 4) {
            ImGui::Text("Glyph Library Unsupported Characters: %d", glyphLibraryUnsupportedCount);
            ImGui::Text("Glyph Library Generated Waypoints: %d", glyphLibraryWaypointCount);
        }
        ImGui::Text("Floor Y: %.4f", floorY);
        ImGui::Text("Paper Thickness Offset: %.4f", paperThicknessOffset);
        ImGui::Text("Paper Y: %.4f", handwritingOptions.paperY);
        ImGui::Text(
            "Paper Attached To Floor: %s",
            lockPaperToFloor ? "true" : "manual"
        );
        ImGui::Text("Lift Height: %.4f", handwritingOptions.liftHeight);
        ImGui::Text(
            "Paper Origin: x %.4f, z %.4f",
            handwritingOptions.paperOrigin.x,
            handwritingOptions.paperOrigin.z
        );
        if (!paperPresets.empty()) {
            if (ImGui::Combo(
                    "Paper Preset",
                    &selectedPaperPreset,
                    &paperPresetNames[0],
                    static_cast<int>(paperPresetNames.size()))) {
                applyPaperPreset(selectedPaperPreset);
            }
            ImGui::Text("Current Paper Preset: %s", activePaperPreset.name.c_str());
            const PaperTextureBinding* activePaperTextureBinding = NULL;
            if (selectedPaperPreset >= 0 && selectedPaperPreset < static_cast<int>(paperTextureBindings.size())) {
                activePaperTextureBinding = &paperTextureBindings[selectedPaperPreset];
            }
            ImGui::Text("Albedo Texture Path: %s", activePaperPreset.albedoTexturePath.c_str());
            ImGui::Text("Normal Texture Path: %s", activePaperPreset.normalTexturePath.c_str());
            ImGui::Text("Roughness Texture Path: %s", activePaperPreset.roughnessTexturePath.c_str());
            if (activePaperTextureBinding) {
                ImGui::Text("Loaded Paper Albedo: %s", activePaperTextureBinding->loadedAlbedoPath.c_str());
                ImGui::Text(
                    "Albedo Texture Loaded: %s",
                    activePaperTextureBinding->usingStructuredAlbedo ? "true" : "fallback"
                );
                ImGui::Text(
                    "Normal Texture Exists: %s",
                    activePaperTextureBinding->normalExists ? "true" : "missing"
                );
                ImGui::Text(
                    "Roughness Texture Exists: %s",
                    activePaperTextureBinding->roughnessExists ? "true" : "missing"
                );
                if (!activePaperTextureBinding->structuredAlbedoExists) {
                    ImGui::Text("Warning: structured albedo missing; using placeholder or baseColor fallback.");
                }
                if (!activePaperTextureBinding->normalExists) {
                    ImGui::Text("Warning: normal texture missing. Normal mapping is not enabled in this step.");
                }
                if (!activePaperTextureBinding->roughnessExists) {
                    ImGui::Text("Warning: roughness texture missing. Roughness map shading is not enabled in this step.");
                }
            }
            ImGui::Text("NormalGL Warning: normal.png should be ambientCG NormalGL. NormalDX may flip the green channel.");
            float paperBaseColor[3] = {
                activePaperPreset.baseColor.r,
                activePaperPreset.baseColor.g,
                activePaperPreset.baseColor.b
            };
            ImGui::ColorEdit3(
                "Paper Base Color",
                paperBaseColor,
                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoPicker
            );
            bool paperInfluenceChanged = false;
            paperInfluenceChanged |= ImGui::SliderFloat("Paper Roughness", &activePaperPreset.roughness, 0.0f, 1.0f);
            paperInfluenceChanged |= ImGui::SliderFloat("Paper Absorbency", &activePaperPreset.absorbency, 0.0f, 1.0f);
            paperInfluenceChanged |= ImGui::SliderFloat("Paper Fiber Noise", &activePaperPreset.fiberNoise, 0.0f, 1.0f);
            paperInfluenceChanged |= ImGui::SliderFloat(
                "Stroke Opacity Multiplier",
                &activePaperPreset.strokeOpacityMultiplier,
                0.2f,
                1.3f
            );
            paperInfluenceChanged |= ImGui::SliderFloat(
                "Stamp Size Multiplier",
                &activePaperPreset.stampSizeMultiplier,
                0.6f,
                1.6f
            );
            paperInfluenceChanged |= ImGui::SliderFloat(
                "Edge Noise Strength",
                &activePaperPreset.edgeNoiseStrength,
                0.0f,
                1.0f
            );
            if (paperInfluenceChanged) {
                applyPaperInfluence();
            }
        }
        glm::vec3 toolTipLocalOffset = robotKinematics.getToolTipLocalOffset();
        bool toolTipOffsetChanged = false;
        toolTipOffsetChanged |= ImGui::SliderFloat("Tool Tip Offset X", &toolTipLocalOffset.x, -0.20f, 0.20f);
        toolTipOffsetChanged |= ImGui::SliderFloat("Tool Tip Offset Y", &toolTipLocalOffset.y, -0.05f, 0.25f);
        toolTipOffsetChanged |= ImGui::SliderFloat("Tool Tip Offset Z", &toolTipLocalOffset.z, -0.20f, 0.20f);
        if (toolTipOffsetChanged) {
            robotKinematics.setToolTipLocalOffset(toolTipLocalOffset);
        }

        ImGui::Separator();
        ImGui::Checkbox("Enable Stroke Rendering", &enableStrokeRendering);
        if (ImGui::Button("Clear Stroke")) {
            strokeRenderer.clear();
        }
        if (!penPresets.empty()) {
            if (ImGui::Combo(
                    "Pen Preset",
                    &selectedPenPreset,
                    &penPresetNames[0],
                    static_cast<int>(penPresetNames.size()))) {
                selectedBrushImage = findBrushAssetIndexForPath(penPresets[selectedPenPreset].brushTexturePath);
                brushImageOverrideActive = false;
                applyPenPreset(penPresets[selectedPenPreset], selectedBrushImage);
            }
            ImGui::Text("Selected Pen: %s", penPresets[selectedPenPreset].name.c_str());
            ImGui::Text("Default Brush For Pen: %s", brushFileNameFromPath(penPresets[selectedPenPreset].brushTexturePath).c_str());
            if (!brushAssets.empty()) {
                if (ImGui::Combo(
                        "Brush Image",
                        &selectedBrushImage,
                        &brushAssetNames[0],
                        static_cast<int>(brushAssetNames.size()))) {
                    brushImageOverrideActive =
                        brushAssets[selectedBrushImage].fileName !=
                        brushFileNameFromPath(penPresets[selectedPenPreset].brushTexturePath);
                    applyPenPreset(penPresets[selectedPenPreset], selectedBrushImage);
                }
                const BrushAsset& selectedBrushAsset = brushAssets[selectedBrushImage];
                ImGui::Text("Current Brush Image: %s", selectedBrushAsset.fileName.c_str());
                ImGui::Text("Configured Pen Brush: %s", selectedBrushAsset.texturePath.c_str());
                ImGui::Text("Brush Image Exists: %s", selectedBrushAsset.exists ? "true" : "missing");
                ImGui::Text(
                    "Brush Image Size: %d x %d, channels %d",
                    selectedBrushAsset.width,
                    selectedBrushAsset.height,
                    selectedBrushAsset.channels
                );
                ImGui::Text("Brush Override Active: %s", brushImageOverrideActive ? "true" : "false");
                if (selectedBrushAsset.exists && selectedBrushAsset.width != selectedBrushAsset.height) {
                    ImGui::Text("Warning: selected brush image is non-square and may appear stretched.");
                }
            }
        }
        int strokeRenderMode = static_cast<int>(strokeRenderer.getRenderMode());
        if (ImGui::Combo("Stroke Render Mode", &strokeRenderMode, strokeRenderModeItems, 2)) {
            strokeRenderer.setRenderMode(static_cast<StrokeRenderMode>(strokeRenderMode));
        }
        ImGui::Text("Renderer Brush Texture Path: %s", strokeRenderer.getBrushTexturePath().c_str());
        ImGui::Text("Loaded Brush Texture Path: %s", strokeRenderer.getLoadedBrushTexturePath().c_str());
        ImGui::Text(
            "Brush Texture Loaded: %s",
            strokeRenderer.isBrushTextureLoaded() ? "true" : "procedural fallback"
        );
        float strokeBrushSize = strokeRenderer.getBrushSize();
        if (ImGui::SliderFloat("Brush Size", &strokeBrushSize, 0.005f, 0.080f)) {
            strokeRenderer.setBrushSize(strokeBrushSize);
        }
        float strokeOpacity = strokeRenderer.getOpacity();
        if (ImGui::SliderFloat("Opacity", &strokeOpacity, 0.05f, 1.0f)) {
            strokeRenderer.setOpacity(strokeOpacity);
        }
        glm::vec3 strokeColor = strokeRenderer.getStrokeColor();
        float strokeColorValues[3] = { strokeColor.r, strokeColor.g, strokeColor.b };
        if (ImGui::ColorEdit3("Stroke Color", strokeColorValues)) {
            strokeRenderer.setStrokeColor(glm::vec3(
                strokeColorValues[0],
                strokeColorValues[1],
                strokeColorValues[2]
            ));
        }
        float stampSpacing = strokeRenderer.getStampSpacing();
        if (ImGui::SliderFloat("Stamp Spacing", &stampSpacing, 0.002f, 0.060f)) {
            strokeRenderer.setStampSpacing(stampSpacing);
        }
        ImGui::Separator();
        if (ImGui::Checkbox("Enable Waypoint Playback", &enableWaypointPlayback)) {
            if (enableWaypointPlayback) {
                trajectoryTracker.play(robotKinematics.getToolTipPosition());
            } else {
                trajectoryTracker.pause();
            }
        }
        if (ImGui::Button("Play")) {
            enableWaypointPlayback = true;
            trajectoryTracker.play(robotKinematics.getToolTipPosition());
        }
        ImGui::SameLine();
        if (ImGui::Button("Pause")) {
            trajectoryTracker.pause();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset Playback")) {
            trajectoryTracker.reset(robotKinematics.getToolTipPosition());
        }

        float playbackSpeed = trajectoryTracker.getPlaybackSpeed();
        if (ImGui::SliderFloat("Playback Speed", &playbackSpeed, 0.02f, 0.80f)) {
            trajectoryTracker.setPlaybackSpeed(playbackSpeed);
        }
        float waypointThreshold = trajectoryTracker.getWaypointReachThreshold();
        if (ImGui::SliderFloat("Waypoint Reach Threshold", &waypointThreshold, 0.005f, 0.10f)) {
            trajectoryTracker.setWaypointReachThreshold(waypointThreshold);
        }

        if (enableWaypointPlayback) {
            trajectoryTracker.update(dt, robotKinematics, ikSolver);
            applyPandaTransforms();
        } else if (enableIK) {
            ikSolver.solve(robotKinematics, ikTarget);
            applyPandaTransforms();
        }

        const glm::vec3 endEffectorPosition = robotKinematics.getEndEffectorPosition();
        const glm::vec3 toolTipPosition = robotKinematics.getToolTipPosition();
        const bool currentPenDown = getTrajectoryPenDown();
        strokeRenderer.updateStroke(
            toolTipPosition,
            enableStrokeRendering && currentPenDown
        );
        const bool hasCurrentWaypointForDebug = trajectoryTracker.getWaypointCount() > 0
            && trajectoryTracker.getCurrentWaypointIndex() < trajectoryTracker.getWaypointCount();
        const bool currentWaypointPenDown = hasCurrentWaypointForDebug
            ? trajectoryTracker.getCurrentWaypoint().penDown
            : false;
        const float expectedWaypointY = currentWaypointPenDown
            ? handwritingOptions.paperY
            : handwritingOptions.paperY + handwritingOptions.liftHeight;

        ImGui::Separator();
        ImGui::Text("End Effector Position:");
        ImGui::Text("x: %.4f", endEffectorPosition.x);
        ImGui::Text("y: %.4f", endEffectorPosition.y);
        ImGui::Text("z: %.4f", endEffectorPosition.z);
        ImGui::Text("End Effector Height Above Paper: %.4f", endEffectorPosition.y - handwritingOptions.paperY);
        ImGui::Text("Tool Tip Position:");
        ImGui::Text("x: %.4f", toolTipPosition.x);
        ImGui::Text("y: %.4f", toolTipPosition.y);
        ImGui::Text("z: %.4f", toolTipPosition.z);
        ImGui::Text("Tool Tip Height Above Paper: %.4f", toolTipPosition.y - handwritingOptions.paperY);
        ImGui::Text("Playback penDown: %s", currentPenDown ? "true" : "false");
        ImGui::Text("Stroke Point Count: %d", strokeRenderer.getStrokePointCount());
        ImGui::Text("Stroke Segment Count: %d", strokeRenderer.getStrokeSegmentCount());
        ImGui::Text("Stamp Count: %d", strokeRenderer.getStampCount());
        if (pathMode != 0) {
            ImGui::Text("Expected Current Waypoint Y: %.4f", expectedWaypointY);
        } else {
            ImGui::Text("Expected Current Waypoint Y: n/a for Test Waypoints");
        }
        ImGui::Text("Tool Tip Target Position:");
        ImGui::Text("x: %.4f", ikTarget.x);
        ImGui::Text("y: %.4f", ikTarget.y);
        ImGui::Text("z: %.4f", ikTarget.z);
        ImGui::Text("IK Error Norm: %.6f", ikSolver.getLastErrorNorm());
        ImGui::Text("IK Iterations: %d", ikSolver.getLastIterationCount());

        ImGui::Separator();
        ImGui::Text("Waypoint Playback:");
        ImGui::Text("Playing: %s", trajectoryTracker.isPlaying() ? "true" : "false");
        ImGui::Text("Finished: %s", trajectoryTracker.isFinished() ? "true" : "false");
        ImGui::Text(
            "Current Waypoint Index: %d / %d",
            trajectoryTracker.getCurrentWaypointIndex(),
            trajectoryTracker.getWaypointCount()
        );
        if (trajectoryTracker.getWaypointCount() > 0
            && trajectoryTracker.getCurrentWaypointIndex() < trajectoryTracker.getWaypointCount()) {
            const Waypoint& currentWaypoint = trajectoryTracker.getCurrentWaypoint();
            ImGui::Text("Current Waypoint Position:");
            ImGui::Text("x: %.4f", currentWaypoint.position.x);
            ImGui::Text("y: %.4f", currentWaypoint.position.y);
            ImGui::Text("z: %.4f", currentWaypoint.position.z);
            ImGui::Text("Current Waypoint penDown: %s", currentWaypoint.penDown ? "true" : "false");
            if (pathMode != 0) {
                const float waypointExpectedY = currentWaypoint.penDown
                    ? handwritingOptions.paperY
                    : handwritingOptions.paperY + handwritingOptions.liftHeight;
                ImGui::Text("Current Waypoint Expected Y: %.4f", waypointExpectedY);
                ImGui::Text("Current Waypoint Y Error: %.6f", currentWaypoint.position.y - waypointExpectedY);
            } else {
                ImGui::Text("Current Waypoint Y Check: n/a for Test Waypoints");
            }
        } else {
            ImGui::Text("Current Waypoint Position: finished");
            ImGui::Text("Current Waypoint penDown: n/a");
        }
        const glm::vec3 interpolatedTargetPosition = trajectoryTracker.getInterpolatedTargetPosition();
        ImGui::Text("Interpolated Target Position:");
        ImGui::Text("x: %.4f", interpolatedTargetPosition.x);
        ImGui::Text("y: %.4f", interpolatedTargetPosition.y);
        ImGui::Text("z: %.4f", interpolatedTargetPosition.z);
        ImGui::Text("Distance To Current Waypoint: %.6f", trajectoryTracker.getDistanceToCurrentWaypoint());
        ImGui::Text(
            "Tool Tip Distance To Waypoint: %.6f",
            trajectoryTracker.getToolTipDistanceToCurrentWaypoint()
        );
        ImGui::End();

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
        const glm::mat4 sceneView = view;
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

        if (enableStrokeRendering) {
            strokeRenderer.render(sceneView, projection);
        }

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
