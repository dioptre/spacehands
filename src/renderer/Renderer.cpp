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
static const int PENTA[] = {60, 62, 64, 67, 69, 72, 74, 76, 79, 81, 84, 86};
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
        #version 130
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
        #version 130
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
        #version 130
        in vec3 aPos;
        uniform mat4 view;
        uniform mat4 projection;
        uniform mat4 model;
        void main() {
            gl_Position = projection * view * model * vec4(aPos, 1.0);
            gl_PointSize = 4.0;
        }
    )";

    const std::string starFragShader = R"(
        #version 130
        out vec4 FragColor;
        uniform vec3 color;
        uniform float opacity;
        void main() {
            FragColor = vec4(color, opacity);
        }
    )";

    const std::string freqVertShader = R"(
        #version 130
        in vec2 aPos;
        out vec2 v_uv;
        void main() {
            v_uv = aPos * 0.5 + 0.5;
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )";

    const std::string freqFragShader = R"(
        #version 130
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
        #version 130
        in vec2 aPos;
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )";

    const std::string flatFragShader = R"(
        #version 130
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
    hasAlienModel_ = loadGLBModel("assets/models/alien.glb", alienModel_);
    if (!hasAlienModel_) {
        std::cerr << "[Renderer] Warning: failed to load assets/models/alien.glb\n";
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
    std::uniform_real_distribution<float> velDis(-2.0f, 2.0f);
    for (int i = 0; i < 30; i++) {
        VisualizerParticle p;
        p.pos = pos;
        p.vel = glm::vec3(velDis(gen), velDis(gen), velDis(gen));
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
            color = glm::vec3(0.1f, 0.13f, 0.4f);
            emissive = glm::vec3(0.04f, 0.06f, 0.26f);
            opacity = 0.5f;
        }
    } else if (isDecoy) {
        color = glm::vec3(0.04f, 0.06f, 0.2f);
        emissive = glm::vec3(0.02f, 0.03f, 0.09f);
        opacity = 0.35f;
    } else {
        color = glm::vec3(0.1f, 0.13f, 0.4f);
        emissive = glm::vec3(0.04f, 0.06f, 0.26f);
        opacity = 0.5f;
    }
}

void Renderer::render(const HandList& hands, float dt) {
    if (!window_) return;

    if (osc_) {
        int target = -1;
        if (state_ == VisualizerState::PLAYING && collected_ >= 0 && collected_ < (int)sequence_.size()) {
            target = sequence_[collected_];
        }
        osc_->setTargetNode(target);
    }

    // Window size might change
    glfwGetWindowSize(window_, &width_, &height_);
    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window_, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    float aspect = (float)width_ / (float)height_;

    // Handle background clear
    float clearRamp = (state_ == VisualizerState::CLIMAX) ? std::min(climaxTimer_ / 3.5f, 1.0f) : 0.0f;
    if (state_ == VisualizerState::CONGRATULATIONS) clearRamp = 1.0f;
    glClearColor(0.0f, 0.0f, 0.03f * (1.0f - clearRamp), 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Matrix calculations
    glm::mat4 projection = glm::perspective(glm::radians(55.0f), aspect, 0.01f, 100.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.5f, 4.5f), glm::vec3(0.0f, 0.5f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    glUseProgram(mainShader_);
    glUniformMatrix4fv(glGetUniformLocation(mainShader_, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(mainShader_, "view"), 1, GL_FALSE, glm::value_ptr(view));

    glUseProgram(starfieldShader_);
    glUniformMatrix4fv(glGetUniformLocation(starfieldShader_, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(starfieldShader_, "view"), 1, GL_FALSE, glm::value_ptr(view));

    // Update game logic states
    updateGridPositions(dt);

    sequenceAge_ += dt;
    if (sequenceAge_ >= 180.0f && state_ == VisualizerState::IDLE && hands.empty()) {
        resetGame();
        sequenceAge_ = 0.0f;
    }

    // Update fade timer and handle phase transition
    float fadeOpacity = 0.0f;
    if (fadeState_ == FadeState::FADE_OUT) {
        fadeTimer_ += dt;
        fadeOpacity = std::min(fadeTimer_ / 0.5f, 1.0f);
        if (fadeTimer_ >= 0.5f) {
            resetGame(); // resets state_ to IDLE
            fadeState_ = FadeState::FADE_IN;
            fadeTimer_ = 0.0f;
        }
    } else if (fadeState_ == FadeState::FADE_IN) {
        fadeTimer_ += dt;
        fadeOpacity = 1.0f - std::min(fadeTimer_ / 0.5f, 1.0f);
        if (fadeTimer_ >= 0.5f) {
            fadeState_ = FadeState::NONE;
            fadeTimer_ = 0.0f;
        }
    }

    // State Machine
    if (state_ == VisualizerState::IDLE) {
        if (!hands.empty()) {
            if (glfwGetTime() - lastPreviewTime_ >= 40.0f) {
                state_ = VisualizerState::SEQUENCE_PREVIEW;
                previewStep_ = 0;
                previewTimer_ = -1.0f; // 1s initial mute delay
                osc_->setPreviewMute(true); // mute hand instruments during preview
            } else {
                state_ = VisualizerState::PLAYING;
            }
            idleTimer_ = 0.0f;
        }
        idleTimer_ += dt;
    } 
    else if (state_ == VisualizerState::SEQUENCE_PREVIEW) {
        previewTimer_ += dt;
        
        if (previewTimer_ >= 0.0f) {
            if (previewStep_ < 6) {
                float targetTime = previewStep_ * 0.7f;
                if (previewTimer_ >= targetTime) {
                    int noteIdx = sequence_[previewStep_];
                    osc_->sendDirtPlay(PENTA[noteIdx], 0.75f, 0.4f);
                    previewStep_++;
                }
            }
            if (previewTimer_ >= 6 * 0.7f) {
                hasSeenPreview_ = true;
                lastPreviewTime_ = glfwGetTime();
                state_ = VisualizerState::PLAYING;
                osc_->setPreviewMute(false); // unmute hand instruments for gameplay
            }
        }
    } 
    else if (state_ == VisualizerState::PLAYING) {
        if (hands.empty()) {
            idleTimer_ += dt;
            if (idleTimer_ > 8.0f) {
                resetGame();
            }
        } else {
            idleTimer_ = 0.0f;
        }

        // Touch detection
        if (collected_ < 6) {
            int targetNodeIdx = sequence_[collected_];
            glm::vec3 targetPos = gridPositions_[targetNodeIdx];
            bool touching = false;

            for (const auto& h : hands) {
                float mx = cfg_->mirror_x ? (1.0f - h.x) : h.x;
                int gesture = (int)h.gesture;

                if (gesture == 7) { // THUMBS_DOWN (skip note)
                    collected_++;
                    updateMelody();
                    holdTimer_ = 0.0f;
                    break;
                }

                glm::vec3 handPos;
                if (gesture == 2) { // FIST
                    if (!hasFrozenHand_) {
                        frozenHandPos_ = handWorldPos(mx, h.y, h.z, aspect);
                        hasFrozenHand_ = true;
                    }
                    handPos = frozenHandPos_;
                } else {
                    hasFrozenHand_ = false;
                    handPos = handWorldPos(mx, h.y, h.z, aspect);
                }

                float radiusMult = 1.0f;
                if (gesture == 3) radiusMult = 0.5f;
                if (gesture == 5) radiusMult = 1.5f;

                float dx = std::abs(handPos.x - targetPos.x);
                float dy = std::abs(handPos.y - targetPos.y);
                float dz = std::abs(handPos.z - targetPos.z);

                if (dx < 0.9f * radiusMult && dy < 0.9f * radiusMult && dz < 0.8f * 2.0f) {
                    touching = true;
                    if (gesture == 3) {
                        holdTimer_ = 1.0f;
                    }
                }
            }

            if (touching) {
                holdTimer_ += dt / 0.6f;
            } else {
                holdTimer_ = std::max(0.0f, holdTimer_ - dt * 2.0f);
            }

            if (holdTimer_ >= 1.0f) {
                int collectedMidi = PENTA[targetNodeIdx];
                burstParticles(targetPos);
                osc_->sendDirtPlay(collectedMidi, 0.8f, 0.4f);
                osc_->sendTidalCtrl("transformation_note", (float)collectedMidi);
                collected_++;
                holdTimer_ = 0.0f;
                updateMelody();

                if (collected_ >= 6) {
                    state_ = VisualizerState::CLIMAX;
                    climaxTimer_ = 0.0f;
                    osc_->sendTidalCtrl("transformation_climax", 1.0f);
                }
            }
        }
    } 
    else if (state_ == VisualizerState::CLIMAX) {
        climaxTimer_ += dt;
        if (climaxTimer_ > 3.5f) {
            state_ = VisualizerState::CONGRATULATIONS;
            congratsTimer_ = 0.0f;
        }
    } 
    else if (state_ == VisualizerState::CONGRATULATIONS) {
        if (fadeState_ != FadeState::FADE_OUT) {
            congratsTimer_ += dt;
            if (congratsTimer_ > 15.0f) {
                fadeState_ = FadeState::FADE_OUT;
                fadeTimer_ = 0.0f;
            }
        }
    }

    // Render Starfield
    glUseProgram(starfieldShader_);
    glm::mat4 starModel = glm::rotate(glm::mat4(1.0f), (float)glfwGetTime() * 0.005f, glm::vec3(0.0f, 1.0f, 0.0f));
    glUniformMatrix4fv(glGetUniformLocation(starfieldShader_, "model"), 1, GL_FALSE, glm::value_ptr(starModel));
    glUniform3f(glGetUniformLocation(starfieldShader_, "color"), 0.53f, 0.33f, 1.0f);
    glUniform1f(glGetUniformLocation(starfieldShader_, "opacity"), 0.8f);

    glBindVertexArray(starfieldVAO_);
    glDrawArrays(GL_POINTS, 0, 4000);

    // Render Nodes (if not in CLIMAX / CONGRATS)
    if (state_ != VisualizerState::CLIMAX && state_ != VisualizerState::CONGRATULATIONS) {
        for (int i = 0; i < NUM_NODES; i++) {
            glm::vec3 cellColor, emissiveColor;
            float opacity, scale;
            bool drawWireframe;
            updateCellVisuals(i, cellColor, emissiveColor, opacity, scale, drawWireframe);

            if (state_ == VisualizerState::SEQUENCE_PREVIEW) {
                if (previewTimer_ < 0.0f) {
                    // Delay phase: keep all cells dimmed
                    cellColor = glm::vec3(0.0f, 0.06f, 0.13f);
                    emissiveColor = glm::vec3(0.0f);
                    opacity = 0.15f;
                } else {
                    int activeStep = previewStep_ > 0 ? previewStep_ - 1 : 0;
                    int previewNode = sequence_[activeStep];
                    if (i == previewNode) {
                        cellColor = glm::vec3(1.0f, 1.0f, 1.0f);
                        emissiveColor = glm::vec3(0.4f, 0.2f, 1.0f);
                        opacity = 1.0f;
                        scale = 1.2f + 0.08f * std::sin(glfwGetTime() * 8.0f);
                    } else if (cfg_->show_preview_history && std::find(sequence_.begin(), sequence_.begin() + activeStep, i) != sequence_.begin() + activeStep) {
                        cellColor = glm::vec3(0.06f, 0.13f, 0.26f);
                        emissiveColor = glm::vec3(0.0f);
                        opacity = 0.35f;
                    } else {
                        cellColor = glm::vec3(0.0f, 0.06f, 0.13f);
                        emissiveColor = glm::vec3(0.0f);
                        opacity = 0.15f;
                    }
                }
            }

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, gridPositions_[i]);
            model = glm::scale(model, glm::vec3(0.8f * scale));

            drawCube(model, cellColor, emissiveColor, opacity, false);

            if (drawWireframe) {
                int activeStep = previewStep_ > 0 ? previewStep_ - 1 : 0;
                glm::vec3 wireColor = (state_ == VisualizerState::SEQUENCE_PREVIEW && previewTimer_ >= 0.0f && i == sequence_[activeStep]) 
                    ? glm::vec3(1.0f) : glm::vec3(0.2f, 0.4f, 1.0f);
                drawCube(model, glm::vec3(0.0f), wireColor, opacity * 0.8f, true);
            }

            // Draw alien model if loaded and node is not collected
            auto it = std::find(sequence_.begin(), sequence_.end(), i);
            int seqPos = (it != sequence_.end()) ? std::distance(sequence_.begin(), it) : -1;
            bool isCollected = (seqPos != -1 && seqPos < collected_);

            if (hasAlienModel_ && !isCollected) {
                glm::mat4 alienModelMat = glm::mat4(1.0f);
                float bob = 0.55f + 0.08f * std::sin(glfwGetTime() * 1.2f + i * 0.8f);
                glm::vec3 alienPos = gridPositions_[i] + glm::vec3(0.0f, bob, 0.0f);
                alienModelMat = glm::translate(alienModelMat, alienPos);
                alienModelMat = glm::rotate(alienModelMat, (float)glfwGetTime() * 0.48f + i * 0.5f, glm::vec3(0.0f, 1.0f, 0.0f));
                alienModelMat = glm::scale(alienModelMat, glm::vec3(0.35f * scale));

                glm::vec3 alienColor = glm::vec3(0.7f, 0.9f, 1.0f);
                glm::vec3 alienEmissive = emissiveColor * 0.5f + glm::vec3(0.1f, 0.2f, 0.4f);
                drawModel(alienModel_, alienModelMat, alienColor, alienEmissive, opacity);
            }
        }
    }

    // Render Hold Ring
    if (state_ == VisualizerState::PLAYING && holdTimer_ > 0.0f && collected_ < 6) {
        int targetNodeIdx = sequence_[collected_];
        glm::vec3 targetPos = gridPositions_[targetNodeIdx];
        float radius = (holdTimer_ * 1.2f + 0.8f) * 0.55f;
        drawRing(targetPos, radius, glm::vec3(1.0f, 1.0f, 1.0f), 0.8f);
    }

    // Render Climax Ring (expanding cylinder/ring)
    if (state_ == VisualizerState::CLIMAX) {
        float p = std::min(climaxTimer_ / 3.5f, 1.0f);
        float radius = 0.01f + p * 8.0f;
        float opacity = std::sin(p * M_PI) * 0.9f;
        drawRing(glm::vec3(0.0f, 0.5f, 0.0f), radius, glm::vec3(0.53f, 0.26f, 1.0f), opacity);
    }

    // Render particles
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
        glUniform3f(glGetUniformLocation(starfieldShader_, "color"), 0.5f, 0.8f, 1.0f);
        glUniform1f(glGetUniformLocation(starfieldShader_, "opacity"), 0.8f);

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

    // Render Hand Cursors
    if (lerpedHandPositions_.size() < hands.size()) {
        lerpedHandPositions_.resize(hands.size(), glm::vec3(0.0f));
    }
    if (prevHandPositions_.size() < hands.size()) {
        prevHandPositions_.resize(hands.size(), glm::vec3(0.0f));
    }
    for (size_t i = 0; i < hands.size(); i++) {
        float mx = cfg_->mirror_x ? (1.0f - hands[i].x) : hands[i].x;
        glm::vec3 wp = handWorldPos(mx, hands[i].y, hands[i].z, aspect);
        
        glm::vec3 prevPos = lerpedHandPositions_[i];
        lerpedHandPositions_[i] = glm::mix(lerpedHandPositions_[i], wp, 0.3f);
        glm::vec3 vel = lerpedHandPositions_[i] - prevPos;

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, lerpedHandPositions_[i]);

        // Apply lean rotation (tilts) matching JS physics
        float posX = lerpedHandPositions_[i].x;
        float posY = lerpedHandPositions_[i].y;
        float rotZ = -posX * 0.3f - vel.x * 6.0f;
        float rotX =  posY * 0.2f + vel.y * 4.0f;
        float rotY =  posX * 0.2f;

        model = glm::rotate(model, rotY, glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, rotX, glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, rotZ, glm::vec3(0.0f, 0.0f, 1.0f));

        glm::vec3 hColor(1.0f);
        if (i == 0)      hColor = glm::vec3(0.94f, 0.96f, 1.0f);
        else if (i == 1) hColor = glm::vec3(1.0f, 0.88f, 0.66f);
        else if (i == 2) hColor = glm::vec3(0.66f, 1.0f, 0.86f);
        else             hColor = glm::vec3(1.0f, 0.66f, 1.0f);

        if (hasHandModel_) {
            model = glm::scale(model, glm::vec3(0.28f));
            drawModel(handModel_, model, hColor, hColor * 0.3f, 0.9f);
        } else {
            model = glm::scale(model, glm::vec3(0.12f));
            drawSphere(model, hColor, hColor * 0.3f, 0.9f);
        }
    }

    // Render Frequency Tunnel (during CLIMAX & CONGRATS overlay)
    if (state_ == VisualizerState::CLIMAX || state_ == VisualizerState::CONGRATULATIONS) {
        float fTime = (state_ == VisualizerState::CLIMAX) ? climaxTimer_ : (3.5f + congratsTimer_);
        glUseProgram(freqShader_);
        glUniform1f(glGetUniformLocation(freqShader_, "u_time"), fTime);
        glUniform2f(glGetUniformLocation(freqShader_, "u_resolution"), (float)width_, (float)height_);
        
        float offsetX = ((float)cfg_->crop_left + ((float)width_ - (float)cfg_->crop_left - (float)cfg_->crop_right) * 0.5f) - (float)width_ * 0.5f;
        glUniform2f(glGetUniformLocation(freqShader_, "u_offset"), offsetX, 0.0f);

        glBindVertexArray(quadVAO_);
        glDisable(GL_DEPTH_TEST);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glEnable(GL_DEPTH_TEST);
    }

    // Draw fade overlay if active
    if (fadeState_ != FadeState::NONE) {
        glUseProgram(flatShader_);
        glUniform1f(glGetUniformLocation(flatShader_, "u_opacity"), fadeOpacity);
        glBindVertexArray(quadVAO_);
        glDisable(GL_DEPTH_TEST);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glEnable(GL_DEPTH_TEST);
    }

    // Swap buffers & poll events
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
