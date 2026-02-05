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

#include "shader.hpp"

#include <fstream>
#include <sstream>
#include <iostream>

namespace cisalpine {

Shader::~Shader() {
    if (programId != 0) {
        glDeleteProgram(programId);
    }
}

Shader::Shader(Shader &&other) noexcept : programId(other.programId) {
    other.programId = 0;
}

Shader& Shader::operator=(Shader &&other) noexcept {
    if (this != &other) {
        if (programId != 0) {
            glDeleteProgram(programId);
        }
        programId = other.programId;
        other.programId = 0;
    }
    return *this;
}

std::string Shader::readFile(std::string_view filePath) {
    std::ifstream file{std::string(filePath)};
    if (!file.is_open()) {
        std::cerr << "Failed to open shader file: " << filePath << std::endl;
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

GLuint Shader::compileShader(GLenum type, const std::string& source, std::string_view path) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    if (!checkCompileErrors(shader, path)) {
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

bool Shader::checkCompileErrors(GLuint shader, std::string_view path) {
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        GLchar infoLog[1024];
        glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
        std::cerr << "Shader compilation error in " << path << ":\n" << infoLog << std::endl;
        return false;
    }
    return true;
}

bool Shader::checkLinkErrors(GLuint program) {
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (!success) {
        GLchar infoLog[1024];
        glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
        std::cerr << "Shader link error:\n" << infoLog << std::endl;
        return false;
    }
    return true;
}

bool Shader::loadFromFile(std::string_view vertexPath, std::string_view fragmentPath) {
    std::string vertSource = readFile(vertexPath);
    std::string fragSource = readFile(fragmentPath);

    if (vertSource.empty() || fragSource.empty()) {
        return false;
    }

    GLuint vertShader = compileShader(GL_VERTEX_SHADER, vertSource, vertexPath);
    if (vertShader == 0) return false;

    GLuint fragShader = compileShader(GL_FRAGMENT_SHADER, fragSource, fragmentPath);
    if (fragShader == 0) {
        glDeleteShader(vertShader);
        return false;
    }

    programId = glCreateProgram();
    glAttachShader(programId, vertShader);
    glAttachShader(programId, fragShader);
    glLinkProgram(programId);

    glDeleteShader(vertShader);
    glDeleteShader(fragShader);

    if (!checkLinkErrors(programId)) {
        glDeleteProgram(programId);
        programId = 0;
        return false;
    }

    return true;
}

bool Shader::loadCompute(std::string_view computePath) {
    std::string source = readFile(computePath);
    if (source.empty()) {
        return false;
    }

    GLuint computeShader = compileShader(GL_COMPUTE_SHADER, source, computePath);
    if (computeShader == 0) return false;

    programId = glCreateProgram();
    glAttachShader(programId, computeShader);
    glLinkProgram(programId);

    glDeleteShader(computeShader);

    if (!checkLinkErrors(programId)) {
        glDeleteProgram(programId);
        programId = 0;
        return false;
    }

    return true;
}

void Shader::use() const {
    glUseProgram(programId);
}

void Shader::dispatch(GLuint x, GLuint y, GLuint z) const {
    use();
    glDispatchCompute(x, y, z);
}

void Shader::setBool(std::string_view name, bool value) const {
    glUniform1i(glGetUniformLocation(programId, name.data()), static_cast<int>(value));
}

void Shader::setInt(std::string_view name, int value) const {
    glUniform1i(glGetUniformLocation(programId, name.data()), value);
}

void Shader::setUint(std::string_view name, uint32_t value) const {
    glUniform1ui(glGetUniformLocation(programId, name.data()), value);
}

void Shader::setFloat(std::string_view name, float value) const {
    glUniform1f(glGetUniformLocation(programId, name.data()), value);
}

void Shader::setVec2(std::string_view name, float x, float y) const {
    glUniform2f(glGetUniformLocation(programId, name.data()), x, y);
}

void Shader::setVec4(std::string_view name, float x, float y, float z, float w) const {
    glUniform4f(glGetUniformLocation(programId, name.data()), x, y, z, w);
}

}
