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

#include "app.hpp"

#include <iostream>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>


namespace cisalpine {

void App::init(int worldW, int worldH) {
    worldWidth = worldW;
    worldHeight = worldH;

    // init GLFW
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    int windowWidth, windowHeight;
    calculateWindowSize(windowWidth, windowHeight);

    // create window
    window = glfwCreateWindow(windowWidth, windowHeight, "Cisalpine Engine", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        throw std::runtime_error("Failed to initialize GLAD");
    }

    // debug info
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    // Calculate layout
    updateLayout(windowWidth, windowHeight);

    // Create world
    world = std::make_unique<World>(worldWidth, worldHeight);
    if (!world->init()) {
        throw std::runtime_error("Failed to initialize world");
    }

    lastFrameTime = static_cast<float>(glfwGetTime());
}

void App::calculateWindowSize(int& windowWidth, int& windowHeight) {
    // Viewport size = world size * pixel scale
    int viewportWidth = worldWidth * pixelScale;
    int viewportHeight = worldHeight * pixelScale;

    // Add UI panels
    windowWidth = viewportWidth + layout.sidePanelWidth;
    windowHeight = viewportHeight + layout.topPanelHeight + layout.bottomPanelHeight;
}

void App::updateLayout(int windowWidth, int windowHeight) {
    // Viewport takes up space minus the side panel
    layout.viewportX = 0;
    layout.viewportY = layout.bottomPanelHeight;
    layout.viewportWidth = windowWidth - layout.sidePanelWidth;
    layout.viewportHeight = windowHeight - layout.topPanelHeight - layout.bottomPanelHeight;
}

bool App::screenToWorld(double screenX, double screenY, int& worldX, int& worldY) {
    // Check if within viewport bounds
    if (screenX < layout.viewportX ||
        screenX >= layout.viewportX + layout.viewportWidth ||
        screenY < layout.viewportY ||
        screenY >= layout.viewportY + layout.viewportHeight) {
        return false;
        }

    // Convert to viewport-local coordinates
    double localX = screenX - layout.viewportX;
    double localY = screenY - layout.viewportY;

    // Flip Y (screen Y is top-down, world Y is bottom-up)
    localY = layout.viewportHeight - localY;

    // Scale to world coordinates
    worldX = static_cast<int>(localX / pixelScale);
    worldY = static_cast<int>(localY / pixelScale);

    return (worldX >= 0 && worldX < worldWidth && worldY >= 0 && worldY < worldHeight);
}

const char* App::getElementName(Element elem) const {
    switch (elem) {
        case Element::Empty: return "Eraser";
        case Element::Sand:  return "Sand";
        case Element::Stone: return "Stone";
        case Element::Water: return "Water";
        case Element::Lava:  return "Lava";
        case Element::Wood:  return "Wood";
        case Element::Fire:  return "Fire";
        case Element::Smoke: return "Smoke";
        case Element::Dirt:  return "Dirt";
        case Element::Seed:  return "Seed";
        case Element::Grass: return "Grass";
        default:             return "Unknown";
    }
}

void App::handleInput() {
    ImGuiIO& io = ImGui::GetIO();

    // Don't allow drawing if interacting with imgui
    if (io.WantCaptureMouse) {
        isDrawing = false;
        return;
    }

    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    bool leftPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool rightPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

    if (leftPressed || rightPressed) {
        int worldX, worldY;
        if (screenToWorld(mouseX, mouseY, worldX, worldY)) {
            Element drawElement = leftPressed ? selectedElement : Element::Empty;

            for (int dy = -brushSize; dy <= brushSize; dy++) {
                for (int dx = -brushSize; dx <= brushSize; dx++) {
                    bool paint = false;

                    if (selectedBrush == BrushShape::Square) {
                        paint = true;
                    }
                    else if (selectedBrush == BrushShape::Circle) {
                        if (dx*dx + dy*dy <= brushSize*brushSize) paint = true;
                    }
                    else if (selectedBrush == BrushShape::Star) {
                        // Manhattan distance for star shape
                        if (abs(dx) + abs(dy) <= brushSize) paint = true;
                    }

                    if (paint) {
                        // Smoke emitted by mouse needs life set, handled in spawnParticle
                        world->spawnParticle(worldX + dx, worldY + dy, drawElement);
                    }
                }
            }
        }
        isDrawing = true;
    }
    else {
        isDrawing = false;
    }

    // Number keys for element selection
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) selectedElement = Element::Sand;
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) selectedElement = Element::Stone;
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) selectedElement = Element::Water;
    if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) selectedElement = Element::Lava;
    if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) selectedElement = Element::Wood;
    if (glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS) selectedElement = Element::Fire;
    if (glfwGetKey(window, GLFW_KEY_7) == GLFW_PRESS) selectedElement = Element::Smoke;
    if (glfwGetKey(window, GLFW_KEY_8) == GLFW_PRESS) selectedElement = Element::Dirt;
    if (glfwGetKey(window, GLFW_KEY_9) == GLFW_PRESS) selectedElement = Element::Seed;
    if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS) selectedElement = Element::Empty;
}

void App::renderUI() {
    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);

    ImGui::SetNextWindowPos(ImVec2(static_cast<float>(windowWidth - layout.sidePanelWidth), 0));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(layout.sidePanelWidth),
                                     static_cast<float>(windowHeight)));

    ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoTitleBar;

    ImGui::Begin("Tools", nullptr, panelFlags);

    ImGui::Text("Cisalpine Engine");
    ImGui::Separator();

    ImGui::Text("World: %dx%d", worldWidth, worldHeight);
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

    // ELEMENTS
    ImGui::Separator();
    ImGui::Text("Elements");

    const Element elements[] = {
        Element::Sand, Element::Dirt, Element::Stone, Element::Wood,
        Element::Water, Element::Lava, Element::Fire, Element::Smoke,
        Element::Seed, Element::Empty
    };

    for (auto elem : elements) {
        if (ImGui::RadioButton(getElementName(elem), selectedElement == elem)) {
            selectedElement = elem;
        }
        if ((int)elem % 2 != 0) ImGui::SameLine(); // Two columns
    }
    ImGui::NewLine();

    // BRUSH
    ImGui::Separator();
    ImGui::Text("Brush");
    ImGui::SliderInt("Size", &brushSize, 1, 15);

    if (ImGui::RadioButton("Circle", selectedBrush == BrushShape::Circle)) selectedBrush = BrushShape::Circle;
    ImGui::SameLine();
    if (ImGui::RadioButton("Square", selectedBrush == BrushShape::Square)) selectedBrush = BrushShape::Square;
    ImGui::SameLine();
    if (ImGui::RadioButton("Star", selectedBrush == BrushShape::Star)) selectedBrush = BrushShape::Star;

    // SIMULATION
    ImGui::Separator();
    ImGui::Text("Simulation");

    SimulationSettings& simSettings = world->simulationSettings();

    ImGui::SliderFloat("Water Visc", &simSettings.waterViscosity, 0.0f, 0.9f);
    ImGui::SliderFloat("Lava Visc", &simSettings.lavaViscosity, 0.0f, 0.95f);
    ImGui::SliderInt("Steps/Frame", &simSettings.stepsPerFrame, 1, 10);

    // RENDER
    ImGui::Separator();
    ImGui::Text("Rendering");

    RenderSettings& settings = world->renderSettings();

    ImGui::ColorEdit3("Background", &settings.backgroundColor.r);

    ImGui::Checkbox("Lava Glow", &settings.glowEnabled);

    if (settings.glowEnabled) {
        ImGui::SliderFloat("Intensity", &settings.glowIntensity, 0.1f, 2.0f);
        ImGui::SliderFloat("Radius", &settings.glowRadius, 2.0f, 20.0f);
    }

    // ACTIONS
    ImGui::Separator();
    if (ImGui::Button("Clear World", ImVec2(-1, 0))) {
        world->clear();
    }

    // CONTROLS
    ImGui::Separator();
    ImGui::Text("Controls");
    ImGui::BulletText("LMB: Draw");
    ImGui::BulletText("RMB: Erase");
    ImGui::BulletText("1-4: Select element");
    ImGui::BulletText("0: Eraser");

    ImGui::Separator();
    if (isDrawing) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Drawing: %s", getElementName(selectedElement));
    } else {
        ImGui::Text("Selected: %s", getElementName(selectedElement));
    }

    ImGui::End();
}

void App::run() {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Calculate delta time
        float currentTime = static_cast<float>(glfwGetTime());
        float dt = currentTime - lastFrameTime;
        lastFrameTime = currentTime;

        if (dt > 0.1f) dt = 0.1f;

        handleInput();

        // Update simulation
        world->update(dt);

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        renderUI();

        // Render
        ImGui::Render();

        int displayW, displayH;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Render world to viewport area
        world->render(layout.viewportX, layout.viewportY,
                      layout.viewportWidth, layout.viewportHeight);

        // Render ImGui on top
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }
}

void App::shutdown() {
    world.reset();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
}

}