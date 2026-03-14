// OpenGL loader — MUST be first, before anything that might include GL/gl.h
#include <glad/glad.h>

#include "rendering/Shader.h"
#include "utils/Logger.h"

#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>

Shader::Shader() = default;

Shader::~Shader()
{
    if (m_programId) {
        glDeleteProgram(m_programId);
    }
}

bool Shader::LoadFromFiles(const std::string& vertexPath, const std::string& fragmentPath)
{
    // Read vertex shader
    std::ifstream vFile(vertexPath);
    if (!vFile.is_open()) {
        Logger::Error("Cannot open vertex shader: %s", vertexPath.c_str());
        return false;
    }
    std::stringstream vStream;
    vStream << vFile.rdbuf();
    std::string vertexSrc = vStream.str();

    // Read fragment shader
    std::ifstream fFile(fragmentPath);
    if (!fFile.is_open()) {
        Logger::Error("Cannot open fragment shader: %s", fragmentPath.c_str());
        return false;
    }
    std::stringstream fStream;
    fStream << fFile.rdbuf();
    std::string fragmentSrc = fStream.str();

    return LoadFromSource(vertexSrc, fragmentSrc);
}

bool Shader::LoadFromSource(const std::string& vertexSrc, const std::string& fragmentSrc)
{
    unsigned int vertexShader, fragmentShader;

    if (!CompileShader(vertexShader, GL_VERTEX_SHADER, vertexSrc))
        return false;

    if (!CompileShader(fragmentShader, GL_FRAGMENT_SHADER, fragmentSrc)) {
        glDeleteShader(vertexShader);
        return false;
    }

    bool success = LinkProgram(vertexShader, fragmentShader);

    // Shaders can be deleted after linking
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return success;
}

bool Shader::CompileShader(unsigned int& shaderId, unsigned int type, const std::string& source)
{
    shaderId = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shaderId, 1, &src, nullptr);
    glCompileShader(shaderId);

    int success;
    glGetShaderiv(shaderId, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shaderId, 512, nullptr, log);
        Logger::Error("Shader compilation failed: %s", log);
        return false;
    }
    return true;
}

bool Shader::LinkProgram(unsigned int vertexShader, unsigned int fragmentShader)
{
    m_programId = glCreateProgram();
    glAttachShader(m_programId, vertexShader);
    glAttachShader(m_programId, fragmentShader);
    glLinkProgram(m_programId);

    int success;
    glGetProgramiv(m_programId, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(m_programId, 512, nullptr, log);
        Logger::Error("Shader link failed: %s", log);
        return false;
    }
    return true;
}

void Shader::Use() const
{
    glUseProgram(m_programId);
}

int Shader::GetUniformLocation(const std::string& name) const
{
    return glGetUniformLocation(m_programId, name.c_str());
}

void Shader::SetMat4(const std::string& name, const glm::mat4& value) const
{
    glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::SetVec3(const std::string& name, const glm::vec3& value) const
{
    glUniform3fv(GetUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::SetVec4(const std::string& name, const glm::vec4& value) const
{
    glUniform4fv(GetUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::SetFloat(const std::string& name, float value) const
{
    glUniform1f(GetUniformLocation(name), value);
}

void Shader::SetInt(const std::string& name, int value) const
{
    glUniform1i(GetUniformLocation(name), value);
}
