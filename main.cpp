#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <string>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "Camera.h"
#include "Shader.h"
#include "Terrain.h"
#include <imgui.h>
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// Default window size for the OpenGL viewport.
const unsigned SCR_W = 1920, SCR_H = 1080;

void framebuffer_size_callback(GLFWwindow*, int w, int h) {
    glViewport(0, 0, w, h);
}

bool mouseCaptured = true;
bool firstMouse = true;
float lastX = SCR_W * 0.5f;
float lastY = SCR_H * 0.5f;

Camera camera;
const std::filesystem::path ANALYSIS_DIR = "outputs/benchmark_results";
const std::string BENCHMARK_CSV_PATH = (ANALYSIS_DIR / "terrain_metrics_v2.csv").string();
const std::string SWEEP_CSV_PATH = (ANALYSIS_DIR / "parameter_sweep_report_v2.csv").string();

void mouse_callback(GLFWwindow* /*wnd*/, double xpos, double ypos) {
    ImGuiIO& io = ImGui::GetIO();
    if (!mouseCaptured || io.WantCaptureMouse)
        return;

    if (firstMouse) {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
    }
    float xoff = (float)xpos - lastX;
    float yoff = lastY - (float)ypos;
    lastX = (float)xpos;
    lastY = (float)ypos;

    camera.ProcessMouseMovement(xoff, yoff);
}


int main() {

    srand(static_cast<unsigned>(time(nullptr)));
    // Initialize the OpenGL window and input callbacks.
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    GLFWwindow* window = glfwCreateWindow(SCR_W, SCR_H, "Terrain", NULL, NULL);
    if (!window) { std::cerr << "Failed GLFW\n"; return -1; }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Load OpenGL entry points and enable depth testing for 3D terrain.
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed GLAD\n"; return -1;
    }
    glEnable(GL_DEPTH_TEST);

    glFrontFace(GL_CW);

    // Initialize the immediate-mode UI used for live terrain tuning.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // Shader program used by the terrain renderer.
    Shader terrainShader("shaders/terrain.vert", "shaders/terrain.frag");

    // Generate the initial terrain mesh from layered Perlin noise.
    Terrain terrain(128, 64.0f);
    float terrainAmplitude = 42.0f;
    float terrainFrequency = 0.035f;
    int terrainOctaves = 6;
    float terrainOffset = 0.0f;
    float terrainPersistence = 0.50f;
    float terrainLacunarity = 2.0f;
    float terrainHeightPower = 1.75f;
    terrain.generate(terrainAmplitude, terrainFrequency, terrainOctaves, terrainOffset,
        terrainPersistence, terrainLacunarity, terrainHeightPower);

    // Load a 2D texture and choose the OpenGL format from its channel count.
    auto loadTex = [&](const char* path) -> GLuint {
        int w, h, n;
        unsigned char* data = stbi_load(path, &w, &h, &n, 0);
        if (!data) {
            std::cerr << "Failed to load texture at: " << path << "\n";
            return 0;
        }

        GLenum internalFormat, format;
        if (n == 1) {
            internalFormat = GL_R8;
            format = GL_RED;
            // Single-channel textures need byte alignment to avoid row padding issues.
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        }
        else if (n == 3) {
            internalFormat = GL_RGB8;
            format = GL_RGB;
        }
        else if (n == 4) {
            internalFormat = GL_RGBA8;
            format = GL_RGBA;
        }
        else {
            stbi_image_free(data);
            std::cerr << "Unsupported channels: " << n << " in " << path << "\n";
            return 0;
        }

        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat,
            w, h, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        // Repeating mipmapped textures keep the terrain surface detailed at distance.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
        return tex;
        };


    // Grass material texture set.
    GLuint grassAlbedoTex = loadTex("textures/Grass004_1K_JPG_Color.jpg");
    GLuint grassNormalTex = loadTex("textures/Grass004_1K_JPG_NormalGL.jpg");
    GLuint grassRoughnessTex = loadTex("textures/Grass004_1K_JPG_Roughness.jpg");
    GLuint grassAOTex = loadTex("textures/Grass004_1K_JPG_AmbientOcclusion.jpg");
    // Rock material texture set.
    GLuint rockAlbedoTex = loadTex("textures/rock_surface_diff_1k.jpg");
    GLuint rockNormalTex = loadTex("textures/rock_surface_nor_gl_1k.jpg");
    GLuint rockRoughnessTex = loadTex("textures/rock_surface_rough_1k.jpg");
    GLuint rockAOTex = loadTex("textures/rock_surface_ao_1k.jpg");
    // Snow material texture set.
    GLuint snowAlbedoTex = loadTex("textures/Snow004_1K-JPG_Color.jpg");
    GLuint snowNormalTex = loadTex("textures/Snow004_1K-JPG_NormalGL.jpg");
    GLuint snowRoughnessTex = loadTex("textures/Snow004_1K-JPG_Roughness.jpg");

    // Lighting and material transition parameters exposed in the control panel.
    float sunElevationDeg = 15.0f;
    float sunAzimuthDeg = 0.0f;
    float ambientIntensity = 0.23f;
    float diffuseIntensity = 4.4f;
    float specularIntensity = 0.4f;
    glm::vec3 sunColor(1.00f, 0.98f, 0.60f);

    float grassToRockStart = 8.0f;
    float grassToRockEnd = 12.0f;
    float rockToSnowStart = 18.0f;
    float rockToSnowEnd = 20.0f;

    bool erosionRunning = false;
    int erosionIterationsPerFrame = 350;
    long long liveErosionIterations = 0;
    Terrain::ErosionSettings erosionSettings;
    float lastBenchmarkGenMs = 0.0f;
    float lastBenchmarkErosionMs = 0.0f;
    float lastBenchmarkThermalMs = 0.0f;
    bool benchmarkExported = false;

    const char* sweepParameterNames[] = {
        "Evaporation Rate",
        "Initial Water Volume",
        "Inertia",
        "Capacity Scale",
        "Iteration Count"
    };
    // Sweep state is kept outside the UI block so each frame can advance one experiment step.
    int sweepTargetParameter = 0;
    float sweepStartValue = 0.0f;
    float sweepEndValue = 1.0f;
    int sweepNumberOfSteps = 5;
    int sweepIterations = 350;
    int sweepGridSize = 128;
    Terrain::ErosionSettings sweepBaselineSettings;
    bool sweepRunning = false;
    int sweepCurrentStep = 0;
    float sweepLastExperimentalValue = 0.0f;
    float sweepLastGenMs = 0.0f;
    float sweepLastHydraulicMs = 0.0f;
    float sweepLastThermalMs = 0.0f;
    bool sweepCompleted = false;

    bool showErosionHeatmap = false;
    float heatmapScale = 12.0f;

    float el = glm::radians(sunElevationDeg);
    float az = glm::radians(sunAzimuthDeg);
    // Convert elevation/azimuth controls into a normalized world-space light direction.
    glm::vec3 L = glm::normalize(glm::vec3(
        cos(el) * cos(az),
        sin(el),
        cos(el) * sin(az)
    ));
    glm::vec3 sunDir = L;



    // Main application loop: handle input, update simulations, draw terrain and UI.
    while (!glfwWindowShouldClose(window)) {
        float current = (float)glfwGetTime();
        static float lastTime = current;
        float deltaTime = current - lastTime;
        lastTime = current;

        // Camera input: right mouse captures the cursor, WASD moves through the scene.
        glfwPollEvents();
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            if (!mouseCaptured) {
                mouseCaptured = true;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                firstMouse = true;  // Reset mouse delta to prevent a camera jump.
            }
        }
        else {
            if (mouseCaptured) {
                mouseCaptured = false;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camera.ProcessKeyboard(FORWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camera.ProcessKeyboard(BACKWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camera.ProcessKeyboard(LEFT, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camera.ProcessKeyboard(RIGHT, deltaTime);

        // Build all ImGui panels before the terrain render pass.
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        {
            ImGui::Begin("Terrain Controls");

            if (ImGui::CollapsingHeader("Terrain Shape", ImGuiTreeNodeFlags_DefaultOpen)) {
                bool terrainChanged = false;
                terrainChanged |= ImGui::SliderFloat("Amplitude", &terrainAmplitude, 0.0f, 300.0f);
                terrainChanged |= ImGui::SliderFloat("Frequency", &terrainFrequency, 0.001f, 0.1f, "%.4f");
                terrainChanged |= ImGui::SliderInt("Octaves", &terrainOctaves, 1, 10);
                terrainChanged |= ImGui::SliderFloat("Persistence", &terrainPersistence, 0.10f, 0.90f);
                terrainChanged |= ImGui::SliderFloat("Lacunarity", &terrainLacunarity, 1.10f, 4.00f);
                terrainChanged |= ImGui::SliderFloat("Height power", &terrainHeightPower, 0.50f, 4.00f);
                terrainChanged |= ImGui::SliderFloat("Offset", &terrainOffset, -1000.0f, 1000.0f);
                if (ImGui::Button("Recommended terrain")) {
                    terrainAmplitude = 42.0f;
                    terrainFrequency = 0.035f;
                    terrainOctaves = 6;
                    terrainPersistence = 0.50f;
                    terrainLacunarity = 2.0f;
                    terrainHeightPower = 1.75f;
                    terrainOffset = 0.0f;
                    terrainChanged = true;
                }
                if (terrainChanged) {
                    // Rebuilding the mesh also resets live erosion because the base height field changed.
                    terrain.generate(terrainAmplitude, terrainFrequency, terrainOctaves, terrainOffset,
                        terrainPersistence, terrainLacunarity, terrainHeightPower);
                    liveErosionIterations = 0;
                }
            }

            if (ImGui::CollapsingHeader("Erosion Simulation", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Run Erosion", &erosionRunning);
                ImGui::SameLine();
                if (ImGui::Button("Step Once")) {
                    terrain.simulateErosion(erosionIterationsPerFrame, erosionSettings);
                    liveErosionIterations += erosionIterationsPerFrame;
                }
                ImGui::Text("Live iterations since reset: %lld", liveErosionIterations);
                if (ImGui::Button("Reset simulation state")) {
                    erosionRunning = false;
                    liveErosionIterations = 0;
                    terrain.resetToInitialTerrain();
                }
                ImGui::SliderInt("Drops / frame", &erosionIterationsPerFrame, 1, 3000);
                ImGui::SliderInt("Max droplet steps", &erosionSettings.maxSteps, 1, 300);
                ImGui::SliderFloat("Initial water", &erosionSettings.initialWater, 0.01f, 5.0f);
                ImGui::SliderFloat("Evaporation", &erosionSettings.evaporation, 0.0f, 0.99f);
                ImGui::SliderFloat("Capacity scale", &erosionSettings.capacityScale, 0.0f, 2.0f);
                ImGui::SliderFloat("Deposition rate", &erosionSettings.depositionRate, 0.0f, 1.0f);
                ImGui::SliderFloat("Erosion rate", &erosionSettings.erosionRate, 0.0f, 1.0f);
                ImGui::SliderFloat("Inertia", &erosionSettings.inertia, 0.0f, 0.99f);
                ImGui::SliderInt("Thermal passes", &erosionSettings.thermalIterations, 0, 20);
                ImGui::SliderFloat("Thermal talus", &erosionSettings.thermalTalus, 0.0f, 1.0f);
                ImGui::SliderFloat("Thermal strength", &erosionSettings.thermalStrength, 0.0f, 1.0f);
                ImGui::SliderFloat("Min water", &erosionSettings.minWater, 0.0f, 0.5f);
                if (ImGui::Button("Recommended erosion")) {
                    erosionIterationsPerFrame = 350;
                    erosionSettings = Terrain::ErosionSettings();
                }
                if (ImGui::Button("Run Benchmark & Export CSV")) {
                    // Benchmark uses the current UI settings, then records generation and erosion timings.
                    auto genStart = std::chrono::high_resolution_clock::now();
                    terrain.generate(terrainAmplitude, terrainFrequency, terrainOctaves, terrainOffset,
                        terrainPersistence, terrainLacunarity, terrainHeightPower);
                    liveErosionIterations = 0;
                    auto genEnd = std::chrono::high_resolution_clock::now();
                    lastBenchmarkGenMs = std::chrono::duration<float, std::milli>(genEnd - genStart).count();

                    terrain.simulateErosion(erosionIterationsPerFrame, erosionSettings);
                    lastBenchmarkErosionMs = terrain.getLastHydraulicTimeMs();
                    lastBenchmarkThermalMs = terrain.getLastThermalTimeMs();
                    std::filesystem::create_directories(ANALYSIS_DIR);
                    terrain.exportMetricsToCSV(BENCHMARK_CSV_PATH, erosionIterationsPerFrame,
                        lastBenchmarkGenMs, lastBenchmarkErosionMs, lastBenchmarkThermalMs,
                        terrain.getLastAveragePathLength(), 0.0f, "Baseline");
                    benchmarkExported = true;
                }
                if (benchmarkExported) {
                    ImGui::Text("CSV: %s", BENCHMARK_CSV_PATH.c_str());
                    ImGui::Text("Gen %.2f ms | Hydraulic %.2f ms | Thermal %.2f ms",
                        lastBenchmarkGenMs, lastBenchmarkErosionMs, lastBenchmarkThermalMs);
                }
            }

            if (ImGui::CollapsingHeader("Visualization", ImGuiTreeNodeFlags_DefaultOpen)) {
                // The heatmap colors signed height deltas without changing the terrain geometry.
                ImGui::Checkbox("Erosion heatmap", &showErosionHeatmap);
                ImGui::SliderFloat("Heatmap intensity", &heatmapScale, 1.0f, 80.0f);
                if (ImGui::Button("Clear heatmap")) {
                    terrain.clearErosionHeatmap();
                }
            }

            if (ImGui::CollapsingHeader("Material Heights", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::SliderFloat("Grass to rock start", &grassToRockStart, -20.0f, 100.0f);
                ImGui::SliderFloat("Grass to rock end", &grassToRockEnd, -20.0f, 100.0f);
                ImGui::SliderFloat("Rock to snow start", &rockToSnowStart, -20.0f, 150.0f);
                ImGui::SliderFloat("Rock to snow end", &rockToSnowEnd, -20.0f, 150.0f);
            }

            if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::SliderFloat("Sun Azimuth", &sunAzimuthDeg, 0.0f, 360.0f);
                ImGui::SliderFloat("Sun Elevation", &sunElevationDeg, 0.0f, 360.0f);
                ImGui::ColorEdit3("Sun Color", (float*)&sunColor);
                ImGui::SliderFloat("Ambient", &ambientIntensity, 0.0f, 5.0f);
                ImGui::SliderFloat("Diffuse", &diffuseIntensity, 0.0f, 20.0f);
                ImGui::SliderFloat("Specular", &specularIntensity, 0.0f, 2.0f);
            }

            ImGui::End();

            // Automated sweeps compare one erosion parameter at a time while holding the others fixed.
            ImGui::Begin("Geological Analysis & Parameter Sweep");
            ImGui::Combo("Target Parameter", &sweepTargetParameter, sweepParameterNames, IM_ARRAYSIZE(sweepParameterNames));
            if (sweepTargetParameter == 0 || sweepTargetParameter == 2) {
                ImGui::SliderFloat("Start Value", &sweepStartValue, 0.0f, 0.99f, "%.3f");
                ImGui::SliderFloat("End Value", &sweepEndValue, 0.0f, 0.99f, "%.3f");
            }
            else if (sweepTargetParameter == 1) {
                ImGui::SliderFloat("Start Value", &sweepStartValue, 0.01f, 5.0f, "%.3f");
                ImGui::SliderFloat("End Value", &sweepEndValue, 0.01f, 5.0f, "%.3f");
            }
            else if (sweepTargetParameter == 3) {
                ImGui::SliderFloat("Start Value", &sweepStartValue, 0.0f, 2.0f, "%.3f");
                ImGui::SliderFloat("End Value", &sweepEndValue, 0.0f, 2.0f, "%.3f");
            }
            else {
                ImGui::SliderFloat("Start Value", &sweepStartValue, 1.0f, 1000000.0f, "%.0f");
                ImGui::SliderFloat("End Value", &sweepEndValue, 1.0f, 1000000.0f, "%.0f");
            }
            ImGui::SliderInt("Number of Steps", &sweepNumberOfSteps, 3, 10);
            ImGui::Separator();
            ImGui::SliderInt("Iterations per step", &sweepIterations, 1, 1000000);
            ImGui::SliderInt("Fixed grid size", &sweepGridSize, 32, 1024);
            ImGui::SliderInt("Baseline max droplet steps", &sweepBaselineSettings.maxSteps, 1, 300);
            ImGui::SliderFloat("Baseline initial water", &sweepBaselineSettings.initialWater, 0.01f, 5.0f);
            ImGui::SliderFloat("Baseline evaporation", &sweepBaselineSettings.evaporation, 0.0f, 0.99f);
            ImGui::SliderFloat("Baseline inertia", &sweepBaselineSettings.inertia, 0.0f, 0.99f);
            ImGui::SliderFloat("Baseline capacity scale", &sweepBaselineSettings.capacityScale, 0.0f, 2.0f);
            ImGui::SliderFloat("Baseline deposition rate", &sweepBaselineSettings.depositionRate, 0.0f, 1.0f);
            ImGui::SliderFloat("Baseline erosion rate", &sweepBaselineSettings.erosionRate, 0.0f, 1.0f);
            ImGui::SliderInt("Thermal passes", &sweepBaselineSettings.thermalIterations, 0, 20);
            ImGui::SliderFloat("Thermal talus", &sweepBaselineSettings.thermalTalus, 0.0f, 1.0f);
            ImGui::SliderFloat("Thermal strength", &sweepBaselineSettings.thermalStrength, 0.0f, 1.0f);
            ImGui::SliderFloat("Baseline min water", &sweepBaselineSettings.minWater, 0.0f, 0.5f);

            if (!sweepRunning && ImGui::Button("Execute Automated Parameter Sweep")) {
                std::filesystem::create_directories(ANALYSIS_DIR);
                sweepRunning = true;
                sweepCompleted = false;
                sweepCurrentStep = 0;
            }

            if (sweepRunning) {
                int totalSteps = std::max(3, sweepNumberOfSteps);
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                    "Running Sweep Simulation Step %d/%d...", sweepCurrentStep + 1, totalSteps);

                // Interpolate the selected parameter from start to end across the requested steps.
                float stepSize = (totalSteps > 1)
                    ? (sweepEndValue - sweepStartValue) / static_cast<float>(totalSteps - 1)
                    : 0.0f;
                sweepLastExperimentalValue = sweepStartValue + static_cast<float>(sweepCurrentStep) * stepSize;
                if (sweepTargetParameter == 0 || sweepTargetParameter == 2) {
                    sweepLastExperimentalValue = std::max(0.0f, std::min(0.99f, sweepLastExperimentalValue));
                }
                else if (sweepTargetParameter == 1) {
                    sweepLastExperimentalValue = std::max(0.01f, sweepLastExperimentalValue);
                }
                else if (sweepTargetParameter == 3) {
                    sweepLastExperimentalValue = std::max(0.0f, std::min(2.0f, sweepLastExperimentalValue));
                }

                const int activeGridSize = sweepGridSize;
                int activeIterations = sweepIterations;
                // Iteration count is special: it changes the simulation length, not an erosion coefficient.
                if (sweepTargetParameter == 4) {
                    activeIterations = std::max(1, static_cast<int>(std::round(sweepLastExperimentalValue)));
                    sweepLastExperimentalValue = static_cast<float>(activeIterations);
                }

                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                    "Step %d / %d (Value: %.4f, Fixed Grid: %d)",
                    sweepCurrentStep + 1, totalSteps, sweepLastExperimentalValue, activeGridSize);
                glfwPollEvents();
                glFlush();
                Terrain sweepTerrain(activeGridSize, 64.0f);
                auto sweepGenStart = std::chrono::high_resolution_clock::now();
                // Each sweep step uses a fresh terrain so results are comparable across parameter values.
                sweepTerrain.generate(terrainAmplitude, terrainFrequency, terrainOctaves, terrainOffset,
                    terrainPersistence, terrainLacunarity, terrainHeightPower);
                sweepTerrain.clearErosionHeatmap();
                auto sweepGenEnd = std::chrono::high_resolution_clock::now();
                sweepLastGenMs = std::chrono::duration<float, std::milli>(sweepGenEnd - sweepGenStart).count();

                Terrain::ErosionSettings testSettings = sweepBaselineSettings;
                if (sweepTargetParameter == 0) testSettings.evaporation = sweepLastExperimentalValue;
                if (sweepTargetParameter == 1) testSettings.initialWater = sweepLastExperimentalValue;
                if (sweepTargetParameter == 2) testSettings.inertia = sweepLastExperimentalValue;
                if (sweepTargetParameter == 3) testSettings.capacityScale = sweepLastExperimentalValue;

                std::srand(1337);
                // Fixed random seed keeps droplet paths reproducible between sweep steps.
                sweepTerrain.simulateErosion(activeIterations, testSettings);
                glfwPollEvents();
                glFlush();
                sweepLastHydraulicMs = sweepTerrain.getLastHydraulicTimeMs();
                sweepLastThermalMs = sweepTerrain.getLastThermalTimeMs();
                sweepTerrain.exportMetricsToCSV(SWEEP_CSV_PATH, activeIterations,
                    sweepLastGenMs, sweepLastHydraulicMs, sweepLastThermalMs,
                    sweepTerrain.getLastAveragePathLength(), sweepLastExperimentalValue,
                    sweepParameterNames[sweepTargetParameter]);

                sweepCurrentStep++;
                if (sweepCurrentStep >= totalSteps) {
                    sweepRunning = false;
                    sweepCompleted = true;
                }
            }

            if (sweepCompleted) {
                ImGui::Text("Sweep CSV: %s", SWEEP_CSV_PATH.c_str());
            }
            ImGui::Text("Last value %.4f | Gen %.2f ms | Hydraulic %.2f ms | Thermal %.2f ms",
                sweepLastExperimentalValue, sweepLastGenMs, sweepLastHydraulicMs, sweepLastThermalMs);
            ImGui::End();

            float currentElevationRadians = glm::radians(sunElevationDeg);
            float currentAzimuthRadians = glm::radians(sunAzimuthDeg);
            // Recompute sunlight after UI edits so lighting responds immediately.
            glm::vec3 currentSunDirection = glm::normalize(glm::vec3(
                cos(currentElevationRadians) * cos(currentAzimuthRadians),
                sin(currentElevationRadians),
                cos(currentElevationRadians) * sin(currentAzimuthRadians)
            ));
            sunDir = currentSunDirection;

            if (erosionRunning) {
                terrain.simulateErosion(erosionIterationsPerFrame, erosionSettings);
                liveErosionIterations += erosionIterationsPerFrame;
            }
        }


        // Render pass: clear the frame, bind uniforms/textures, then draw the terrain.
        glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        terrainShader.use();
        // Camera matrices are updated every frame because the user can fly freely.
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 proj = glm::perspective(glm::radians(camera.Zoom),
            float(SCR_W) / SCR_H,
            0.1f, 500.0f);
        terrainShader.setMat4("model", model);
        terrainShader.setMat4("view", view);
        terrainShader.setMat4("projection", proj);
        // View position is needed by the PBR shader for specular highlights.
        terrainShader.setVec3("viewPos", camera.Position);

        // Directional sunlight and terrain-material controls.
        terrainShader.setVec3("lightDir", sunDir);
        terrainShader.setVec3("lightColor", sunColor * diffuseIntensity);
        terrainShader.setFloat("ambientFactor", ambientIntensity);
        terrainShader.setFloat("specularFactor", specularIntensity);
        terrainShader.setFloat("grassToRockStart", grassToRockStart);
        terrainShader.setFloat("grassToRockEnd", grassToRockEnd);
        terrainShader.setFloat("rockToSnowStart", rockToSnowStart);
        terrainShader.setFloat("rockToSnowEnd", rockToSnowEnd);
        terrainShader.setBool("showErosionHeatmap", showErosionHeatmap);
        terrainShader.setFloat("heatmapScale", heatmapScale);

        // Bind grass textures.
        terrainShader.setInt("grassAlbedo", 0);
        terrainShader.setInt("grassNormal", 1);
        terrainShader.setInt("grassRoughness", 2);
        terrainShader.setInt("grassAO", 3);
        glActiveTexture(GL_TEXTURE0);  glBindTexture(GL_TEXTURE_2D, grassAlbedoTex);
        glActiveTexture(GL_TEXTURE1);  glBindTexture(GL_TEXTURE_2D, grassNormalTex);
        glActiveTexture(GL_TEXTURE2);  glBindTexture(GL_TEXTURE_2D, grassRoughnessTex);
        glActiveTexture(GL_TEXTURE3);  glBindTexture(GL_TEXTURE_2D, grassAOTex);

        // Bind rock textures.
        terrainShader.setInt("rockAlbedo", 4);
        terrainShader.setInt("rockNormal", 5);
        terrainShader.setInt("rockRoughness", 6);
        terrainShader.setInt("rockAO", 7);
        glActiveTexture(GL_TEXTURE4);  glBindTexture(GL_TEXTURE_2D, rockAlbedoTex);
        glActiveTexture(GL_TEXTURE5);  glBindTexture(GL_TEXTURE_2D, rockNormalTex);
        glActiveTexture(GL_TEXTURE6);  glBindTexture(GL_TEXTURE_2D, rockRoughnessTex);
        glActiveTexture(GL_TEXTURE7);  glBindTexture(GL_TEXTURE_2D, rockAOTex);

        // Bind snow textures.
        terrainShader.setInt("snowAlbedo", 8);
        terrainShader.setInt("snowNormal", 9);
        terrainShader.setInt("snowRoughness", 10);
        glActiveTexture(GL_TEXTURE8);  glBindTexture(GL_TEXTURE_2D, snowAlbedoTex);
        glActiveTexture(GL_TEXTURE9);  glBindTexture(GL_TEXTURE_2D, snowNormalTex);
        glActiveTexture(GL_TEXTURE10); glBindTexture(GL_TEXTURE_2D, snowRoughnessTex);
        


        terrain.draw(terrainShader);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // Release UI, window, and GLFW resources in reverse setup order.
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
