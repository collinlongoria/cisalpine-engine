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

#include "world.hpp"
#include <iostream>
#include <vector>

namespace cisalpine {

World::World(int width, int height)
    : worldWidth(width), worldHeight(height) {
}

World::~World() {
    if (stateTextures[0]) glDeleteTextures(2, stateTextures);
    if (colorTexture) glDeleteTextures(1, &colorTexture);
    if (displayTexture) glDeleteTextures(1, &displayTexture);
    if (quadVAO) glDeleteVertexArrays(1, &quadVAO);
    if (quadVBO) glDeleteBuffers(1, &quadVBO);
}

bool World::init() {
    // Load shaders
    if (!simulationShader.loadCompute("shaders/simulation.comp")) {
        std::cerr << "Failed to load simulation shader" << std::endl;
        return false;
    }
    if (!renderShader.loadCompute("shaders/render.comp")) {
        std::cerr << "Failed to load render shader" << std::endl;
        return false;
    }
    if (!lightingShader.loadCompute("shaders/lighting.comp")) {
        std::cerr << "Failed to load lighting shader" << std::endl;
        return false;
    }
    if (!quadShader.loadFromFile("shaders/quad.vert", "shaders/quad.frag")) {
        std::cerr << "Failed to load quad shader" << std::endl;
        return false;
    }

    createTextures();
    createQuad();

    return true;
}

void World::createTextures() {
    // Create state textures (RGBA8UI)
    glGenTextures(2, stateTextures);
    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, stateTextures[i]);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8UI, worldWidth, worldHeight);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Clear to empty
        std::vector<uint8_t> clearData(worldWidth * worldHeight * 4, 0);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, worldWidth, worldHeight,
            GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, clearData.data());
    }

    // Create color texture (RGBA8)
    glGenTextures(1, &colorTexture);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, worldWidth, worldHeight);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create display texture (RGBA8)
    glGenTextures(1, &displayTexture);
    glBindTexture(GL_TEXTURE_2D, displayTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, worldWidth, worldHeight);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void World::createQuad() {
    float vertices[] = {
        // pos       // uv
        -1.0f,  1.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,

        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
}

void World::spawnParticle(int x, int y, Element element) {
    if (x < 0 || x >= worldWidth || y < 0 || y >= worldHeight) return;

    uint8_t pixel[4] = {
        static_cast<uint8_t>(element),
        0,
        128,
        0
    };

    glBindTexture(GL_TEXTURE_2D, stateTextures[currentBuffer]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, 1, 1, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, pixel);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void World::clear() {
    std::vector<uint8_t> clearData(worldWidth * worldHeight * 4, 0);

    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, stateTextures[i]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, worldWidth, worldHeight,
            GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, clearData.data());
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

void World::simulationStep() {
    int nextBuffer = 1 - currentBuffer;

    // Bind textures to image units
    glBindImageTexture(0, stateTextures[currentBuffer], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8UI);
    glBindImageTexture(1, stateTextures[nextBuffer], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8UI);

    // Run simulation shader
    simulationShader.use();
    simulationShader.setVec2("worldSize", static_cast<float>(worldWidth), static_cast<float>(worldHeight));
    simulationShader.setFloat("time", simulationTime);
    simulationShader.setUint("frameCount", frameCount);
    simulationShader.setFloat("waterViscosity", simSettings.waterViscosity);
    simulationShader.setFloat("lavaViscosity", simSettings.lavaViscosity);

    GLuint workGroupsX = (worldWidth + 15) / 16;
    GLuint workGroupsY = (worldHeight + 15) / 16;
    glDispatchCompute(workGroupsX, workGroupsY, 1);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    swapBuffers();
    frameCount++;
}

void World::update(float dt) {
    accumulatedTime += dt;
    simulationTime += dt;

    // Fixed timestep simulation
    while (accumulatedTime >= FIXED_TIMESTEP) {
        simulationStep();
        accumulatedTime -= FIXED_TIMESTEP;
    }
}

void World::render(int screenX, int screenY, int screenWidth, int screenHeight) {
    GLuint workGroupsX = (worldWidth + 15) / 16;
    GLuint workGroupsY = (worldHeight + 15) / 16;

    // Pass 1: Convert state texture to colors
    glBindImageTexture(0, stateTextures[currentBuffer], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8UI);
    glBindImageTexture(1, colorTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);

    renderShader.use();
    renderShader.setVec4("backgroundColor",
        renderSettingsData.backgroundColor.r,
        renderSettingsData.backgroundColor.g,
        renderSettingsData.backgroundColor.b,
        renderSettingsData.backgroundColor.a);
    renderShader.setFloat("time", simulationTime);

    glDispatchCompute(workGroupsX, workGroupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // Pass 2: Apply lighting/glow
    glBindImageTexture(0, stateTextures[currentBuffer], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8UI);
    glBindImageTexture(1, colorTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
    glBindImageTexture(2, displayTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);

    lightingShader.use();
    lightingShader.setBool("glowEnabled", renderSettingsData.glowEnabled);
    lightingShader.setFloat("glowIntensity", renderSettingsData.glowIntensity);
    lightingShader.setFloat("glowRadius", renderSettingsData.glowRadius);
    lightingShader.setFloat("time", simulationTime);

    glDispatchCompute(workGroupsX, workGroupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // Pass 3: Draw quad with display texture
    glViewport(screenX, screenY, screenWidth, screenHeight);

    quadShader.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, displayTexture);
    quadShader.setInt("displayTex", 0);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void World::swapBuffers() {
    currentBuffer = 1 - currentBuffer;
}

}
