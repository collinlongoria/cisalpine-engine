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

    // Load wind brush shader
    if (!physicsBrushShader.loadCompute("shaders/physics_brush.comp", header)) {
        throw std::runtime_error("Failed to load physics brush shader");
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

    // Tilde key toggle for debug overlay
    bool tildePressed = glfwGetKey(window, GLFW_KEY_GRAVE_ACCENT) == GLFW_PRESS;
    if (tildePressed && !lastTildePressed) {
        debugOverlayOpen = !debugOverlayOpen;
    }
    lastTildePressed = tildePressed;

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

    // Check if Wind element is selected
    int windId = registry.getId("Wind");
    bool isWindSelected = (selectedElementId == windId && windId >= 0);

    // WIND TOOL
    if (isWindSelected && leftPressed) {
        int worldX, worldY;
        if (screenToWorld(mouseX, mouseY, worldX, worldY)) {
            if (mouseDragging) {
                // Calculate drag delta in world space
                float dx = static_cast<float>(worldX - lastMouseWorldX);
                float dy = static_cast<float>(worldY - lastMouseWorldY);
                float dragLen = std::sqrt(dx * dx + dy * dy);

                if (dragLen > 0.5f) {
                    // Normalize and scale by drag speed
                    float forceX = dx / dragLen;
                    float forceY = dy / dragLen;
                    float strength = std::min(dragLen * 2.0f, 12.0f);

                    // Dispatch wind brush shader
                    physicsBrushShader.use();
                    physicsBrushShader.setInt("brushX", worldX);
                    physicsBrushShader.setInt("brushY", worldY);
                    physicsBrushShader.setInt("brushSize", brushSize);
                    physicsBrushShader.setFloat("forceX", forceX);
                    physicsBrushShader.setFloat("forceY", forceY);
                    physicsBrushShader.setFloat("strength", strength);

                    // Bind force field for read/write
                    glBindImageTexture(0, world->getCurrentForceTexture(), 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);
                    // Bind state texture so physics brush can read element types
                    glBindImageTexture(1, world->getCurrentTexture(), 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8UI);

                    int groups = (brushSize * 2 + 16) / 16;
                    physicsBrushShader.dispatch(groups, groups, 1);
                    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

                    // Wake chunks in the brush area
                    world->wakeChunkAt(worldX, worldY);
                    world->wakeChunkAt(worldX - brushSize, worldY - brushSize);
                    world->wakeChunkAt(worldX + brushSize, worldY + brushSize);
                }
            }

            lastMouseWorldX = worldX;
            lastMouseWorldY = worldY;
            mouseDragging = true;
            isDrawing = true;
        }

        lastMousePressed = leftPressed;
        return; // Skip normal brush logic when using wind
    }
    else {
        mouseDragging = false;
    }

    // NORMAL BRUSH LOGIC
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

            // Wake chunks in the brush area so simulation picks up new elements
            world->wakeChunkAt(worldX, worldY);
            world->wakeChunkAt(worldX - effectiveBrushSize, worldY - effectiveBrushSize);
            world->wakeChunkAt(worldX + effectiveBrushSize, worldY + effectiveBrushSize);

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

    // Simulation
    ImGui::Text("Simulation");
    ImGui::Spacing();

    SimulationSettings& simSettings = world->simulationSettings();
    ImGui::SliderInt("Sim Speed", &simSettings.stepsPerFrame, 1, 10);
    ImGui::Checkbox("Chunk Sleep", &simSettings.chunkSleepEnabled);
    if (simSettings.chunkSleepEnabled) {
        ImGui::Text("Chunks: %d/%d active", world->getActiveChunkCount(), world->getTotalChunks());
        ImGui::Text("Grid: %dx%d", world->getChunkGridWidth(), world->getChunkGridHeight());
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Rendering
    ImGui::Text("Rendering");
    ImGui::Spacing();

    RenderSettings& settings = world->renderSettings();

    ImGui::ColorEdit3("Background", &settings.backgroundColor.r);

    ImGui::Spacing();

    ImGui::Checkbox("Sky Gradient", &settings.skyEnabled);
    if (settings.skyEnabled) {
        ImGui::SliderFloat("Time of Day", &settings.timeOfDay, 0.0f, 1.0f);
        ImGui::Checkbox("Show Sun", &settings.showSun);

        // Show derived ambient (read-only info)
        ImGui::Text("Ambient: %.2f (auto)", settings.ambientLight);
    } else {
        ImGui::SliderFloat("Ambient Light", &settings.ambientLight, 0.0f, 1.0f);
    }

    // Update day/night presets based on sky time
    if (settings.skyEnabled) {
        float sunAngle = settings.timeOfDay * 3.14159f;
        float dayFactor = sin(sunAngle);
        dayFactor = fmax(dayFactor, 0.0f);
        settings.ambientLight = 0.05f + dayFactor * 0.7f;
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

    // Camera
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

    // Close Button
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

void App::renderDebugOverlay(float dt) {
    if (!debugOverlayOpen) return;

    ImGui::SetNextWindowPos(ImVec2(8, 8), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340, 520), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.08f, 0.92f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.12f, 0.12f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.18f, 0.18f, 0.28f, 1.0f));

    if (ImGui::Begin("Debug [~]", &debugOverlayOpen, flags)) {

        // GPU Info
        if (ImGui::CollapsingHeader("GPU Info", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Renderer: %s", glGetString(GL_RENDERER));
            ImGui::Text("Vendor:   %s", glGetString(GL_VENDOR));
            ImGui::Text("OpenGL:   %s", glGetString(GL_VERSION));
            ImGui::Text("GLSL:     %s", glGetString(GL_SHADING_LANGUAGE_VERSION));

            // Query some GPU limits
            GLint maxWorkGroupCount[3], maxWorkGroupSize[3], maxComputeInvocations;
            glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &maxWorkGroupCount[0]);
            glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &maxWorkGroupCount[1]);
            glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &maxWorkGroupSize[0]);
            glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &maxWorkGroupSize[1]);
            glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &maxComputeInvocations);

            GLint maxTextureSize, maxSSBOSize;
            glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
            glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &maxSSBOSize);

            ImGui::Text("Max Tex:  %d", maxTextureSize);
            ImGui::Text("Max SSBO: %d MB", maxSSBOSize / (1024 * 1024));
            ImGui::Text("Max WG:   %dx%d (%d inv)",
                maxWorkGroupSize[0], maxWorkGroupSize[1], maxComputeInvocations);
        }

        // Pass Timings
        if (ImGui::CollapsingHeader("GPU Pass Timings", ImGuiTreeNodeFlags_DefaultOpen)) {
            float totalMs = world->getTotalGPUTimeMs();
            ImGui::Text("Total GPU: %.2f ms", totalMs);
            ImGui::Separator();

            // Timing bars
            float maxBarMs = 2.0f; // Scale: 2ms = full bar width
            float availWidth = ImGui::GetContentRegionAvail().x;

            for (int i = 0; i < TIMER_COUNT; i++) {
                const GPUTimerQuery& timer = world->getTimer(i);
                float ms = timer.averageMs;

                // Color based on time: green < 0.5ms, yellow < 1ms, red > 1ms
                ImVec4 barColor;
                if (ms < 0.3f)       barColor = ImVec4(0.2f, 0.8f, 0.3f, 0.8f);
                else if (ms < 0.7f)  barColor = ImVec4(0.8f, 0.8f, 0.2f, 0.8f);
                else                 barColor = ImVec4(0.9f, 0.3f, 0.2f, 0.8f);

                float fraction = ms / maxBarMs;
                if (fraction > 1.0f) fraction = 1.0f;

                // Draw label + bar
                ImGui::Text("%-14s", gpuTimerName(i));
                ImGui::SameLine(120);
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);

                char overlay[32];
                snprintf(overlay, sizeof(overlay), "%.3f ms", ms);
                ImGui::ProgressBar(fraction, ImVec2(availWidth - 130, 14), overlay);
                ImGui::PopStyleColor();
            }
        }

        // View Mode
        if (ImGui::CollapsingHeader("View Mode", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Current: %s", debugViewModeName(debugViewMode));
            ImGui::Spacing();

            float btnW = 95.0f;
            for (int i = 0; i < static_cast<int>(DebugViewMode::COUNT); i++) {
                DebugViewMode mode = static_cast<DebugViewMode>(i);
                bool isActive = (debugViewMode == mode);

                if (isActive) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.6f, 0.9f, 1.0f));
                }

                if (ImGui::Button(debugViewModeName(mode), ImVec2(btnW, 0))) {
                    debugViewMode = mode;
                }

                if (isActive) {
                    ImGui::PopStyleColor(2);
                }

                // 3 buttons per row
                if ((i % 3) != 2 && i + 1 < static_cast<int>(DebugViewMode::COUNT)) {
                    ImGui::SameLine();
                }
            }
        }

        // Chunk Map
        if (ImGui::CollapsingHeader("Chunk Activity Map")) {
            int gridW = world->getChunkGridWidth();
            int gridH = world->getChunkGridHeight();

            ImGui::Text("Grid: %dx%d  Active: %d/%d",
                gridW, gridH, world->getActiveChunkCount(), world->getTotalChunks());

            // Rate-limit the GPU readback
            chunkMapRefreshTimer -= dt;
            if (chunkMapRefreshTimer <= 0.0f) {
                cachedChunkGrid = world->readChunkGrid();
                chunkMapRefreshTimer = CHUNK_MAP_REFRESH_INTERVAL;
            }

            if (!cachedChunkGrid.empty()) {
                // Draw ASCII-style chunk map using ImGui drawing
                // Each chunk = a small colored square
                float cellSize = 6.0f;

                // Limit display size - if grid is very large, shrink cells
                float maxDisplayWidth = ImGui::GetContentRegionAvail().x - 4.0f;
                if (gridW * cellSize > maxDisplayWidth) {
                    cellSize = maxDisplayWidth / static_cast<float>(gridW);
                    if (cellSize < 2.0f) cellSize = 2.0f;
                }

                ImVec2 origin = ImGui::GetCursorScreenPos();
                ImDrawList* drawList = ImGui::GetWindowDrawList();

                for (int y = 0; y < gridH; y++) {
                    for (int x = 0; x < gridW; x++) {
                        // Flip Y so bottom of world is at bottom of display
                        int flippedY = gridH - 1 - y;
                        int idx = flippedY * gridW + x;

                        bool awake = (idx < static_cast<int>(cachedChunkGrid.size())) && (cachedChunkGrid[idx] != 0);

                        ImU32 color = awake
                            ? IM_COL32(60, 200, 80, 220)   // Green = active
                            : IM_COL32(30, 30, 40, 180);   // Dark = sleeping

                        float px = origin.x + x * cellSize;
                        float py = origin.y + y * cellSize;

                        drawList->AddRectFilled(
                            ImVec2(px, py),
                            ImVec2(px + cellSize - 1.0f, py + cellSize - 1.0f),
                            color
                        );
                    }
                }

                // Reserve space for the drawn grid
                ImGui::Dummy(ImVec2(gridW * cellSize, gridH * cellSize));
            }
        }

        // Frame Stats
        if (ImGui::CollapsingHeader("Frame Stats")) {
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            ImGui::Text("Frame time: %.2f ms", 1000.0f / ImGui::GetIO().Framerate);
            ImGui::Text("World: %dx%d", worldWidth, worldHeight);
            ImGui::Text("Sim steps/frame: %d", world->simulationSettings().stepsPerFrame);
            ImGui::Text("Chunk sleep: %s", world->simulationSettings().chunkSleepEnabled ? "ON" : "OFF");
        }
    }
    ImGui::End();

    ImGui::PopStyleColor(3);
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
    if (world->simulationSettings().chunkSleepEnabled) {
        ImGui::Text("Chunks: %d/%d", world->getActiveChunkCount(), world->getTotalChunks());
    }

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
        // Special display for Wind
        const char* displayName;
        if (id == 0) {
            displayName = "Eraser";
        }
        else if (names[i] == "Wind") {
            displayName = "Wind";
        }
        else {
            displayName = names[i].c_str();
        }

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

    // Show hint for wind
    int windId = registry.getId("Wind");
    if (selectedElementId == windId && windId >= 0) {
        ImGui::TextColored(ImVec4(0.7f, 0.85f, 1.0f, 1.0f), "Drag to push!");
    }

    ImGui::SliderInt("Size", &brushSize, 1, 15);

    if (ImGui::RadioButton("Circle", selectedBrush == BrushShape::Circle)) selectedBrush = BrushShape::Circle;
    ImGui::SameLine();
    if (ImGui::RadioButton("Square", selectedBrush == BrushShape::Square)) selectedBrush = BrushShape::Square;
    ImGui::SameLine();
    if (ImGui::RadioButton("Star", selectedBrush == BrushShape::Star)) selectedBrush = BrushShape::Star;

    // SKY / LIGHTING
    ImGui::Separator();
    ImGui::Text("Lighting");
    {
        RenderSettings& settings = world->renderSettings();

        // Sky toggle
        ImGui::Checkbox("Sky", &settings.skyEnabled);

        if (settings.skyEnabled) {
            // Time of day slider
            // Show label based on time
            const char* timeLabel;
            float t = settings.timeOfDay;
            if (t < 0.15f || t > 0.88f) timeLabel = "Night";
            else if (t < 0.3f) timeLabel = "Dawn";
            else if (t < 0.42f) timeLabel = "Morning";
            else if (t < 0.58f) timeLabel = "Noon";
            else if (t < 0.7f) timeLabel = "Afternoon";
            else if (t < 0.8f) timeLabel = "Dusk";
            else timeLabel = "Twilight";

            ImGui::SliderFloat("Time", &settings.timeOfDay, 0.0f, 1.0f, timeLabel);

            // Derive ambient from time of day automatically
            float sunAngle = settings.timeOfDay * 3.14159f; // 0 to PI over 0..1
            float dayFactor = sin(sunAngle);
            dayFactor = fmax(dayFactor, 0.0f);
            settings.ambientLight = 0.05f + dayFactor * 0.7f;

            ImGui::Checkbox("Show Sun", &settings.showSun);
        } else {
            ImGui::SliderFloat("Ambient", &settings.ambientLight, 0.0f, 1.0f);
        }
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
    ImGui::BulletText("~: Debug Panel");

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
        renderDebugOverlay(dt);

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
                      camCenterX, camCenterY, camera.zoom, debugViewMode);

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