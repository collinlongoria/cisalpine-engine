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

    // Bind registry SSBO (binding point 2 matches shader layout)
    registry.bindSSBO(2);

    // Load brush shader with same header for element defines
    if (!brushShader.loadCompute("shaders/brush.comp", header)) {
        throw std::runtime_error("Failed to load brush shader");
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

    // SIMULATION
    ImGui::Separator();
    ImGui::Text("Simulation");

    SimulationSettings& simSettings = world->simulationSettings();

    ImGui::SliderInt("Sim Speed", &simSettings.stepsPerFrame, 1, 10);

    // RENDER
    ImGui::Separator();
    ImGui::Text("Rendering");

    RenderSettings& settings = world->renderSettings();

    ImGui::ColorEdit3("Background", &settings.backgroundColor.r);

    ImGui::Checkbox("Glow", &settings.glowEnabled);

    if (settings.glowEnabled) {
        ImGui::SliderFloat("Glow Radius", &settings.glowRadius, 2.0f, 20.0f);
        ImGui::SliderFloat("Glow Power", &settings.glowIntensity, 0.1f, 2.0f);
    }

    ImGui::SliderFloat("Ambient", &settings.ambientLight, 0.0f, 1.0f);
    ImGui::SliderFloat("Specular", &settings.specularStrength, 0.0f, 2.0f);
    ImGui::SliderInt("Bounces", &settings.lightBounces, 0, 6);

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