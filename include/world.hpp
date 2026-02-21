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
    float glowRadius = 3.3f;
    float ambientLight = 0.65f;
    float specularStrength = 0.6f;
    int lightBounces = 3;

    // Sky system
    bool skyEnabled = true;
    float timeOfDay = 0.35f;  // 0.0 = midnight, 0.25 = sunrise, 0.5 = noon, 0.75 = sunset
    bool showSun = true;
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
    COUNT
};

inline const char* debugViewModeName(DebugViewMode mode) {
    switch (mode) {
        case DebugViewMode::Final:      return "Final";
        case DebugViewMode::Color:      return "Color";
        case DebugViewMode::Normals:    return "Normals";
        case DebugViewMode::Lightmap:   return "Lightmap";
        case DebugViewMode::State:      return "State (IDs)";
        case DebugViewMode::Sky:        return "Sky";
        case DebugViewMode::ForceField: return "Force Field";
        default: return "Unknown";
    }
}

// GPU Timer query system
struct GPUTimerQuery {
    static constexpr int HISTORY_SIZE = 60; // rolling average over N frames

    GLuint queryObjects[2] = {0, 0}; // double-buffered
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

    // Call once per frame to collect previous frame's result
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

            // Compute rolling average
            float sum = 0.0f;
            for (int i = 0; i < HISTORY_SIZE; i++) sum += timings[i];
            averageMs = sum / static_cast<float>(HISTORY_SIZE);
            resultReady = true;
        }
        // Swap for next frame
        currentQuery = readQuery;
    }
};

// Named timer indices
enum GPUTimerIndex {
    TIMER_SIMULATION = 0,
    TIMER_FORCE_UPDATE,
    TIMER_CHUNK_BUILD,
    TIMER_RENDER,
    TIMER_LIGHTING,
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
        case TIMER_LIGHTING:     return "Lighting";
        case TIMER_COMPOSITE:    return "Composite";
        case TIMER_QUAD_BLIT:    return "Quad Blit";
        default: return "Unknown";
    }
}

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

    // Get current state texture for brush shader
    GLuint getCurrentTexture() const { return stateTextures[currentBuffer]; }
    GLuint getDisplayTexture() const { return displayTexture; }

    // Get current force field texture for wind brush
    GLuint getCurrentForceTexture() const { return forceTextures[currentForceBuffer]; }

    // Wake a chunk at a given world-space pixel (for brush placement, etc.)
    void wakeChunkAt(int worldX, int worldY);

    // Chunk debug info
    int getChunkGridWidth() const { return chunkGridWidth; }
    int getChunkGridHeight() const { return chunkGridHeight; }
    int getTotalChunks() const { return totalChunks; }
    int getActiveChunkCount() const { return lastActiveChunkCount; }

    // Get the chunk grid SSBO that the simulation writes wake flags into (for brush shader)
    GLuint getChunkWakeSSBO() const { return chunkGridSSBO[1 - currentChunkGrid]; }
    GLuint getChunkWakeBindingPoint() const { return 5; } // binding point for wake writes

    RenderSettings& renderSettings() { return renderSettingsData; }
    const RenderSettings& renderSettings() const { return renderSettingsData; }

    SimulationSettings& simulationSettings() { return simSettings; }
    const SimulationSettings& simulationSettings() const { return simSettings; }

    // GPU timer access for debug overlay
    const GPUTimerQuery& getTimer(int idx) const { return gpuTimers[idx]; }
    float getTotalGPUTimeMs() const;

    // Chunk grid readback for ASCII map
    // Returns a vector of 0/1 for each chunk (row-major, bottom-to-top)
    std::vector<uint32_t> readChunkGrid() const;

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

    // Double-buffered force field textures (RGBA16F)
    // RG = force vector (x, y), BA = reserved
    GLuint forceTextures[2] = {0, 0};
    int currentForceBuffer = 0;

    // === Chunk Sleep System ===
    int chunkGridWidth = 0;   // Number of chunks horizontally
    int chunkGridHeight = 0;  // Number of chunks vertically
    int totalChunks = 0;

    // Chunk Grid SSBO: uint[] where 0=asleep, 1=awake
    // Double-buffered: [0] = current frame read, [1] = next frame write (sim wakes into this)
    GLuint chunkGridSSBO[2] = {0, 0};
    int currentChunkGrid = 0;

    // Active Chunk List + Indirect Dispatch SSBO
    // Layout: [num_groups_x, num_groups_y(=1), num_groups_z(=1), active_chunk_ids...]
    GLuint activeChunkSSBO = 0;

    // For debug/stats readback
    int lastActiveChunkCount = 0;

    // Chunk system shaders
    Shader chunkBuildShader;    // Pass A: builds dispatch list from chunk grid

    // Rendering textures
    GLuint colorTexture = 0; // Raw element colors (RGBA8)
    GLuint normalTexture = 0; // Per-pixel normals (RGBA16F: xy=normal, z=height, w=specular)
    GLuint lightmapTexture = 0; // Accumulated light (RGBA16F: rgb=light color, a=intensity)
    GLuint lightmapPingPong = 0; // Ping-pong for light propagation
    GLuint displayTexture = 0; // Final composited output (RGBA8)
    GLuint skyTexture = 0; // Sky gradient background (RGBA8)

    // Shaders
    Shader simulationShader;
    Shader forceUpdateShader; // Force field propagation and decay
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
    void wakeAllChunks();          // Wake everything (used on clear, init)
    void buildActiveChunkList();   // Pass A: scan chunk grid → build dispatch list
};

}

#endif //CISALPINE_WORLD_HPP