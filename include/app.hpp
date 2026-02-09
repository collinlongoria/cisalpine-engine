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

#include "registry.hpp"

namespace cisalpine {

struct UILayout {
    // World viewport
    int viewportX = 0;
    int viewportY = 0;
    int viewportWidth = 0;
    int viewportHeight = 0;

    // UI Panel sizes
    int sidePanelWidth = 200;
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

    // UI State
    bool settingsOpen = false;
    bool isDayMode = true;
    float dayAmbient = 0.65f;
    float nightAmbient = -0.65f;

    // Logic
    Registry registry;
    Shader brushShader;

    void calculateWindowSize(int& windowWidth, int& windowHeight);
    void updateLayout(int windowWidth, int windowHeight);
    void handleInput(float dt);
    void renderUI();
    void renderSettingsWindow();

    // Convert screen coords to world coords
    bool screenToWorld(double screenX, double screenY, int& worldX, int& worldY);

    // Scroll Callback
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    void onScroll(double yoffset);
};

}


#endif //CISALPINE_APP_HPP