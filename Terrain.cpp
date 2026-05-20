#include "Terrain.h"
#include "Shader.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

namespace {
    const int VERTEX_STRIDE = 15;
    const int POSITION_OFFSET = 0;
    const int NORMAL_OFFSET = 3;
    const int UV_OFFSET = 6;
    const int TANGENT_OFFSET = 8;
    const int EROSION_OFFSET = 14;

    const int PERLIN_PERMUTATION[256] = {
        151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96, 53, 194, 233, 7, 225,
        140, 36, 103, 30, 69, 142, 8, 99, 37, 240, 21, 10, 23, 190, 6, 148,
        247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32,
        57, 177, 33, 88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175,
        74, 165, 71, 134, 139, 48, 27, 166, 77, 146, 158, 231, 83, 111, 229, 122,
        60, 211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244, 102, 143, 54,
        65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169,
        200, 196, 135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64,
        52, 217, 226, 250, 124, 123, 5, 202, 38, 147, 118, 126, 255, 82, 85, 212,
        207, 206, 59, 227, 47, 16, 58, 17, 182, 189, 28, 42, 223, 183, 170, 213,
        119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9,
        129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104,
        218, 246, 97, 228, 251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241,
        81, 51, 145, 235, 249, 14, 239, 107, 49, 192, 214, 31, 181, 199, 106, 157,
        184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254, 138, 236, 205, 93,
        222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180
    };

    int permutation(int index) {
        return PERLIN_PERMUTATION[index & 255];
    }

    float fade(float t) {
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    float lerp(float a, float b, float t) {
        return a + t * (b - a);
    }

    float gradientDot(int hash, float x, float z) {
        switch (hash & 7) {
        case 0: return  x + z;
        case 1: return -x + z;
        case 2: return  x - z;
        case 3: return -x - z;
        case 4: return  x;
        case 5: return -x;
        case 6: return  z;
        default: return -z;
        }
    }
}

float TerrainNoise::perlinNoise(float x, float z) {
    // 1. Find the integer lattice cell that contains the point.
    float xFloor = std::floor(x);
    float zFloor = std::floor(z);

    int x0 = static_cast<int>(xFloor);
    int z0 = static_cast<int>(zFloor);
    int x1 = x0 + 1;
    int z1 = z0 + 1;

    // 2. Convert the point to local cell coordinates in [0, 1].
    float localX = x - xFloor;
    float localZ = z - zFloor;
    float u = fade(localX);
    float v = fade(localZ);

    // 3. Hash each corner to choose a deterministic gradient.
    int bottomLeft = permutation(permutation(x0) + z0);
    int bottomRight = permutation(permutation(x1) + z0);
    int topLeft = permutation(permutation(x0) + z1);
    int topRight = permutation(permutation(x1) + z1);

    // 4. Dot gradients with corner offsets, then smooth-interpolate them.
    float bottom = lerp(
        gradientDot(bottomLeft, localX, localZ),
        gradientDot(bottomRight, localX - 1.0f, localZ),
        u
    );
    float top = lerp(
        gradientDot(topLeft, localX, localZ - 1.0f),
        gradientDot(topRight, localX - 1.0f, localZ - 1.0f),
        u
    );

    // 5. Normalize approximately to the [-1, 1] range expected by terrain octaves.
    float value = lerp(bottom, top, v) * 0.70710678f;
    return std::max(-1.0f, std::min(1.0f, value));
}

Terrain::Terrain(int gridSize, float worldSize)
    : GRID_SIZE(gridSize), WORLD_SIZE(worldSize)
{
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
}

Terrain::~Terrain() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
}

void Terrain::generate(float amplitude, float frequency, int octaves, float offset,
    float persistence, float lacunarity, float heightPower) {
    int N = GRID_SIZE;
    const float safePersistence = std::max(0.0f, std::min(0.95f, persistence));
    const float safeLacunarity = std::max(1.01f, lacunarity);
    const float safeHeightPower = std::max(0.10f, heightPower);
    vertices.clear();
    vertices.reserve(N * N * VERTEX_STRIDE);

    // 1) генерим позиции, нормали-заглушки, UV, тангенты-заглушки
    for (int z = 0; z < N; ++z) {
        for (int x = 0; x < N; ++x) {
            float u = float(x) / (N - 1);
            float v = float(z) / (N - 1);
            float xPos = (u - 0.5f) * WORLD_SIZE;
            float zPos = (v - 0.5f) * WORLD_SIZE;

            // Multi-octave Perlin noise (fBm): big forms first, small details later.
            float n = 0.0f;
            float octaveFrequency = frequency;
            float octaveAmplitude = 1.0f;
            float amplitudeSum = 0.0f;
            for (int o = 0; o < octaves; ++o) {
                n += TerrainNoise::perlinNoise(
                    xPos * octaveFrequency + offset,
                    zPos * octaveFrequency + offset
                ) * octaveAmplitude;
                amplitudeSum += octaveAmplitude;
                octaveFrequency *= safeLacunarity;
                octaveAmplitude *= safePersistence;
            }
            n = (n / amplitudeSum) * 0.5f + 0.5f;
            n = std::max(0.0f, std::min(1.0f, n));
            n = pow(n, safeHeightPower);
            float yPos = n * amplitude;

            // push: pos
            vertices.insert(vertices.end(), { xPos, yPos, zPos });
            // normal placeholder
            vertices.insert(vertices.end(), { 0.0f, 0.0f, 0.0f });
            // uv (тайлинг=10)
            vertices.insert(vertices.end(), { u * 10.0f, v * 10.0f });
            // tangent placeholder
            vertices.insert(vertices.end(), { 0.0f, 0.0f, 0.0f });
            // bitangent placeholder
            vertices.insert(vertices.end(), { 0.0f, 0.0f, 0.0f });
            // signed erosion heat value: negative=eroded, positive=deposited
            vertices.push_back(0.0f);
        }
    }

    // 2) индексы
    indices.clear();
    for (int z = 0; z < N - 1; ++z) {
        for (int x = 0; x < N - 1; ++x) {
            unsigned int i = z * N + x;
            indices.insert(indices.end(), {
                i, i + 1, i + N,
                i + 1, i + N + 1, i + N
                });
        }
    }
    indexCount = indices.size();

    // 3) вычисляем нормали и тангенты
    computeNormals();
    computeTangents();

    // 4) заливаем в буферы
    setupMesh();
}

void Terrain::computeNormals() {
    int N = GRID_SIZE;
    std::vector<glm::vec3> norms(N * N, glm::vec3(0.0f));

    auto pos = [&](int idx) {
        return glm::vec3(
            vertices[idx * VERTEX_STRIDE + POSITION_OFFSET + 0],
            vertices[idx * VERTEX_STRIDE + POSITION_OFFSET + 1],
            vertices[idx * VERTEX_STRIDE + POSITION_OFFSET + 2]
        );
        };

    for (size_t i = 0; i < indices.size(); i += 3) {
        int a = indices[i + 0], b = indices[i + 1], c = indices[i + 2];
        glm::vec3 v0 = pos(a), v1 = pos(b), v2 = pos(c);
        glm::vec3 n = glm::normalize(glm::cross(v2 - v0, v1 - v0));
        norms[a] += n;
        norms[b] += n;
        norms[c] += n;
    }
    for (int i = 0; i < N * N; ++i) {
        glm::vec3 n = glm::normalize(norms[i]);
        float* f = &vertices[i * VERTEX_STRIDE] + NORMAL_OFFSET;
        f[0] = n.x; f[1] = n.y; f[2] = n.z;
    }
}

void Terrain::computeTangents() {
    int N = GRID_SIZE;
    std::vector<glm::vec3> tans(N * N, glm::vec3(0.0f));
    std::vector<glm::vec3> bits(N * N, glm::vec3(0.0f));

    auto pos = [&](int idx) {
        float* f = &vertices[idx * VERTEX_STRIDE];
        return glm::vec3(f[0], f[1], f[2]);
        };
    auto uv = [&](int idx) {
        float* f = &vertices[idx * VERTEX_STRIDE] + UV_OFFSET;
        return glm::vec2(f[0], f[1]);
        };
    auto norm = [&](int idx) {
        float* f = &vertices[idx * VERTEX_STRIDE] + NORMAL_OFFSET;
        return glm::vec3(f[0], f[1], f[2]);
        };

    for (size_t i = 0; i < indices.size(); i += 3) {
        int i0 = indices[i + 0], i1 = indices[i + 1], i2 = indices[i + 2];
        glm::vec3 p0 = pos(i0), p1 = pos(i1), p2 = pos(i2);
        glm::vec2 uv0 = uv(i0), uv1 = uv(i1), uv2 = uv(i2);

        glm::vec3 e1 = p1 - p0;
        glm::vec3 e2 = p2 - p0;
        glm::vec2 dUV1 = uv1 - uv0;
        glm::vec2 dUV2 = uv2 - uv0;

        float r = 1.0f / (dUV1.x * dUV2.y - dUV1.y * dUV2.x);
        glm::vec3 tangent = (e1 * dUV2.y - e2 * dUV1.y) * r;
        glm::vec3 bitan = (e2 * dUV1.x - e1 * dUV2.x) * r;

        tans[i0] += tangent;  tans[i1] += tangent;  tans[i2] += tangent;
        bits[i0] += bitan;    bits[i1] += bitan;    bits[i2] += bitan;
    }

    for (int i = 0; i < N * N; ++i) {
        glm::vec3 T = tans[i];
        glm::vec3 Nrm = norm(i);
        // ортогонализуем
        T = glm::normalize(T - Nrm * glm::dot(Nrm, T));
        glm::vec3 B = glm::normalize(glm::cross(Nrm, T));

        float* f = &vertices[i * VERTEX_STRIDE] + TANGENT_OFFSET;
        f[0] = T.x; f[1] = T.y; f[2] = T.z;
        f[3] = B.x; f[4] = B.y; f[5] = B.z;
    }
}

void Terrain::setupMesh() {
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
        vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned),
        indices.data(), GL_STATIC_DRAW);

    GLsizei stride = VERTEX_STRIDE * sizeof(float);
    // aPos
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    // aNormal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(NORMAL_OFFSET * sizeof(float)));
    // aTexCoord
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(UV_OFFSET * sizeof(float)));
    // aTangent
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(TANGENT_OFFSET * sizeof(float)));
    // aBitangent
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride, (void*)((TANGENT_OFFSET + 3) * sizeof(float)));
    // aErosionDelta
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, stride, (void*)(EROSION_OFFSET * sizeof(float)));

    glBindVertexArray(0);
}

void Terrain::draw(const Shader& shader) const {
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, (GLsizei)indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}
void Terrain::clearErosionHeatmap() {
    const int N = GRID_SIZE;
    for (int i = 0; i < N * N; ++i) {
        vertices[i * VERTEX_STRIDE + EROSION_OFFSET] = 0.0f;
    }

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float), vertices.data());
}
void Terrain::simulateErosion(int iterations, const ErosionSettings& settings) {
    const int N = GRID_SIZE;
    auto hRef = [&](int x, int z) -> float& {
        return vertices[(z * N + x) * VERTEX_STRIDE + POSITION_OFFSET + 1];
        };
    auto erosionRef = [&](int x, int z) -> float& {
        return vertices[(z * N + x) * VERTEX_STRIDE + EROSION_OFFSET];
        };
    auto random01 = []() -> float {
        return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        };

    struct HeightGradient {
        float height;
        float gradX;
        float gradZ;
    };

    auto sampleHeightGradient = [&](float x, float z) -> HeightGradient {
        x = std::max(0.0f, std::min(static_cast<float>(N - 1) - 0.001f, x));
        z = std::max(0.0f, std::min(static_cast<float>(N - 1) - 0.001f, z));

        int x0 = static_cast<int>(std::floor(x));
        int z0 = static_cast<int>(std::floor(z));
        x0 = std::max(0, std::min(N - 2, x0));
        z0 = std::max(0, std::min(N - 2, z0));
        int x1 = x0 + 1;
        int z1 = z0 + 1;

        float tx = x - static_cast<float>(x0);
        float tz = z - static_cast<float>(z0);

        float h00 = hRef(x0, z0);
        float h10 = hRef(x1, z0);
        float h01 = hRef(x0, z1);
        float h11 = hRef(x1, z1);

        HeightGradient result;
        result.height = h00 * (1.0f - tx) * (1.0f - tz)
            + h10 * tx * (1.0f - tz)
            + h01 * (1.0f - tx) * tz
            + h11 * tx * tz;
        result.gradX = (h10 - h00) * (1.0f - tz) + (h11 - h01) * tz;
        result.gradZ = (h01 - h00) * (1.0f - tx) + (h11 - h10) * tx;
        return result;
        };

    auto applyHeightDelta = [&](float x, float z, float delta) {
        x = std::max(0.0f, std::min(static_cast<float>(N - 1) - 0.001f, x));
        z = std::max(0.0f, std::min(static_cast<float>(N - 1) - 0.001f, z));

        int x0 = static_cast<int>(std::floor(x));
        int z0 = static_cast<int>(std::floor(z));
        x0 = std::max(0, std::min(N - 2, x0));
        z0 = std::max(0, std::min(N - 2, z0));
        int x1 = x0 + 1;
        int z1 = z0 + 1;

        float tx = x - static_cast<float>(x0);
        float tz = z - static_cast<float>(z0);
        float w00 = (1.0f - tx) * (1.0f - tz);
        float w10 = tx * (1.0f - tz);
        float w01 = (1.0f - tx) * tz;
        float w11 = tx * tz;

        auto applyVertex = [&](int vx, int vz, float weight) {
            float weightedDelta = delta * weight;
            hRef(vx, vz) = std::max(0.0f, hRef(vx, vz) + weightedDelta);
            erosionRef(vx, vz) += weightedDelta;
            };

        applyVertex(x0, z0, w00);
        applyVertex(x1, z0, w10);
        applyVertex(x0, z1, w01);
        applyVertex(x1, z1, w11);
        };

    const int maxSteps = std::max(1, settings.maxSteps);
    const float initialWater = std::max(0.001f, settings.initialWater);
    const float evaporation = std::max(0.0f, std::min(0.99f, settings.evaporation));
    const float capacityScale = std::max(0.0f, settings.capacityScale);
    const float depositionRate = std::max(0.0f, std::min(1.0f, settings.depositionRate));
    const float erosionRate = std::max(0.0f, std::min(1.0f, settings.erosionRate));
    const float inertia = std::max(0.0f, std::min(0.99f, settings.inertia));
    const int thermalIterations = std::max(0, settings.thermalIterations);
    const float thermalTalus = std::max(0.0f, settings.thermalTalus);
    const float thermalStrength = std::max(0.0f, std::min(1.0f, settings.thermalStrength));
    const float minWater = std::max(0.0f, settings.minWater);

    for (int iter = 0; iter < iterations; ++iter) {
        float posX = random01() * static_cast<float>(N - 1);
        float posZ = random01() * static_cast<float>(N - 1);
        float dirX = 0.0f;
        float dirZ = 0.0f;
        float sediment = 0.0f;
        float water = initialWater;

        for (int step = 0; step < maxSteps; ++step) {
            HeightGradient current = sampleHeightGradient(posX, posZ);

            dirX = dirX * inertia - current.gradX * (1.0f - inertia);
            dirZ = dirZ * inertia - current.gradZ * (1.0f - inertia);
            float dirLength = std::sqrt(dirX * dirX + dirZ * dirZ);
            if (dirLength < 0.0001f) {
                applyHeightDelta(posX, posZ, sediment);
                break;
            }
            dirX /= dirLength;
            dirZ /= dirLength;

            float nextX = posX + dirX;
            float nextZ = posZ + dirZ;
            if (nextX < 0.0f || nextX >= static_cast<float>(N - 1)
                || nextZ < 0.0f || nextZ >= static_cast<float>(N - 1)) {
                applyHeightDelta(posX, posZ, sediment);
                break;
            }

            HeightGradient next = sampleHeightGradient(nextX, nextZ);
            float heightDelta = next.height - current.height;
            float capacity = std::max(-heightDelta * capacityScale * water, 0.0f);

            if (heightDelta > 0.0f || sediment > capacity) {
                float depositAmount = heightDelta > 0.0f
                    ? std::min(sediment, heightDelta)
                    : (sediment - capacity) * depositionRate;
                sediment -= depositAmount;
                applyHeightDelta(posX, posZ, depositAmount);
            }
            else {
                float erodeAmount = std::min((capacity - sediment) * erosionRate, current.height);
                sediment += erodeAmount;
                applyHeightDelta(posX, posZ, -erodeAmount);
            }

            posX = nextX;
            posZ = nextZ;
            water *= 1.0f - evaporation;
            if (water < minWater) {
                applyHeightDelta(posX, posZ, sediment);
                break;
            }
        }
    }


    std::vector<float> heightDeltas(N * N, 0.0f);
    const int neighborOffsets[8][2] = {
        {-1, -1}, {0, -1}, {1, -1},
        {-1,  0},          {1,  0},
        {-1,  1}, {0,  1}, {1,  1}
    };

    for (int pass = 0; pass < thermalIterations; ++pass) {
        std::fill(heightDeltas.begin(), heightDeltas.end(), 0.0f);

        for (int z = 1; z < N - 1; ++z) {
            for (int x = 1; x < N - 1; ++x) {
                float centerHeight = hRef(x, z);
                for (const auto& offset : neighborOffsets) {
                    int nx = x + offset[0];
                    int nz = z + offset[1];
                    float diff = centerHeight - hRef(nx, nz);
                    if (diff <= thermalTalus) {
                        continue;
                    }

                    float transfer = (diff - thermalTalus) * thermalStrength * 0.125f;
                    int centerIndex = z * N + x;
                    int neighborIndex = nz * N + nx;
                    heightDeltas[centerIndex] -= transfer;
                    heightDeltas[neighborIndex] += transfer;
                }
            }
        }

        for (int z = 0; z < N; ++z) {
            for (int x = 0; x < N; ++x) {
                float delta = heightDeltas[z * N + x];
                if (delta == 0.0f) {
                    continue;
                }
                hRef(x, z) = std::max(0.0f, hRef(x, z) + delta);
                erosionRef(x, z) += delta;
            }
        }
    }
    computeNormals();
    computeTangents();
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float), vertices.data());
}