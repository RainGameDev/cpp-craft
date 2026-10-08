#pragma once

#include <cstdint>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

struct Mesh {
  vk::raii::Buffer buffer{nullptr};
  vk::raii::DeviceMemory memory{nullptr};
  vk::DeviceSize vertexOffset = 0;
  vk::DeviceSize indexOffset = 0;
  uint32_t vertexCount = 0;
  uint32_t indexCount = 0;
  bool empty() const { return indexCount == 0 || !*buffer; }
};
