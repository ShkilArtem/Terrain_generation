#include <iostream>
#include <cstdlib>
#include <ctime>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "Camera.h"
#include "Shader.h"
#include "Terrain.h"
#include <imgui.h>
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// размеры окна
const unsigned SCR_W = 1920, SCR_H = 1080;

void framebuffer_size_callback(GLFWwindow*, int w, int h) {
    glViewport(0, 0, w, h);
}

bool mouseCaptured = true;
bool firstMouse = true;
float lastX = SCR_W * 0.5f;
float lastY = SCR_H * 0.5f;

Camera camera;

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
    // GLFW
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

    // GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed GLAD\n"; return -1;
    }
    glEnable(GL_DEPTH_TEST);

    glFrontFace(GL_CW);

    // ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // камера
    

    // шейдеры
    Shader terrainShader("shaders/terrain.vert", "shaders/terrain.frag");

    // террейн
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

    // shadow map setup omitted for brevity...
    // lightSpaceMatrix, FBO и depthTexture надо создать здесь

    float sunElevationDeg = 15.0f;               // угол возвышения над горизонтом
    float sunAzimuthDeg = 0.0f;               // направление по горизонтали (по желанию)
    float ambientIntensity = 0.23f;
    float diffuseIntensity = 4.4f;
    float specularIntensity = 0.4f;
    glm::vec3 sunColor(1.00f, 0.98f, 0.60f);

    float grassToRockStart = 8.0f;
    float grassToRockEnd = 12.0f;
    float rockToSnowStart = 18.0f;
    float rockToSnowEnd = 20.0f;
    glm::vec3 grassColor(0.30f, 0.58f, 0.24f);
    glm::vec3 rockColor(0.48f, 0.45f, 0.40f);
    glm::vec3 snowColor(0.92f, 0.94f, 0.90f);

    bool erosionRunning = false;
    int erosionIterationsPerFrame = 350;
    Terrain::ErosionSettings erosionSettings;

    float el = glm::radians(sunElevationDeg);
    float az = glm::radians(sunAzimuthDeg);
    glm::vec3 L = glm::normalize(glm::vec3(
        cos(el) * cos(az),
        sin(el),
        cos(el) * sin(az)
    ));
    glm::vec3 sunDir = L;



    // цикл
    while (!glfwWindowShouldClose(window)) {
        float current = (float)glfwGetTime();
        static float lastTime = current;
        float deltaTime = current - lastTime;
        lastTime = current;

        // ввод
        glfwPollEvents();
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            if (!mouseCaptured) {
                mouseCaptured = true;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                firstMouse = true;  // сбросим дельту мыши, чтобы избежать рывка
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

        // ImGui
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
                    terrain.generate(terrainAmplitude, terrainFrequency, terrainOctaves, terrainOffset,
                        terrainPersistence, terrainLacunarity, terrainHeightPower);
                }
            }

            if (ImGui::CollapsingHeader("Erosion Simulation", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Run Erosion", &erosionRunning);
                ImGui::SameLine();
                if (ImGui::Button("Step Once")) {
                    terrain.simulateErosion(erosionIterationsPerFrame, erosionSettings);
                }
                ImGui::SliderInt("Drops / frame", &erosionIterationsPerFrame, 1, 3000);
                ImGui::SliderInt("Max droplet steps", &erosionSettings.maxSteps, 1, 300);
                ImGui::SliderFloat("Initial water", &erosionSettings.initialWater, 0.01f, 5.0f);
                ImGui::SliderFloat("Evaporation", &erosionSettings.evaporation, 0.0f, 0.99f);
                ImGui::SliderFloat("Capacity scale", &erosionSettings.capacityScale, 0.0f, 2.0f);
                ImGui::SliderFloat("Deposition rate", &erosionSettings.depositionRate, 0.0f, 1.0f);
                ImGui::SliderFloat("Erosion rate", &erosionSettings.erosionRate, 0.0f, 1.0f);
                ImGui::SliderFloat("Min water", &erosionSettings.minWater, 0.0f, 0.5f);
                if (ImGui::Button("Recommended erosion")) {
                    erosionIterationsPerFrame = 350;
                    erosionSettings = Terrain::ErosionSettings();
                }
            }

            if (ImGui::CollapsingHeader("Material Heights", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::SliderFloat("Grass to rock start", &grassToRockStart, -20.0f, 100.0f);
                ImGui::SliderFloat("Grass to rock end", &grassToRockEnd, -20.0f, 100.0f);
                ImGui::SliderFloat("Rock to snow start", &rockToSnowStart, -20.0f, 150.0f);
                ImGui::SliderFloat("Rock to snow end", &rockToSnowEnd, -20.0f, 150.0f);
                ImGui::ColorEdit3("Grass color", (float*)&grassColor);
                ImGui::ColorEdit3("Rock color", (float*)&rockColor);
                ImGui::ColorEdit3("Snow color", (float*)&snowColor);
            }

            if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::SliderFloat("Sun Azimuth", &sunAzimuthDeg, 0.0f, 360.0f);
                ImGui::SliderFloat("Sun Elevation", &sunElevationDeg, 0.0f, 90.0f);
                ImGui::ColorEdit3("Sun Color", (float*)&sunColor);
                ImGui::SliderFloat("Ambient", &ambientIntensity, 0.0f, 5.0f);
                ImGui::SliderFloat("Diffuse", &diffuseIntensity, 0.0f, 20.0f);
                ImGui::SliderFloat("Specular", &specularIntensity, 0.0f, 2.0f);
            }

            ImGui::End();

            float el = glm::radians(sunElevationDeg);
            float az = glm::radians(sunAzimuthDeg);
            glm::vec3 L = glm::normalize(glm::vec3(
                cos(el) * cos(az),
                sin(el),
                cos(el) * sin(az)
            ));
            sunDir = L;

            if (erosionRunning) {
                terrain.simulateErosion(erosionIterationsPerFrame, erosionSettings);
            }
        }


        // рендер
        glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        terrainShader.use();
        // задаём uniform'ы: model, view, projection, sun, colors
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 proj = glm::perspective(glm::radians(camera.Zoom),
            float(SCR_W) / SCR_H,
            0.1f, 500.0f);
        terrainShader.setMat4("model", model);
        terrainShader.setMat4("view", view);
        terrainShader.setMat4("projection", proj);
        // shadow map и параметры солнца нужно тоже сюда

        // позиция камеры в шейдер
        terrainShader.setVec3("viewPos", camera.Position);

        // параметры направленного света (Солнце)
        terrainShader.setVec3("lightDir", sunDir);
        terrainShader.setVec3("lightColor", sunColor * diffuseIntensity);
        terrainShader.setFloat("ambientFactor", ambientIntensity);
        terrainShader.setFloat("specularFactor", specularIntensity);
        terrainShader.setFloat("grassToRockStart", grassToRockStart);
        terrainShader.setFloat("grassToRockEnd", grassToRockEnd);
        terrainShader.setFloat("rockToSnowStart", rockToSnowStart);
        terrainShader.setFloat("rockToSnowEnd", rockToSnowEnd);
        terrainShader.setVec3("grassColor", grassColor);
        terrainShader.setVec3("rockColor", rockColor);
        terrainShader.setVec3("snowColor", snowColor);

        terrain.draw(terrainShader);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // очистка
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
