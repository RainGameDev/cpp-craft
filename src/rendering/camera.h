#include <glm/glm.hpp>

class Camera {
private:
  glm::vec3 front;
  glm::vec3 up;
  glm::vec3 right;
  glm::vec3 worldUp;
  glm::vec3 direction;

  float yaw;
  float pitch;

  float movementSpeed;
  float mouseSensitivity;
  float fov;

public:
  glm::vec3 position;

  Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, -3.0f),
         glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = -90.0f,
         float pitch = 0.0f);

  void updateCameraVectors();

  glm::mat4 getViewMatrix() const;
  glm::mat4 getProjectionMatrix(float aspectRatio, float nearPlane = 0.01f,
                                float farPlane = 10000.0f) const;

  void processKeyboard(glm::vec3 inputDirection, float deltaTime);
  void processMouseMovement(float xOffset, float yOffset,
                            bool constrainPitch = true);
  void processMouseScroll(float yOffset);

  glm::vec3 getPosition() const { return position; }
  glm::vec3 getFront() const { return front; }
  float getZoom() const { return fov; }
};
