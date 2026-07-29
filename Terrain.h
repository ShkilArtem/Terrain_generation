#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glad/glad.h>

class Shader; // Forward declaration keeps this header independent from Shader.h.

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
        float thermalTalus = 0.5f;
        float thermalStrength = 0.035f;
        float minWater = 0.01f;
    };

    Terrain(int gridSize, float worldSize);
    ~Terrain();

    // Builds a height field from layered noise and uploads the mesh to OpenGL.
    void generate(float amplitude, float frequency, int octaves, float offset,
        float persistence, float lacunarity, float heightPower);
    void draw(const Shader& shader) const;
    void simulateErosion(int iterations, const ErosionSettings& settings);
    void clearErosionHeatmap();
    void resetToInitialTerrain();
    void exportMetricsToCSV(const std::string& filename, int iterations,
        float timeGenMs, float timeErosionMs, float timeThermalMs,
        float avgPathLength, float experimentalValue, const std::string& experimentalParameter) const;
    float getLastHydraulicTimeMs() const { return lastHydraulicTimeMs; }
    float getLastThermalTimeMs() const { return lastThermalTimeMs; }
    float getLastAveragePathLength() const { return lastAveragePathLength; }
private:
    int   GRID_SIZE;
    float WORLD_SIZE;
    GLuint VAO, VBO, EBO;
    size_t indexCount;
    float lastHydraulicTimeMs = 0.0f;
    float lastThermalTimeMs = 0.0f;
    float lastAveragePathLength = 0.0f;

    // Vertex layout: position | normal | uv | tangent | bitangent | erosionDelta | hardness.
    std::vector<float> vertices;
    std::vector<float> initialHeights;
    std::vector<unsigned int> indices;

    void computeNormals();
    void computeTangents();
    void setupMesh();
};
