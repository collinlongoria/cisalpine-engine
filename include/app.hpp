/*
* File: app
* Project: cisalpine
* Author: colli
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

    // Input state
    bool isDrawing = false;

    void calculateWindowSize(int& windowWidth, int& windowHeight);
    void updateLayout(int windowWidth, int windowHeight);
    void handleInput();
    void renderUI();

    // Convert screen coords to world coords
    bool screenToWorld(double screenX, double screenY, int& worldX, int& worldY);
};

}


#endif //CISALPINE_APP_HPP