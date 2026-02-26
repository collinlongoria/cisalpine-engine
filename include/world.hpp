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
#include <array>
#include <string>
#include <vector>

#include "shader.hpp"
#include <glm/glm.hpp>

namespace cisalpine {

// Chunk system constants
static constexpr int CHUNK_SIZE = 32;  // Each chunk is 32x32 pixels

// Radiance Cascade constants
static constexpr int RC_BASE_RAYS    = 4;   // Angular rays at cascade level 0
static constexpr int RC_MAX_CASCADES = 8;   // Maximum cascade levels

// Simulation settings
struct SimulationSettings {
    // Simulation loops per frame
    int stepsPerFrame = 4;

    // Chunk sleep system
    bool chunkSleepEnabled = true;
};

// Rendering Settings
struct RenderSettings {
    glm::vec4 backgroundColor = glm::vec4(0.05f, 0.05f, 0.08f, 1.0f);
    bool glowEnabled = true;
    float glowIntensity = 0.25f;
    float glowRadius = 3.3f;     // Kept for UI compat, now controls RC cascade count
    float ambientLight = 0.65f;
    float specularStrength = 0.6f;
    int lightBounces = 3;         // Now repurposed: number of RC cascade levels (1-7)

    // Sky system
    bool skyEnabled = true;
    float timeOfDay = 0.35f;
    bool showSun = true;

    // Radiance Cascades specific
    int rcCascadeLevels = 5;      // Number of cascade levels (1-7)
};

// Struct matching GL_DISPATCH_INDIRECT_BUFFER layout
struct DispatchIndirectCommand {
    GLuint num_groups_x;
    GLuint num_groups_y;
    GLuint num_groups_z;
};

// Debug visualization modes
enum class DebugViewMode {
    Final = 0,     // Normal composited output
    Color,         // Raw element colors (pre-lighting)
    Normals,       // Normal map visualization
    Lightmap,      // Light accumulation only
    State,         // Raw state texture (element IDs as colors)
    Sky,           // Sky texture only
    ForceField,    // Force field visualization
    RadianceField, // Emitter + opacity G-buffer
    COUNT
};

inline const char* debugViewModeName(DebugViewMode mode) {
    switch (mode) {
        case DebugViewMode::Final:         return "Final";
        case DebugViewMode::Color:         return "Color";
        case DebugViewMode::Normals:       return "Normals";
        case DebugViewMode::Lightmap:      return "Lightmap";
        case DebugViewMode::State:         return "State (IDs)";
        case DebugViewMode::Sky:           return "Sky";
        case DebugViewMode::ForceField:    return "Force Field";
        case DebugViewMode::RadianceField: return "Radiance Field";
        default: return "Unknown";
    }
}

// GPU Timer query system
struct GPUTimerQuery {
    static constexpr int HISTORY_SIZE = 60;

    GLuint queryObjects[2] = {0, 0};
    int currentQuery = 0;
    bool resultReady = false;

    float timings[HISTORY_SIZE] = {};
    int writeIndex = 0;
    float averageMs = 0.0f;

    void init() {
        glGenQueries(2, queryObjects);
    }

    void destroy() {
        if (queryObjects[0]) glDeleteQueries(2, queryObjects);
        queryObjects[0] = queryObjects[1] = 0;
    }

    void begin() {
        glBeginQuery(GL_TIME_ELAPSED, queryObjects[currentQuery]);
    }

    void end() {
        glEndQuery(GL_TIME_ELAPSED);
    }

    void collect() {
        int readQuery = 1 - currentQuery;
        GLuint available = 0;
        glGetQueryObjectuiv(queryObjects[readQuery], GL_QUERY_RESULT_AVAILABLE, &available);
        if (available) {
            GLuint64 elapsed = 0;
            glGetQueryObjectui64v(queryObjects[readQuery], GL_QUERY_RESULT, &elapsed);
            float ms = static_cast<float>(elapsed) / 1000000.0f;
            timings[writeIndex] = ms;
            writeIndex = (writeIndex + 1) % HISTORY_SIZE;

            float sum = 0.0f;
            for (int i = 0; i < HISTORY_SIZE; i++) sum += timings[i];
            averageMs = sum / static_cast<float>(HISTORY_SIZE);
            resultReady = true;
        }
        currentQuery = readQuery;
    }
};

// Named timer indices
enum GPUTimerIndex {
    TIMER_SIMULATION = 0,
    TIMER_FORCE_UPDATE,
    TIMER_CHUNK_BUILD,
    TIMER_RENDER,
    TIMER_RC_EMITTERS,    // NEW: Radiance field extraction
    TIMER_RC_CASCADE,     // NEW: Cascade merge passes
    TIMER_RC_RESOLVE,     // NEW: Cascade resolve to lightmap
    TIMER_COMPOSITE,
    TIMER_QUAD_BLIT,
    TIMER_COUNT
};

inline const char* gpuTimerName(int idx) {
    switch (idx) {
        case TIMER_SIMULATION:   return "Simulation";
        case TIMER_FORCE_UPDATE: return "Force Update";
        case TIMER_CHUNK_BUILD:  return "Chunk Build";
        case TIMER_RENDER:       return "Render";
        case TIMER_RC_EMITTERS:  return "RC Emitters";
        case TIMER_RC_CASCADE:   return "RC Cascade";
        case TIMER_RC_RESOLVE:   return "RC Resolve";
        case TIMER_COMPOSITE:    return "Composite";
        case TIMER_QUAD_BLIT:    return "Quad Blit";
        default: return "Unknown";
    }
}

// Radiance Cascade level descriptor (computed on CPU)
struct RCLevelInfo {
    int spacing;       // 2^N - probe spacing in pixels
    int numRays;       // BASE_RAYS * 2^N
    int probeCountX;   // ceil(worldWidth / spacing)
    int probeCountY;   // ceil(worldHeight / spacing)
    int dataOffset;    // Offset into cascadeData SSBO
    int totalEntries;  // probeCountX * probeCountY * numRays
};

class World {
public:
    World(int width, int height);
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    bool init(const std::string& shaderHeader);
    void update(float dt);
    void render(int screenX, int screenY, int screenWidth, int screenHeight,
        float camX, float camY, float camZoom, DebugViewMode viewMode = DebugViewMode::Final);

    void clear();

    int width() const { return worldWidth; }
    int height() const { return worldHeight; }

    GLuint getCurrentTexture() const { return stateTextures[currentBuffer]; }
    GLuint getDisplayTexture() const { return displayTexture; }
    GLuint getCurrentForceTexture() const { return forceTextures[currentForceBuffer]; }

    void wakeChunkAt(int worldX, int worldY);

    int getChunkGridWidth() const { return chunkGridWidth; }
    int getChunkGridHeight() const { return chunkGridHeight; }
    int getTotalChunks() const { return totalChunks; }
    int getActiveChunkCount() const { return lastActiveChunkCount; }

    GLuint getChunkWakeSSBO() const { return chunkGridSSBO[1 - currentChunkGrid]; }
    GLuint getChunkWakeBindingPoint() const { return 5; }

    RenderSettings& renderSettings() { return renderSettingsData; }
    const RenderSettings& renderSettings() const { return renderSettingsData; }

    SimulationSettings& simulationSettings() { return simSettings; }
    const SimulationSettings& simulationSettings() const { return simSettings; }

    const GPUTimerQuery& getTimer(int idx) const { return gpuTimers[idx]; }
    float getTotalGPUTimeMs() const;

    std::vector<uint32_t> readChunkGrid() const;

private:
    int worldWidth;
    int worldHeight;

    // Timing
    float accumulatedTime = 0.0f;
    float simulationTime = 0.0f;
    uint32_t frameCount = 0;
    static constexpr float FIXED_TIMESTEP = 1.0f / 60.0f;

    // Double-buffered state textures (RGBA8UI)
    GLuint stateTextures[2] = {0, 0};
    int currentBuffer = 0;

    // Double-buffered force field textures (RGBA16F)
    GLuint forceTextures[2] = {0, 0};
    int currentForceBuffer = 0;

    // === Chunk Sleep System ===
    int chunkGridWidth = 0;
    int chunkGridHeight = 0;
    int totalChunks = 0;
    GLuint chunkGridSSBO[2] = {0, 0};
    int currentChunkGrid = 0;
    GLuint activeChunkSSBO = 0;
    int lastActiveChunkCount = 0;
    Shader chunkBuildShader;

    // === Radiance Cascades ===
    GLuint radianceFieldTexture = 0;  // RGBA16F: RGB=emission, A=opacity
    GLuint cascadeDataSSBO = 0;       // Flat SSBO for all cascade level radiance data
    size_t cascadeDataSize = 0;       // Total size of cascade SSBO in bytes
    int    rcNumLevels = 0;           // Currently configured number of levels
    std::vector<RCLevelInfo> rcLevels; // Per-level metadata

    // RC Shaders
    Shader rcEmitterShader;    // Extracts emission + opacity G-buffer
    Shader rcCascadeShader;    // Per-level cascade merge
    Shader rcResolveShader;    // Resolves cascade 0 → lightmap

    // Rendering textures
    GLuint colorTexture = 0;
    GLuint normalTexture = 0;
    GLuint lightmapTexture = 0;
    GLuint lightmapPingPong = 0;  // Kept for compatibility, may be removed
    GLuint displayTexture = 0;
    GLuint skyTexture = 0;

    // Shaders
    Shader simulationShader;
    Shader forceUpdateShader;
    Shader renderShader;
    Shader compositeShader;
    Shader quadShader;

    // Quad for rendering
    GLuint quadVAO = 0;
    GLuint quadVBO = 0;

    // Settings
    RenderSettings renderSettingsData;
    SimulationSettings simSettings;

    // GPU Timer queries
    GPUTimerQuery gpuTimers[TIMER_COUNT];

    // Helpers
    void createTextures();
    void createQuad();
    void createChunkBuffers();
    void swapBuffers();
    void swapForceBuffers();
    void swapChunkGrids();
    void simulationStep();
    void forceUpdateStep();

    // Chunk helpers
    void wakeAllChunks();
    void buildActiveChunkList();

    // Radiance Cascade helpers
    void createRCBuffers();
    void rebuildRCLevels(int numLevels);
    void renderRadianceCascades();
};

}

#endif //CISALPINE_WORLD_HPP