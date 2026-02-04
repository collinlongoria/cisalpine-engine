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

struct ImGuiIO;

namespace cisalpine {

class App {
public:
    App() {};
    ~App() = default;

    void init();
    void update();
    void shutdown();

private:
    GLFWwindow* window{};
};

}


#endif //CISALPINE_APP_HPP