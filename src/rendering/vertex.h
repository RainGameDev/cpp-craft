#pragma once

#include <array>

#include <glm/glm.hpp>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

struct Vertex {
  glm::vec3 pos;
  glm::vec3 color;
  glm::vec2 uv;

  static vk::VertexInputBindingDescription getBindingDescription() {
    return {.binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = vk::VertexInputRate::eVertex};
  }

  static std::array<vk::VertexInputAttributeDescription, 3>
  getAttributeDescriptions() {
    return {{{.location = 0,
              .binding = 0,
              .format = vk::Format::eR32G32B32Sfloat,
              .offset = offsetof(Vertex, pos)},
             {.location = 1,
              .binding = 0,
              .format = vk::Format::eR32G32B32Sfloat,
              .offset = offsetof(Vertex, color)},
             {              .location = 2,
              .binding = 0,
              .format = vk::Format::eR32G32Sfloat,
              .offset = offsetof(Vertex, uv)}}};
  }
};
