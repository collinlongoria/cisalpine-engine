/*
* File: registry.hpp
* Project: Cisalpine Engine
* Author: Collin Longoria
* Created on: 2/4/2026
*
* Copyright (c) 2025 Collin Longoria
*
* This software is released under the MIT License.
* https://opensource.org/licenses/MIT
*/

#ifndef CISALPINE_REGISTRY_HPP
#define CISALPINE_REGISTRY_HPP

#include <vector>
#include <string>
#include <map>
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include <glad/glad.h>

#include "compiler.hpp"

namespace cisalpine {

struct GPUElementData {
    glm::vec4 color;            // 16 bytes (offset 0)
    int type;                   // 4 bytes  (offset 16)
    float density;              // 4 bytes  (offset 20)
    float viscosity;            // 4 bytes  (offset 24)
    int glow;                   // 4 bytes  (offset 28)
    int gemstone;               // 4 bytes  (offset 32)
    float lightRadius;          // 4 bytes  (offset 36)
    float lightIntensity;       // 4 bytes  (offset 40)
    float ior;                  // 4 bytes  (offset 44)
    float opacity;              // 4 bytes  (offset 48)
    float specularPower;        // 4 bytes  (offset 52)
    int _pad1, _pad2;           // 8 bytes  (offset 56)
    glm::vec4 transmissionTint; // 16 bytes (offset 64)
    float baseHeight;           // 4 bytes  (offset 80)
    float colorVariation;       // 4 bytes  (offset 84)
    int animType;               // 4 bytes  (offset 88)
    float mass;                 // 4 bytes  (offset 92)
};
static_assert(sizeof(GPUElementData) == 96, "GPUElementData must be 96 bytes for std430");

class Registry {
public:
    void load(const std::string& filename);

    // Generates #defines for shader
    std::string getShaderHeader() const;

    // Gets the DSL-generated GLSL code for injection into simulation.comp
    std::string getDSLShaderCode() const;

    // Binds the SSBO to binding point
    void bindSSBO(GLuint bindingPoint) const;

    // Helpers for UI
    const std::vector<std::string>& getNames() const { return names; }
    int getId(const std::string& name) const;
    size_t getElementCount() const { return gpuData.size(); }

    // Color access for UI
    glm::vec4 getColor(int id) const;

    // Single-click flag for UI input handling
    bool isSingleClick(int id) const;

    bool isHidden(int id) const;

    // Max life for brush initialization
    int getMaxLife(int id) const;

    // Get the DSL compiler for custom data initialization
    const DSLCompiler& getDSLCompiler() const { return dslCompiler; }

    // Get the display names
    const std::vector<std::string>& getDisplayNames() const { return displayNames; }

private:
    std::vector<GPUElementData> gpuData;
    std::vector<std::string> names;
    std::map<std::string, int> nameToId;
    std::vector<bool> singleClickFlags;
    std::vector<bool> hiddenFlags;
    std::vector<int> maxLifeValues;
    std::vector<std::string> displayNames;
    GLuint ssbo = 0;

    // DSL Compiler
    DSLCompiler dslCompiler;

    int parseType(const std::string& typeStr);
    int parseAnimType(const std::string& animStr);
};

}

#endif //CISALPINE_REGISTRY_HPP
