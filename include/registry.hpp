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
// NOTE std430 rules: vec4 = 16-byte aligned, float/int = 4-byte aligned
struct GPUElementData {
    // Base and Old School Elements
    glm::vec4 color;        // 16 bytes (offset 0)
    int type;               // 4 bytes  (offset 16) - 0=static, 1=granular, 2=liquid, 3=gas
    float density;          // 4 bytes  (offset 20)
    float viscosity;        // 4 bytes  (offset 24)
    float probability;      // 4 bytes  (offset 28) - burn chance
    int flammability;       // 4 bytes  (offset 32)
    int glow;               // 4 bytes  (offset 36)
    int maxLife;            // 4 bytes  (offset 40)
    int gemstone;           // 4 bytes  (offset 44) - gemstone flag for specular normals
    float lightRadius;      // 4 bytes  (offset 48) - radius of light emission
    float lightIntensity;   // 4 bytes  (offset 52) - intensity of light emission
    float ior;              // 4 bytes  (offset 56) - index of refraction (gemstones)

    // Thermal Properties
    float baseTemperature;  // 4 bytes  (offset 60)
    float conductivity;     // 4 bytes  (offset 64)
    float specificHeat;     // 4 bytes  (offset 68)

    // Visual Properties
    float opacity;              // 4 bytes  (offset 72)
    float specularPower;        // 4 bytes  (offset 76)
    glm::vec4 transmissionTint; // 16 bytes (offset 80)
    float baseHeight;           // 4 bytes  (offset 96)
    float colorVariation;       // 4 bytes  (offset 100)
    int animType;               // 4 bytes  (offset 104) - 0=None, 1=Liquid, 2=Fire, 3=Void, 4=BlackHole
    int _pad;                   // 4 bytes  (offset 108)
    float highTempTransition;   // 4 bytes  (offset 112)
    int highTempElement;        // 4 bytes  (offset 116)
    float lowTempTransition;    // 4 bytes  (offset 120)
    int lowTempElement;         // 4 bytes  (offset 124)
    int tempModifierType;       // 4 bytes  (offset 128) - 0=Standard, 1=Exothermic, 2=Endothermic
    float tempModifierRate;     // 4 bytes  (offset 132)
    float mass;                 // 4 bytes  (offset 136)
    int _pad3;                  // 4 bytes  (offset 140)
    int _pad4;                  // 4 bytes  (offset 144)
    int _pad5;                  // 4 bytes  (offset 148)
    int _pad6;                  // 4 bytes  (offset 152)
    int _pad7;                  // 4 bytes  (offset 156)
};
static_assert(sizeof(GPUElementData) == 160, "GPUElementData must be 160 bytes for std430");

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

    // Base temperature for brush temperature stamping
    float getBaseTemperature(int id) const;

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