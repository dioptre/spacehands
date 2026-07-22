#pragma once

#include <vector>
#include <string>
#include <memory>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "Types.h"
#include "util/Config.h"
#include "audio/OscSender.h"

enum class VisualizerState {
    IDLE,
    SEQUENCE_PREVIEW,
    PLAYING,
    CLIMAX,
    CONGRATULATIONS
};

struct VisualizerParticle {
    glm::vec3 pos;
    glm::vec3 vel;
    float life;
};

struct GLBModel {
    struct Mesh {
        unsigned int vao = 0;
        unsigned int vbo = 0;
        unsigned int ebo = 0;
        int indexCount = 0;
    };
    std::vector<Mesh> meshes;

    void draw() const;
    void cleanup();
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool init(int width, int height, bool fullscreen, OscSender& osc, const Config& cfg);
    void render(const HandList& hands, const std::vector<TargetSpawn>& spawns, float dt);
    void close();
    bool shouldClose() const;
    const std::vector<int>& getSequence() const { return sequence_; }


private:
    // Window state
    GLFWwindow* window_ = nullptr;
    int width_ = 1280;
    int height_ = 720;
    bool fullscreen_ = false;

    // References
    OscSender* osc_ = nullptr;
    const Config* cfg_ = nullptr;

    // Game state
    VisualizerState state_ = VisualizerState::IDLE;
    std::vector<int> sequence_;
    int collected_ = 0;
    float holdTimer_ = 0.0f;
    float idleTimer_ = 0.0f;
    float climaxTimer_ = 0.0f;
    float congratsTimer_ = 0.0f;
    float sequenceAge_ = 0.0f;
    int previewStep_ = 0;
    float previewTimer_ = 0.0f;
    float lastPreviewTime_ = -999.0f;
    glm::vec3 frozenHandPos_{0.0f};
    bool hasFrozenHand_ = false;
    bool hasSeenPreview_ = false;
    
    struct VisualizerNote {
        glm::vec3 pos;
        float progress = 0.0f; // 0.0 to 1.2 (1.0 is hit target)
        int lane = 0;          // 0: UL, 1: UR, 2: LL, 3: LR
        int hand = 0;          // 0 or 1
        bool hit = false;
        bool playCustom = false;
        int midi = 60;
        float gain = 1.0f;
        float sustain = 1.0f;
        std::string instrument = "arpy";
    };
    std::vector<VisualizerNote> notes_;
    int score_ = 0;
    int combo_ = 0;
    int maxCombo_ = 0;

    enum class FadeState { NONE, FADE_OUT, FADE_IN };
    FadeState fadeState_ = FadeState::NONE;
    float fadeTimer_ = 0.0f;

    // Fibonacci sphere nodes
    static constexpr int NUM_NODES = 12;
    static constexpr float SPHERE_RADIUS = 1.2f;
    std::vector<glm::vec3> sphereBasePositions_;
    std::vector<glm::vec3> gridPositions_;
    float sphereRotY_ = 0.0f;

    // Particle system
    std::vector<VisualizerParticle> particles_;

    // OpenGL objects
    GLuint mainShader_ = 0;
    GLuint starfieldShader_ = 0;
    GLuint freqShader_ = 0;
    GLuint flatShader_ = 0;

    // Mesh VAOs and VBOs
    GLuint cubeVAO_ = 0, cubeVBO_ = 0;
    GLuint cubeEdgesVAO_ = 0, cubeEdgesVBO_ = 0;
    GLuint sphereVAO_ = 0, sphereVBO_ = 0, sphereEBO_ = 0;
    int sphereIndexCount_ = 0;
    GLuint starfieldVAO_ = 0, starfieldVBO_ = 0;
    GLuint quadVAO_ = 0, quadVBO_ = 0;
    GLuint lineVAO_ = 0, lineVBO_ = 0; // for ring/beams

    // GLB models
    GLBModel alienModel_;
    GLBModel handModel_;
    bool hasAlienModel_ = false;
    bool hasHandModel_ = false;

    // Hand lerped positions for smooth visuals
    std::vector<glm::vec3> lerpedHandPositions_;
    std::vector<glm::vec3> prevHandPositions_;

    // Helper functions
    void resetGame();
    void updateMelody();
    void updateGridPositions(float dt);
    glm::vec3 handWorldPos(float hx, float hy, float hz, float aspect);
    void burstParticles(const glm::vec3& pos);
    void updateCellVisuals(int i, glm::vec3& color, glm::vec3& emissive, float& opacity, float& scale, bool& drawWireframe);
    bool loadGLBModel(const std::string& path, GLBModel& outModel);

    // Shader loading helpers
    GLuint compileShader(const std::string& source, GLenum type);
    GLuint linkProgram(GLuint vert, GLuint frag);
    void initShaders();
    void initMeshes();
    void drawCube(const glm::mat4& model, const glm::vec3& color, const glm::vec3& emissive, float opacity, bool wireframe);
    void drawSphere(const glm::mat4& model, const glm::vec3& color, const glm::vec3& emissive, float opacity);
    void drawRing(const glm::vec3& center, float radius, const glm::vec3& color, float opacity);
    void drawModel(const GLBModel& model, const glm::mat4& modelMatrix, const glm::vec3& color, const glm::vec3& emissive, float opacity);
};
