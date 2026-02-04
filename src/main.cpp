/*
* File: main
* Project: cisalpine-engine
* Author: colli
* Created on: 2/4/2026
*
* Copyright (c) 2025 Collin Longoria
*
* This software is released under the MIT License.
* https://opensource.org/licenses/MIT
*/

#include <app.hpp>
#include <iostream>

int main() {
    try {
        cisalpine::App app{};
        app.init();
        app.update();
        app.shutdown();
    }  catch (const std::exception& e) {
        std::cerr << "[FATAL] " << e.what() << '\n';
    }
}
