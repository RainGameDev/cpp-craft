#include "camera.h"
#include "glm/ext/matrix_transform.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/glm.hpp>

Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch)
    : position(position), worldUp(up), yaw(yaw), pitch(pitch),
      movementSpeed(2.5f), mouseSensitivity(0.1f), fov(90.0f) {
  updateCameraVectors();
}

void Camera::processKeyboard(glm::vec3 inputDirection, float deltaTime) {
  glm::vec3 forward = glm::normalize(glm::vec3(front.x, 0.0f, front.z));
  glm::vec3 movement = forward * inputDirection.z + right * inputDirection.x +
                       worldUp * inputDirection.y;
  position += movement * movementSpeed * deltaTime;
}

void Camera::processMouseMovement(float xOffset, float yOffset,
                                  bool constrainPitch) {
  xOffset *= mouseSensitivity;
  yOffset *= mouseSensitivity;

  yaw += xOffset;
  pitch += yOffset;

  if (constrainPitch) {
    pitch = std::clamp(pitch, -89.0f, 89.0f);
  }

  updateCameraVectors();
}

void Camera::updateCameraVectors() {
  glm::vec3 newFront;
  newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
  newFront.y = sin(glm::radians(pitch));
  newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
  front = glm::normalize(newFront);

  right = glm::normalize(glm::cross(front, worldUp));
  up = glm::normalize(glm::cross(right, front));
}

glm::mat4 Camera::getProjectionMatrix(float aspectRatio, float nearPlane,
                                      float farPlane) const {
  return glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
}

glm::mat4 Camera::getViewMatrix() const {
  return glm::lookAt(position, position + front, up);
}
