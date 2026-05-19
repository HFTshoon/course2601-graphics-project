#include "stroke_renderer.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>

#include <glad/glad.h>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "stb_image.h"

StrokeRenderer::StrokeRenderer()
    : renderMode_(StrokeRenderMode::LineStrip),
      strokeActive_(false),
      hasLastStampPosition_(false),
      lastStampPosition_(0.0f),
      brushSize_(0.025f),
      opacity_(0.9f),
      strokeColor_(0.02f, 0.018f, 0.015f),
      stampSpacing_(0.012f),
      paperOpacityMultiplier_(1.0f),
      paperStampSizeMultiplier_(1.0f),
      paperEdgeNoiseStrength_(0.0f),
      paperFiberNoise_(0.0f),
      paperY_(0.0f),
      paperEpsilon_(0.003f),
      paperNormalTextureID_(0),
      paperRoughnessTextureID_(0),
      paperOriginXZ_(0.0f),
      paperSize_(1.0f, 1.0f),
      paperRightXZ_(1.0f, 0.0f),
      paperUpXZ_(0.0f, 1.0f),
      paperUvScale_(1.0f),
      paperMapStrokeModulationEnabled_(true),
      roughnessInfluence_(0.3f),
      normalInfluence_(0.15f),
      paperNoiseScale_(60.0f),
      flipNormalY_(false),
      minPointSpacing_(0.002f),
      strokePointCount_(0),
      brushTexturePath_("../assets/brushes/basic_circle.png"),
      loadedBrushTexturePath_(""),
      brushTextureID_(0),
      brushTextureInitialized_(false),
      brushTextureLoadedFromFile_(false),
      lineVao_(0),
      lineVbo_(0),
      lineShaderProgram_(0),
      stampVao_(0),
      stampVbo_(0),
      stampShaderProgram_(0),
      glResourcesInitialized_(false)
{
}

void StrokeRenderer::clear()
{
    strokeSegments_.clear();
    brushStamps_.clear();
    strokeActive_ = false;
    hasLastStampPosition_ = false;
    strokePointCount_ = 0;
}

void StrokeRenderer::updateStroke(const glm::vec3& toolTipWorldPosition, bool penDown)
{
    if (!penDown) {
        endStroke();
        return;
    }

    if (!strokeActive_) {
        beginStroke();
    }

    if (strokeSegments_.empty()) {
        return;
    }

    const glm::vec3 strokePoint = projectToolTipToStrokePlane(toolTipWorldPosition);
    std::vector<glm::vec3>& currentSegment = strokeSegments_.back();
    if (currentSegment.empty()
        || glm::length(strokePoint - currentSegment.back()) >= minPointSpacing_) {
        currentSegment.push_back(strokePoint);
        ++strokePointCount_;
    }

    updateStampTrail(strokePoint);
}

void StrokeRenderer::render(const glm::mat4& view, const glm::mat4& projection)
{
    if (strokePointCount_ == 0 && brushStamps_.empty()) {
        return;
    }

    ensureGLResources();
    if (!glResourcesInitialized_) {
        return;
    }

    const GLboolean wasBlendEnabled = glIsEnabled(GL_BLEND);
    GLboolean wasDepthMaskEnabled = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &wasDepthMaskEnabled);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    if (renderMode_ == StrokeRenderMode::LineStrip) {
        renderLineStrips(view, projection);
    } else {
        renderBrushStamps(view, projection);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    glLineWidth(1.0f);
    glPointSize(1.0f);
    glDepthMask(wasDepthMaskEnabled);
    if (!wasBlendEnabled) {
        glDisable(GL_BLEND);
    }
}

void StrokeRenderer::setRenderMode(StrokeRenderMode mode)
{
    renderMode_ = mode;
}

StrokeRenderMode StrokeRenderer::getRenderMode() const
{
    return renderMode_;
}

void StrokeRenderer::setBrushSize(float brushSize)
{
    brushSize_ = std::max(0.001f, brushSize);
    for (size_t stampIndex = 0; stampIndex < brushStamps_.size(); ++stampIndex) {
        brushStamps_[stampIndex].size = brushSize_;
    }
}

float StrokeRenderer::getBrushSize() const
{
    return brushSize_;
}

void StrokeRenderer::setOpacity(float opacity)
{
    opacity_ = std::max(0.0f, std::min(1.0f, opacity));
    for (size_t stampIndex = 0; stampIndex < brushStamps_.size(); ++stampIndex) {
        brushStamps_[stampIndex].opacity = opacity_;
    }
}

float StrokeRenderer::getOpacity() const
{
    return opacity_;
}

void StrokeRenderer::setStrokeColor(const glm::vec3& color)
{
    strokeColor_ = glm::clamp(color, glm::vec3(0.0f), glm::vec3(1.0f));
    for (size_t stampIndex = 0; stampIndex < brushStamps_.size(); ++stampIndex) {
        brushStamps_[stampIndex].color = strokeColor_;
    }
}

glm::vec3 StrokeRenderer::getStrokeColor() const
{
    return strokeColor_;
}

void StrokeRenderer::setStampSpacing(float spacing)
{
    stampSpacing_ = std::max(0.001f, spacing);
}

float StrokeRenderer::getStampSpacing() const
{
    return stampSpacing_;
}

void StrokeRenderer::setPaperInfluence(
    float opacityMultiplier,
    float sizeMultiplier,
    float edgeNoiseStrength,
    float fiberNoise)
{
    paperOpacityMultiplier_ = std::max(0.0f, opacityMultiplier);
    paperStampSizeMultiplier_ = std::max(0.1f, sizeMultiplier);
    paperEdgeNoiseStrength_ = std::max(0.0f, std::min(1.0f, edgeNoiseStrength));
    paperFiberNoise_ = std::max(0.0f, std::min(1.0f, fiberNoise));
}

void StrokeRenderer::setPaperY(float paperY)
{
    paperY_ = paperY;
}

float StrokeRenderer::getPaperY() const
{
    return paperY_;
}

void StrokeRenderer::setPaperMaterialTextures(unsigned int normalTextureID, unsigned int roughnessTextureID)
{
    paperNormalTextureID_ = normalTextureID;
    paperRoughnessTextureID_ = roughnessTextureID;
}

void StrokeRenderer::setPaperMapping(
    const glm::vec2& originXZ,
    const glm::vec2& size,
    float uvScale,
    const glm::vec2& rightXZ,
    const glm::vec2& upXZ)
{
    paperOriginXZ_ = originXZ;
    paperSize_ = glm::vec2(
        std::max(0.001f, size.x),
        std::max(0.001f, size.y)
    );
    paperRightXZ_ = glm::length(rightXZ) > 1e-5f
        ? glm::normalize(rightXZ)
        : glm::vec2(1.0f, 0.0f);
    paperUpXZ_ = glm::length(upXZ) > 1e-5f
        ? glm::normalize(upXZ)
        : glm::vec2(0.0f, 1.0f);
    paperUvScale_ = std::max(0.01f, uvScale);
}

void StrokeRenderer::setPaperMapStrokeModulation(
    bool enabled,
    float roughnessInfluence,
    float normalInfluence,
    float noiseScale,
    bool flipNormalY)
{
    paperMapStrokeModulationEnabled_ = enabled;
    roughnessInfluence_ = std::max(0.0f, std::min(1.0f, roughnessInfluence));
    normalInfluence_ = std::max(0.0f, std::min(1.0f, normalInfluence));
    paperNoiseScale_ = std::max(1.0f, noiseScale);
    flipNormalY_ = flipNormalY;
}

void StrokeRenderer::setBrushTexturePath(const std::string& path)
{
    if (brushTexturePath_ == path) {
        ensureBrushTexture();
        return;
    }

    brushTexturePath_ = path;
    loadedBrushTexturePath_.clear();
    brushTextureInitialized_ = false;
    brushTextureLoadedFromFile_ = false;
    if (brushTextureID_ != 0) {
        glDeleteTextures(1, &brushTextureID_);
        brushTextureID_ = 0;
    }
    ensureBrushTexture();
}

const std::string& StrokeRenderer::getBrushTexturePath() const
{
    return brushTexturePath_;
}

const std::string& StrokeRenderer::getLoadedBrushTexturePath() const
{
    return loadedBrushTexturePath_;
}

bool StrokeRenderer::isBrushTextureLoaded() const
{
    return brushTextureInitialized_ && brushTextureLoadedFromFile_;
}

int StrokeRenderer::getStrokePointCount() const
{
    return strokePointCount_;
}

int StrokeRenderer::getStrokeSegmentCount() const
{
    int segmentCount = 0;
    for (size_t segmentIndex = 0; segmentIndex < strokeSegments_.size(); ++segmentIndex) {
        if (!strokeSegments_[segmentIndex].empty()) {
            ++segmentCount;
        }
    }
    return segmentCount;
}

int StrokeRenderer::getStampCount() const
{
    return static_cast<int>(brushStamps_.size());
}

void StrokeRenderer::beginStroke()
{
    if (strokeActive_) {
        return;
    }

    strokeSegments_.push_back(std::vector<glm::vec3>());
    strokeActive_ = true;
    hasLastStampPosition_ = false;
}

void StrokeRenderer::endStroke()
{
    strokeActive_ = false;
    hasLastStampPosition_ = false;
    if (!strokeSegments_.empty() && strokeSegments_.back().empty()) {
        strokeSegments_.pop_back();
    }
}

void StrokeRenderer::addStamp(const glm::vec3& position)
{
    BrushStamp stamp;
    stamp.position = position;
    stamp.color = strokeColor_;
    stamp.size = brushSize_;
    stamp.opacity = opacity_;
    brushStamps_.push_back(stamp);
}

void StrokeRenderer::updateStampTrail(const glm::vec3& projectedPosition)
{
    if (!hasLastStampPosition_) {
        addStamp(projectedPosition);
        lastStampPosition_ = projectedPosition;
        hasLastStampPosition_ = true;
        return;
    }

    const glm::vec3 delta = projectedPosition - lastStampPosition_;
    const float distance = glm::length(delta);
    if (distance < stampSpacing_) {
        return;
    }

    const glm::vec3 direction = delta / distance;
    const int stampCount = static_cast<int>(std::floor(distance / stampSpacing_));
    for (int stampIndex = 1; stampIndex <= stampCount; ++stampIndex) {
        const glm::vec3 stampPosition = lastStampPosition_ + direction * (stampSpacing_ * static_cast<float>(stampIndex));
        addStamp(stampPosition);
    }

    lastStampPosition_ += direction * (stampSpacing_ * static_cast<float>(stampCount));
}

void StrokeRenderer::ensureGLResources()
{
    if (glResourcesInitialized_) {
        ensureBrushTexture();
        return;
    }

    lineShaderProgram_ = compileLineShaderProgram();
    stampShaderProgram_ = compileBrushShaderProgram();
    if (lineShaderProgram_ == 0 || stampShaderProgram_ == 0) {
        return;
    }

    glGenVertexArrays(1, &lineVao_);
    glGenBuffers(1, &lineVbo_);
    glBindVertexArray(lineVao_);
    glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);

    glGenVertexArrays(1, &stampVao_);
    glGenBuffers(1, &stampVbo_);
    glBindVertexArray(stampVao_);
    glBindBuffer(GL_ARRAY_BUFFER, stampVbo_);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glResourcesInitialized_ = true;
    ensureBrushTexture();
}

void StrokeRenderer::ensureBrushTexture()
{
    if (brushTextureInitialized_) {
        return;
    }

    if (brushTextureID_ != 0) {
        glDeleteTextures(1, &brushTextureID_);
        brushTextureID_ = 0;
    }

    loadedBrushTexturePath_.clear();
    brushTextureLoadedFromFile_ = false;

    brushTextureID_ = loadBrushTextureFromFile(brushTexturePath_);
    if (brushTextureID_ != 0) {
        loadedBrushTexturePath_ = brushTexturePath_;
        brushTextureLoadedFromFile_ = true;
    }
    const std::string fallbackBrushPath = "../assets/brushes/basic_circle.png";
    if (brushTextureID_ == 0 && brushTexturePath_ != fallbackBrushPath) {
        brushTextureID_ = loadBrushTextureFromFile(fallbackBrushPath);
        if (brushTextureID_ != 0) {
            loadedBrushTexturePath_ = fallbackBrushPath;
            brushTextureLoadedFromFile_ = true;
        }
    }
    if (brushTextureID_ == 0) {
        brushTextureID_ = createFallbackBrushTexture();
        if (brushTextureID_ != 0) {
            loadedBrushTexturePath_ = "procedural fallback brush";
            brushTextureLoadedFromFile_ = false;
        }
    }

    brushTextureInitialized_ = brushTextureID_ != 0;
}

unsigned int StrokeRenderer::compileLineShaderProgram() const
{
    const char* vertexShaderSource =
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPosition;\n"
        "uniform mat4 view;\n"
        "uniform mat4 projection;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = projection * view * vec4(aPosition, 1.0);\n"
        "}\n";

    const char* fragmentShaderSource =
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "uniform vec4 strokeColor;\n"
        "void main()\n"
        "{\n"
        "    FragColor = strokeColor;\n"
        "}\n";

    return compileShaderProgramFromSource(vertexShaderSource, fragmentShaderSource, "STROKE_LINE");
}

unsigned int StrokeRenderer::compileBrushShaderProgram() const
{
    std::string vertexShaderSource;
    std::string fragmentShaderSource;
    const bool loadedVertex = readTextFile("../shaders/brush_stamp.vs", vertexShaderSource);
    const bool loadedFragment = readTextFile("../shaders/brush_stamp.fs", fragmentShaderSource);

    if (loadedVertex && loadedFragment) {
        return compileShaderProgramFromSource(vertexShaderSource.c_str(), fragmentShaderSource.c_str(), "BRUSH_STAMP");
    }

    const char* fallbackVertex =
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPosition;\n"
        "layout (location = 1) in vec2 aTexCoord;\n"
        "uniform mat4 view;\n"
        "uniform mat4 projection;\n"
        "out vec2 TexCoord;\n"
        "out vec3 WorldPosition;\n"
        "void main()\n"
        "{\n"
        "    TexCoord = aTexCoord;\n"
        "    WorldPosition = aPosition;\n"
        "    gl_Position = projection * view * vec4(aPosition, 1.0);\n"
        "}\n";
    const char* fallbackFragment =
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "in vec2 TexCoord;\n"
        "uniform sampler2D brushTexture;\n"
        "uniform float opacity;\n"
        "uniform vec3 strokeColor;\n"
        "uniform float edgeNoiseStrength;\n"
        "uniform float fiberNoise;\n"
        "in vec3 WorldPosition;\n"
        "float hash21(vec2 p)\n"
        "{\n"
        "    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);\n"
        "}\n"
        "float valueNoise(vec2 p)\n"
        "{\n"
        "    vec2 i = floor(p);\n"
        "    vec2 f = fract(p);\n"
        "    f = f * f * (3.0 - 2.0 * f);\n"
        "    float a = hash21(i);\n"
        "    float b = hash21(i + vec2(1.0, 0.0));\n"
        "    float c = hash21(i + vec2(0.0, 1.0));\n"
        "    float d = hash21(i + vec2(1.0, 1.0));\n"
        "    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);\n"
        "}\n"
        "void main()\n"
        "{\n"
        "    vec4 tex = texture(brushTexture, TexCoord);\n"
        "    float edgeMask = 1.0 - smoothstep(0.22, 0.86, tex.a);\n"
        "    float edgeNoise = valueNoise(WorldPosition.xz * 95.0);\n"
        "    float fiber = valueNoise(vec2(WorldPosition.x * 28.0, WorldPosition.z * 170.0));\n"
        "    float edgeVariation = mix(1.0, 0.52 + 0.74 * edgeNoise, edgeNoiseStrength * edgeMask);\n"
        "    float fiberVariation = mix(1.0, 0.78 + 0.32 * fiber, fiberNoise);\n"
        "    float alpha = tex.a * opacity * edgeVariation * fiberVariation;\n"
        "    if (alpha < 0.01) discard;\n"
        "    FragColor = vec4(strokeColor, alpha);\n"
        "}\n";
    return compileShaderProgramFromSource(fallbackVertex, fallbackFragment, "BRUSH_STAMP_FALLBACK");
}

unsigned int StrokeRenderer::compileShaderProgramFromSource(
    const char* vertexShaderSource,
    const char* fragmentShaderSource,
    const char* debugName) const
{
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    int success = 0;
    char infoLog[1024];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 1024, NULL, infoLog);
        std::cout << "ERROR::" << debugName << "::VERTEX_SHADER\n" << infoLog << std::endl;
        glDeleteShader(vertexShader);
        return 0;
    }

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 1024, NULL, infoLog);
        std::cout << "ERROR::" << debugName << "::FRAGMENT_SHADER\n" << infoLog << std::endl;
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return 0;
    }

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 1024, NULL, infoLog);
        std::cout << "ERROR::" << debugName << "::SHADER_PROGRAM\n" << infoLog << std::endl;
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(shaderProgram);
        return 0;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return shaderProgram;
}

unsigned int StrokeRenderer::loadBrushTextureFromFile(const std::string& path) const
{
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (pixels == NULL) {
        return 0;
    }

    unsigned int textureID = 0;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(pixels);
    return textureID;
}

unsigned int StrokeRenderer::createFallbackBrushTexture() const
{
    const int size = 64;
    std::vector<unsigned char> pixels(size * size * 4, 0);
    const float center = (static_cast<float>(size) - 1.0f) * 0.5f;
    const float radius = center;

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float dx = (static_cast<float>(x) - center) / radius;
            const float dy = (static_cast<float>(y) - center) / radius;
            const float distance = std::sqrt(dx * dx + dy * dy);
            const float alpha = std::max(0.0f, 1.0f - distance);
            const float softenedAlpha = alpha * alpha * (3.0f - 2.0f * alpha);
            const int index = (y * size + x) * 4;
            pixels[index + 0] = 0;
            pixels[index + 1] = 0;
            pixels[index + 2] = 0;
            pixels[index + 3] = static_cast<unsigned char>(std::min(255.0f, softenedAlpha * 255.0f));
        }
    }

    unsigned int textureID = 0;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0]);
    glBindTexture(GL_TEXTURE_2D, 0);
    return textureID;
}

bool StrokeRenderer::readTextFile(const std::string& path, std::string& contents) const
{
    std::ifstream file(path.c_str());
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    contents = buffer.str();
    return true;
}

void StrokeRenderer::renderLineStrips(const glm::mat4& view, const glm::mat4& projection)
{
    if (strokePointCount_ == 0) {
        return;
    }

    glLineWidth(std::max(1.0f, brushSize_ * 180.0f));
    glPointSize(std::max(1.0f, brushSize_ * 180.0f));

    glUseProgram(lineShaderProgram_);
    glUniformMatrix4fv(glGetUniformLocation(lineShaderProgram_, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(lineShaderProgram_, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform4f(
        glGetUniformLocation(lineShaderProgram_, "strokeColor"),
        strokeColor_.r,
        strokeColor_.g,
        strokeColor_.b,
        std::min(1.0f, opacity_ * paperOpacityMultiplier_)
    );

    glBindVertexArray(lineVao_);
    glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);

    for (size_t segmentIndex = 0; segmentIndex < strokeSegments_.size(); ++segmentIndex) {
        const std::vector<glm::vec3>& segment = strokeSegments_[segmentIndex];
        if (segment.empty()) {
            continue;
        }

        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(segment.size() * sizeof(glm::vec3)),
            &segment[0],
            GL_DYNAMIC_DRAW
        );

        if (segment.size() == 1) {
            glDrawArrays(GL_POINTS, 0, 1);
        } else {
            glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(segment.size()));
        }
    }
}

void StrokeRenderer::renderBrushStamps(const glm::mat4& view, const glm::mat4& projection)
{
    if (brushStamps_.empty()) {
        return;
    }

    ensureBrushTexture();
    if (!brushTextureInitialized_) {
        return;
    }

    glUseProgram(stampShaderProgram_);
    glUniformMatrix4fv(glGetUniformLocation(stampShaderProgram_, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(stampShaderProgram_, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform1i(glGetUniformLocation(stampShaderProgram_, "brushTexture"), 0);
    glUniform1i(glGetUniformLocation(stampShaderProgram_, "paperNormalMap"), 1);
    glUniform1i(glGetUniformLocation(stampShaderProgram_, "paperRoughnessMap"), 2);
    glUniform2f(glGetUniformLocation(stampShaderProgram_, "paperOriginXZ"), paperOriginXZ_.x, paperOriginXZ_.y);
    glUniform2f(glGetUniformLocation(stampShaderProgram_, "paperSize"), paperSize_.x, paperSize_.y);
    glUniform2f(glGetUniformLocation(stampShaderProgram_, "paperRightXZ"), paperRightXZ_.x, paperRightXZ_.y);
    glUniform2f(glGetUniformLocation(stampShaderProgram_, "paperUpXZ"), paperUpXZ_.x, paperUpXZ_.y);
    glUniform1f(glGetUniformLocation(stampShaderProgram_, "paperUvScale"), paperUvScale_);
    glUniform1i(
        glGetUniformLocation(stampShaderProgram_, "enablePaperMapModulation"),
        paperMapStrokeModulationEnabled_ ? 1 : 0
    );
    glUniform1i(glGetUniformLocation(stampShaderProgram_, "flipNormalY"), flipNormalY_ ? 1 : 0);
    glUniform1f(glGetUniformLocation(stampShaderProgram_, "roughnessInfluence"), roughnessInfluence_);
    glUniform1f(glGetUniformLocation(stampShaderProgram_, "normalInfluence"), normalInfluence_);
    glUniform1f(glGetUniformLocation(stampShaderProgram_, "paperNoiseScale"), paperNoiseScale_);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, brushTextureID_);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, paperNormalTextureID_);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, paperRoughnessTextureID_);
    glBindVertexArray(stampVao_);
    glBindBuffer(GL_ARRAY_BUFFER, stampVbo_);

    for (size_t stampIndex = 0; stampIndex < brushStamps_.size(); ++stampIndex) {
        const BrushStamp& stamp = brushStamps_[stampIndex];
        const float halfSize = stamp.size * paperStampSizeMultiplier_ * 0.5f;
        const glm::vec3& c = stamp.position;
        const float vertices[] = {
            c.x - halfSize, c.y, c.z - halfSize, 0.0f, 0.0f,
            c.x + halfSize, c.y, c.z - halfSize, 1.0f, 0.0f,
            c.x + halfSize, c.y, c.z + halfSize, 1.0f, 1.0f,
            c.x - halfSize, c.y, c.z - halfSize, 0.0f, 0.0f,
            c.x + halfSize, c.y, c.z + halfSize, 1.0f, 1.0f,
            c.x - halfSize, c.y, c.z + halfSize, 0.0f, 1.0f
        };

        glUniform1f(
            glGetUniformLocation(stampShaderProgram_, "opacity"),
            std::min(1.0f, stamp.opacity * paperOpacityMultiplier_)
        );
        glUniform3f(
            glGetUniformLocation(stampShaderProgram_, "strokeColor"),
            stamp.color.r,
            stamp.color.g,
            stamp.color.b
        );
        glUniform1f(glGetUniformLocation(stampShaderProgram_, "edgeNoiseStrength"), paperEdgeNoiseStrength_);
        glUniform1f(glGetUniformLocation(stampShaderProgram_, "fiberNoise"), paperFiberNoise_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

glm::vec3 StrokeRenderer::projectToolTipToStrokePlane(const glm::vec3& toolTipWorldPosition) const
{
    return glm::vec3(toolTipWorldPosition.x, paperY_ + paperEpsilon_, toolTipWorldPosition.z);
}
