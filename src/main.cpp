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
    cisalpine::App app;

    try {
        app.init(256,256);
        app.run();
        app.shutdown();
    }  catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
