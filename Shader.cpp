#include "Shader.h"
#include <fstream>
#include <sstream>
#include <iostream>

Shader::Shader(const char* vertexPath, const char* fragmentPath) {
    std::string vertexCode, fragmentCode;
    std::ifstream vShaderFile(vertexPath), fShaderFile(fragmentPath);
    if (!vShaderFile || !fShaderFile) {
        std::cerr << "ERROR: Shader file not found\n";
    }
    std::stringstream vss, fss;
    vss << vShaderFile.rdbuf(); vertexCode = vss.str();
    fss << fShaderFile.rdbuf(); fragmentCode = fss.str();
    const char* vCode = vertexCode.c_str();
    const char* fCode = fragmentCode.c_str();
    unsigned int vertex, fragment;
    // Compile the vertex shader stage.
    vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vCode, NULL);
    glCompileShader(vertex);
    checkCompileErrors(vertex, "VERTEX");
    // Compile the fragment shader stage.
    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fCode, NULL);
    glCompileShader(fragment);
    checkCompileErrors(fragment, "FRAGMENT");
    // Link both stages into a shader program.
    ID = glCreateProgram();
    glAttachShader(ID, vertex);
    glAttachShader(ID, fragment);
    glLinkProgram(ID);
    checkCompileErrors(ID, "PROGRAM");
    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

void Shader::use() const {
    glUseProgram(ID);
}

int Shader::getUniformLocation(const std::string& name) const {
    if (uniformLocCache.count(name))
        return uniformLocCache[name];
    int loc = glGetUniformLocation(ID, name.c_str());
    uniformLocCache[name] = loc;
    return loc;
}

void Shader::setBool(const std::string& name, bool value) const {
    glUniform1i(getUniformLocation(name), (int)value);
}
void Shader::setInt(const std::string& name, int value) const {
    glUniform1i(getUniformLocation(name), value);
}
void Shader::setFloat(const std::string& name, float value) const {
    glUniform1f(getUniformLocation(name), value);
}
void Shader::setVec3(const std::string& name, const glm::vec3& v) const {
    glUniform3fv(getUniformLocation(name), 1, &v[0]);
}
void Shader::setMat4(const std::string& name, const glm::mat4& m) const {
    glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, &m[0][0]);
}

void Shader::checkCompileErrors(unsigned int shader, const std::string& type) const {
    int success;
    char infoLog[1024];
    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n"
                << infoLog << "\n";
        }
    }
    else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n"
                << infoLog << "\n";
        }
    }
}
