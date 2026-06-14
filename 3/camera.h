#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum CameraDirection {
    FORWARD, BACKWARD, LEFT, RIGHT, UP, DOWN
};

class Camera {
public:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;

    float yaw;
    float pitch;
    float speed;
    float sensitivity;

    Camera(glm::vec3 pos = glm::vec3(0.0f, 20.0f, 0.0f))
        : position(pos), worldUp(glm::vec3(0.0f, 1.0f, 0.0f)),
          yaw(-90.0f), pitch(-15.0f),
          speed(20.0f), sensitivity(0.15f)
    {
        updateVectors();
    }

    glm::mat4 getViewMatrix() {
        return glm::lookAt(position, position + front, up);
    }

    void processKeyboard(CameraDirection dir, float deltaTime) {
        float velocity = speed * deltaTime;
        switch (dir) {
            case FORWARD:  position += front * velocity; break;
            case BACKWARD: position -= front * velocity; break;
            case LEFT:     position -= right * velocity; break;
            case RIGHT:    position += right * velocity; break;
            case UP:       position += up * velocity; break;
            case DOWN:     position -= up * velocity; break;
        }
    }

    void processMouseMovement(float xoffset, float yoffset, bool constrainPitch = true) {
        xoffset *= sensitivity;
        yoffset *= sensitivity;
        yaw   += xoffset;
        pitch += yoffset;
        if (constrainPitch) {
            if (pitch > 89.0f)  pitch = 89.0f;
            if (pitch < -89.0f) pitch = -89.0f;
        }
        updateVectors();
    }

private:
    void updateVectors() {
        glm::vec3 f;
        f.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        f.y = sin(glm::radians(pitch));
        f.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(f);
        right = glm::normalize(glm::cross(front, worldUp));
        up    = glm::normalize(glm::cross(right, front));
    }
};

#endif
