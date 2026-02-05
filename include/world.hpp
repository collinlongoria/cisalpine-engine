/*
* File: world
* Project: cisalpine
* Author: colli
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

// Element types
// Stored in R channel of state texture
// TODO currently must match shader constraints
enum class Element : uint8_t {
    Empty = 0,
    Sand = 1,
    Stone = 2,
    Water = 3,
    Lava = 4,
    Wood = 5,
    Fire = 6,
    Smoke = 7,
    Dirt = 8,
    Seed = 9,
    Grass = 10,
    COUNT
};

// Simulation settings
struct SimulationSettings {
    // Viscosity: 0 = instant flow, 1 = no flow
    float waterViscosity = 0.05f;
    float lavaViscosity = 0.75f;

    // Simulation loops per frame
    int stepsPerFrame = 4;
};

// Rendering Settings
struct RenderSettings {
    glm::vec4 backgroundColor = glm::vec4(0.05f, 0.05f, 0.08f, 1.0f);
    bool glowEnabled = true;
    float glowIntensity = 0.25f;
    float glowRadius = 3.3f;
};

class World {
public:
    World(int width, int height);
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    bool init();
    void update(float dt);
    void render(int screenX, int screenY, int screenWidth, int screenHeight);

    void spawnParticle(int x, int y, Element element);
    void clear();

    int width() const { return worldWidth; }
    int height() const { return worldHeight; }

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
    // R = element, G = state, B = velocity
    GLuint stateTextures[2] = {0, 0};
    int currentBuffer = 0;

    // Rendered world texture
    GLuint colorTexture = 0;
    GLuint displayTexture = 0; // w/ lighting

    // Shaders
    Shader simulationShader;
    Shader renderShader;
    Shader lightingShader;
    Shader quadShader; // for blit to screen

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