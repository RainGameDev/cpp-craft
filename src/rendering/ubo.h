#include <glm/glm.hpp>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

struct UniformBufferObject {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 proj;
};

constexpr uint32_t maxConcurrentFrames = 2;

struct UboBuffer {
  vk::raii::Buffer buffer{nullptr};
  vk::raii::DeviceMemory memory{nullptr};
  void *mapped = nullptr;
};
