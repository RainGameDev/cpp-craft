#pragma once
#include "FastNoise/Generators/Simplex.h"
#include "voxels/chunk.h"
#include "voxels/voxel_def.h"
#include <FastNoise/FastNoise.h>
#include <unordered_map>
#include <unordered_set>

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

class VoxelRegistry;

class World {
  std::unordered_map<ChunkPos, Chunk, ChunkPosHash> storedChunks;
  // Positions proven to generate all-air. Missing chunks already read as
  // AIR, so these are remembered instead of stored to bound memory.
  std::unordered_set<ChunkPos, ChunkPosHash> knownEmptyChunks;

  int seed;
  FastNoise::SmartNode<FastNoise::Simplex> simplex =
      FastNoise::New<FastNoise::Simplex>();

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
  Chunk *ensureChunk(ChunkPos chunkPos, const VoxelRegistry &registry);
  bool isKnownEmpty(ChunkPos chunkPos) const {
    return knownEmptyChunks.contains(chunkPos);
  }
  size_t chunkCount() const { return storedChunks.size(); }
  size_t knownEmptyCount() const { return knownEmptyChunks.size(); }
  void evictEmpty(ChunkPos chunkPos) { knownEmptyChunks.erase(chunkPos); }
  void evictEmptyBeyond(ChunkPos playerChunk, int radius);
  void removeChunk(ChunkPos chunkPos) { storedChunks.erase(chunkPos); }
  auto &allChunks() { return storedChunks; }
  const auto &allChunks() const { return storedChunks; }

  // AIR when chunk is missing (treat unloaded as empty for meshing).
  VoxelId getBlock(int worldX, int worldY, int worldZ) const;
  void setBlock(int worldX, int worldY, int worldZ, VoxelId voxelId);
};
