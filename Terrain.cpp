#include "Terrain.h"
#include "Shader.h"
#include <glm/gtc/noise.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>


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

void Terrain::generate(float amplitude, float frequency, int octaves, float offset) {
    int N = GRID_SIZE;
    vertices.clear();
    vertices.reserve(N * N * 14);

    // 1) генерим позиции, нормали-заглушки, UV, тангенты-заглушки
    for (int z = 0; z < N; ++z) {
        for (int x = 0; x < N; ++x) {
            float u = float(x) / (N - 1);
            float v = float(z) / (N - 1);
            float xPos = (u - 0.5f) * WORLD_SIZE;
            float zPos = (v - 0.5f) * WORLD_SIZE;

            // Perlin noise
            float n = 0, freq = frequency, amp = 1, maxA = 0;
            for (int o = 0; o < octaves; ++o) {
                n += glm::perlin(glm::vec2(xPos * freq + offset, zPos * freq + offset)) * amp;
                maxA += amp;
                freq *= 2;
                amp *= 0.5f;
            }
            n = (n / maxA)* 0.5f + 0.5f;
            n = pow(n, 2.0f);
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
            vertices[idx * 14 + 0],
            vertices[idx * 14 + 1],
            vertices[idx * 14 + 2]
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
        float* f = &vertices[i * 14] + 3;
        f[0] = n.x; f[1] = n.y; f[2] = n.z;
    }
}

void Terrain::computeTangents() {
    int N = GRID_SIZE;
    std::vector<glm::vec3> tans(N * N, glm::vec3(0.0f));
    std::vector<glm::vec3> bits(N * N, glm::vec3(0.0f));

    auto pos = [&](int idx) {
        float* f = &vertices[idx * 14];
        return glm::vec3(f[0], f[1], f[2]);
        };
    auto uv = [&](int idx) {
        float* f = &vertices[idx * 14] + 6;
        return glm::vec2(f[0], f[1]);
        };
    auto norm = [&](int idx) {
        float* f = &vertices[idx * 14] + 3;
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

        float* f = &vertices[i * 14] + 8;
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

    GLsizei stride = 14 * sizeof(float);
    // aPos
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    // aNormal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    // aTexCoord
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    // aTangent
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));
    // aBitangent
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride, (void*)(11 * sizeof(float)));

    glBindVertexArray(0);
}

void Terrain::draw(const Shader& shader) const {
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, (GLsizei)indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}
void Terrain::simulateErosion(int iterations) {
    const int N = GRID_SIZE;
    auto hRef = [&](int x, int z) -> float& {
        return vertices[(z * N + x) * 14 + 1];  // y компонента
        };

    for (int iter = 0; iter < iterations; ++iter) {
        int cx = rand() % N;
        int cz = rand() % N;
        float sediment = 0.0f;
        float water = 1.0f;

        for (int step = 0; step < 100; ++step) {
            float& h = hRef(cx, cz);

            // найти самого низкого соседа
            int lx = cx, lz = cz;
            float lh = h;
            for (int dz = -1; dz <= 1; ++dz) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (!dx && !dz) continue;
                    int nx = cx + dx, nz = cz + dz;
                    if (nx < 0 || nx >= N || nz < 0 || nz >= N) continue;
                    float nh = hRef(nx, nz);
                    if (nh < lh) { lh = nh; lx = nx; lz = nz; }
                }
            }

            float dh = h - lh;
            if (dh <= 0.0f) { h += sediment; break; }

            float cap = dh * 0.1f * water;   // можно вынести в параметры ImGui

            if (sediment > cap) {
                float dep = (sediment - cap) * 0.5f;
                sediment -= dep;
                h += dep;
            }
            else {
                float er = std::min((cap - sediment) * 0.2f, h);
                sediment += er;
                h -= er;
            }

            cx = lx; cz = lz;
            water *= 0.9f;
            if (water < 0.01f) { hRef(cx, cz) += sediment; break; }
        }
    }

    computeNormals();
    computeTangents(); // см. пункт 2
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float), vertices.data());
}