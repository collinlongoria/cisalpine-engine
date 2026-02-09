/*
* File: app
* Project: Cisalpine Engine
* Author: Collin Longoria
* Created on: 2/4/2026
*
* Copyright (c) 2025 Collin Longoria
*
* This software is released under the MIT License.
* https://opensource.org/licenses/MIT
*/

#include "app.hpp"

#include <iostream>
#include <cmath>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>


namespace cisalpine {

void App::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    if (app) app->onScroll(yoffset);
}

void App::onScroll(double yoffset) {
    // Don't zoom if ImGui wants the mouse
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    float zoomFactor = 1.0f + camera.zoomSpeed;
    if (yoffset > 0) {
        camera.targetZoom *= zoomFactor;
    } else if (yoffset < 0) {
        camera.targetZoom /= zoomFactor;
    }
    if (camera.targetZoom < Camera::MIN_ZOOM) camera.targetZoom = Camera::MIN_ZOOM;
    if (camera.targetZoom > Camera::MAX_ZOOM) camera.targetZoom = Camera::MAX_ZOOM;
}

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

    // Set up scroll callback
    glfwSetWindowUserPointer(window, this);
    glfwSetScrollCallback(window, scrollCallback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        throw std::runtime_error("Failed to initialize GLAD");
    }

    // debug info
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

    // load registry
    registry.load("data/elements.json");

    // init imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    // Calculate layout
    updateLayout(windowWidth, windowHeight);

    // Get shader header from registry
    std::string header = registry.getShaderHeader();

    // Create world
    world = std::make_unique<World>(worldWidth, worldHeight);
    if (!world->init(header)) {
        throw std::runtime_error("Failed to initialize world");
    }

    // Bind registry SSBO
    registry.bindSSBO(2);

    // Load brush shader
    if (!brushShader.loadCompute("shaders/brush.comp", header)) {
        throw std::runtime_error("Failed to load brush shader");
    }

    // Set initial ambient from day mode
    world->renderSettings().ambientLight = dayAmbient;

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

    // Convert to viewport-local coordinates (0..1)
    double localX = screenX - layout.viewportX;
    double localY = screenY - layout.viewportY;

    // Flip Y (screen Y is top-down, world Y is bottom-up)
    localY = layout.viewportHeight - localY;

    // Normalize to 0..1 within viewport
    double normX = localX / layout.viewportWidth;
    double normY = localY / layout.viewportHeight;

    // Camera transform: account for zoom and pan
    // The visible region in world space
    float visibleW = static_cast<float>(worldWidth) / camera.zoom;
    float visibleH = static_cast<float>(worldHeight) / camera.zoom;

    // Center of view in world coords
    float centerX = static_cast<float>(worldWidth) * 0.5f + camera.panX;
    float centerY = static_cast<float>(worldHeight) * 0.5f + camera.panY;

    // Map normalized viewport coords to world coords
    worldX = static_cast<int>(centerX - visibleW * 0.5f + normX * visibleW);
    worldY = static_cast<int>(centerY - visibleH * 0.5f + normY * visibleH);

    return (worldX >= 0 && worldX < worldWidth && worldY >= 0 && worldY < worldHeight);
}

void App::handleInput(float dt) {
    ImGuiIO& io = ImGui::GetIO();

    // Allow panning even if ImGui has keyboard focus for text input,
    // but not if settings window is open
    if (!settingsOpen) {
        float panAmount = camera.panSpeed * dt / camera.zoom;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
            camera.panY += panAmount;
            }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
            camera.panY -= panAmount;
            }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
            camera.panX -= panAmount;
            }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
            camera.panX += panAmount;
            }
    }

    // Smooth zoom interpolation
    float zoomDiff = camera.targetZoom - camera.zoom;
    camera.zoom += zoomDiff * 0.15f; // Smooth approach
    if (std::abs(zoomDiff) < 0.001f) camera.zoom = camera.targetZoom;

    // Clamp pan after zoom change
    camera.clampPan(worldWidth, worldHeight);

    // Don't allow drawing if interacting with imgui or settings is open
    if (io.WantCaptureMouse || settingsOpen) {
        isDrawing = false;
        return;
    }

    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    bool leftPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool rightPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

    bool shouldDraw = false;

    // Data-driven single-click check from registry
    bool isSingleClickItem = registry.isSingleClick(selectedElementId);

    if (leftPressed) {
        if (isSingleClickItem) {
            if (!lastMousePressed) shouldDraw = true; // Only on first frame of press
        } else {
            shouldDraw = true; // Continuous
        }
    }
    if (rightPressed) shouldDraw = true; // Eraser is always continuous

    lastMousePressed = leftPressed; // Update state

    if (shouldDraw) {
        int worldX, worldY;
        if (screenToWorld(mouseX, mouseY, worldX, worldY)) {
            // Determine if this is an erase action:
            // Right-click always erases, OR left-click with Empty selected
            bool erasing = rightPressed || (selectedElementId == 0);

            // For single-click items, force brush to size 1, circle
            int effectiveBrushSize = isSingleClickItem ? 0 : brushSize;
            int effectiveBrushShape = isSingleClickItem ? 0 : static_cast<int>(selectedBrush);

            // Dispatch Brush Shader
            brushShader.use();
            brushShader.setInt("brushX", worldX);
            brushShader.setInt("brushY", worldY);
            brushShader.setInt("brushSize", effectiveBrushSize);
            brushShader.setInt("brushShape", effectiveBrushShape);
            brushShader.setUint("drawElement", static_cast<uint32_t>(selectedElementId));
            brushShader.setBool("isEraser", erasing);

            // Bind current state texture for read/write
            glBindImageTexture(0, world->getCurrentTexture(), 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8UI);

            // Dispatch enough groups to cover brush size
            int groups = (effectiveBrushSize * 2 + 16) / 16;
            brushShader.dispatch(groups, groups, 1);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

            isDrawing = true;
        }
    } else {
        isDrawing = false;
    }
}

void App::renderSettingsWindow() {
    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);

    // Full-screen overlay
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(windowWidth), static_cast<float>(windowHeight)));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove |
                              ImGuiWindowFlags_NoResize |
                              ImGuiWindowFlags_NoCollapse |
                              ImGuiWindowFlags_NoTitleBar;

    // Semi-transparent background
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.10f, 0.95f));
    ImGui::Begin("##SettingsOverlay", nullptr, flags);
    ImGui::PopStyleColor();

    // Center the settings content
    float contentWidth = 400.0f;
    float startX = (static_cast<float>(windowWidth) - contentWidth) * 0.5f;
    float startY = 40.0f;

    ImGui::SetCursorPos(ImVec2(startX, startY));
    ImGui::BeginChild("SettingsContent", ImVec2(contentWidth, static_cast<float>(windowHeight) - 80.0f));

    ImGui::Text("Settings");
    ImGui::Separator();
    ImGui::Spacing();

    // ─── Simulation ───
    ImGui::Text("Simulation");
    ImGui::Spacing();

    SimulationSettings& simSettings = world->simulationSettings();
    ImGui::SliderInt("Sim Speed", &simSettings.stepsPerFrame, 1, 10);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ─── Rendering ───
    ImGui::Text("Rendering");
    ImGui::Spacing();

    RenderSettings& settings = world->renderSettings();

    ImGui::ColorEdit3("Background", &settings.backgroundColor.r);

    ImGui::Spacing();

    ImGui::SliderFloat("Ambient Light", &settings.ambientLight, 0.0f, 1.0f);

    // Update day/night presets if ambient was changed via slider
    // (so the toggle stays consistent)
    if (isDayMode && std::abs(settings.ambientLight - dayAmbient) > 0.01f) {
        dayAmbient = settings.ambientLight;
    } else if (!isDayMode && std::abs(settings.ambientLight - nightAmbient) > 0.01f) {
        nightAmbient = settings.ambientLight;
    }

    ImGui::Spacing();

    ImGui::Checkbox("Glow", &settings.glowEnabled);

    if (settings.glowEnabled) {
        ImGui::SliderFloat("Glow Radius", &settings.glowRadius, 2.0f, 20.0f);
        ImGui::SliderFloat("Glow Power", &settings.glowIntensity, 0.1f, 2.0f);
    }

    ImGui::SliderFloat("Specular", &settings.specularStrength, 0.0f, 2.0f);
    ImGui::SliderInt("Light Bounces", &settings.lightBounces, 0, 6);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ─── Camera ───
    ImGui::Text("Camera");
    ImGui::Spacing();
    ImGui::Text("Zoom: %.1fx", camera.zoom);
    if (ImGui::Button("Reset Camera")) {
        camera.zoom = 1.0f;
        camera.targetZoom = 1.0f;
        camera.panX = 0.0f;
        camera.panY = 0.0f;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ─── Close Button ───
    float buttonWidth = 120.0f;
    ImGui::SetCursorPosX((contentWidth - buttonWidth) * 0.5f);
    if (ImGui::Button("Close", ImVec2(buttonWidth, 32.0f))) {
        settingsOpen = false;
    }

    // Also close with Escape
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        settingsOpen = false;
    }

    ImGui::EndChild();
    ImGui::End();
}

void App::renderUI() {
    // If settings window is open, render it instead of sidebar
    if (settingsOpen) {
        renderSettingsWindow();
        return;
    }

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
    ImGui::Text("Zoom: %.1fx", camera.zoom);

    // ELEMENTS - Data-driven from registry
    ImGui::Separator();
    ImGui::Text("Elements");

    const auto& names = registry.getNames();
    float availWidth = ImGui::GetContentRegionAvail().x;

    for (size_t i = 0; i < names.size(); i++) {
        if (names[i].empty()) continue;

        int id = static_cast<int>(i);
        bool isSelected = (selectedElementId == id);

        // Get element color from registry
        glm::vec4 elemColor = registry.getColor(id);

        // Display name: "Eraser" for Empty, otherwise use registry name
        const char* displayName = (id == 0) ? "Eraser" : names[i].c_str();

        // Colored capsule button
        // Background: element color (dimmed if not selected, bright if selected)
        float brightness = isSelected ? 1.0f : 0.5f;
        ImVec4 bgColor(elemColor.r * brightness, elemColor.g * brightness,
                       elemColor.b * brightness, 1.0f);

        // Text color: pick white or black based on luminance for readability
        float luminance = 0.299f * elemColor.r + 0.587f * elemColor.g + 0.114f * elemColor.b;
        ImVec4 textColor = (luminance * brightness > 0.45f)
            ? ImVec4(0.0f, 0.0f, 0.0f, 1.0f)
            : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

        // Hover/active colors
        ImVec4 hoverColor(
            fmin(elemColor.r * 0.8f + 0.2f, 1.0f),
            fmin(elemColor.g * 0.8f + 0.2f, 1.0f),
            fmin(elemColor.b * 0.8f + 0.2f, 1.0f),
            1.0f);
        ImVec4 activeColor(elemColor.r, elemColor.g, elemColor.b, 1.0f);

        // Selection border
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
        }

        ImGui::PushStyleColor(ImGuiCol_Button, bgColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);
        ImGui::PushStyleColor(ImGuiCol_Text, textColor);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f); // Capsule shape

        // Two-column layout: each button takes half the available width minus spacing
        float buttonWidth = (availWidth - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        if (ImGui::Button(displayName, ImVec2(buttonWidth, 24.0f))) {
            selectedElementId = id;
        }

        ImGui::PopStyleVar(1);  // FrameRounding
        ImGui::PopStyleColor(4); // Button, Hovered, Active, Text

        if (isSelected) {
            ImGui::PopStyleVar(1);  // FrameBorderSize
            ImGui::PopStyleColor(1); // Border
        }

        // Two-column: put next button on same line if this was an even-indexed visible item
        // Use a simple approach: odd IDs go on same line
        if ((i % 2) == 0 && i + 1 < names.size()) {
            ImGui::SameLine();
        }
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

    // DAY / NIGHT TOGGLE
    ImGui::Separator();
    ImGui::Text("Lighting");
    {
        RenderSettings& settings = world->renderSettings();

        // Styled toggle buttons
        float toggleWidth = (availWidth - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

        // Day button
        if (isDayMode) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.75f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
        }
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.8f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.85f, 0.75f, 0.3f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

        if (ImGui::Button("Day", ImVec2(toggleWidth, 26.0f))) {
            isDayMode = true;
            settings.ambientLight = dayAmbient;
        }

        ImGui::PopStyleVar(1);
        ImGui::PopStyleColor(4);

        ImGui::SameLine();

        // Night button
        if (!isDayMode) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.35f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 1.0f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
        }
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.45f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.15f, 0.35f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

        if (ImGui::Button("Night", ImVec2(toggleWidth, 26.0f))) {
            isDayMode = false;
            settings.ambientLight = nightAmbient;
        }

        ImGui::PopStyleVar(1);
        ImGui::PopStyleColor(4);
    }

    // ACTIONS
    ImGui::Separator();
    if (ImGui::Button("Clear World", ImVec2(-1, 0))) {
        world->clear();
    }

    if (ImGui::Button("Settings", ImVec2(-1, 0))) {
        settingsOpen = true;
    }

    // CONTROLS
    ImGui::Separator();
    ImGui::Text("Controls");
    ImGui::BulletText("LMB: Draw");
    ImGui::BulletText("RMB: Erase");
    ImGui::BulletText("WASD: Pan");
    ImGui::BulletText("Scroll: Zoom");

    ImGui::Separator();
    const char* selectedName = (selectedElementId == 0) ? "Eraser"
        : (selectedElementId < static_cast<int>(names.size()) ? names[selectedElementId].c_str() : "Unknown");
    if (isDrawing) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Drawing: %s", selectedName);
    } else {
        ImGui::Text("Selected: %s", selectedName);
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

        handleInput(dt);

        // Update simulation
        if (!settingsOpen) {
            world->update(dt);
        }

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


        // Render world to viewport area with camera
        float camCenterX = static_cast<float>(worldWidth) * 0.5f + camera.panX;
        float camCenterY = static_cast<float>(worldHeight) * 0.5f + camera.panY;
        world->render(layout.viewportX, layout.viewportY,
                      layout.viewportWidth, layout.viewportHeight,
                      camCenterX, camCenterY, camera.zoom);

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