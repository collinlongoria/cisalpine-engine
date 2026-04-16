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
*
* MODIFIED: DSL Integration - Removed flammability, probability, maxLife from
*           GPU struct. Added DSL script compilation from elements.json.
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

    nlohmann::ordered_json j = nlohmann::ordered_json::parse(f);

    int elementCount = static_cast<int>(j.size());
    gpuData.resize(elementCount);
    names.resize(elementCount);
    displayNames.resize(elementCount);
    singleClickFlags.resize(elementCount, false);
    hiddenFlags.resize(elementCount, false);
    maxLifeValues.resize(elementCount, 0);

    // Initialize all elements with defaults
    for (auto& d : gpuData) {
        d = GPUElementData{};
        d.color = glm::vec4(1.0f, 0.0f, 1.0f, 1.0f);
        d.type = 0;
        d.density = 0.0f;
        d.viscosity = 0.0f;
        d.glow = 0;
        d.gemstone = 0;
        d.lightRadius = 0.0f;
        d.lightIntensity = 0.0f;
        d.ior = 1.0f;
        d.opacity = 0.85f;
        d.specularPower = 1.0f;
        d.transmissionTint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        d.baseHeight = 1.0f;
        d.colorVariation = 0.1f;
        d.animType = 0;
        d.mass = 1.0f;
        d._pad1 = 0;
        d._pad2 = 0;
    }

    // Auto-assign IDs based on JSON entry order
    int autoId = 0;
    for (auto& [key, val] : j.items()) {
        int id;
        if (val.contains("id")) {
            id = val["id"].get<int>();
        } else {
            id = autoId;
        }
        autoId = id + 1;

        // Grow vectors if needed
        if (id >= static_cast<int>(gpuData.size())) {
            int newSize = id + 1;
            gpuData.resize(newSize);
            names.resize(newSize);
            displayNames.resize(newSize);
            singleClickFlags.resize(newSize, false);
            hiddenFlags.resize(newSize, false);
            maxLifeValues.resize(newSize, 0);
            for (int i = elementCount; i < newSize; i++) {
                gpuData[i] = GPUElementData{};
                gpuData[i].mass = 1.0f;
            }
            elementCount = newSize;
        }

        names[id] = key;
        nameToId[key] = id;

        if (val.contains("displayName")) {
            displayNames[id] = val["displayName"].get<std::string>();
        }
        else {
            displayNames[id] = key;
        }

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
        d.glow = val.value("glow", false) ? 1 : 0;
        d.gemstone = val.value("gemstone", false) ? 1 : 0;
        d.lightRadius = val.value("lightRadius", 0.0f);
        d.lightIntensity = val.value("lightIntensity", 0.0f);
        d.ior = val.value("ior", 1.45f);

        if (val.contains("opacity")) {
            d.opacity = val["opacity"].get<float>();
        } else {
            int type = d.type;
            if (type == 3) d.opacity = 0.05f;
            else if (type == 2) d.opacity = 0.15f;
            else if (d.gemstone == 1) d.opacity = 0.2f;
            else d.opacity = 0.85f;
        }

        // Specular Power
        if (val.contains("specularPower")) {
            d.specularPower = val["specularPower"].get<float>();
        } else {
            if (d.gemstone == 1) d.specularPower = 64.0f;
            else if (d.type == 2) d.specularPower = 16.0f;
            else d.specularPower = 1.0f;
        }

        // Transmission Tint
        if (val.contains("transmissionTint")) {
            auto t = val["transmissionTint"];
            d.transmissionTint = glm::vec4(
                t[0].get<float>(),
                t[1].get<float>(),
                t[2].get<float>(),
                t.size() > 3 ? t[3].get<float>() : 1.0f
            );
        } else {
            d.transmissionTint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        }

        // Base Height
        if (val.contains("baseHeight")) {
            d.baseHeight = val["baseHeight"].get<float>();
        } else {
            int type = d.type;
            if (type == 0) d.baseHeight = 1.0f;
            else if (type == 1) d.baseHeight = 0.8f;
            else if (type == 2) d.baseHeight = 0.3f;
            else if (type == 3) d.baseHeight = 0.1f;
        }

        // Color Variation
        d.colorVariation = val.value("colorVariation", 0.1f);

        // Animation Type
        if (val.contains("animType")) {
            d.animType = parseAnimType(val["animType"].get<std::string>());
        } else {
            d.animType = 0;
        }

        // Mass
        d.mass = val.value("mass", 1.0f);
        d._pad1 = 0;
        d._pad2 = 0;

        // CPU-only properties
        singleClickFlags[id] = val.value("singleClick", false);
        hiddenFlags[id] = val.value("hidden", false);
        maxLifeValues[id] = val.value("life", 0);

        // DSL Script compilation
        if (val.contains("script")) {
            std::string script = val["script"].get<std::string>();
            if (!script.empty()) {
                dslCompiler.compile(key, id, script);
            }
        }
    }

    // Upload to GPU
    if (ssbo == 0) glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 gpuData.size() * sizeof(GPUElementData),
                 gpuData.data(),
                 GL_STATIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    std::cout << "Loaded " << gpuData.size() << " elements from registry ("
              << sizeof(GPUElementData) << " bytes per element)" << std::endl;

    if (dslCompiler.hasScripts()) {
        std::cout << "DSL scripts compiled and ready for GLSL injection" << std::endl;
    }
}

int Registry::parseType(const std::string& typeStr) {
    if (typeStr == "Static") return 0;
    if (typeStr == "Granular") return 1;
    if (typeStr == "Liquid") return 2;
    if (typeStr == "Gas") return 3;
    return 0;
}

int Registry::parseAnimType(const std::string& animStr) {
    if (animStr == "None") return 0;
    if (animStr == "Liquid") return 1;
    if (animStr == "Fire") return 2;
    if (animStr == "Void") return 3;
    if (animStr == "BlackHole") return 4;
    if (animStr == "Rainbow") return 5;
    return 0;
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

std::string Registry::getDSLShaderCode() const {
    return dslCompiler.emitGLSL();
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
    return glm::vec4(1.0f, 0.0f, 1.0f, 1.0f);
}

bool Registry::isSingleClick(int id) const {
    if (id >= 0 && id < static_cast<int>(singleClickFlags.size())) {
        return singleClickFlags[id];
    }
    return false;
}

bool Registry::isHidden(int id) const {
    if (id >= 0 && id < static_cast<int>(hiddenFlags.size())) {
        return hiddenFlags[id];
    }
    return false;
}

int Registry::getMaxLife(int id) const {
    if (id >= 0 && id < static_cast<int>(maxLifeValues.size())) {
        return maxLifeValues[id];
    }
    return 0;
}

}
