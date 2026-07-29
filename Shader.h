#pragma once
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glad/glad.h>

class Shader {
public:
    unsigned int ID;
    Shader(const char* vertexPath, const char* fragmentPath);
    void use() const;
    // Uniform helpers centralize shader parameter uploads and location caching.
    void setBool(const std::string& name, bool value) const;
    void setInt(const std::string& name, int  value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec3(const std::string& name, const glm::vec3& value) const;
    void setMat4(const std::string& name, const glm::mat4& mat) const;

private:
    mutable std::unordered_map<std::string, int> uniformLocCache;
    int getUniformLocation(const std::string& name) const;
    void checkCompileErrors(unsigned int shader, const std::string& type) const;
};
