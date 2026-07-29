#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum Camera_Movement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT
};

class Camera {
public:
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;
    // Euler angles controlling the camera direction.
    float Yaw;
    float Pitch;
    // Movement and lens settings.
    float MovementSpeed;
    float MouseSensitivity;
    float Zoom;

    Camera(glm::vec3 position = glm::vec3(0.0f, 50.0f, 100.0f),
           glm::vec3 up       = glm::vec3(0.0f, 1.0f, 0.0f),
           float yaw          = -90.0f,
           float pitch        = -20.0f);

    glm::mat4 GetViewMatrix() const;
    void ProcessKeyboard(Camera_Movement direction, float deltaTime);
    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);
    void ProcessMouseScroll(float yoffset);

private:
    void updateCameraVectors();
};
