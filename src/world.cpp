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
    // Calculate chunk grid dimensions
    chunkGridWidth  = (worldWidth  + CHUNK_SIZE - 1) / CHUNK_SIZE;
    chunkGridHeight = (worldHeight + CHUNK_SIZE - 1) / CHUNK_SIZE;
    totalChunks     = chunkGridWidth * chunkGridHeight;
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
    if (chunkGridSSBO[0]) glDeleteBuffers(2, chunkGridSSBO);
    if (activeChunkSSBO) glDeleteBuffers(1, &activeChunkSSBO);

    // Destroy GPU timers
    for (int i = 0; i < TIMER_COUNT; i++) {
        gpuTimers[i].destroy();
    }
}

bool World::init(const std::string& shaderHeader) {
    // Build an augmented shader header that includes chunk constants
    std::string fullHeader = shaderHeader;
    fullHeader += "#define CHUNK_SIZE " + std::to_string(CHUNK_SIZE) + "\n";
    fullHeader += "#define CHUNK_GRID_W " + std::to_string(chunkGridWidth) + "\n";
    fullHeader += "#define CHUNK_GRID_H " + std::to_string(chunkGridHeight) + "\n";
    fullHeader += "#define TOTAL_CHUNKS " + std::to_string(totalChunks) + "\n";
    fullHeader += "\n";

    // Load shaders
    if (!simulationShader.loadCompute("shaders/simulation.comp", fullHeader)) {
        std::cerr << "Failed to load simulation shader" << std::endl;
        return false;
    }
    if (!forceUpdateShader.loadCompute("shaders/physics.comp", fullHeader)) {
        std::cerr << "Failed to load physics shader" << std::endl;
        return false;
    }
    if (!renderShader.loadCompute("shaders/render.comp", fullHeader)) {
        std::cerr << "Failed to load render shader" << std::endl;
        return false;
    }
    if (!lightingShader.loadCompute("shaders/lighting.comp", fullHeader)) {
        std::cerr << "Failed to load lighting shader" << std::endl;
        return false;
    }
    if (!compositeShader.loadCompute("shaders/composite.comp", fullHeader)) {
        std::cerr << "Failed to load composite shader" << std::endl;
        return false;
    }
    if (!quadShader.loadFromFile("shaders/quad.vert", "shaders/quad.frag")) {
        std::cerr << "Failed to load quad shader" << std::endl;
        return false;
    }
    if (!chunkBuildShader.loadCompute("shaders/chunk_build.comp", fullHeader)) {
        std::cerr << "Failed to load chunk build shader" << std::endl;
        return false;
    }

    createTextures();
    createQuad();
    createChunkBuffers();

    // Initialize GPU timer queries
    for (int i = 0; i < TIMER_COUNT; i++) {
        gpuTimers[i].init();
    }

    // Wake all chunks for the first frame
    wakeAllChunks();

    std::cout << "Chunk system initialized: " << chunkGridWidth << "x" << chunkGridHeight
              << " grid (" << totalChunks << " chunks of " << CHUNK_SIZE << "x" << CHUNK_SIZE << ")" << std::endl;

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

void World::createChunkBuffers() {
    // Double-buffered chunk grid SSBOs
    glGenBuffers(2, chunkGridSSBO);
    std::vector<GLuint> zeros(totalChunks, 0);
    for (int i = 0; i < 2; i++) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunkGridSSBO[i]);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                     totalChunks * sizeof(GLuint),
                     zeros.data(),
                     GL_DYNAMIC_DRAW);
    }

    // Active chunk list SSBO: 3 uints for dispatch header + totalChunks for IDs
    glGenBuffers(1, &activeChunkSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, activeChunkSSBO);
    size_t activeListSize = (3 + totalChunks) * sizeof(GLuint);
    std::vector<GLuint> initData(3 + totalChunks, 0);
    initData[1] = 1; // num_groups_y = 1
    initData[2] = 1; // num_groups_z = 1
    glBufferData(GL_SHADER_STORAGE_BUFFER, activeListSize, initData.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
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

    // Wake all chunks so the cleared state gets processed
    wakeAllChunks();
}

// === Chunk System ===

void World::wakeAllChunks() {
    // Set all entries in the current-read chunk grid to 1 (awake)
    std::vector<GLuint> allAwake(totalChunks, 1);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunkGridSSBO[currentChunkGrid]);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, totalChunks * sizeof(GLuint), allAwake.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void World::wakeChunkAt(int worldX, int worldY) {
    if (worldX < 0 || worldX >= worldWidth || worldY < 0 || worldY >= worldHeight) return;

    int cx = worldX / CHUNK_SIZE;
    int cy = worldY / CHUNK_SIZE;
    int chunkIdx = cy * chunkGridWidth + cx;

    // Write 1 to the current-read chunk grid so it's picked up this frame
    GLuint one = 1;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunkGridSSBO[currentChunkGrid]);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, chunkIdx * sizeof(GLuint), sizeof(GLuint), &one);

    // Also wake neighbors for boundary safety
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int nx = cx + dx;
            int ny = cy + dy;
            if (nx >= 0 && nx < chunkGridWidth && ny >= 0 && ny < chunkGridHeight) {
                int nIdx = ny * chunkGridWidth + nx;
                glBufferSubData(GL_SHADER_STORAGE_BUFFER, nIdx * sizeof(GLuint), sizeof(GLuint), &one);
            }
        }
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void World::buildActiveChunkList() {
    // Reset the dispatch counter to 0
    GLuint zero = 0;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, activeChunkSSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint), &zero); // num_groups_x = 0
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Bind SSBOs for the chunk build shader:
    // binding 4 = chunk grid (current read)
    // binding 5 = chunk grid (next frame write - clear to 0)
    // binding 6 = active chunk list + indirect dispatch
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, chunkGridSSBO[currentChunkGrid]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, chunkGridSSBO[1 - currentChunkGrid]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, activeChunkSSBO);

    chunkBuildShader.use();

    // Dispatch one thread per chunk
    GLuint workGroups = (totalChunks + 63) / 64;
    glDispatchCompute(workGroups, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

    // Optional: read back count for debug display
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, activeChunkSSBO);
    GLuint count;
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint), &count);
    lastActiveChunkCount = static_cast<int>(count);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void World::swapChunkGrids() {
    currentChunkGrid = 1 - currentChunkGrid;
}

// === Simulation ===

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
    gpuTimers[TIMER_FORCE_UPDATE].begin();
    forceUpdateStep();
    gpuTimers[TIMER_FORCE_UPDATE].end();

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

    gpuTimers[TIMER_SIMULATION].begin();

    if (simSettings.chunkSleepEnabled) {
        // Pass A: Build the active chunk list from current chunk grid
        gpuTimers[TIMER_SIMULATION].end(); // end sim timer briefly for chunk build

        gpuTimers[TIMER_CHUNK_BUILD].begin();
        buildActiveChunkList();
        gpuTimers[TIMER_CHUNK_BUILD].end();

        gpuTimers[TIMER_SIMULATION].begin(); // restart sim timer for actual dispatch

        simulationShader.use();

        // Bind chunk SSBOs for the simulation shader
        // binding 4 = chunk grid (read, for this frame)
        // binding 5 = chunk grid (next frame, sim writes wakes here)
        // binding 6 = active chunk list (read chunk IDs)
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, chunkGridSSBO[currentChunkGrid]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, chunkGridSSBO[1 - currentChunkGrid]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, activeChunkSSBO);

        simulationShader.setBool("useChunkSleep", true);

        // Pass B: Indirect dispatch — only process awake chunks
        glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, activeChunkSSBO);
        glDispatchComputeIndirect(0);
        glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, 0);

        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

        // Swap chunk grids: the "next frame write" becomes the "current read" for next step
        swapChunkGrids();
    } else {
        // Fallback: classic full-world dispatch
        simulationShader.setBool("useChunkSleep", false);

        GLuint workGroupsX = (worldWidth + 15) / 16;
        GLuint workGroupsY = (worldHeight + 15) / 16;
        glDispatchCompute(workGroupsX, workGroupsY, 1);

        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    gpuTimers[TIMER_SIMULATION].end();

    swapBuffers();
    frameCount++;
}

void World::update(float dt) {
    // Collect all GPU timer results from previous frame
    for (int i = 0; i < TIMER_COUNT; i++) {
        gpuTimers[i].collect();
    }

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
    float camX, float camY, float camZoom, DebugViewMode viewMode) {
    GLuint workGroupsX = (worldWidth + 15) / 16;
    GLuint workGroupsY = (worldHeight + 15) / 16;

    // Pass 1: Convert state texture to colors
    gpuTimers[TIMER_RENDER].begin();

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

    gpuTimers[TIMER_RENDER].end();

    // Pass 2: Light Propagation
    gpuTimers[TIMER_LIGHTING].begin();

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
        GLuint readLight  = (bounce == 0) ? lightmapTexture : ((bounce % 2 == 0) ? lightmapTexture : lightmapPingPong);
        GLuint writeLight = (bounce % 2 == 0) ? lightmapPingPong : lightmapTexture;

        glBindImageTexture(0, stateTextures[currentBuffer], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8UI);
        glBindImageTexture(1, normalTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
        glBindImageTexture(3, readLight, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
        glBindImageTexture(4, writeLight, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

        lightingShader.setInt("bouncePass", bounce);

        glDispatchCompute(workGroupsX, workGroupsY, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    gpuTimers[TIMER_LIGHTING].end();

    // Determine which lightmap has the final result
    GLuint finalLightmap = (bounces % 2 == 0) ? lightmapTexture : lightmapPingPong;
    if (bounces == 0) finalLightmap = lightmapTexture;

    // Pass 3: Composite
    gpuTimers[TIMER_COMPOSITE].begin();

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

    gpuTimers[TIMER_COMPOSITE].end();

    // Pass 4: Blit to screen
    gpuTimers[TIMER_QUAD_BLIT].begin();

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

    // Choose which texture to display based on debug view mode
    GLuint blitTexture = displayTexture;
    int viewModeInt = 0; // 0=normal sampler2D, 1=normals (needs remap), 2=lightmap (float), 3=force field (float)
    switch (viewMode) {
        case DebugViewMode::Final:      blitTexture = displayTexture; viewModeInt = 0; break;
        case DebugViewMode::Color:      blitTexture = colorTexture;   viewModeInt = 0; break;
        case DebugViewMode::Normals:    blitTexture = normalTexture;  viewModeInt = 1; break;
        case DebugViewMode::Lightmap:   blitTexture = finalLightmap;  viewModeInt = 2; break;
        case DebugViewMode::State:      blitTexture = displayTexture; viewModeInt = 0; break; // State needs special handling - we use displayTexture as fallback
        case DebugViewMode::Sky:        blitTexture = skyTexture;     viewModeInt = 0; break;
        case DebugViewMode::ForceField: blitTexture = forceTextures[currentForceBuffer]; viewModeInt = 3; break;
        default: break;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, blitTexture);
    quadShader.setInt("displayTex", 0);
    quadShader.setInt("viewMode", viewModeInt);

    // Still bind these for the normal view path
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, getCurrentForceTexture());
    quadShader.setInt("physicsTex", 1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, normalTexture);
    quadShader.setInt("normalTex", 2);

    // Pass UV range as uniforms for camera
    quadShader.setVec4("uvBounds", uMin, vMin, uMax, vMax);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    gpuTimers[TIMER_QUAD_BLIT].end();
}

float World::getTotalGPUTimeMs() const {
    float total = 0.0f;
    for (int i = 0; i < TIMER_COUNT; i++) {
        total += gpuTimers[i].averageMs;
    }
    return total;
}

std::vector<uint32_t> World::readChunkGrid() const {
    std::vector<uint32_t> grid(totalChunks, 0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunkGridSSBO[currentChunkGrid]);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, totalChunks * sizeof(GLuint), grid.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    return grid;
}

void World::swapBuffers() {
    currentBuffer = 1 - currentBuffer;
}

void World::swapForceBuffers() {
    currentForceBuffer = 1 - currentForceBuffer;
}

}