#define CGLTF_IMPLEMENTATION
#include "../../third_party/cgltf.h"
#include "Renderer.h"
#include <iostream>
#include <cmath>
#include <random>
#include <algorithm>
#include <numeric>
#include <glm/gtc/type_ptr.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// Cube mesh data with normals
static const float cubeVertices[] = {
    // positions          // normals
    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
     0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
    -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
     0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
    -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,

    -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
    -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
    -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
    -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
    -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
    -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,

     0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
     0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
     0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
     0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
     0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
     0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

    -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
     0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
     0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
     0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

    -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
    -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
    -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f
};

// Cube edge lines (24 vertices representing the 12 outer edges, with dummy normals)
static const float cubeEdgesVertices[] = {
    // Bottom face edges
    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f,  0.0f,
     0.5f, -0.5f, -0.5f,  0.0f,  0.0f,  0.0f,

     0.5f, -0.5f, -0.5f,  0.0f,  0.0f,  0.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  0.0f,  0.0f,

     0.5f,  0.5f, -0.5f,  0.0f,  0.0f,  0.0f,
    -0.5f,  0.5f, -0.5f,  0.0f,  0.0f,  0.0f,

    -0.5f,  0.5f, -0.5f,  0.0f,  0.0f,  0.0f,
    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f,  0.0f,

    // Top face edges
    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  0.0f,
     0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  0.0f,

     0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  0.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  0.0f,

     0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  0.0f,
    -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  0.0f,

    -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  0.0f,
    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  0.0f,

    // Vertical edges
    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f,  0.0f,
    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  0.0f,

     0.5f, -0.5f, -0.5f,  0.0f,  0.0f,  0.0f,
     0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  0.0f,

     0.5f,  0.5f, -0.5f,  0.0f,  0.0f,  0.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  0.0f,

    -0.5f,  0.5f, -0.5f,  0.0f,  0.0f,  0.0f,
    -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  0.0f
};

// Pentatonic MIDI notes & Names
static const int PENTA[] = {53, 55, 57, 58, 60, 62, 64, 65, 67, 69, 70, 72};
static const char* NOTE_NAMES[] = {"c4","d4","e4","g4","a4","c5","d5","e5","g5","a5","c6","d6"};

// Helper to generate a UV sphere
static void createSphere(float radius, int sectors, int stacks, std::vector<float>& vertices, std::vector<unsigned int>& indices) {
    float x, y, z, xy;
    float nx, ny, nz, lengthInv = 1.0f / radius;

    float sectorStep = 2.0f * M_PI / sectors;
    float stackStep = M_PI / stacks;
    float sectorAngle, stackAngle;

    for (int i = 0; i <= stacks; ++i) {
        stackAngle = M_PI / 2.0f - i * stackStep;
        xy = radius * cosf(stackAngle);
        z = radius * sinf(stackAngle);

        for (int j = 0; j <= sectors; ++j) {
            sectorAngle = j * sectorStep;

            x = xy * cosf(sectorAngle);
            y = xy * sinf(sectorAngle);
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);

            nx = x * lengthInv;
            ny = y * lengthInv;
            nz = z * lengthInv;
            vertices.push_back(nx);
            vertices.push_back(ny);
            vertices.push_back(nz);
        }
    }

    int k1, k2;
    for (int i = 0; i < stacks; ++i) {
        k1 = i * (sectors + 1);
        k2 = k1 + sectors + 1;

        for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
            if (i != 0) {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }
            if (i != (stacks - 1)) {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }
}

Renderer::Renderer() {}

Renderer::~Renderer() {
    close();
}

bool Renderer::shouldClose() const {
    return window_ && glfwWindowShouldClose(window_);
}

void Renderer::close() {
    if (window_) {
        if (cubeVAO_) glDeleteVertexArrays(1, &cubeVAO_);
        if (cubeVBO_) glDeleteBuffers(1, &cubeVBO_);
        if (cubeEdgesVAO_) glDeleteVertexArrays(1, &cubeEdgesVAO_);
        if (cubeEdgesVBO_) glDeleteBuffers(1, &cubeEdgesVBO_);
        if (sphereVAO_) glDeleteVertexArrays(1, &sphereVAO_);
        if (sphereVBO_) glDeleteBuffers(1, &sphereVBO_);
        if (sphereEBO_) glDeleteBuffers(1, &sphereEBO_);
        if (starfieldVAO_) glDeleteVertexArrays(1, &starfieldVAO_);
        if (starfieldVBO_) glDeleteBuffers(1, &starfieldVBO_);
        if (quadVAO_) glDeleteVertexArrays(1, &quadVAO_);
        if (quadVBO_) glDeleteBuffers(1, &quadVBO_);
        if (lineVAO_) glDeleteVertexArrays(1, &lineVAO_);
        if (lineVBO_) glDeleteBuffers(1, &lineVBO_);

        alienModel_.cleanup();
        handModel_.cleanup();

        if (mainShader_) glDeleteProgram(mainShader_);
        if (starfieldShader_) glDeleteProgram(starfieldShader_);
        if (freqShader_) glDeleteProgram(freqShader_);
        if (flatShader_) glDeleteProgram(flatShader_);

        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
    }
}

GLuint Renderer::compileShader(const std::string& source, GLenum type) {
    GLuint s = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);

    GLint success;
    glGetShaderiv(s, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(s, 512, nullptr, infoLog);
        std::cerr << "[Renderer] Shader Compilation Error (" 
                  << (type == GL_VERTEX_SHADER ? "VERT" : "FRAG") << "):\n"
                  << infoLog << "\n";
        glDeleteShader(s);
        return 0;
    }
    return s;
}

GLuint Renderer::linkProgram(GLuint vert, GLuint frag) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vert);
    glAttachShader(p, frag);

    // Bind standard layout attribute indices before linking for GLSL 130 / OpenGL 3.0 compatibility
    glBindAttribLocation(p, 0, "aPos");
    glBindAttribLocation(p, 1, "aNormal");

    glLinkProgram(p);

    GLint success;
    glGetProgramiv(p, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(p, 512, nullptr, infoLog);
        std::cerr << "[Renderer] Program Link Error:\n" << infoLog << "\n";
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

void Renderer::initShaders() {
    const std::string mainVertShader = R"(
        #version 150
        in vec3 aPos;
        in vec3 aNormal;

        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;

        out vec3 FragPos;
        out vec3 Normal;

        void main() {
            FragPos = vec3(model * vec4(aPos, 1.0));
            Normal = mat3(model) * aNormal;
            gl_Position = projection * view * model * vec4(aPos, 1.0);
        }
    )";

    const std::string mainFragShader = R"(
        #version 150
        out vec4 FragColor;

        in vec3 FragPos;
        in vec3 Normal;

        uniform vec3 objectColor;
        uniform vec3 emissiveColor;
        uniform float opacity;

        void main() {
            vec3 norm = normalize(Normal);
            vec3 lightDir = normalize(vec3(2.0, 4.0, 3.0));
            float diff = max(dot(norm, lightDir), 0.0);
            vec3 diffuse = diff * vec3(0.8, 0.6, 1.0) * objectColor;
            vec3 ambient = vec3(0.2, 0.25, 0.3) * objectColor;
            vec3 result = ambient + diffuse + emissiveColor;
            FragColor = vec4(result, opacity);
        }
    )";

    const std::string starVertShader = R"(
        #version 150
        in vec3 aPos;
        uniform mat4 view;
        uniform mat4 projection;
        uniform mat4 model;
        void main() {
            gl_Position = projection * view * model * vec4(aPos, 1.0);
            gl_PointSize = 12.0;
        }
    )";

    const std::string starFragShader = R"(
        #version 150
        out vec4 FragColor;
        uniform vec3 color;
        uniform float opacity;
        void main() {
            FragColor = vec4(color, opacity);
        }
    )";

    const std::string freqVertShader = R"(
        #version 150
        in vec2 aPos;
        out vec2 v_uv;
        void main() {
            v_uv = aPos * 0.5 + 0.5;
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )";

    const std::string freqFragShader = R"(
        #version 150
        out vec4 fragColor;
        in vec2 v_uv;
        uniform float u_time;
        uniform vec2 u_resolution;
        uniform vec2 u_offset;

        void main() {
            fragColor = vec4(0.0, 0.0, 0.0, 0.0);
            vec2 fragCoord = v_uv * u_resolution;
            
            // Expanding circle from centre
            float ramp = clamp(u_time / 7.0, 0.0, 1.0);
            float expand = ramp * ramp * (3.0 - 2.0 * ramp);
            vec2 centre = u_resolution * 0.5 + u_offset;
            float maxRadius = length(u_resolution) * 0.8;
            float dist = length(fragCoord - centre);
            float edgeR = expand * maxRadius;
            float edge = smoothstep(edgeR, edgeR - 60.0, dist);
            if (edge < 0.01) { fragColor = vec4(0.0, 0.0, 0.0, 0.0); return; }

            float norm = 0.5 + 0.15 * sin(u_time * 0.3) + 0.08 * sin(u_time * 0.71);
            int count = int(24.0 * norm);
            if (count > 32) count = 32;
            if (count < 1) count = 1;
 
            vec4 accum = vec4(0.0);
            vec2 R = u_resolution;
            vec2 baseU = (fragCoord * 2.0 - R - 2.0) / R.x;
            float invRx = 0.25 / R.x;

            for (int s = 0; s < 32; s++) {
                if (s >= count) break;
                vec2 offset = vec2(float(s % 8), float(s / 8)) * invRx;
                vec2 u2 = baseU + offset;
                u2 = floor((6.0 - vec2(atan(u2.y, u2.x) / 3.0, length(u2))) * R) + 0.5;
                accum += max(
                    1.0 - fract(vec4(7.0, 6.0, 4.0, 0.0) * 0.02
                        + (u2.y * 0.02 + u2.x * 0.4) * fract(u2.x * 0.61)
                        + u_time) * 5.0,
                    0.0) / 12.0;
            }
            fragColor = accum * edge;
        }
    )";

    GLuint vMain = compileShader(mainVertShader, GL_VERTEX_SHADER);
    GLuint fMain = compileShader(mainFragShader, GL_FRAGMENT_SHADER);
    mainShader_ = linkProgram(vMain, fMain);
    glDeleteShader(vMain);
    glDeleteShader(fMain);

    GLuint vStar = compileShader(starVertShader, GL_VERTEX_SHADER);
    GLuint fStar = compileShader(starFragShader, GL_FRAGMENT_SHADER);
    starfieldShader_ = linkProgram(vStar, fStar);
    glDeleteShader(vStar);
    glDeleteShader(fStar);

    GLuint vFreq = compileShader(freqVertShader, GL_VERTEX_SHADER);
    GLuint fFreq = compileShader(freqFragShader, GL_FRAGMENT_SHADER);
    freqShader_ = linkProgram(vFreq, fFreq);
    glDeleteShader(vFreq);
    glDeleteShader(fFreq);

    const std::string flatVertShader = R"(
        #version 150
        in vec2 aPos;
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )";

    const std::string flatFragShader = R"(
        #version 150
        out vec4 fragColor;
        uniform float u_opacity;
        void main() {
            fragColor = vec4(0.0, 0.0, 0.0, u_opacity);
        }
    )";

    GLuint vFlat = compileShader(flatVertShader, GL_VERTEX_SHADER);
    GLuint fFlat = compileShader(flatFragShader, GL_FRAGMENT_SHADER);
    flatShader_ = linkProgram(vFlat, fFlat);
    glDeleteShader(vFlat);
    glDeleteShader(fFlat);
}

void Renderer::initMeshes() {
    // 1. Cube
    glGenVertexArrays(1, &cubeVAO_);
    glGenBuffers(1, &cubeVBO_);
    glBindVertexArray(cubeVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // 1b. Cube Edges (for wireframe rendering without diagonals)
    glGenVertexArrays(1, &cubeEdgesVAO_);
    glGenBuffers(1, &cubeEdgesVBO_);
    glBindVertexArray(cubeEdgesVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, cubeEdgesVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeEdgesVertices), cubeEdgesVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // 2. Sphere
    std::vector<float> sphereVerts;
    std::vector<unsigned int> sphereInds;
    createSphere(1.0f, 24, 24, sphereVerts, sphereInds);
    sphereIndexCount_ = sphereInds.size();

    glGenVertexArrays(1, &sphereVAO_);
    glGenBuffers(1, &sphereVBO_);
    glGenBuffers(1, &sphereEBO_);

    glBindVertexArray(sphereVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, sphereVBO_);
    glBufferData(GL_ARRAY_BUFFER, sphereVerts.size() * sizeof(float), sphereVerts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEBO_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sphereInds.size() * sizeof(unsigned int), sphereInds.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // 3. Starfield
    std::vector<float> starCoords;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-20.0f, 20.0f);
    for (int i = 0; i < 4000; i++) {
        starCoords.push_back(dis(gen));
        starCoords.push_back(dis(gen));
        starCoords.push_back(dis(gen));
    }

    glGenVertexArrays(1, &starfieldVAO_);
    glGenBuffers(1, &starfieldVBO_);
    glBindVertexArray(starfieldVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, starfieldVBO_);
    glBufferData(GL_ARRAY_BUFFER, starCoords.size() * sizeof(float), starCoords.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // 4. Quad
    float quadVerts[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
        -1.0f,  1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f
    };
    glGenVertexArrays(1, &quadVAO_);
    glGenBuffers(1, &quadVBO_);
    glBindVertexArray(quadVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // 5. Line
    glGenVertexArrays(1, &lineVAO_);
    glGenBuffers(1, &lineVBO_);
}

void Renderer::drawCube(const glm::mat4& model, const glm::vec3& color, const glm::vec3& emissive, float opacity, bool wireframe) {
    glUseProgram(mainShader_);
    glUniformMatrix4fv(glGetUniformLocation(mainShader_, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform3fv(glGetUniformLocation(mainShader_, "objectColor"), 1, glm::value_ptr(color));
    glUniform3fv(glGetUniformLocation(mainShader_, "emissiveColor"), 1, glm::value_ptr(emissive));
    glUniform1f(glGetUniformLocation(mainShader_, "opacity"), opacity);

    if (wireframe) {
        glBindVertexArray(cubeEdgesVAO_);
        glDrawArrays(GL_LINES, 0, 24);
    } else {
        glBindVertexArray(cubeVAO_);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
}

void Renderer::drawSphere(const glm::mat4& model, const glm::vec3& color, const glm::vec3& emissive, float opacity) {
    glUseProgram(mainShader_);
    glUniformMatrix4fv(glGetUniformLocation(mainShader_, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform3fv(glGetUniformLocation(mainShader_, "objectColor"), 1, glm::value_ptr(color));
    glUniform3fv(glGetUniformLocation(mainShader_, "emissiveColor"), 1, glm::value_ptr(emissive));
    glUniform1f(glGetUniformLocation(mainShader_, "opacity"), opacity);

    glBindVertexArray(sphereVAO_);
    glDrawElements(GL_TRIANGLES, sphereIndexCount_, GL_UNSIGNED_INT, 0);
}

void Renderer::drawRing(const glm::vec3& center, float radius, const glm::vec3& color, float opacity) {
    std::vector<float> lineVerts;
    int segments = 64;
    for (int i = 0; i <= segments; i++) {
        float theta = 2.0f * M_PI * float(i) / float(segments);
        lineVerts.push_back(center.x + radius * std::cos(theta));
        lineVerts.push_back(center.y + radius * std::sin(theta));
        lineVerts.push_back(center.z);
    }

    glUseProgram(starfieldShader_);
    glUniformMatrix4fv(glGetUniformLocation(starfieldShader_, "model"), 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
    glUniform3fv(glGetUniformLocation(starfieldShader_, "color"), 1, glm::value_ptr(color));
    glUniform1f(glGetUniformLocation(starfieldShader_, "opacity"), opacity);

    glBindVertexArray(lineVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO_);
    glBufferData(GL_ARRAY_BUFFER, lineVerts.size() * sizeof(float), lineVerts.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glDrawArrays(GL_LINE_STRIP, 0, segments + 1);
}

bool Renderer::init(int width, int height, bool fullscreen, OscSender& osc, const Config& cfg) {
    width_ = width;
    height_ = height;
    fullscreen_ = fullscreen;
    osc_ = &osc;
    cfg_ = &cfg;

    // Set default song and ensure backing track is muted on boot
    if (osc_) {
        osc_->setActiveSong(6);
        osc_->sendMuteBacking(true);
    }

    if (!glfwInit()) {
        std::cerr << "[Renderer] Failed to initialize GLFW\n";
        return false;
    }

#ifdef PLATFORM_MAC
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif
    glfwWindowHint(GLFW_SAMPLES, 4); // Enable 4x MSAA

    GLFWmonitor* monitor = nullptr;
    if (fullscreen_) {
        monitor = glfwGetPrimaryMonitor();
        if (monitor) {
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            if (mode) {
                if (width_ <= 0 || height_ <= 0) {
                    width_ = mode->width;
                    height_ = mode->height;
                }
                glfwWindowHint(GLFW_RED_BITS, mode->redBits);
                glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
                glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
                glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
            }
        }
    }

    window_ = glfwCreateWindow(width_, height_, "Spacehands - Transformation Station", monitor, nullptr);
    if (!window_) {
        const char* description = nullptr;
        int errorCode = glfwGetError(&description);
        std::cerr << "[Renderer] Failed to create GLFW window. GLFW Error Code: " << errorCode 
                  << ", Description: " << (description ? description : "None") << "\n";
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1); // Enable V-Sync

    // Set key callback to handle Ctrl+Q for clean shutdown
    glfwSetKeyCallback(window_, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_Q && action == GLFW_PRESS && (mods & GLFW_MOD_CONTROL)) {
            std::cout << "[Renderer] Ctrl+Q pressed — initiating shutdown\n";
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    });

    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "[Renderer] Warning: glewInit() failed with error: " 
                  << glewGetErrorString(err) << ". Continuing anyway...\n";
    }

    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window_, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_PROGRAM_POINT_SIZE);

    glEnable(GL_MULTISAMPLE);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glLineWidth(2.5f);

    initShaders();
    initMeshes();

    // Load GLB Models
    hasAlienModel_ = loadGLBModel("assets/models/faces.glb", alienModel_);
    if (!hasAlienModel_) {
        std::cerr << "[Renderer] Warning: failed to load assets/models/faces.glb\n";
    }
    hasHandModel_ = loadGLBModel("assets/models/hand.glb", handModel_);
    if (!hasHandModel_) {
        std::cerr << "[Renderer] Warning: failed to load assets/models/hand.glb\n";
    }

    for (int i = 0; i < NUM_NODES; i++) {
        float phi = std::acos(1.0f - 2.0f * (i + 0.5f) / NUM_NODES);
        float theta = M_PI * (1.0f + std::sqrt(5.0f)) * i;
        sphereBasePositions_.push_back(glm::vec3(
            std::sin(phi) * std::cos(theta),
            std::cos(phi),
            std::sin(phi) * std::sin(theta)
        ));
        gridPositions_.push_back(glm::vec3(0.0f));
    }

    resetGame();
    return true;
}

void Renderer::resetGame() {
    std::vector<int> pool(NUM_NODES);
    std::iota(pool.begin(), pool.end(), 0);
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(pool.begin(), pool.end(), g);
    
    sequence_.assign(pool.begin(), pool.begin() + 6); // NUM_NOTES = 6
    collected_ = 0;
    holdTimer_ = 0.0f;
    idleTimer_ = 0.0f;
    hasFrozenHand_ = false;
    hasSeenPreview_ = false;
    lastPreviewTime_ = -999.0f;
    previewStep_ = 0;
    previewTimer_ = 0.0f;
    sequenceAge_ = 0.0f;
    state_ = VisualizerState::IDLE;
    if (osc_) {
        osc_->sendMuteBacking(true);
    }
    osc_->setPreviewMute(false);

    updateMelody();
}

void Renderer::updateMelody() {
    if (collected_ == 0) {
        osc_->sendTidalCtrl("transformation_active", 0.0f);
        return;
    }
    std::string notes = "";
    for (int i = 0; i < collected_; i++) {
        if (i > 0) notes += " ";
        notes += NOTE_NAMES[sequence_[i]];
    }
    osc_->sendTidalCtrlStr("transformation_notes", notes.c_str());
    osc_->sendTidalCtrl("transformation_active", 1.0f);
    osc_->sendTidalCtrl("transformation_count", (float)collected_);
}

void Renderer::updateGridPositions(float dt) {
    sphereRotY_ += 0.04f * dt;
    float cosR = std::cos(sphereRotY_);
    float sinR = std::sin(sphereRotY_);
    for (int i = 0; i < NUM_NODES; i++) {
        const auto& b = sphereBasePositions_[i];
        gridPositions_[i] = glm::vec3(
            b.x * cosR + b.z * sinR,
            b.y,
            -b.x * sinR + b.z * cosR
        ) * SPHERE_RADIUS;
    }
}

glm::vec3 Renderer::handWorldPos(float hx, float hy, float hz, float aspect) {
    float fovY = glm::radians(55.0f);
    float halfH = std::tan(fovY / 2.0f) * 4.5f;
    float halfW = halfH * aspect;

    float totalW = (float)width_ + (float)cfg_->crop_left + (float)cfg_->crop_right;
    float hxAdj = (totalW > 0.0f) ? (hx * totalW - (float)cfg_->crop_left) / (float)width_ : hx;

    float mx = (hxAdj - 0.5f) * halfW * 2.0f;
    float my = -(hy - 0.5f) * halfH * 2.0f;
    float wz = (hz - 0.5f) * SPHERE_RADIUS * 2.5f;
    return glm::vec3(mx, my, wz);
}

void Renderer::burstParticles(const glm::vec3& pos) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> velX(-1.5f, 1.5f);
    std::uniform_real_distribution<float> velY(2.5f, 6.0f); // upward sparks!
    std::uniform_real_distribution<float> velZ(-1.5f, 1.5f);
    for (int i = 0; i < 25; i++) {
        VisualizerParticle p;
        p.pos = pos;
        p.vel = glm::vec3(velX(gen), velY(gen), velZ(gen));
        p.life = 1.0f;
        particles_.push_back(p);
    }

}

void Renderer::updateCellVisuals(int i, glm::vec3& color, glm::vec3& emissive, float& opacity, float& scale, bool& drawWireframe) {
    auto it = std::find(sequence_.begin(), sequence_.end(), i);
    int seqPos = (it != sequence_.end()) ? std::distance(sequence_.begin(), it) : -1;

    bool isDecoy = (seqPos == -1);
    bool isCollected = (seqPos != -1 && seqPos < collected_);
    bool isActive = (seqPos == collected_ && state_ == VisualizerState::PLAYING);

    scale = 1.0f;
    drawWireframe = true;

    if (isCollected) {
        color = glm::vec3(0.0f, 0.06f, 0.13f);
        emissive = glm::vec3(0.0f, 0.03f, 0.07f);
        opacity = 0.2f;
        scale = 0.8f;
    } else if (isActive) {
        bool highlight = true;
        if (cfg_ && !cfg_->show_hints && collected_ > 0) {
            highlight = false;
        }
        if (highlight) {
            color = glm::vec3(1.0f, 1.0f, 1.0f);
            emissive = glm::vec3(0.26f, 0.13f, 0.66f);
            opacity = 0.9f;
        } else {
            if (cfg_ && !cfg_->show_hints) {
                color = glm::vec3(0.04f, 0.06f, 0.2f);
                emissive = glm::vec3(0.02f, 0.03f, 0.09f);
                opacity = 0.35f;
            } else {
                color = glm::vec3(0.1f, 0.13f, 0.4f);
                emissive = glm::vec3(0.04f, 0.06f, 0.26f);
                opacity = 0.5f;
            }
        }
    } else if (isDecoy) {
        color = glm::vec3(0.3f, 0.45f, 0.8f);
        emissive = glm::vec3(0.12f, 0.18f, 0.4f);
        opacity = 0.8f;
    } else {
        if (cfg_ && !cfg_->show_hints) {
            color = glm::vec3(0.3f, 0.45f, 0.8f);
            emissive = glm::vec3(0.12f, 0.18f, 0.4f);
            opacity = 0.8f;
        } else {
            color = glm::vec3(0.5f, 0.65f, 1.0f);
            emissive = glm::vec3(0.2f, 0.3f, 0.6f);
            opacity = 0.9f;
        }
    }
}

void Renderer::render(const HandList& hands, const std::vector<TargetSpawn>& spawns, float dt) {
    // All 10 lanes are active by default to support 10-note melodic songs mapping
    int activeLanes = 10;
    // travelTime matches cycle duration (1.0 / 0.5375) at 129 BPM (Here Comes The Sun) for perfect downbeat sync
    float travelTime = 1.8605f;
    int currentLevel = 4;

    // 10 distinct 3D receptor coordinates forming a vertical sloped triangle (4, 3, 2, 1)
    std::array<glm::vec3, 10> receptors = {
        glm::vec3(-1.5f, -0.8f, 0.0f),  // Row 4, 0: Green (Low-Far-Left)
        glm::vec3(-0.5f, -0.8f, 0.0f),  // Row 4, 1: Red (Low-Far-Mid-Left)
        glm::vec3(0.5f, -0.8f, 0.0f),   // Row 4, 2: Yellow (Low-Far-Mid-Right)
        glm::vec3(1.5f, -0.8f, 0.0f),   // Row 4, 3: Blue (Low-Far-Right)
        
        glm::vec3(-1.0f, -0.1f, 0.3f),  // Row 3, 4: Cyan (Mid-Lower-Left)
        glm::vec3(0.0f, -0.1f, 0.3f),   // Row 3, 5: Magenta (Mid-Lower-Center)
        glm::vec3(1.0f, -0.1f, 0.3f),   // Row 3, 6: Orange (Mid-Lower-Right)
        
        glm::vec3(-0.5f, 0.5f, 0.6f),   // Row 2, 7: Purple (Mid-Upper-Left)
        glm::vec3(0.5f, 0.5f, 0.6f),    // Row 2, 8: Pink (Mid-Upper-Right)
        
        glm::vec3(0.0f, 1.1f, 0.9f)     // Row 1, 9: Gold/White (Top Peak)
    };

    std::array<glm::vec3, 10> laneColors = {
        glm::vec3(0.1f, 0.95f, 0.2f),   // 0: Green
        glm::vec3(1.0f, 0.15f, 0.15f),  // 1: Red
        glm::vec3(1.0f, 0.85f, 0.05f),  // 2: Yellow
        glm::vec3(0.15f, 0.55f, 1.0f),  // 3: Blue
        
        glm::vec3(0.0f, 1.0f, 1.0f),    // 4: Cyan
        glm::vec3(1.0f, 0.0f, 1.0f),    // 5: Magenta
        glm::vec3(1.0f, 0.5f, 0.0f),    // 6: Orange
        
        glm::vec3(0.6f, 0.2f, 1.0f),    // 7: Purple
        glm::vec3(1.0f, 0.4f, 0.7f),    // 8: Pink
        
        glm::vec3(0.95f, 0.9f, 1.0f)    // 9: Gold/White
    };

    // State machine updates
    if (state_ == VisualizerState::IDLE) {
        notes_.clear();
        // Wait for first hand to start game
        if (!hands.empty()) {
            state_ = VisualizerState::PLAYING;
            climaxTimer_ = 0.0f; // song progress timer
            idleTimer_ = 0.0f;   // hand-loss timeout timer
            score_ = 0;
            combo_ = 0;
            if (osc_) {
                osc_->sendReflexHush(); // play hush first!
                osc_->setReflexActive(true);
                osc_->setReflexCps(0.3f);
                osc_->sendTidalCtrl("reflex_active", 1.0f);
                osc_->sendTidalCtrl("reflex_song_6", 1.0f);
                osc_->sendTidalCtrl("reflex_cps", 0.3f);
                osc_->sendTidalCtrlStr("reflex_notes", "<[~@2 b4 a4@2 b4@2 g4] [~@2 b4 g4 a4 b4 ~@2] [~@2 b4 a4@2 b4@2 g4] [~@2 d5 b4@2 a4 ~@2] [~ b4@2 a4@2 g4@2 ~] [g4@2 b4 d5@2 b4@2 g4] [a4@2 c5 b4@2 a4@2 fs4] [g4@2 a4 b4@2 d5@2 ~]>");
                osc_->sendMuteBacking(false);
            }
        }
    } else if (state_ == VisualizerState::PLAYING) {
        climaxTimer_ += dt; // tracks song playtime
        
        // 20s hand-loss timeout check
        if (hands.empty()) {
            idleTimer_ += dt;
            if (idleTimer_ >= 20.0f) {
                state_ = VisualizerState::IDLE;
                if (osc_) {
                    osc_->sendTidalCtrl("reflex_active", 0.0f);
                    osc_->sendTidalCtrl("reflex_song_6", 0.0f);
                    osc_->sendTidalCtrlStr("reflex_notes", "~");
                    osc_->sendMuteBacking(true);
                    osc_->sendReflexHush(); // hush on timeout
                }
            }
        } else {
            idleTimer_ = 0.0f;
        }

        // Song end check (length is 60 seconds at 129 BPM)
        if (climaxTimer_ >= 60.0f) {
            state_ = VisualizerState::CONGRATULATIONS;
            congratsTimer_ = 0.0f;
            if (osc_) {
                osc_->sendTidalCtrl("reflex_active", 0.0f);
                osc_->sendTidalCtrl("reflex_song_6", 0.0f);
                osc_->sendTidalCtrlStr("reflex_notes", "~");
                osc_->sendMuteBacking(true);
                osc_->sendReflexHush(); // hush at song end
            }
        }
    } else if (state_ == VisualizerState::CONGRATULATIONS) {
        congratsTimer_ += dt;
        if (congratsTimer_ >= 6.0f) {
            state_ = VisualizerState::IDLE;
        }
    }

    // Process new spawns (only if playing)
    if (state_ == VisualizerState::PLAYING) {
        for (const auto& spawn : spawns) {
            VisualizerNote note;
            
            // Map the raw midi note to the closest lane spatially (left-to-right)
            static const int LANE_PITCHES[10] = { 50, 52, 54, 55, 57, 59, 60, 62, 64, 66 };
            static const int SORTED_LANES[10] = { 0, 4, 7, 1, 9, 5, 8, 2, 6, 3 };
            int closestDiff = 999;
            int closestPitchIdx = 0;
            int midiVal = spawn.midi;
            for (int i = 0; i < 10; ++i) {
                int diff = std::abs(LANE_PITCHES[i] - midiVal);
                if (diff < closestDiff) {
                    closestDiff = diff;
                    closestPitchIdx = i;
                }
            }
            note.lane = SORTED_LANES[closestPitchIdx];
            
            note.hand = spawn.hand;
            note.progress = 0.0f;
            note.hit = false;
            note.playCustom = spawn.playCustom;
            note.midi = spawn.midi;
            note.gain = spawn.gain;
            note.sustain = spawn.sustain;
            note.instrument = spawn.instrument;
            notes_.push_back(note);
        }
    }

    // Fallback spawner (disabled to prevent random cues from clashing with the melody)
    static float lastOscTime = 999.0f;
    if (!spawns.empty()) {
        lastOscTime = 0.0f;
    } else {
        lastOscTime += dt;
    }

    static float spawnTimer = 0.0f;
    if (false && state_ == VisualizerState::PLAYING && lastOscTime > 3.0f) {
        spawnTimer += dt;
        if (spawnTimer >= 0.5f) {
            spawnTimer = 0.0f;
            if (rand() % 100 < 65) {
                VisualizerNote note;
                note.lane = rand() % 10;
                note.hand = rand() % 2;
                note.progress = 0.0f;
                note.hit = false;
                notes_.push_back(note);
            }
        }
    }

    // Window size update
    glfwGetWindowSize(window_, &width_, &height_);
    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window_, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    float aspect = (float)width_ / (float)height_;

    // Deep space dark background clear
    glClearColor(0.01f, 0.01f, 0.04f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Matrices
    glm::mat4 projection = glm::perspective(glm::radians(55.0f), aspect, 0.01f, 100.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.5f, 4.5f), glm::vec3(0.0f, 0.5f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    glUseProgram(mainShader_);
    glUniformMatrix4fv(glGetUniformLocation(mainShader_, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(mainShader_, "view"), 1, GL_FALSE, glm::value_ptr(view));

    glUseProgram(starfieldShader_);
    glUniformMatrix4fv(glGetUniformLocation(starfieldShader_, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(starfieldShader_, "view"), 1, GL_FALSE, glm::value_ptr(view));

    // Render Starfield background (cosmic space dust)
    glUseProgram(starfieldShader_);
    glm::mat4 starModel = glm::rotate(glm::mat4(1.0f), (float)glfwGetTime() * 0.015f, glm::vec3(0.0f, 1.0f, 0.0f));
    glUniformMatrix4fv(glGetUniformLocation(starfieldShader_, "model"), 1, GL_FALSE, glm::value_ptr(starModel));
    glUniform3f(glGetUniformLocation(starfieldShader_, "color"), 0.2f, 0.5f, 0.9f);
    glUniform1f(glGetUniformLocation(starfieldShader_, "opacity"), 0.6f);
    glBindVertexArray(starfieldVAO_);
    glDrawArrays(GL_POINTS, 0, 4000);

    // Helper: draw flat ring lying in XZ plane
    auto drawFlatRing = [&](const glm::vec3& center, float radius, const glm::vec3& color, float opacity) {
        std::vector<float> lineVerts;
        int segments = 64;
        for (int i = 0; i <= segments; i++) {
            float theta = 2.0f * M_PI * float(i) / float(segments);
            lineVerts.push_back(center.x + radius * std::cos(theta));
            lineVerts.push_back(center.y);
            lineVerts.push_back(center.z + radius * std::sin(theta));
        }

        glUseProgram(starfieldShader_);
        glUniformMatrix4fv(glGetUniformLocation(starfieldShader_, "model"), 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
        glUniform3fv(glGetUniformLocation(starfieldShader_, "color"), 1, glm::value_ptr(color));
        glUniform1f(glGetUniformLocation(starfieldShader_, "opacity"), opacity);

        glBindVertexArray(lineVAO_);
        glBindBuffer(GL_ARRAY_BUFFER, lineVBO_);
        glBufferData(GL_ARRAY_BUFFER, lineVerts.size() * sizeof(float), lineVerts.data(), GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glDrawArrays(GL_LINE_STRIP, 0, segments + 1);
    };

    // Render State Drawing
    if (state_ == VisualizerState::IDLE) {
        // Idle state: show single large rotating faces.glb head in the middle of the screen
        if (hasAlienModel_) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(0.0f, 0.5f, 1.2f));
            
            float angle = glfwGetTime() * 1.0f;
            model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, angle * 0.25f, glm::vec3(1.0f, 0.0f, 0.05f));
            model = glm::scale(model, glm::vec3(0.95f));

            float pulse = 0.85f + 0.15f * std::sin(glfwGetTime() * 2.0f);
            glm::vec3 col(0.95f, 0.9f, 0.6f); // Warm Gold
            drawModel(alienModel_, model, col * pulse, col * 0.4f, 0.9f);
        }
    } else if (state_ == VisualizerState::PLAYING) {
        // Draw sloped Guitar Hero Fretboard highway lines branching into 3D lanes!
        glUseProgram(flatShader_);
        glUniform1f(glGetUniformLocation(flatShader_, "u_opacity"), 0.2f);
        
        float horizonZ = -10.0f;
        float horizonY = 1.2f;
        std::array<float, 10> horizonX = {
            -0.3f, -0.1f, 0.1f, 0.3f,   // Row 4 (0-3)
            -0.2f, 0.0f, 0.2f,          // Row 3 (4-6)
            -0.1f, 0.1f,                // Row 2 (7-8)
            0.0f                        // Row 1 (9)
        };

        std::vector<float> boardVerts;
        float fretPhase = glm::fract(glfwGetTime() * 0.35f);

        for (int i = 0; i < 10; i++) {
            // Track line
            boardVerts.push_back(horizonX[i]); boardVerts.push_back(horizonY); boardVerts.push_back(horizonZ);
            boardVerts.push_back(receptors[i].x); boardVerts.push_back(receptors[i].y); boardVerts.push_back(receptors[i].z);
            
            // Scrolling fret indicators
            for (int f = 0; f < 5; f++) {
                float ft = glm::fract(fretPhase + (float)f / 5.0f);
                float fx = glm::mix(horizonX[i], receptors[i].x, ft);
                float fy = glm::mix(horizonY, receptors[i].y, ft);
                float fz = glm::mix(horizonZ, receptors[i].z, ft);
                
                boardVerts.push_back(fx - 0.1f * (1.0f - ft)); boardVerts.push_back(fy); boardVerts.push_back(fz);
                boardVerts.push_back(fx + 0.1f * (1.0f - ft)); boardVerts.push_back(fy); boardVerts.push_back(fz);
            }
        }

        GLuint boardVAO, boardVBO;
        glGenVertexArrays(1, &boardVAO);
        glGenBuffers(1, &boardVBO);
        glBindVertexArray(boardVAO);
        glBindBuffer(GL_ARRAY_BUFFER, boardVBO);
        glBufferData(GL_ARRAY_BUFFER, boardVerts.size() * sizeof(float), boardVerts.data(), GL_STREAM_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_LINES, 0, boardVerts.size() / 3);
        glDeleteBuffers(1, &boardVBO);
        glDeleteVertexArrays(1, &boardVAO);

        // Static variables for receptor hit flames
        static std::array<float, 10> flameIntensity = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

        // Update active notes, check collision, and trigger sparks/flames on hit
        for (auto it = notes_.begin(); it != notes_.end(); ) {
            it->progress += dt / travelTime;

            float t = std::min(it->progress, 1.2f);
            float nx = glm::mix(horizonX[it->lane], receptors[it->lane].x, t);
            float ny = glm::mix(horizonY, receptors[it->lane].y, t);
            float nz = glm::mix(horizonZ, receptors[it->lane].z, t);
            it->pos = glm::vec3(nx, ny, nz);

            if (std::abs(it->progress - 1.0f) < 0.18f && !it->hit) {
                for (const auto& h : hands) {
                    float mx = cfg_->mirror_x ? (1.0f - h.x) : h.x;
                    glm::vec3 handPos = handWorldPos(mx, h.y, h.z, aspect);
                    
                    float dx = std::abs(handPos.x - receptors[it->lane].x);
                    float dy = std::abs(handPos.y - receptors[it->lane].y);
                    float dz = std::abs(handPos.z - receptors[it->lane].z);
                    
                    if (dx < 1.15f && dy < 1.05f && dz < 2.5f) {
                        it->hit = true;
                        combo_++;
                        score_ += 100 * combo_;
                        
                        flameIntensity[it->lane] = 1.0f;
                        burstParticles(receptors[it->lane]);
                        
                        if (osc_) {
                            if (it->playCustom) {
                                osc_->sendDirtPlay(it->midi, it->gain, it->sustain, it->instrument, 0);
                            } else if (osc_->getActiveSong() == 6) {
                                // Map target's visual lane directly to the correct G Major diatonic vocal note pitch!
                                int midi = 55; // Fallback G4
                                switch (it->lane) {
                                    case 0:  midi = 50; break; // D4
                                    case 1:  midi = 52; break; // E4
                                    case 2:  midi = 54; break; // F#4
                                    case 3:  midi = 55; break; // G4
                                    case 4:  midi = 57; break; // A4
                                    case 5:  midi = 59; break; // B4
                                    case 6:  midi = 60; break; // C5
                                    case 7:  midi = 62; break; // D5
                                    case 8:  midi = 64; break; // E5
                                    case 9:  midi = 66; break; // F#5
                                    default: midi = 55; break;
                                }
                                osc_->sendDirtPlay(midi, 1.25f, 0.65f, "supermandolin", 11);
                            } else {
                                osc_->sendDirtPlay(PENTA[it->lane * 3 % 12], 0.85f, 0.45f);
                            }
                        }
                        break;
                    }
                }
            }

            if (it->progress > 1.2f) {
                if (!it->hit) {
                    combo_ = 0;
                }
                it = notes_.erase(it);
            } else {
                ++it;
            }
        }

        // Draw Receptors
        for (int i = 0; i < 10; i++) {
            glm::vec3 col = laneColors[i];
            float opacity = 0.8f;
            float vOpacity = 0.5f;
            float radius = 0.35f + 0.03f * std::sin(glfwGetTime() * 10.0f);
            
            drawFlatRing(receptors[i], radius, col, opacity);
            drawRing(receptors[i], radius * 0.7f, col * 0.7f, vOpacity);
        }

        // Draw Receptor hit flames
        for (int i = 0; i < 10; i++) {
            flameIntensity[i] = std::max(flameIntensity[i] - dt * 3.5f, 0.0f);
            if (flameIntensity[i] > 0.01f) {
                glm::mat4 model = glm::translate(glm::mat4(1.0f), receptors[i] + glm::vec3(0.0f, 0.4f * flameIntensity[i], 0.0f));
                model = glm::scale(model, glm::vec3(0.4f, 0.9f * flameIntensity[i], 0.4f));
                glm::vec3 fColor = laneColors[i] + glm::vec3(0.4f);
                drawSphere(model, fColor, fColor * 0.8f, 0.75f * flameIntensity[i]);
            }
        }

        // Draw Scrolling Notes (tumbling faces.glb model)
        for (const auto& note : notes_) {
            if (note.hit) continue;

            glm::vec3 col = laneColors[note.lane];
            float noteScale = glm::mix(0.05f, 0.20f, std::min(note.progress, 1.0f));

            if (hasAlienModel_) {
                glm::mat4 model = glm::translate(glm::mat4(1.0f), note.pos);
                
                float angle = glfwGetTime() * 2.5f + note.progress * 4.0f;
                model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::rotate(model, angle * 0.4f, glm::vec3(1.0f, 0.0f, 0.1f));
                
                float scaleVal = noteScale * 0.32f;
                model = glm::scale(model, glm::vec3(scaleVal));
                
                drawModel(alienModel_, model, col, col * 0.4f, 0.95f);
            } else {
                glm::mat4 model = glm::translate(glm::mat4(1.0f), note.pos);
                model = glm::scale(model, glm::vec3(noteScale, noteScale * 0.3f, noteScale));
                drawSphere(model, col, col * 0.6f, 0.95f);
            }
        }
    } else if (state_ == VisualizerState::CONGRATULATIONS) {
        // Fireworks state: spawn new explosions periodically
        static float fireworkTimer = 0.0f;
        fireworkTimer += dt;
        if (fireworkTimer >= 0.32f) {
            fireworkTimer = 0.0f;
            
            // Random coordinates across screen field
            float fx = ((rand() % 200) - 100) / 45.0f;  // -2.2 to 2.2
            float fy = ((rand() % 150) - 50) / 45.0f;   // -1.1 to 2.2
            float fz = ((rand() % 100) - 50) / 45.0f;   // -1.1 to 1.1

            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<float> velX(-3.5f, 3.5f);
            std::uniform_real_distribution<float> velY(-3.5f, 3.5f);
            std::uniform_real_distribution<float> velZ(-3.5f, 3.5f);
            for (int i = 0; i < 45; i++) {
                VisualizerParticle p;
                p.pos = glm::vec3(fx, fy, fz);
                p.vel = glm::vec3(velX(gen), velY(gen), velZ(gen));
                p.life = 1.4f;
                particles_.push_back(p);
            }
        }
    }

    // Update and Render Sparks/Particles from Hits & Fireworks!
    std::vector<float> particleCoords;
    for (auto it = particles_.begin(); it != particles_.end(); ) {
        it->life -= dt * 1.5f;
        if (it->life <= 0.0f) {
            it = particles_.erase(it);
        } else {
            it->pos += it->vel * dt;
            particleCoords.push_back(it->pos.x);
            particleCoords.push_back(it->pos.y);
            particleCoords.push_back(it->pos.z);
            ++it;
        }
    }

    if (!particleCoords.empty()) {
        glUseProgram(starfieldShader_);
        glUniformMatrix4fv(glGetUniformLocation(starfieldShader_, "model"), 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
        
        // Beautiful rainbow shifting color for fireworks and hits!
        float r = 0.5f + 0.5f * std::sin(glfwGetTime() * 4.0f);
        float g = 0.5f + 0.5f * std::sin(glfwGetTime() * 5.0f);
        float b = 0.5f + 0.5f * std::sin(glfwGetTime() * 6.0f);
        glUniform3f(glGetUniformLocation(starfieldShader_, "color"), r, g, b);
        glUniform1f(glGetUniformLocation(starfieldShader_, "opacity"), 0.95f);

        GLuint partVAO, partVBO;
        glGenVertexArrays(1, &partVAO);
        glGenBuffers(1, &partVBO);
        glBindVertexArray(partVAO);
        glBindBuffer(GL_ARRAY_BUFFER, partVBO);
        glBufferData(GL_ARRAY_BUFFER, particleCoords.size() * sizeof(float), particleCoords.data(), GL_STREAM_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glDrawArrays(GL_POINTS, 0, particleCoords.size() / 3);

        glDeleteBuffers(1, &partVBO);
        glDeleteVertexArrays(1, &partVAO);
    }

    // Render Hand Cursors (Neon gold/purple spheres)
    for (size_t i = 0; i < hands.size(); i++) {
        float mx = cfg_->mirror_x ? (1.0f - hands[i].x) : hands[i].x;
        glm::vec3 hp = handWorldPos(mx, hands[i].y, hands[i].z, aspect);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, hp);
        model = glm::rotate(model, hp.x * 0.2f, glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, hp.y * 0.15f, glm::vec3(1.0f, 0.0f, 0.0f));

        glm::vec3 hColor(1.0f, 0.75f, 0.15f); // Neon Gold
        if (i == 1) hColor = glm::vec3(1.0f, 0.25f, 0.9f); // Hand 2: Neon Purple
        
        if (hasHandModel_) {
            model = glm::scale(model, glm::vec3(0.35f));
            drawModel(handModel_, model, hColor, hColor * 0.4f, 0.95f);
        } else {
            model = glm::scale(model, glm::vec3(0.18f));
            drawSphere(model, hColor, hColor * 0.4f, 0.95f);
        }
    }

    // Render V-Sync swaps and process GLFW inputs
    glfwSwapBuffers(window_);
    glfwPollEvents();

}

void GLBModel::draw() const {
    for (const auto& mesh : meshes) {
        glBindVertexArray(mesh.vao);
        glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
    }
}

void GLBModel::cleanup() {
    for (auto& mesh : meshes) {
        if (mesh.vao) glDeleteVertexArrays(1, &mesh.vao);
        if (mesh.vbo) glDeleteBuffers(1, &mesh.vbo);
        if (mesh.ebo) glDeleteBuffers(1, &mesh.ebo);
    }
    meshes.clear();
}

void Renderer::drawModel(const GLBModel& model, const glm::mat4& modelMatrix, const glm::vec3& color, const glm::vec3& emissive, float opacity) {
    glUseProgram(mainShader_);
    glUniformMatrix4fv(glGetUniformLocation(mainShader_, "model"), 1, GL_FALSE, glm::value_ptr(modelMatrix));
    glUniform3fv(glGetUniformLocation(mainShader_, "objectColor"), 1, glm::value_ptr(color));
    glUniform3fv(glGetUniformLocation(mainShader_, "emissiveColor"), 1, glm::value_ptr(emissive));
    glUniform1f(glGetUniformLocation(mainShader_, "opacity"), opacity);

    model.draw();
}

bool Renderer::loadGLBModel(const std::string& path, GLBModel& outModel) {
    cgltf_options options = {};
    cgltf_data* data = nullptr;
    cgltf_result result = cgltf_parse_file(&options, path.c_str(), &data);
    if (result != cgltf_result_success) {
        std::cerr << "[GLTF] Failed to parse: " << path << "\n";
        return false;
    }

    result = cgltf_load_buffers(&options, data, path.c_str());
    if (result != cgltf_result_success) {
        std::cerr << "[GLTF] Failed to load buffers for: " << path << "\n";
        cgltf_free(data);
        return false;
    }

    for (cgltf_size m = 0; m < data->meshes_count; ++m) {
        cgltf_mesh* mesh = &data->meshes[m];
        for (cgltf_size p = 0; p < mesh->primitives_count; ++p) {
            cgltf_primitive* prim = &mesh->primitives[p];
            if (prim->type != cgltf_primitive_type_triangles) continue;

            std::vector<float> vertexData; // x,y,z, nx,ny,nz
            std::vector<unsigned int> indices;

            cgltf_accessor* posAccessor = nullptr;
            cgltf_accessor* normAccessor = nullptr;

            for (cgltf_size a = 0; a < prim->attributes_count; ++a) {
                cgltf_attribute* attr = &prim->attributes[a];
                if (attr->type == cgltf_attribute_type_position) {
                    posAccessor = attr->data;
                } else if (attr->type == cgltf_attribute_type_normal) {
                    normAccessor = attr->data;
                }
            }

            if (!posAccessor) continue;

            cgltf_size numVertices = posAccessor->count;
            vertexData.resize(numVertices * 6, 0.0f);

            for (cgltf_size i = 0; i < numVertices; ++i) {
                float pos[3] = {0.0f};
                cgltf_accessor_read_float(posAccessor, i, pos, 3);
                vertexData[i * 6 + 0] = pos[0];
                vertexData[i * 6 + 1] = pos[1];
                vertexData[i * 6 + 2] = pos[2];
            }

            if (normAccessor) {
                for (cgltf_size i = 0; i < numVertices; ++i) {
                    float norm[3] = {0.0f};
                    cgltf_accessor_read_float(normAccessor, i, norm, 3);
                    vertexData[i * 6 + 3] = norm[0];
                    vertexData[i * 6 + 4] = norm[1];
                    vertexData[i * 6 + 5] = norm[2];
                }
            }

            if (prim->indices) {
                indices.resize(prim->indices->count);
                for (cgltf_size i = 0; i < prim->indices->count; ++i) {
                    indices[i] = cgltf_accessor_read_index(prim->indices, i);
                }
            } else {
                indices.resize(numVertices);
                for (cgltf_size i = 0; i < numVertices; ++i) {
                    indices[i] = (unsigned int)i;
                }
            }

            GLBModel::Mesh glMesh;
            glMesh.indexCount = indices.size();

            glGenVertexArrays(1, &glMesh.vao);
            glGenBuffers(1, &glMesh.vbo);
            glGenBuffers(1, &glMesh.ebo);

            glBindVertexArray(glMesh.vao);

            glBindBuffer(GL_ARRAY_BUFFER, glMesh.vbo);
            glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glMesh.ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);

            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(1);

            outModel.meshes.push_back(glMesh);
        }
    }

    cgltf_free(data);
    std::cout << "[GLTF] Loaded " << path << " with " << outModel.meshes.size() << " submeshes.\n";
    return true;
}
