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
    glm::vec4 color;        // 16 bytes (offset 0)
    int type;               // 4 bytes  (offset 16) - 0=static, 1=granular, 2=liquid, 3=gas
    float density;          // 4 bytes  (offset 20)
    float viscosity;        // 4 bytes  (offset 24)
    float probability;      // 4 bytes  (offset 28) - burn chance
    int flammability;       // 4 bytes  (offset 32)
    int glow;               // 4 bytes  (offset 36)
    int maxLife;            // 4 bytes  (offset 40)
    int _pad;               // 4 bytes  (offset 44) - pad to 48 bytes (multiple of 16)
};
static_assert(sizeof(GPUElementData) == 48, "GPUElementData must be 48 bytes for std430");

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

private:
    std::vector<GPUElementData> gpuData;
    std::vector<std::string> names;
    std::map<std::string, int> nameToId;
    std::vector<bool> singleClickFlags; // CPU-side only, not on GPU
    GLuint ssbo = 0;

    int parseType(const std::string& typeStr);
};

}

#endif //CISALPINE_REGISTRY_HPP