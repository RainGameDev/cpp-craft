#include "rendering/vulkan_application.h"
#include "voxels/chunk_mesher.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstring>
#include <mutex>
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

  vk::raii::Fence copyFence(device, vk::FenceCreateInfo{});
  vk::SubmitInfo submitInfo{.commandBufferCount = 1,
                            .pCommandBuffers = &*copyCommandBuffer};

  queue.submit(submitInfo, *copyFence);
  std::ignore = device.waitForFences(*copyFence, vk::True, UINT64_MAX);
}

Mesh VulkanApplication::uploadMeshData(const MeshData &meshData) {
  Mesh mesh{};
  if (meshData.empty())
    return mesh;

  const vk::DeviceSize vertSize = sizeof(Vertex) * meshData.vertices.size();
  const vk::DeviceSize idxSize = sizeof(uint32_t) * meshData.indices.size();
  mesh.indexOffset = (vertSize + 3) & ~vk::DeviceSize(3);
  const vk::DeviceSize total = mesh.indexOffset + idxSize;
  if (total > stagingSize)
    return mesh; // oversize fallback skip or temp path

  memcpy(stagingMapped, meshData.vertices.data(), vertSize);
  memcpy(static_cast<char *>(stagingMapped) + mesh.indexOffset,
         meshData.indices.data(), idxSize);

  createBuffer(total,
               vk::BufferUsageFlagBits::eVertexBuffer |
                   vk::BufferUsageFlagBits::eIndexBuffer |
                   vk::BufferUsageFlagBits::eTransferDst,
               vk::MemoryPropertyFlagBits::eDeviceLocal, mesh.buffer,
               mesh.memory);

  // submit
  vk::CommandBufferAllocateInfo alloc{.commandPool = *commandPool,
                                      .level = vk::CommandBufferLevel::ePrimary,
                                      .commandBufferCount = 1};
  auto cb = vk::raii::CommandBuffers(device, alloc);
  cb.front().begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  const std::array<vk::BufferCopy, 2> regions{{
      {.srcOffset = 0, .dstOffset = mesh.vertexOffset, .size = vertSize},
      {.srcOffset = mesh.indexOffset,
       .dstOffset = mesh.indexOffset,
       .size = idxSize},
  }};
  cb.front().copyBuffer(*stagingBuffer, *mesh.buffer, regions);
  cb.front().end();

  vk::raii::Fence f(device, vk::FenceCreateInfo{});
  vk::CommandBuffer raw = *cb.front();
  queue.submit(vk::SubmitInfo{.commandBufferCount = 1, .pCommandBuffers = &raw},
               *f);
  std::ignore = device.waitForFences(*f, vk::True, UINT64_MAX);

  mesh.vertexCount = (uint32_t)meshData.vertices.size();
  mesh.indexCount = (uint32_t)meshData.indices.size();
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
    chunk->markClean();
    return;
  }
  Mesh mesh = uploadMeshData(meshData);
  chunkMeshes.insert_or_assign(chunkPos, std::move(mesh));
  chunk->markClean();
}

void VulkanApplication::kickMeshJobs() {
  static constexpr ChunkPos OFFSETS[6] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                                          {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
  static constexpr int KICK_BUDGET = 16; // snapshots/frame; bounds memcpy
  AtlasLayout layout = voxelAtlas.layout();
  const ChunkPos center = currentPlayerChunk;

  // Gather dirty chunks closest first so the circle around the player
  // meshes before distant columns.
  std::vector<ChunkPos> dirty;
  dirty.reserve(64);
  for (auto &kv : world.allChunks()) {
    if (!kv.second.dirty)
      continue;
    const ChunkPos pos = kv.first;
    {
      std::lock_guard<std::mutex> lock(meshMutex);
      if (meshInFlight.contains(pos))
        continue;
    }
    dirty.push_back(pos);
  }
  std::sort(dirty.begin(), dirty.end(),
            [&center](const ChunkPos &a, const ChunkPos &b) {
              int adx = a.x - center.x, ady = a.y - center.y,
                  adz = a.z - center.z;
              int bdx = b.x - center.x, bdy = b.y - center.y,
                  bdz = b.z - center.z;
              return adx * adx + ady * ady + adz * adz <
                     bdx * bdx + bdy * bdy + bdz * bdz;
            });

  int kicked = 0;
  for (const ChunkPos pos : dirty) {
    if (kicked >= KICK_BUDGET)
      break;
    Chunk *chunk = world.getChunk(pos);
    if (!chunk || !chunk->dirty)
      continue;
    {
      std::lock_guard<std::mutex> lock(meshMutex);
      if (meshInFlight.contains(pos))
        continue;
      meshInFlight.insert(pos);
    }

    // Snapshot on main thread
    ChunkMeshSnapshot snapshot;
    snapshot.central = *chunk;
    const uint64_t version = chunk->version;
    for (int i = 0; i < 6; ++i) {
      ChunkPos neighborPos{pos.x + OFFSETS[i].x, pos.y + OFFSETS[i].y,
                           pos.z + OFFSETS[i].z};
      if (Chunk *neighbor = world.getChunk(neighborPos))
        snapshot.neighbors[i] = *neighbor;
    }

    threadPool.enqueue([this, snapshot = std::move(snapshot), pos, version,
                        layout]() mutable {
      MeshData meshData = meshChunkSnapshot(snapshot, voxelRegistry, layout);
      std::lock_guard<std::mutex> lock(meshMutex);
      completedMeshes.push_back(
          CompletedMesh{pos, std::move(meshData), version});
    });
    ++kicked;
  }
}

void VulkanApplication::drainCompletedMeshes(int budget) {
  for (int i = 0; i < budget; ++i) {
    // Upload closest first: a far worker finishing early must not jump
    // ahead of the player's own chunk.
    CompletedMesh job;
    {
      std::lock_guard<std::mutex> lock(meshMutex);
      if (completedMeshes.empty())
        return;
      size_t best = 0;
      long bestDist = LONG_MAX;
      for (size_t idx = 0; idx < completedMeshes.size(); ++idx) {
        const ChunkPos &pp = completedMeshes[idx].pos;
        int dx = pp.x - currentPlayerChunk.x, dy = pp.y - currentPlayerChunk.y,
            dz = pp.z - currentPlayerChunk.z;
        long d = (long)dx * dx + (long)dy * dy + (long)dz * dz;
        if (d < bestDist) {
          bestDist = d;
          best = idx;
        }
      }
      job = std::move(completedMeshes[best]);
      completedMeshes.erase(completedMeshes.begin() + (long)best);
    }

    Chunk *chunk = world.getChunk(job.pos);
    if (!chunk) {
      chunkMeshes.erase(job.pos);
      std::lock_guard<std::mutex> lock(meshMutex);
      meshInFlight.erase(job.pos);
      continue;
    }
    if (chunk->version != job.version) {
      // Edited after the snapshot
      std::lock_guard<std::mutex> lock(meshMutex);
      meshInFlight.erase(job.pos);
      continue;
    }
    if (job.data.empty()) {
      chunkMeshes.erase(job.pos);
    } else {
      Mesh mesh = uploadMeshData(job.data);
      chunkMeshes.insert_or_assign(job.pos, std::move(mesh));
    }
    chunk->markClean();
    {
      std::lock_guard<std::mutex> lock(meshMutex);
      meshInFlight.erase(job.pos);
    }
  }
}

void VulkanApplication::dropMeshJobsFor(const ChunkPos &chunkPos) {
  std::lock_guard<std::mutex> lock(meshMutex);
  meshInFlight.erase(chunkPos);
  std::erase_if(completedMeshes,
                [&](const CompletedMesh &m) { return m.pos == chunkPos; });
}

void VulkanApplication::updateDirtyChunks() {
  kickMeshJobs();
  drainCompletedMeshes(4);
}

void VulkanApplication::createStagingBuffer() {
  createBuffer(stagingSize, vk::BufferUsageFlagBits::eTransferSrc,
               vk::MemoryPropertyFlagBits::eHostVisible |
                   vk::MemoryPropertyFlagBits::eHostCoherent,
               stagingBuffer, stagingMemory);
  stagingMapped = stagingMemory.mapMemory(0, stagingSize);
}
