/*
* File: registry.cpp
* Project: Cisalpine Engine
* Author: Collin Longoria
* Created on: 2/4/2026
*
* Copyright (c) 2025 Collin Longoria
*
* This software is released under the MIT License.
* https://opensource.org/licenses/MIT
*/

#include "registry.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>

namespace cisalpine {

void Registry::load(const std::string &filename) {
    std::ifstream f(filename);
    if (!f.is_open()) {
        throw std::runtime_error("Failed to open elements file: " + filename);
    }

    nlohmann::json j = nlohmann::json::parse(f);

    // Resize vector to fit largest ID found + 1
    int maxId = 0;
    for (auto& [key, val] : j.items()) {
        if (val.contains("id") && val["id"].get<int>() > maxId) {
            maxId = val["id"].get<int>();
        }
    }
    gpuData.resize(maxId + 1);
    names.resize(maxId + 1);
    singleClickFlags.resize(maxId + 1, false);

    // Initialize all elements with defaults
    for (auto& d : gpuData) {
        d = GPUElementData{};
        d.color = glm::vec4(1.0f, 0.0f, 1.0f, 1.0f);
        d.type = 0;
        d.density = 0.0f;
        d.viscosity = 0.0f;
        d.probability = 0.0f;
        d.flammability = 0;
        d.glow = 0;
        d.maxLife = 0;
        d.gemstone = 0;
        d.lightRadius = 0.0f;
        d.lightIntensity = 0.0f;
        d.ior = 1.0f;
        d._pad = 0;
    }

    for (auto& [key, val] : j.items()) {
        int id = val["id"].get<int>();
        names[id] = key;
        nameToId[key] = id;

        GPUElementData& d = gpuData[id];

        // Color
        if (val.contains("color")) {
            auto c = val["color"];
            d.color = glm::vec4(
                c[0].get<float>(),
                c[1].get<float>(),
                c[2].get<float>(),
                c[3].get<float>()
            );
        }

        // Properties
        d.type = parseType(val.value("type", "Static"));
        d.density = val.value("density", 10.0f);
        d.viscosity = val.value("viscosity", 0.0f);
        d.flammability = val.value("flammable", false) ? 1 : 0;
        d.probability = val.value("burnChance", 0.0f);
        d.glow = val.value("glow", false) ? 1 : 0;
        d.maxLife = val.value("life", 0);
        d.gemstone = val.value("gemstone", false) ? 1 : 0;
        d.lightRadius = val.value("lightRadius", 0.0f);
        d.lightIntensity = val.value("lightIntensity", 0.0f);
        d.ior = val.value("ior", 1.45f); // default IOR for glass-like
        d._pad = 0;

        // CPU-only properties
        singleClickFlags[id] = val.value("singleClick", false);
    }

    // Upload to GPU
    if (ssbo == 0) glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 gpuData.size() * sizeof(GPUElementData),
                 gpuData.data(),
                 GL_STATIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    std::cout << "Loaded " << gpuData.size() << " elements from registry" << std::endl;
}

int Registry::parseType(const std::string& typeStr) {
    if (typeStr == "Static") return 0;
    if (typeStr == "Granular") return 1;
    if (typeStr == "Liquid") return 2;
    if (typeStr == "Gas") return 3;
    return 0; // Default to Static
}

std::string Registry::getShaderHeader() const {
    std::string header = "// Auto-generated element defines\n";
    for (const auto& [name, id] : nameToId) {
        std::string upperName = name;
        std::transform(upperName.begin(), upperName.end(), upperName.begin(), ::toupper);
        header += "#define " + upperName + " " + std::to_string(id) + "u\n";
    }
    header += "#define MAX_ELEMENTS " + std::to_string(gpuData.size()) + "u\n";
    header += "\n";
    return header;
}

void Registry::bindSSBO(GLuint bindingPoint) const {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPoint, ssbo);
}

int Registry::getId(const std::string& name) const {
    auto it = nameToId.find(name);
    if (it != nameToId.end()) {
        return it->second;
    }
    return -1;
}

glm::vec4 Registry::getColor(int id) const {
    if (id >= 0 && id < static_cast<int>(gpuData.size())) {
        return gpuData[id].color;
    }
    return glm::vec4(1.0f, 0.0f, 1.0f, 1.0f); // Magenta fallback
}

bool Registry::isSingleClick(int id) const {
    if (id >= 0 && id < static_cast<int>(singleClickFlags.size())) {
        return singleClickFlags[id];
    }
    return false;
}

}