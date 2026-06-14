#pragma once

#include <Glad/glad.h>
#include <string>

class Shader {
public:
    Shader(const std::string& vertexPath, const std::string& fragmentPath);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void use() const;

    void setFloat(const std::string& name, float value) const;
    void setInt(const std::string& name, int value) const;
    void setVec4(const std::string& name, float x, float y, float z, float w) const;
    void setMat4(const std::string& name, const float* mat) const;

private:
    GLuint m_program;

    static std::string readFile(const std::string& path);
    static GLuint compileShader(const std::string& source, GLenum type, const std::string& debugName);
};