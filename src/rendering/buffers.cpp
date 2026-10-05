#include "rendering/vulkan_application.h"
#include "voxels/chunk_mesher.h"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

void VulkanApplication::createBuffer(vk::DeviceSize size,
                                     vk::BufferUsageFlags usage,
                                     vk::MemoryPropertyFlags properties,
                                     vk::raii::Buffer &buffer,
                                     vk::raii::DeviceMemory &bufferMemory) {
  vk::BufferCreateInfo bufferInfo{
      .size = size, .usage = usage, .sharingMode = vk::SharingMode::eExclusive};
  buffer = vk::raii::Buffer(device, bufferInfo);

  vk::MemoryRequirements memoryRequirements = buffer.getMemoryRequirements();

  vk::MemoryAllocateInfo allocInfo{
      .allocationSize = memoryRequirements.size,
      .memoryTypeIndex =
          findMemoryType(memoryRequirements.memoryTypeBits, properties)};

  bufferMemory = vk::raii::DeviceMemory(device, allocInfo);
  buffer.bindMemory(*bufferMemory, 0);
}

void VulkanApplication::copyBuffer(vk::raii::Buffer &srcBuffer,
                                   vk::raii::Buffer &dstBuffer,
                                   vk::DeviceSize size) {
  vk::CommandBufferAllocateInfo allocInfo{.commandPool = *commandPool,
                                          .level =
                                              vk::CommandBufferLevel::ePrimary,
                                          .commandBufferCount = 1};

  auto commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
  auto copyCommandBuffer = std::move(commandBuffers.front());

  copyCommandBuffer.begin(
      {.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

  vk::BufferCopy copyRegion{.srcOffset = 0, .dstOffset = 0, .size = size};
  copyCommandBuffer.copyBuffer(*srcBuffer, *dstBuffer, copyRegion);

  copyCommandBuffer.end();

  vk::SubmitInfo submitInfo{.commandBufferCount = 1,
                            .pCommandBuffers = &*copyCommandBuffer};

  queue.submit(submitInfo, nullptr);
  queue.waitIdle();
}

Mesh VulkanApplication::uploadMeshData(const MeshData &meshData) {
  Mesh mesh{};
  if (meshData.empty())
    return mesh;

  vk::DeviceSize vertexBufferSize = sizeof(Vertex) * meshData.vertices.size();
  vk::raii::Buffer vertexStagingBuffer{nullptr};
  vk::raii::DeviceMemory vertexStagingMemory{nullptr};
  createBuffer(vertexBufferSize, vk::BufferUsageFlagBits::eTransferSrc,
               vk::MemoryPropertyFlagBits::eHostVisible |
                   vk::MemoryPropertyFlagBits::eHostCoherent,
               vertexStagingBuffer, vertexStagingMemory);

  void *mappedVertices = vertexStagingMemory.mapMemory(0, vertexBufferSize);
  std::memcpy(mappedVertices, meshData.vertices.data(), vertexBufferSize);
  vertexStagingMemory.unmapMemory();

  createBuffer(vertexBufferSize,
               vk::BufferUsageFlagBits::eVertexBuffer |
                   vk::BufferUsageFlagBits::eTransferDst,
               vk::MemoryPropertyFlagBits::eDeviceLocal, mesh.vertexBuffer,
               mesh.vertexMemory);

  copyBuffer(vertexStagingBuffer, mesh.vertexBuffer, vertexBufferSize);

  vk::DeviceSize indexBufferSize = sizeof(uint32_t) * meshData.indices.size();
  vk::raii::Buffer indexStagingBuffer{nullptr};
  vk::raii::DeviceMemory indexStagingMemory{nullptr};
  createBuffer(indexBufferSize, vk::BufferUsageFlagBits::eTransferSrc,
               vk::MemoryPropertyFlagBits::eHostVisible |
                   vk::MemoryPropertyFlagBits::eHostCoherent,
               indexStagingBuffer, indexStagingMemory);

  void *mappedIndices = indexStagingMemory.mapMemory(0, indexBufferSize);
  std::memcpy(mappedIndices, meshData.indices.data(), indexBufferSize);
  indexStagingMemory.unmapMemory();

  createBuffer(indexBufferSize,
               vk::BufferUsageFlagBits::eIndexBuffer |
                   vk::BufferUsageFlagBits::eTransferDst,
               vk::MemoryPropertyFlagBits::eDeviceLocal, mesh.indexBuffer,
               mesh.indexMemory);

  copyBuffer(indexStagingBuffer, mesh.indexBuffer, indexBufferSize);

  mesh.indexCount = static_cast<uint32_t>(meshData.indices.size());
  return mesh;
}

void VulkanApplication::rebuildChunk(const ChunkPos &chunkPos) {
  Chunk *chunk = world.getChunk(chunkPos);
  if (!chunk) {
    chunkMeshes.erase(chunkPos);
    return;
  }
  MeshData meshData =
      meshChunk(*chunk, world, voxelRegistry, voxelAtlas.layout());
  if (meshData.empty()) {
    chunkMeshes.erase(chunkPos);
    chunk->dirty = false;
    return;
  }
  Mesh mesh = uploadMeshData(meshData);
  chunkMeshes.insert_or_assign(chunkPos, std::move(mesh));
  chunk->dirty = false;
}

void VulkanApplication::updateDirtyChunks() {
  for (auto &chunkEntry : world.allChunks()) {
    if (chunkEntry.second.dirty)
      rebuildChunk(chunkEntry.first);
  }
}
