/*
* File: world.hpp
* Project: Cisalpine Engine
* Author: Collin Longoria
* Created on: 2/4/2026
*
* Copyright (c) 2025 Collin Longoria
*
* This software is released under the MIT License.
* https://opensource.org/licenses/MIT
*/

#ifndef CISALPINE_WORLD_HPP
#define CISALPINE_WORLD_HPP
#include <glad/glad.h>
#include <cstdint>

#include "shader.hpp"
#include <glm/glm.hpp>

namespace cisalpine {

// Simulation settings
struct SimulationSettings {
    // Simulation loops per frame
    int stepsPerFrame = 4;
};

// Rendering Settings
struct RenderSettings {
    glm::vec4 backgroundColor = glm::vec4(0.05f, 0.05f, 0.08f, 1.0f);
    bool glowEnabled = true;
    float glowIntensity = 0.25f;
    float glowRadius = 3.3f;
    float ambientLight = 0.15f;
    float specularStrength = 0.6f;
    int lightBounces = 3;
};

class World {
public:
    World(int width, int height);
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    bool init(const std::string& shaderHeader);
    void update(float dt);
    void render(int screenX, int screenY, int screenWidth, int screenHeight);

    void clear();

    int width() const { return worldWidth; }
    int height() const { return worldHeight; }

    // Get current state texture for brush shader
    GLuint getCurrentTexture() const { return stateTextures[currentBuffer]; }
    GLuint getDisplayTexture() const { return displayTexture; }

    RenderSettings& renderSettings() { return renderSettingsData; }
    const RenderSettings& renderSettings() const { return renderSettingsData; }

    SimulationSettings& simulationSettings() { return simSettings; }
    const SimulationSettings& simulationSettings() const { return simSettings; }

private:
    int worldWidth;
    int worldHeight;

    // Timing
    float accumulatedTime = 0.0f;
    float simulationTime = 0.0f;
    uint32_t frameCount = 0;
    static constexpr float FIXED_TIMESTEP = 1.0f / 60.0f; // 60 sim steps / second

    // Double-buffered state textures (RGBA8UI)
    // R = element, G = life/state, B = velocity/misc, A = flags
    GLuint stateTextures[2] = {0, 0};
    int currentBuffer = 0;

    // Rendering textures
    GLuint colorTexture = 0; // Raw element colors (RGBA8)
    GLuint normalTexture = 0; // Per-pixel normals (RGBA16F: xy=normal, z=height, w=specular)
    GLuint lightmapTexture = 0; // Accumulated light (RGBA16F: rgb=light color, a=intensity)
    GLuint lightmapPingPong = 0; // Ping-pong for light propagation
    GLuint displayTexture = 0; // Final composited output (RGBA8)

    // Shaders
    Shader simulationShader;
    Shader renderShader; // Takes sim state and creates color + normals
    Shader lightingShader; // Light propagation and accumulation
    Shader compositeShader; // Final composition
    Shader quadShader; // Blit to screen

    // Quad for rendering
    GLuint quadVAO = 0;
    GLuint quadVBO = 0;

    // Settings
    RenderSettings renderSettingsData;
    SimulationSettings simSettings;

    // Helpers
    void createTextures();
    void createQuad();
    void swapBuffers();
    void simulationStep();
};

}

#endif //CISALPINE_WORLD_HPP