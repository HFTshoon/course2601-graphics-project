#ifndef STROKE_RENDERER_H
#define STROKE_RENDERER_H

#include <string>
#include <vector>

#include <glm/glm.hpp>

enum class StrokeRenderMode {
    LineStrip = 0,
    ImageBrushStamp = 1
};

class StrokeRenderer {
public:
    StrokeRenderer();

    void clear();

    void updateStroke(const glm::vec3& toolTipWorldPosition, bool penDown);
    void render(const glm::mat4& view, const glm::mat4& projection);

    void setRenderMode(StrokeRenderMode mode);
    StrokeRenderMode getRenderMode() const;

    void setBrushSize(float brushSize);
    float getBrushSize() const;

    void setOpacity(float opacity);
    float getOpacity() const;

    void setStrokeColor(const glm::vec3& color);
    glm::vec3 getStrokeColor() const;

    void setStampSpacing(float spacing);
    float getStampSpacing() const;

    void setPaperInfluence(
        float opacityMultiplier,
        float sizeMultiplier,
        float edgeNoiseStrength,
        float fiberNoise
    );

    void setPaperY(float paperY);
    float getPaperY() const;

    void setPaperMaterialTextures(unsigned int normalTextureID, unsigned int roughnessTextureID);
    void setPaperMapping(
        const glm::vec2& originXZ,
        const glm::vec2& size,
        float uvScale,
        const glm::vec2& rightXZ = glm::vec2(1.0f, 0.0f),
        const glm::vec2& upXZ = glm::vec2(0.0f, 1.0f)
    );
    void setPaperMapStrokeModulation(
        bool enabled,
        float roughnessInfluence,
        float normalInfluence,
        float noiseScale,
        bool flipNormalY
    );

    void setBrushTexturePath(const std::string& path);
    const std::string& getBrushTexturePath() const;
    const std::string& getLoadedBrushTexturePath() const;
    bool isBrushTextureLoaded() const;

    int getStrokePointCount() const;
    int getStrokeSegmentCount() const;
    int getStampCount() const;

private:
    struct BrushStamp {
        glm::vec3 position;
        glm::vec3 color;
        float size;
        float opacity;
    };

    void beginStroke();
    void endStroke();
    void addStamp(const glm::vec3& position);
    void updateStampTrail(const glm::vec3& projectedPosition);

    void ensureGLResources();
    void ensureBrushTexture();
    unsigned int compileLineShaderProgram() const;
    unsigned int compileBrushShaderProgram() const;
    unsigned int compileShaderProgramFromSource(
        const char* vertexShaderSource,
        const char* fragmentShaderSource,
        const char* debugName
    ) const;
    unsigned int loadBrushTextureFromFile(const std::string& path) const;
    unsigned int createFallbackBrushTexture() const;
    bool readTextFile(const std::string& path, std::string& contents) const;

    void renderLineStrips(const glm::mat4& view, const glm::mat4& projection);
    void renderBrushStamps(const glm::mat4& view, const glm::mat4& projection);
    glm::vec3 projectToolTipToStrokePlane(const glm::vec3& toolTipWorldPosition) const;

    StrokeRenderMode renderMode_;
    std::vector<std::vector<glm::vec3>> strokeSegments_;
    std::vector<BrushStamp> brushStamps_;
    bool strokeActive_;
    bool hasLastStampPosition_;
    glm::vec3 lastStampPosition_;
    float brushSize_;
    float opacity_;
    glm::vec3 strokeColor_;
    float stampSpacing_;
    float paperOpacityMultiplier_;
    float paperStampSizeMultiplier_;
    float paperEdgeNoiseStrength_;
    float paperFiberNoise_;
    float paperY_;
    float paperEpsilon_;
    unsigned int paperNormalTextureID_;
    unsigned int paperRoughnessTextureID_;
    glm::vec2 paperOriginXZ_;
    glm::vec2 paperSize_;
    glm::vec2 paperRightXZ_;
    glm::vec2 paperUpXZ_;
    float paperUvScale_;
    bool paperMapStrokeModulationEnabled_;
    float roughnessInfluence_;
    float normalInfluence_;
    float paperNoiseScale_;
    bool flipNormalY_;
    float minPointSpacing_;
    int strokePointCount_;

    std::string brushTexturePath_;
    std::string loadedBrushTexturePath_;
    unsigned int brushTextureID_;
    bool brushTextureInitialized_;
    bool brushTextureLoadedFromFile_;

    unsigned int lineVao_;
    unsigned int lineVbo_;
    unsigned int lineShaderProgram_;
    unsigned int stampVao_;
    unsigned int stampVbo_;
    unsigned int stampShaderProgram_;
    bool glResourcesInitialized_;
};

#endif
