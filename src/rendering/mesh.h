#pragma once

#include <cstdint>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

struct Mesh {
  vk::raii::Buffer vertexBuffer{nullptr};
  vk::raii::DeviceMemory vertexMemory{nullptr};
  vk::raii::Buffer indexBuffer{nullptr};
  vk::raii::DeviceMemory indexMemory{nullptr};
  uint32_t indexCount = 0;
};
