/*
* File: app.hpp
* Project: Cisalpine Engine
* Author: Collin Longoria
* Created on: 2/4/2026
*
* Copyright (c) 2025 Collin Longoria
*
* This software is released under the MIT License.
* https://opensource.org/licenses/MIT
*/

#ifndef CISALPINE_APP_HPP
#define CISALPINE_APP_HPP

#include <glad/glad.h>
#include "GLFW/glfw3.h"
#include "world.hpp"
#include <memory>

#include "imgui.h"
#include "registry.hpp"

namespace cisalpine {

// Theme
struct Theme {
    // Window / panel backgrounds
    ImVec4 panelBg          = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    ImVec4 childBg          = ImVec4(0.06f, 0.06f, 0.08f, 1.00f);
    ImVec4 popupBg          = ImVec4(0.10f, 0.10f, 0.13f, 1.00f);

    // Borders
    ImVec4 border           = ImVec4(0.20f, 0.20f, 0.25f, 0.60f);

    // Title bar
    ImVec4 titleBg          = ImVec4(0.10f, 0.10f, 0.14f, 1.00f);
    ImVec4 titleBgActive    = ImVec4(0.14f, 0.14f, 0.20f, 1.00f);

    // Accent (tabs, sliders, checkboxes, etc.)
    ImVec4 accent           = ImVec4(0.35f, 0.48f, 0.85f, 1.00f);
    ImVec4 accentHovered    = ImVec4(0.45f, 0.58f, 0.95f, 1.00f);
    ImVec4 accentActive     = ImVec4(0.28f, 0.40f, 0.75f, 1.00f);

    // Tabs
    ImVec4 tab              = ImVec4(0.12f, 0.12f, 0.16f, 1.00f);
    ImVec4 tabHovered       = ImVec4(0.22f, 0.22f, 0.30f, 1.00f);
    ImVec4 tabActive        = ImVec4(0.18f, 0.18f, 0.25f, 1.00f);

    // Text
    ImVec4 text             = ImVec4(0.90f, 0.90f, 0.92f, 1.00f);
    ImVec4 textDim          = ImVec4(0.50f, 0.50f, 0.55f, 1.00f);

    // Input fields (search bar, sliders)
    ImVec4 frameBg          = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);
    ImVec4 frameBgHovered   = ImVec4(0.16f, 0.16f, 0.20f, 1.00f);
    ImVec4 frameBgActive    = ImVec4(0.20f, 0.20f, 0.26f, 1.00f);

    // Scrollbar
    ImVec4 scrollbarBg      = ImVec4(0.05f, 0.05f, 0.07f, 0.60f);
    ImVec4 scrollbarGrab    = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    ImVec4 scrollbarHovered = ImVec4(0.35f, 0.35f, 0.42f, 1.00f);

    // Separator
    ImVec4 separator        = ImVec4(0.20f, 0.20f, 0.25f, 0.50f);

    // Button defaults (element buttons override per-element)
    ImVec4 button           = ImVec4(0.16f, 0.16f, 0.20f, 1.00f);
    ImVec4 buttonHovered    = ImVec4(0.24f, 0.24f, 0.30f, 1.00f);
    ImVec4 buttonActive     = ImVec4(0.12f, 0.12f, 0.16f, 1.00f);

    // Font
    const char* fontPath    = "data/fonts/Brain Wants UD.otf";
    float       fontSize    = 14.0f;
};

//Element tab categories
enum class ElementTab { Solids, Liquids, Gases, Lights, Other, COUNT };

struct UILayout {
    // World viewport
    int viewportX = 0;
    int viewportY = 0;
    int viewportWidth = 0;
    int viewportHeight = 0;

    // UI Panel sizes
    int sidePanelWidth = 400;
    int topPanelHeight = 0;
    int bottomPanelHeight = 0;
};

enum class BrushShape { Circle, Square, Star };

struct Camera {
    float zoom = 1.0f;
    float panX = 0.0f;
    float panY = 0.0f;
    float targetZoom = 1.0f;
    float zoomSpeed = 0.15;
    float panSpeed = 200.0f;

    static constexpr float MIN_ZOOM = 1.0f;
    static constexpr float MAX_ZOOM = 8.0f;

    void clampPan(int worldWidth, int worldHeight) {
        // Panning is only allowed after zooming in, and only to world boundary
        float visibleW = static_cast<float>(worldWidth) / zoom;
        float visibleH = static_cast<float>(worldHeight) / zoom;
        float maxPanX = (static_cast<float>(worldWidth) - visibleW) * 0.5f;
        float maxPanY = (static_cast<float>(worldHeight) - visibleH) * 0.5f;
        if (maxPanX < 0.0f) maxPanX = 0.0f;
        if (maxPanY < 0.0f) maxPanY = 0.0f;
        if (panX < -maxPanX) panX = -maxPanX;
        if (panY < -maxPanY) panY = -maxPanY;
        if (panX > maxPanX) panX = maxPanX;
        if (panY > maxPanY) panY = maxPanY;
    }
};

class App {
public:
    App() = default;
    ~App() = default;

    void init(int worldWidth, int worldHeight);
    void run();
    void shutdown();

private:
    GLFWwindow* window = nullptr;
    std::unique_ptr<World> world;

    int worldWidth = 256;
    int worldHeight = 256;
    int pixelScale = 2;

    UILayout layout;
    Camera camera;

    // Timing
    float lastFrameTime = 0.0f;

    // Input state
    bool isDrawing = false;
    bool lastMousePressed = false; // for single click tracking
    int selectedElementId = 1;
    BrushShape selectedBrush = BrushShape::Circle;
    int brushSize = 3;


    // Mouse drag state
    bool mouseDragging = false;
    int lastMouseWorldX = 0;
    int lastMouseWorldY = 0;

    // UI State
    bool settingsOpen = false;
    bool showChunkDebug = false;

    // Element panel state
    ElementTab currentTab = ElementTab::Solids;
    char searchBuffer[64] = {};
    bool searchActive = false;

    // Debug overlay state
    bool debugOverlayOpen = false;
    bool lastTildePressed = false;
    DebugViewMode debugViewMode = DebugViewMode::Final;
    float chunkMapRefreshTimer = 0.0f;
    static constexpr float CHUNK_MAP_REFRESH_INTERVAL = 0.1f; // refresh every 100ms
    std::vector<uint32_t> cachedChunkGrid;

    // Loading screen state
    bool isLoading = true;
    std::string loadingStage = "Initializing...";
    float loadingProgress = 0.0f;

    // Save/Load status message
    std::string statusMessage;
    float statusMessageTimer = 0.0f;

    // Logic
    Registry registry;
    Shader brushShader;
    Shader physicsBrushShader;
    Theme theme;

    void applyTheme();
    void calculateWindowSize(int& windowWidth, int& windowHeight);
    void updateLayout(int windowWidth, int windowHeight);
    void handleInput(float dt);
    void renderUI();
    void renderSettingsWindow();
    void renderDebugOverlay(float dt);
    void renderLoadingScreen();
    void renderElementButtons(const std::vector<std::string>& names, float availWidth);
    bool elementMatchesTab(int id, ElementTab tab) const;
    bool elementMatchesSearch(const std::string& name) const;

    // Save/Load
    void saveLevel();
    void loadLevel();
    std::string openFileDialog(bool save);  // Platform file dialog

    // Convert screen coords to world coords
    bool screenToWorld(double screenX, double screenY, int& worldX, int& worldY);

    // Scroll Callback
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    void onScroll(double yoffset);
};

}


#endif //CISALPINE_APP_HPP