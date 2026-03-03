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

namespace cisalpine {

// GPU struct
struct GPUElementData {
    glm::vec4 color;            // 16 bytes (offset 0)
    int type;                   // 4 bytes  (offset 16)
    float density;              // 4 bytes  (offset 20)
    float viscosity;            // 4 bytes  (offset 24)
    float probability;          // 4 bytes  (offset 28)
    int flammability;           // 4 bytes  (offset 32)
    int glow;                   // 4 bytes  (offset 36)
    int maxLife;                // 4 bytes  (offset 40)
    int gemstone;               // 4 bytes  (offset 44)
    float lightRadius;          // 4 bytes  (offset 48)
    float lightIntensity;       // 4 bytes  (offset 52)
    float ior;                  // 4 bytes  (offset 56)
    float opacity;              // 4 bytes  (offset 60)
    float specularPower;        // 4 bytes  (offset 64)
    int _pad1, _pad2, _pad3;    // 12 bytes (offset 68) - Padding for vec4 alignment
    glm::vec4 transmissionTint; // 16 bytes (offset 80)
    float baseHeight;           // 4 bytes  (offset 96)
    float colorVariation;       // 4 bytes  (offset 100)
    int animType;               // 4 bytes  (offset 104)
    float mass;                 // 4 bytes  (offset 108)
};
static_assert(sizeof(GPUElementData) == 112, "GPUElementData must be 112 bytes for std430");

class Registry {
public:
    void load(const std::string& filename);

    // Generates #defines for shader
    std::string getShaderHeader() const;

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

    // Hidden flag - elements like WoodTip/LeafTip that shouldn't appear in UI
    bool isHidden(int id) const;

    // Max life for brush initialization (255 for Fire, 0 for most elements)
    int getMaxLife(int id) const;

private:
    std::vector<GPUElementData> gpuData;
    std::vector<std::string> names;
    std::map<std::string, int> nameToId;
    std::vector<bool> singleClickFlags; // CPU-side only, not on GPU
    std::vector<bool> hiddenFlags;      // CPU-side only, not on GPU
    GLuint ssbo = 0;

    int parseType(const std::string& typeStr);
    int parseAnimType(const std::string& animStr);
};

}

#endif //CISALPINE_REGISTRY_HPP