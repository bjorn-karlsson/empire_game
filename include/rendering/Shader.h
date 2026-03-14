#pragma once

#include <string>
#include <glm/glm.hpp>

// ─── Shader ───────────────────────────────────────────────────
// Simple wrapper around OpenGL shader programs.
// Loads vertex + fragment shaders from files, compiles them,
// and provides uniform setters.
class Shader {
public:
    Shader();
    ~Shader();

    bool LoadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);
    bool LoadFromSource(const std::string& vertexSrc, const std::string& fragmentSrc);

    void Use() const;

    // Uniform setters
    void SetMat4(const std::string& name, const glm::mat4& value) const;
    void SetVec3(const std::string& name, const glm::vec3& value) const;
    void SetVec4(const std::string& name, const glm::vec4& value) const;
    void SetFloat(const std::string& name, float value) const;
    void SetInt(const std::string& name, int value) const;

    unsigned int GetProgramID() const { return m_programId; }

private:
    bool CompileShader(unsigned int& shaderId, unsigned int type, const std::string& source);
    bool LinkProgram(unsigned int vertexShader, unsigned int fragmentShader);
    int  GetUniformLocation(const std::string& name) const;

    unsigned int m_programId = 0;
};
