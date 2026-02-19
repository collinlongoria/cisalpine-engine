/*
* File: world.cpp
* Project: Cisalpine Engine
* Author: Collin Longoria
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
    if (forceTextures[0]) glDeleteTextures(2, forceTextures);
    if (colorTexture) glDeleteTextures(1, &colorTexture);
    if (normalTexture) glDeleteTextures(1, &normalTexture);
    if (lightmapTexture) glDeleteTextures(1, &lightmapTexture);
    if (lightmapPingPong) glDeleteTextures(1, &lightmapPingPong);
    if (displayTexture) glDeleteTextures(1, &displayTexture);
    if (skyTexture) glDeleteTextures(1, &skyTexture);
    if (quadVAO) glDeleteVertexArrays(1, &quadVAO);
    if (quadVBO) glDeleteBuffers(1, &quadVBO);
}

bool World::init(const std::string& shaderHeader) {
    // Load shaders
    if (!simulationShader.loadCompute("shaders/simulation.comp", shaderHeader)) {
        std::cerr << "Failed to load simulation shader" << std::endl;
        return false;
    }
    if (!forceUpdateShader.loadCompute("shaders/physics.comp", shaderHeader)) {
        std::cerr << "Failed to load physics shader" << std::endl;
        return false;
    }
    if (!renderShader.loadCompute("shaders/render.comp", shaderHeader)) {
        std::cerr << "Failed to load render shader" << std::endl;
        return false;
    }
    if (!lightingShader.loadCompute("shaders/lighting.comp", shaderHeader)) {
        std::cerr << "Failed to load lighting shader" << std::endl;
        return false;
    }
    if (!compositeShader.loadCompute("shaders/composite.comp", shaderHeader)) {
        std::cerr << "Failed to load composite shader" << std::endl;
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

    // Create force field textures (RGBA16F) - double buffered
    glGenTextures(2, forceTextures);
    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, forceTextures[i]);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, worldWidth, worldHeight);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Clear to zero force
        std::vector<float> clearForce(worldWidth * worldHeight * 4, 0.0f);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, worldWidth, worldHeight,
            GL_RGBA, GL_FLOAT, clearForce.data());
    }

    // Create color texture (RGBA8)
    glGenTextures(1, &colorTexture);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, worldWidth, worldHeight);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create normal texture (RGBA16F: xy = normal, z = height, w = specular power)
    glGenTextures(1, &normalTexture);
    glBindTexture(GL_TEXTURE_2D, normalTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, worldWidth, worldHeight);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create lightmap textures (RGBA16F: rgb = light color, a = intensity)
    glGenTextures(1, &lightmapTexture);
    glBindTexture(GL_TEXTURE_2D, lightmapTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, worldWidth, worldHeight);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &lightmapPingPong);
    glBindTexture(GL_TEXTURE_2D, lightmapPingPong);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, worldWidth, worldHeight);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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

    // Create sky texture (RGBA8) - rendered by composite shader when sky enabled
    glGenTextures(1, &skyTexture);
    glBindTexture(GL_TEXTURE_2D, skyTexture);
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

void World::clear() {
    std::vector<uint8_t> clearData(worldWidth * worldHeight * 4, 0);

    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, stateTextures[i]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, worldWidth, worldHeight,
            GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, clearData.data());
    }

    // Also clear force field
    std::vector<float> clearForce(worldWidth * worldHeight * 4, 0.0f);
    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, forceTextures[i]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, worldWidth, worldHeight,
            GL_RGBA, GL_FLOAT, clearForce.data());
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

void World::forceUpdateStep() {
    int nextForceBuffer = 1 - currentForceBuffer;

    // Bind force field textures for read/write
    // binding 0: forceIn (RGBA16F, read)
    // binding 1: forceOut (RGBA16F, write)
    // binding 2: stateIn (RGBA8UI, read) - to detect black holes, bombs, etc.
    glBindImageTexture(0, forceTextures[currentForceBuffer], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
    glBindImageTexture(1, forceTextures[nextForceBuffer], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    glBindImageTexture(2, stateTextures[currentBuffer], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8UI);

    forceUpdateShader.use();
    forceUpdateShader.setVec2("worldSize", static_cast<float>(worldWidth), static_cast<float>(worldHeight));
    forceUpdateShader.setFloat("time", simulationTime);
    forceUpdateShader.setUint("frameCount", frameCount);

    GLuint workGroupsX = (worldWidth + 15) / 16;
    GLuint workGroupsY = (worldHeight + 15) / 16;
    glDispatchCompute(workGroupsX, workGroupsY, 1);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    swapForceBuffers();
}

void World::simulationStep() {
    // First: update the force field (reads current state for black holes/bombs)
    forceUpdateStep();

    int nextBuffer = 1 - currentBuffer;

    // Bind textures to image units
    glBindImageTexture(0, stateTextures[currentBuffer], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8UI);
    glBindImageTexture(1, stateTextures[nextBuffer], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8UI);

    // Bind force field for the simulation shader to read
    glBindImageTexture(3, forceTextures[currentForceBuffer], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);

    // Run simulation shader
    simulationShader.use();
    simulationShader.setVec2("worldSize", static_cast<float>(worldWidth), static_cast<float>(worldHeight));
    simulationShader.setFloat("time", simulationTime);
    simulationShader.setUint("frameCount", frameCount);

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
        for (int i = 0; i < simSettings.stepsPerFrame; i++) {
            simulationStep();
        }
        accumulatedTime -= FIXED_TIMESTEP;
    }
}

void World::render(int screenX, int screenY, int screenWidth, int screenHeight,
    float camX, float camY, float camZoom) {
    GLuint workGroupsX = (worldWidth + 15) / 16;
    GLuint workGroupsY = (worldHeight + 15) / 16;

    // Pass 1: Convert state texture to colors
    // binding 0: stateIn (RGBA8UI, read)
    // binding 1: colorOut (RGBA8, write)
    // binding 2: normalOut (RGBA16F, write)
    // binding 5: skyOut (RGBA8, write) - sky gradient for background
    glBindImageTexture(0, stateTextures[currentBuffer], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8UI);
    glBindImageTexture(1, colorTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
    glBindImageTexture(2, normalTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    glBindImageTexture(5, skyTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);

    renderShader.use();
    renderShader.setVec4("backgroundColor",
        renderSettingsData.backgroundColor.r,
        renderSettingsData.backgroundColor.g,
        renderSettingsData.backgroundColor.b,
        renderSettingsData.backgroundColor.a);
    renderShader.setFloat("time", simulationTime);
    renderShader.setBool("skyEnabled", renderSettingsData.skyEnabled);
    renderShader.setFloat("timeOfDay", renderSettingsData.timeOfDay);
    renderShader.setBool("showSun", renderSettingsData.showSun);

    glDispatchCompute(workGroupsX, workGroupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // Pass 2: Light Propagation
    lightingShader.use();
    lightingShader.setBool("glowEnabled", renderSettingsData.glowEnabled);
    lightingShader.setFloat("glowIntensity", renderSettingsData.glowIntensity);
    lightingShader.setFloat("glowRadius", renderSettingsData.glowRadius);
    lightingShader.setFloat("time", simulationTime);
    lightingShader.setFloat("ambientLight", renderSettingsData.ambientLight);
    lightingShader.setBool("skyEnabled", renderSettingsData.skyEnabled);
    lightingShader.setFloat("timeOfDay", renderSettingsData.timeOfDay);
    lightingShader.setBool("showSun", renderSettingsData.showSun);

    int bounces = renderSettingsData.lightBounces;
    for (int bounce = 0; bounce < bounces; bounce++) {
        // Read from state + normals, ping-pong lightmaps
        GLuint readLight  = (bounce == 0) ? lightmapTexture : ((bounce % 2 == 0) ? lightmapTexture : lightmapPingPong);
        GLuint writeLight = (bounce % 2 == 0) ? lightmapPingPong : lightmapTexture;

        // binding 0: stateIn
        // binding 1: normalIn
        // binding 3: lightIn (read from previous bounce, or empty on first)
        // binding 4: lightOut (write)
        glBindImageTexture(0, stateTextures[currentBuffer], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8UI);
        glBindImageTexture(1, normalTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
        glBindImageTexture(3, readLight, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
        glBindImageTexture(4, writeLight, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

        lightingShader.setInt("bouncePass", bounce);

        glDispatchCompute(workGroupsX, workGroupsY, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    // Determine which lightmap has the final result
    GLuint finalLightmap = (bounces % 2 == 0) ? lightmapTexture : lightmapPingPong;
    // If bounces == 0, we never ran the loop, use lightmapTexture as empty fallback
    if (bounces == 0) finalLightmap = lightmapTexture;

    // Pass 3: Composite
    // binding 0: stateIn
    // binding 1: colorIn
    // binding 2: normalIn
    // binding 3: lightmapIn
    // binding 4: displayOut
    // binding 5: skyIn
    glBindImageTexture(0, stateTextures[currentBuffer], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8UI);
    glBindImageTexture(1, colorTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
    glBindImageTexture(2, normalTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
    glBindImageTexture(3, finalLightmap, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
    glBindImageTexture(4, displayTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
    glBindImageTexture(5, skyTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);

    compositeShader.use();
    compositeShader.setFloat("ambientLight", renderSettingsData.ambientLight);
    compositeShader.setFloat("specularStrength", renderSettingsData.specularStrength);
    compositeShader.setFloat("time", simulationTime);
    compositeShader.setBool("skyEnabled", renderSettingsData.skyEnabled);
    compositeShader.setFloat("timeOfDay", renderSettingsData.timeOfDay);

    glDispatchCompute(workGroupsX, workGroupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);


    // Pass 4: Blit display texture to screen with camera transform
    // Camera defines which portion of the texture to sample
    float visibleW = static_cast<float>(worldWidth) / camZoom;
    float visibleH = static_cast<float>(worldHeight) / camZoom;

    // UV bounds in the display texture (0..1 range)
    float uMin = (camX - visibleW * 0.5f) / static_cast<float>(worldWidth);
    float uMax = (camX + visibleW * 0.5f) / static_cast<float>(worldWidth);
    float vMin = (camY - visibleH * 0.5f) / static_cast<float>(worldHeight);
    float vMax = (camY + visibleH * 0.5f) / static_cast<float>(worldHeight);

    // Clamp to valid range
    if (uMin < 0.0f) { uMax -= uMin; uMin = 0.0f; }
    if (uMax > 1.0f) { uMin -= (uMax - 1.0f); uMax = 1.0f; }
    if (vMin < 0.0f) { vMax -= vMin; vMin = 0.0f; }
    if (vMax > 1.0f) { vMin -= (vMax - 1.0f); vMax = 1.0f; }
    uMin = std::max(0.0f, uMin);
    uMax = std::min(1.0f, uMax);
    vMin = std::max(0.0f, vMin);
    vMax = std::min(1.0f, vMax);
    glViewport(screenX, screenY, screenWidth, screenHeight);

    quadShader.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, displayTexture);
    quadShader.setInt("displayTex", 0);

    // Pass UV range as uniforms for camera
    quadShader.setVec4("uvBounds", uMin, vMin, uMax, vMax);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void World::swapBuffers() {
    currentBuffer = 1 - currentBuffer;
}

void World::swapForceBuffers() {
    currentForceBuffer = 1 - currentForceBuffer;
}

}