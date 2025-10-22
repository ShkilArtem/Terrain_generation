#include <iostream>
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

    srand(time(NULL));
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
    terrain.generate(50.0f, 0.04f, 4, 0.0f);

    // текстуры
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
            // важно дл€ 1-байтовых строк:
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
            // неожиданный формат
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

        // ваши привычные параметры
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
        return tex;
        };


    //Grass
    GLuint grassAlbedoTex = loadTex("textures/Grass004_1K_JPG_Color.jpg");
    GLuint grassNormalTex = loadTex("textures/Grass004_1K_JPG_NormalGL.jpg");
    GLuint grassRoughnessTex = loadTex("textures/Grass004_1K_JPG_Roughness.jpg");
    GLuint grassAOTex = loadTex("textures/Grass004_1K_JPG_AmbientOcclusion.jpg");
    //Rock
    GLuint rockAlbedoTex = loadTex("textures/Rock011_1K-JPG_Color.jpg");
    GLuint rockNormalTex = loadTex("textures/Rock011_1K-JPG_NormalGL.jpg");
    GLuint rockRoughnessTex = loadTex("textures/Rock011_1K-JPG_Roughness.jpg");
    GLuint rockAOTex = loadTex("textures/Rock011_1K-JPG_AmbientOcclusion.jpg");
    //Snow
    GLuint snowAlbedoTex = loadTex("textures/Snow004_1K-JPG_Color.jpg");
    GLuint snowNormalTex = loadTex("textures/Snow004_1K-JPG_NormalGL.jpg");
    GLuint snowRoughnessTex = loadTex("textures/Snow004_1K-JPG_Roughness.jpg");


    // shadow map setup omitted for brevity...
    // lightSpaceMatrix, FBO и depthTexture надо создать здесь

    float sunElevationDeg = 15.0f;               // угол возвышени€ над горизонтом
    float sunAzimuthDeg = 0.0f;               // направление по горизонтали (по желанию)
    float ambientIntensity = 0.23f;
    float diffuseIntensity = 4.4f;
    float specularIntensity = 0.4f;

    bool erosionRunning = false;

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
            static float amp = 50, freq = 0.04f, ofs = 0; 
            static int oct = 4;
            static float sunAzimuth = 0.0f;   // в градусах 0Ц360
            static float sunElevation = 15.0f;  // угол возвышени€ 0Ц90
            static float ambientInt = ambientIntensity;
            static float diffuseInt = diffuseIntensity;
            static float specularInt = specularIntensity;
            static int iterations = 1000;

            ImGui::Begin("Terrain");

            if (ImGui::SliderFloat("Amplitude", &amp, 0, 100)) terrain.generate(amp, freq, oct, ofs);
            if (ImGui::SliderFloat("Frequency", &freq, 0, 0.1f)) terrain.generate(amp, freq, oct, ofs);
            if (ImGui::SliderInt("Octaves", &oct, 1, 8))     terrain.generate(amp, freq, oct, ofs);
            if (ImGui::SliderFloat("Offset", &ofs, -1000, 1000)) terrain.generate(amp, freq, oct, ofs);
            if (ImGui::SliderFloat("Sun Azimuth", &sunAzimuth, 0.0f, 360.0f)); 
            if (ImGui::SliderFloat("Sun Elevation", &sunElevation, 0.0f, 360.0f));
            if (ImGui::SliderFloat("Ambient", &ambientInt, 0.0f, 5.0f));
            if (ImGui::SliderFloat("Diffuse", &diffuseInt, 0.0f, 20.0f));
            if (ImGui::SliderFloat("Specular", &specularInt, 0.0f, 2.0f));

            if (ImGui::Button(erosionRunning ? "Stop Erosion" : "Start Erosion")) {
                // Toggle the erosion simulation on/off
                erosionRunning = !erosionRunning;
            }
            if (ImGui::SliderInt("Iterations", &iterations, 10, 3000));

            ImGui::End();
            {
                // сохран€ем новые значени€
                ambientIntensity = ambientInt;
                diffuseIntensity = diffuseInt;
                specularIntensity = specularInt;

                // пересчЄт направлени€ солнца
                float el = glm::radians(sunElevation);
                float az = glm::radians(sunAzimuth);
                glm::vec3 L = glm::normalize(glm::vec3(
                    cos(el) * cos(az),
                    sin(el),
                    cos(el) * sin(az)
                ));
                sunDir = L;
            }
            if (erosionRunning) {
                terrain.simulateErosion(iterations);
            }
        }


        // рендер
        glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        terrainShader.use();
        // задаЄм uniform'ы: model, view, proj, lightSpaceMatrix, sun, viewPos и текстурные блоки
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 proj = glm::perspective(glm::radians(camera.Zoom),
            float(SCR_W) / SCR_H,
            0.1f, 500.0f);
        terrainShader.setMat4("model", model);
        terrainShader.setMat4("view", view);
        terrainShader.setMat4("projection", proj);
        // shadow map и параметры солнца нужно тоже сюда

        // позици€ камеры в шейдер
        terrainShader.setVec3("viewPos", camera.Position);



        // ¬водим один вектор Ђцвета солнцаї:
        static glm::vec3 sunColor(1.00f, 0.98f, 0.60f);
        ImGui::ColorEdit3("Sun Color", (float*)&sunColor);



        // параметры направленного света (—олнце)
        terrainShader.setVec3("lightDir", sunDir);
        terrainShader.setVec3("lightColor", sunColor * diffuseIntensity);
        terrainShader.setFloat("ambientFactor", ambientIntensity);
        terrainShader.setFloat("specularFactor", specularIntensity);

        // текстуры
        
        // Grass
        terrainShader.setInt("grassAlbedo", 0);
        terrainShader.setInt("grassNormal", 1);
        terrainShader.setInt("grassRoughness", 2);
        terrainShader.setInt("grassAO", 3);
        glActiveTexture(GL_TEXTURE0);  glBindTexture(GL_TEXTURE_2D, grassAlbedoTex);
        glActiveTexture(GL_TEXTURE1);  glBindTexture(GL_TEXTURE_2D, grassNormalTex);
        glActiveTexture(GL_TEXTURE2);  glBindTexture(GL_TEXTURE_2D, grassRoughnessTex);
        glActiveTexture(GL_TEXTURE3);  glBindTexture(GL_TEXTURE_2D, grassAOTex);

        // Rock
        terrainShader.setInt("rockAlbedo", 4);
        terrainShader.setInt("rockNormal", 5);
        terrainShader.setInt("rockRoughness", 6);
        terrainShader.setInt("rockAO", 7);
        glActiveTexture(GL_TEXTURE4);  glBindTexture(GL_TEXTURE_2D, rockAlbedoTex);
        glActiveTexture(GL_TEXTURE5);  glBindTexture(GL_TEXTURE_2D, rockNormalTex);
        glActiveTexture(GL_TEXTURE6);  glBindTexture(GL_TEXTURE_2D, rockRoughnessTex);
        glActiveTexture(GL_TEXTURE7);  glBindTexture(GL_TEXTURE_2D, rockAOTex);

        // Snow
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

    // очистка
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
