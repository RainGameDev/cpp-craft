#pragma once
#include "voxels/chunk.h"
#include "voxels/voxel_def.h"
#include <unordered_map>

// Integer floor division that handles negatives
inline int floorDiv(int value, int divisor) {
  int quotient = value / divisor;
  int remainder = value % divisor;
  if ((remainder != 0) && ((remainder < 0) != (divisor < 0)))
    --quotient;
  return quotient;
}
inline int floorMod(int value, int divisor) {
  return value - floorDiv(value, divisor) * divisor;
}

class World {
  std::unordered_map<ChunkPos, Chunk, ChunkPosHash> storedChunks;

  void markNeighborDirty(ChunkPos chunkPos);

public:
  static ChunkPos toChunkPos(int worldX, int worldY, int worldZ) {
    return {.x = floorDiv(worldX, CW),
            .y = floorDiv(worldY, CH),
            .z = floorDiv(worldZ, CD)};
  }

  bool hasChunk(ChunkPos chunkPos) const {
    return storedChunks.contains(chunkPos);
  }
  Chunk *getChunk(ChunkPos chunkPos) {
    auto foundChunk = storedChunks.find(chunkPos);
    return foundChunk != storedChunks.end() ? &foundChunk->second : nullptr;
  }
  const Chunk *getChunk(ChunkPos chunkPos) const {
    auto foundChunk = storedChunks.find(chunkPos);
    return foundChunk != storedChunks.end() ? &foundChunk->second : nullptr;
  }
  Chunk &getOrCreateChunk(ChunkPos chunkPos) {
    auto [chunkIterator, wasInserted] = storedChunks.try_emplace(chunkPos);
    chunkIterator->second.pos = chunkPos;
    return chunkIterator->second;
  }
  void removeChunk(ChunkPos chunkPos) { storedChunks.erase(chunkPos); }
  auto &allChunks() { return storedChunks; }
  const auto &allChunks() const { return storedChunks; }

  // AIR when chunk is missing (treat unloaded as empty for meshing).
  VoxelId getBlock(int worldX, int worldY, int worldZ) const;
  void setBlock(int worldX, int worldY, int worldZ, VoxelId voxelId);
};
