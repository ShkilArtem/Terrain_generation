#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glad/glad.h>

class Shader; // вперЄд объ€вление

// Standalone Perlin noise implementation used by terrain generation.
namespace TerrainNoise {
    float perlinNoise(float x, float z);
}

class Terrain {
public:
    struct ErosionSettings {
        int maxSteps = 80;
        float initialWater = 1.0f;
        float evaporation = 0.05f;
        float capacityScale = 0.35f;
        float depositionRate = 0.18f;
        float erosionRate = 0.10f;
        float inertia = 0.20f;
        int thermalIterations = 1;
        float thermalTalus = 0.05f;
        float thermalStrength = 0.25f;
        float minWater = 0.01f;
    };

    Terrain(int gridSize, float worldSize);
    ~Terrain();

    // ѕараметры: амплитуда шума, частота, октавы, смещение
    void generate(float amplitude, float frequency, int octaves, float offset,
        float persistence, float lacunarity, float heightPower);
    void draw(const Shader& shader) const;
    void simulateErosion(int iterations, const ErosionSettings& settings);
    void clearErosionHeatmap();
    void exportMetricsToCSV(const std::string& filename, int iterations,
        float timeGenMs, float timeErosionMs, float timeThermalMs,
        float experimentalValue, const std::string& experimentalParameter) const;
    float getLastHydraulicTimeMs() const { return lastHydraulicTimeMs; }
    float getLastThermalTimeMs() const { return lastThermalTimeMs; }
private:
    int   GRID_SIZE;
    float WORLD_SIZE;
    GLuint VAO, VBO, EBO;
    size_t indexCount;
    float lastHydraulicTimeMs = 0.0f;
    float lastThermalTimeMs = 0.0f;

    // x,y,z | nx,ny,nz | uv | tangent | bitangent | erosionDelta | hardness => 16 floats
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    void computeNormals();
    void computeTangents();
    void setupMesh();
};
