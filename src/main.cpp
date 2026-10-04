#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float4.hpp"
#include "rendering/vulkan_application.h"
#include <cstdlib>
#include <exception>
#include <print>

int main() {
  try {
    VulkanApplication app;
    app.run();
  } catch (const std::exception &e) {
    std::print("Err: {}\n", e.what());
    return EXIT_FAILURE;
  }
  std::print("yippe");
  return EXIT_SUCCESS;
}
