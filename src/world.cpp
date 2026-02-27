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
*
* MODIFIED: Phase 1 - Effector List SSBO for scatter-based physics.
*           Phase 2 - Temperature textures + temperature.comp diffusion pass.
*           Replaced bounce-based lighting with Radiance Cascades (RC).
*/

#include "world.hpp"
#include <iostream>
#include <vector>
#include <cmath>

namespace cisalpine {

World::World(int width, int height)
    : worldWidth(width), worldHeight(height) {
    chunkGridWidth  = (worldWidth  + CHUNK_SIZE - 1) / CHUNK_SIZE;
    chunkGridHeight = (worldHeight + CHUNK_SIZE - 1) / CHUNK_SIZE;
    totalChunks     = chunkGridWidth * chunkGridHeight;
}

World::~World() {
    if (stateTextures[0]) glDeleteTextures(2, stateTextures);
    if (forceTextures[0]) glDeleteTextures(2, forceTextures);
    if (temperatureTextures[0]) glDeleteTextures(2, temperatureTextures);
    if (colorTexture) glDeleteTextures(1, &colorTexture);
    if (normalTexture) glDeleteTextures(1, &normalTexture);
    if (lightmapTexture) glDeleteTextures(1, &lightmapTexture);
    if (lightmapPingPong) glDeleteTextures(1, &lightmapPingPong);
    if (displayTexture) glDeleteTextures(1, &displayTexture);
    if (skyTexture) glDeleteTextures(1, &skyTexture);
    if (radianceFieldTexture) glDeleteTextures(1, &radianceFieldTexture);
    if (quadVAO) glDeleteVertexArrays(1, &quadVAO);
    if (quadVBO) glDeleteBuffers(1, &quadVBO);
    if (chunkGridSSBO[0]) glDeleteBuffers(2, chunkGridSSBO);
    if (activeChunkSSBO) glDeleteBuffers(1, &activeChunkSSBO);
    if (cascadeDataSSBO) glDeleteBuffers(1, &cascadeDataSSBO);
    if (effectorSSBO) glDeleteBuffers(1, &effectorSSBO);

    for (int i = 0; i < TIMER_COUNT; i++) {
        gpuTimers[i].destroy();
    }
}

bool World::init(const std::string& shaderHeader) {
    // Build augmented shader header with chunk, RC, and effector constants
    std::string fullHeader = shaderHeader;
    fullHeader += "#define CHUNK_SIZE " + std::to_string(CHUNK_SIZE) + "\n";
    fullHeader += "#define CHUNK_GRID_W " + std::to_string(chunkGridWidth) + "\n";
    fullHeader += "#define CHUNK_GRID_H " + std::to_string(chunkGridHeight) + "\n";
    fullHeader += "#define TOTAL_CHUNKS " + std::to_string(totalChunks) + "\n";
    fullHeader += "#define BASE_RAYS " + std::to_string(RC_BASE_RAYS) + "\n";
    fullHeader += "#define MAX_CASCADES " + std::to_string(RC_MAX_CASCADES) + "\n";
    fullHeader += "#define MAX_EFFECTORS " + std::to_string(MAX_EFFECTORS) + "\n";
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
    if (!temperatureShader.loadCompute("shaders/temperature.comp", fullHeader)) {
        std::cerr << "Failed to load temperature shader" << std::endl;
        return false;
    }
    if (!renderShader.loadCompute("shaders/render.comp", fullHeader)) {
        std::cerr << "Failed to load render shader" << std::endl;
        return false;
    }

    // === Radiance Cascade shaders (replaces lighting.comp) ===
    if (!rcEmitterShader.loadCompute("shaders/rc_emitters.comp", fullHeader)) {
        std::cerr << "Failed to load RC emitter shader" << std::endl;
        return false;
    }
    if (!rcCascadeShader.loadCompute("shaders/rc_cascade.comp", fullHeader)) {
        std::cerr << "Failed to load RC cascade shader" << std::endl;
        return false;
    }
    if (!rcResolveShader.loadCompute("shaders/rc_resolve.comp", fullHeader)) {
        std::cerr << "Failed to load RC resolve shader" << std::endl;
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
    createEffectorBuffer();
    createRCBuffers();

    // Initialize GPU timer queries
    for (int i = 0; i < TIMER_COUNT; i++) {
        gpuTimers[i].init();
    }

    wakeAllChunks();

    std::cout << "Chunk system initialized: " << chunkGridWidth << "x" << chunkGridHeight
              << " grid (" << totalChunks << " chunks of " << CHUNK_SIZE << "x" << CHUNK_SIZE << ")" << std::endl;

    // Build initial cascade level descriptors
    rebuildRCLevels(renderSettingsData.rcCascadeLevels);

    std::cout << "Radiance Cascades initialized: " << rcNumLevels << " levels, "
              << rcLevels[0].probeCountX << "x" << rcLevels[0].probeCountY << " L0 probes" << std::endl;

    std::cout << "Effector buffer initialized: capacity " << MAX_EFFECTORS << " effectors" << std::endl;
    std::cout << "Temperature system initialized: double-buffered R16F textures" << std::endl;

    return true;
}

void World::createTextures() {
    // State textures (RGBA8UI)
    glGenTextures(2, stateTextures);
    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, stateTextures[i]);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8UI, worldWidth, worldHeight);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        std::vector<uint8_t> clearData(worldWidth * worldHeight * 4, 0);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, worldWidth, worldHeight,
            GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, clearData.data());
    }

    // Force field textures (RGBA16F)
    glGenTextures(2, forceTextures);
    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, forceTextures[i]);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, worldWidth, worldHeight);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        std::vector<float> clearForce(worldWidth * worldHeight * 4, 0.0f);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, worldWidth, worldHeight,
            GL_RGBA, GL_FLOAT, clearForce.data());
    }

    glGenTextures(2, temperatureTextures);
    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, temperatureTextures[i]);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_R16F, worldWidth, worldHeight);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Initialize to room temperature (20.0)
        std::vector<float> initTemp(worldWidth * worldHeight, 20.0f);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, worldWidth, worldHeight,
            GL_RED, GL_FLOAT, initTemp.data());
    }

    // Color texture (RGBA8)
    glGenTextures(1, &colorTexture);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, worldWidth, worldHeight);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Normal texture (RGBA16F)
    glGenTextures(1, &normalTexture);
    glBindTexture(GL_TEXTURE_2D, normalTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, worldWidth, worldHeight);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Lightmap textures (RGBA16F) - written by RC resolve
    glGenTextures(1, &lightmapTexture);
    glBindTexture(GL_TEXTURE_2D, lightmapTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, worldWidth, worldHeight);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Keep ping-pong for potential future use
    glGenTextures(1, &lightmapPingPong);
    glBindTexture(GL_TEXTURE_2D, lightmapPingPong);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, worldWidth, worldHeight);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Display texture (RGBA8)
    glGenTextures(1, &displayTexture);
    glBindTexture(GL_TEXTURE_2D, displayTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, worldWidth, worldHeight);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Sky texture (RGBA8)
    glGenTextures(1, &skyTexture);
    glBindTexture(GL_TEXTURE_2D, skyTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, worldWidth, worldHeight);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // === Radiance Field texture (RGBA16F): RGB=emission, A=opacity ===
    glGenTextures(1, &radianceFieldTexture);
    glBindTexture(GL_TEXTURE_2D, radianceFieldTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, worldWidth, worldHeight);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void World::createEffectorBuffer() {
    size_t headerSize = 4 * sizeof(uint32_t);  // 16 bytes: count + 3 padding
    size_t dataSize = MAX_EFFECTORS * sizeof(GPUEffector);
    size_t totalSize = headerSize + dataSize;

    glGenBuffers(1, &effectorSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, effectorSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, totalSize, nullptr, GL_DYNAMIC_DRAW);

    // Initialize count to 0
    uint32_t zero = 0;
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(uint32_t), &zero);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void World::createRCBuffers() {
    glGenBuffers(1, &cascadeDataSSBO);
    // Actual allocation happens in rebuildRCLevels()
}

void World::rebuildRCLevels(int numLevels) {
    numLevels = std::max(1, std::min(numLevels, static_cast<int>(RC_MAX_CASCADES)));
    rcNumLevels = numLevels;
    rcLevels.resize(numLevels);

    // Compute per-level metadata
    int totalEntries = 0;
    for (int n = 0; n < numLevels; n++) {
        RCLevelInfo& lvl = rcLevels[n];
        lvl.spacing    = 1 << n;
        lvl.numRays    = RC_BASE_RAYS * (1 << n);
        lvl.probeCountX = (worldWidth  + lvl.spacing - 1) / lvl.spacing;
        lvl.probeCountY = (worldHeight + lvl.spacing - 1) / lvl.spacing;
        lvl.dataOffset  = totalEntries;
        lvl.totalEntries = lvl.probeCountX * lvl.probeCountY * lvl.numRays;
        totalEntries += lvl.totalEntries;
    }

    // Allocate (or reallocate) the cascade SSBO
    // Each entry is a vec4 (16 bytes)
    cascadeDataSize = static_cast<size_t>(totalEntries) * sizeof(float) * 4;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cascadeDataSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, cascadeDataSize, nullptr, GL_DYNAMIC_DRAW);

    // Clear to zero
    std::vector<float> zeros(totalEntries * 4, 0.0f);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, cascadeDataSize, zeros.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    std::cout << "RC levels rebuilt: " << numLevels << " levels, "
              << totalEntries << " total probe*ray entries ("
              << (cascadeDataSize / 1024) << " KB)" << std::endl;
}

void World::renderRadianceCascades() {
    // Check if cascade level count changed
    int targetLevels = renderSettingsData.rcCascadeLevels;
    if (targetLevels != rcNumLevels) {
        rebuildRCLevels(targetLevels);
    }

    GLuint workGroupsX = (worldWidth + 15) / 16;
    GLuint workGroupsY = (worldHeight + 15) / 16;

    // Step 1: Extract emitters into radiance field G-buffer
    gpuTimers[TIMER_RC_EMITTERS].begin();

    glBindImageTexture(0, stateTextures[currentBuffer], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8UI);
    glBindImageTexture(1, radianceFieldTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

    rcEmitterShader.use();
    rcEmitterShader.setFloat("time", simulationTime);
    rcEmitterShader.setBool("glowEnabled", renderSettingsData.glowEnabled);
    rcEmitterShader.setFloat("glowIntensity", renderSettingsData.glowIntensity);
    rcEmitterShader.setBool("skyEnabled", renderSettingsData.skyEnabled);
    rcEmitterShader.setFloat("timeOfDay", renderSettingsData.timeOfDay);

    glDispatchCompute(workGroupsX, workGroupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    gpuTimers[TIMER_RC_EMITTERS].end();

    // Step 2: Cascade merge passes (highest level → level 0)
    gpuTimers[TIMER_RC_CASCADE].begin();

    // Bind the radiance field for cascade shader to read
    glBindImageTexture(0, radianceFieldTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
    glBindImageTexture(1, stateTextures[currentBuffer], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8UI);

    // Bind cascade SSBO
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, cascadeDataSSBO);

    rcCascadeShader.use();
    rcCascadeShader.setInt("numCascades", rcNumLevels);
    rcCascadeShader.setInt("worldSize", worldWidth);  // Note: using setVec2-like below
    // Set worldSize as ivec2
    glUniform2i(glGetUniformLocation(rcCascadeShader.id(), "worldSize"), worldWidth, worldHeight);
    rcCascadeShader.setFloat("time", simulationTime);
    rcCascadeShader.setBool("skyEnabled", renderSettingsData.skyEnabled);
    rcCascadeShader.setFloat("timeOfDay", renderSettingsData.timeOfDay);

    // Upload per-level uniforms
    for (int n = 0; n < rcNumLevels; n++) {
        std::string offsetName = "cascadeOffsets[" + std::to_string(n) + "]";
        std::string pcxName    = "probeCountX[" + std::to_string(n) + "]";
        std::string pcyName    = "probeCountY[" + std::to_string(n) + "]";

        glUniform1i(glGetUniformLocation(rcCascadeShader.id(), offsetName.c_str()), rcLevels[n].dataOffset);
        glUniform1i(glGetUniformLocation(rcCascadeShader.id(), pcxName.c_str()),    rcLevels[n].probeCountX);
        glUniform1i(glGetUniformLocation(rcCascadeShader.id(), pcyName.c_str()),    rcLevels[n].probeCountY);
    }

    // Dispatch from highest cascade level down to 0
    for (int n = rcNumLevels - 1; n >= 0; n--) {
        rcCascadeShader.use();  // Rebind in case GL state changed
        rcCascadeShader.setInt("cascadeLevel", n);

        // Dispatch enough invocations to cover all probes * rays at this level
        int totalWork = rcLevels[n].totalEntries;
        GLuint workGroups = (totalWork + 63) / 64;  // local_size_x = 64
        glDispatchCompute(workGroups, 1, 1);

        // Barrier between cascade levels so level N can read N+1's results
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    gpuTimers[TIMER_RC_CASCADE].end();

    // Step 3: Resolve cascade 0 → per-pixel lightmap
    gpuTimers[TIMER_RC_RESOLVE].begin();

    glBindImageTexture(0, lightmapTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, cascadeDataSSBO);

    rcResolveShader.use();
    glUniform2i(glGetUniformLocation(rcResolveShader.id(), "worldSize"), worldWidth, worldHeight);

    // Upload per-level uniforms for resolve shader too
    for (int n = 0; n < rcNumLevels; n++) {
        std::string offsetName = "cascadeOffsets[" + std::to_string(n) + "]";
        std::string pcxName    = "probeCountX[" + std::to_string(n) + "]";
        std::string pcyName    = "probeCountY[" + std::to_string(n) + "]";

        glUniform1i(glGetUniformLocation(rcResolveShader.id(), offsetName.c_str()), rcLevels[n].dataOffset);
        glUniform1i(glGetUniformLocation(rcResolveShader.id(), pcxName.c_str()),    rcLevels[n].probeCountX);
        glUniform1i(glGetUniformLocation(rcResolveShader.id(), pcyName.c_str()),    rcLevels[n].probeCountY);
    }

    glDispatchCompute(workGroupsX, workGroupsY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    gpuTimers[TIMER_RC_RESOLVE].end();
}

void World::createChunkBuffers() {
    glGenBuffers(2, chunkGridSSBO);
    std::vector<GLuint> zeros(totalChunks, 0);
    for (int i = 0; i < 2; i++) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunkGridSSBO[i]);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                     totalChunks * sizeof(GLuint),
                     zeros.data(),
                     GL_DYNAMIC_DRAW);
    }

    glGenBuffers(1, &activeChunkSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, activeChunkSSBO);
    size_t activeListSize = (3 + totalChunks) * sizeof(GLuint);
    std::vector<GLuint> initData(3 + totalChunks, 0);
    initData[1] = 1;
    initData[2] = 1;
    glBufferData(GL_SHADER_STORAGE_BUFFER, activeListSize, initData.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void World::createQuad() {
    float vertices[] = {
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

    std::vector<float> clearForce(worldWidth * worldHeight * 4, 0.0f);
    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, forceTextures[i]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, worldWidth, worldHeight,
            GL_RGBA, GL_FLOAT, clearForce.data());
    }

    // Clear temperature to room temp
    std::vector<float> clearTemp(worldWidth * worldHeight, 20.0f);
    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, temperatureTextures[i]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, worldWidth, worldHeight,
            GL_RED, GL_FLOAT, clearTemp.data());
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    wakeAllChunks();
}

void World::wakeAllChunks() {
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

    GLuint one = 1;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunkGridSSBO[currentChunkGrid]);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, chunkIdx * sizeof(GLuint), sizeof(GLuint), &one);

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
    GLuint zero = 0;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, activeChunkSSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint), &zero);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, chunkGridSSBO[currentChunkGrid]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, chunkGridSSBO[1 - currentChunkGrid]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, activeChunkSSBO);

    chunkBuildShader.use();

    GLuint workGroups = (totalChunks + 63) / 64;
    glDispatchCompute(workGroups, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, activeChunkSSBO);
    GLuint count;
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint), &count);
    lastActiveChunkCount = static_cast<int>(count);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void World::swapChunkGrids() {
    currentChunkGrid = 1 - currentChunkGrid;
}

void World::swapTemperatureBuffers() {
    currentTempBuffer = 1 - currentTempBuffer;
}

void World::forceUpdateStep() {
    int nextForceBuffer = 1 - currentForceBuffer;

    glBindImageTexture(0, forceTextures[currentForceBuffer], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
    glBindImageTexture(1, forceTextures[nextForceBuffer], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    glBindImageTexture(2, stateTextures[currentBuffer], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8UI);

    // Phase 1: Bind effector SSBO for reading
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, effectorSSBO);

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

// Phase 2: Temperature diffusion step
void World::temperatureStep() {
    int nextTempBuffer = 1 - currentTempBuffer;

    // Bind temperature textures: read from current, write to next
    glBindImageTexture(4, temperatureTextures[currentTempBuffer], 0, GL_FALSE, 0, GL_READ_ONLY, GL_R16F);
    glBindImageTexture(5, temperatureTextures[nextTempBuffer], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R16F);
    // Also need state texture to know element types
    glBindImageTexture(0, stateTextures[currentBuffer], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8UI);

    temperatureShader.use();
    temperatureShader.setVec2("worldSize", static_cast<float>(worldWidth), static_cast<float>(worldHeight));
    temperatureShader.setFloat("time", simulationTime);
    temperatureShader.setFloat("ambientTemperature", simSettings.ambientTemperature);

    GLuint workGroupsX = (worldWidth + 15) / 16;
    GLuint workGroupsY = (worldHeight + 15) / 16;
    glDispatchCompute(workGroupsX, workGroupsY, 1);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    swapTemperatureBuffers();
}

void World::simulationStep() {
    // Phase 1: Force update reads effectors from PREVIOUS simulation step
    gpuTimers[TIMER_FORCE_UPDATE].begin();
    forceUpdateStep();
    gpuTimers[TIMER_FORCE_UPDATE].end();

    // Phase 1: NOW reset effector count to 0 AFTER physics has read them,
    // but BEFORE simulation writes new ones. This fixes the bug where
    // effectors were being cleared before physics could read them.
    uint32_t zero = 0;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, effectorSSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(uint32_t), &zero);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Phase 2: Temperature diffusion runs BEFORE simulation
    gpuTimers[TIMER_TEMPERATURE].begin();
    temperatureStep();
    gpuTimers[TIMER_TEMPERATURE].end();

    int nextBuffer = 1 - currentBuffer;

    glBindImageTexture(0, stateTextures[currentBuffer], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8UI);
    glBindImageTexture(1, stateTextures[nextBuffer], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8UI);
    glBindImageTexture(3, forceTextures[currentForceBuffer], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);

    // Phase 2: Bind temperature texture for simulation to read
    glBindImageTexture(4, temperatureTextures[currentTempBuffer], 0, GL_FALSE, 0, GL_READ_ONLY, GL_R16F);

    // Phase 1: Bind effector SSBO for simulation to write effectors
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, effectorSSBO);

    simulationShader.use();
    simulationShader.setVec2("worldSize", static_cast<float>(worldWidth), static_cast<float>(worldHeight));
    simulationShader.setFloat("time", simulationTime);
    simulationShader.setUint("frameCount", frameCount);

    gpuTimers[TIMER_SIMULATION].begin();

    if (simSettings.chunkSleepEnabled) {
        gpuTimers[TIMER_SIMULATION].end();

        gpuTimers[TIMER_CHUNK_BUILD].begin();
        buildActiveChunkList();
        gpuTimers[TIMER_CHUNK_BUILD].end();

        gpuTimers[TIMER_SIMULATION].begin();

        simulationShader.use();

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, chunkGridSSBO[currentChunkGrid]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, chunkGridSSBO[1 - currentChunkGrid]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, activeChunkSSBO);

        simulationShader.setBool("useChunkSleep", true);

        glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, activeChunkSSBO);
        glDispatchComputeIndirect(0);
        glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, 0);

        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

        swapChunkGrids();
    } else {
        simulationShader.setBool("useChunkSleep", false);

        GLuint workGroupsX = (worldWidth + 15) / 16;
        GLuint workGroupsY = (worldHeight + 15) / 16;
        glDispatchCompute(workGroupsX, workGroupsY, 1);

        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
    }

    gpuTimers[TIMER_SIMULATION].end();

    swapBuffers();
    frameCount++;
}

void World::update(float dt) {
    for (int i = 0; i < TIMER_COUNT; i++) {
        gpuTimers[i].collect();
    }

    accumulatedTime += dt;
    simulationTime += dt;

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

    // Pass 1: Convert state texture to colors + normals + sky
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

    // Pass 2: Radiance Cascades (replaces old bounce lighting)
    renderRadianceCascades();

    // The lightmap is now in lightmapTexture, written by rc_resolve

    // Pass 3: Composite
    gpuTimers[TIMER_COMPOSITE].begin();

    glBindImageTexture(0, stateTextures[currentBuffer], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8UI);
    glBindImageTexture(1, colorTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
    glBindImageTexture(2, normalTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
    glBindImageTexture(3, lightmapTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
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

    float visibleW = static_cast<float>(worldWidth) / camZoom;
    float visibleH = static_cast<float>(worldHeight) / camZoom;

    float uMin = (camX - visibleW * 0.5f) / static_cast<float>(worldWidth);
    float uMax = (camX + visibleW * 0.5f) / static_cast<float>(worldWidth);
    float vMin = (camY - visibleH * 0.5f) / static_cast<float>(worldHeight);
    float vMax = (camY + visibleH * 0.5f) / static_cast<float>(worldHeight);

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

    // Choose debug view texture
    GLuint blitTexture = displayTexture;
    int viewModeInt = 0;
    switch (viewMode) {
        case DebugViewMode::Final:         blitTexture = displayTexture;     viewModeInt = 0; break;
        case DebugViewMode::Color:         blitTexture = colorTexture;       viewModeInt = 0; break;
        case DebugViewMode::Normals:       blitTexture = normalTexture;      viewModeInt = 1; break;
        case DebugViewMode::Lightmap:      blitTexture = lightmapTexture;    viewModeInt = 2; break;
        case DebugViewMode::State:         blitTexture = displayTexture;     viewModeInt = 0; break;
        case DebugViewMode::Sky:           blitTexture = skyTexture;         viewModeInt = 0; break;
        case DebugViewMode::ForceField:    blitTexture = forceTextures[currentForceBuffer]; viewModeInt = 3; break;
        case DebugViewMode::RadianceField: blitTexture = radianceFieldTexture; viewModeInt = 2; break;
        case DebugViewMode::Temperature:   blitTexture = temperatureTextures[currentTempBuffer]; viewModeInt = 4; break;
        default: break;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, blitTexture);
    quadShader.setInt("displayTex", 0);
    quadShader.setInt("viewMode", viewModeInt);

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