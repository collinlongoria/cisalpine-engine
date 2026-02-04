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

namespace cisalpine {

// Element types
// Stored in R channel of state texture
enum class Element : uint8_t {
    Empty = 0,
    Sand = 1,
};

class World {
public:
    World(int width, int height);
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    bool init();
    void update();
    void render(int screenX, int screenY, int screenWidth, int screenHeight);

    void spawnParticle(int x, int y, Element element);

    int width() const { return worldWidth; }
    int height() const { return worldHeight; }

    GLuint getDisplayTexture() const { return displayTexture; }

private:
    int worldWidth;
    int worldHeight;

    // Double-buffered state textures (RGBA8UI)
    // R = element, G = state, B = velocity
    GLuint stateTextures[2] = {0,0};
    int currentBuffer = 0;

    // Rendered world texture
    GLuint displayTexture = 0;

    // Shaders
    Shader simulationShader;
    Shader renderShader;
    Shader quadShader; // for blit to screen

    // Quad for rendering
    GLuint quadVAO = 0;
    GLuint quadVBO = 0;

    void createTextures();
    void createQuad();
    void swapBuffers();
};

}

#endif //CISALPINE_WORLD_HPP