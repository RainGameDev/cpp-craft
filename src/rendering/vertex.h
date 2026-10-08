#pragma once

#include <array>
#include <cstdint>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

struct Vertex {
  uint32_t data0 = 0;
  uint32_t data1 = 0;

  static Vertex pack(uint32_t x, uint32_t y, uint32_t z, uint32_t face,
                     uint32_t tile, uint32_t corner) {
    return {.data0 = (x & 63u) | ((y & 63u) << 6) | ((z & 63u) << 12) |
                     ((face & 7u) << 18),
            .data1 = (tile & 0x3FFFu) | ((corner & 3u) << 14)};
  }

  static vk::VertexInputBindingDescription getBindingDescription() {
    return {.binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = vk::VertexInputRate::eVertex};
  }

  static std::array<vk::VertexInputAttributeDescription, 2>
  getAttributeDescriptions() {
    return {{{.location = 0,
              .binding = 0,
              .format = vk::Format::eR32Uint,
              .offset = offsetof(Vertex, data0)},
             {.location = 1,
              .binding = 0,
              .format = vk::Format::eR32Uint,
              .offset = offsetof(Vertex, data1)}}};
  }
};

struct ChunkPush {
  int32_t ox = 0, oy = 0, oz = 0;
  int32_t cols = 1, rows = 1;
};
