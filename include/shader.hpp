/*
* File: shader
* Project: cisalpine
* Author: colli
* Created on: 2/4/2026
*
* Copyright (c) 2025 Collin Longoria
*
* This software is released under the MIT License.
* https://opensource.org/licenses/MIT
*/

#ifndef CISALPINE_SHADER_HPP
#define CISALPINE_SHADER_HPP

#include <glad/glad.h>
#include <string>
#include <string_view>

namespace cisalpine {

class Shader {
public:
    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    bool loadFromFile(std::string_view vertexPath, std::string_view fragmentPath);
    bool loadCompute(std::string_view computePath);

    void use() const;
    void dispatch(GLuint x, GLuint y, GLuint z) const;

    GLuint id() const { return programId; }

    // Uniform setters
    void setBool(std::string_view name, bool value) const;
    void setInt(std::string_view name, int value) const;
    void setUint(std::string_view name, uint32_t value) const;
    void setFloat(std::string_view name, float value) const;
    void setVec2(std::string_view name, float x, float y) const;
    void setVec4(std::string_view name, float x, float y, float z, float w) const;

private:
    GLuint programId = 0;

    static std::string readFile(std::string_view filePath);
    static GLuint compileShader(GLenum type, const std::string& source, std::string_view path);
    static bool checkCompileErrors(GLuint shader, std::string_view path);
    static bool checkLinkErrors(GLuint program);
};

}

#endif //CISALPINE_SHADER_HPP