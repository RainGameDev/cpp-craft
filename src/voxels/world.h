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
class BiomeRegistry;

class World {
  std::unordered_map<ChunkPos, Chunk, ChunkPosHash> storedChunks;
  std::unordered_set<ChunkPos, ChunkPosHash> knownEmptyChunks;

  int seed;
  FastNoise::SmartNode<FastNoise::Simplex> simplex =
      FastNoise::New<FastNoise::Simplex>();
  FastNoise::SmartNode<FastNoise::FractalFBm> tempNoise, humidityNoise;
  FastNoise::SmartNode<FastNoise::FractalFBm> caveNoise;
  FastNoise::SmartNode<FastNoise::FractalFBm> caveNoise2;
  FastNoise::SmartNode<FastNoise::FractalFBm> blobNoise;
  FastNoise::SmartNode<FastNoise::FractalFBm> entranceNoise;
  void markNeighborDirty(ChunkPos chunkPos);

public:
  float caveAt2(float x, float y, float z) const {
    return caveNoise2->GenSingle3D(x, y, z, seed);
  }

  float caveAt(float x, float y, float z) const {
    return caveNoise->GenSingle3D(x, y, z, seed);
  }

  float blobAt(float x, float y, float z) const {
    return blobNoise->GenSingle3D(x, y, z, seed);
  }

  float entranceAt(float x, float z) const {
    return sample(entranceNoise, x, z);
  }

  explicit World();
  int seedValue() const { return seed; }

  float sample(const FastNoise::SmartNode<FastNoise::Generator> &n, float x,
               float y) const {
    return n->GenSingle2D(x, y, seed);
  }
  float tempAt(float x, float y) const { return sample(tempNoise, x, y); }
  float humidityAt(float x, float y) const {
    return sample(humidityNoise, x, y);
  }
  float pointAt(float x, float y) const { return sample(simplex, x, y); }

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
  Chunk *ensureChunk(ChunkPos chunkPos, const VoxelRegistry &registry,
                     const BiomeRegistry &biomes);
  bool isKnownEmpty(ChunkPos chunkPos) const {
    return knownEmptyChunks.contains(chunkPos);
  }
  size_t chunkCount() const { return storedChunks.size(); }
  size_t knownEmptyCount() const { return knownEmptyChunks.size(); }
  void evictEmpty(ChunkPos chunkPos) { knownEmptyChunks.erase(chunkPos); }
  void evictEmptyBeyond(ChunkPos playerChunk, int radius);
  void markKnownEmpty(ChunkPos chunkPos) { knownEmptyChunks.insert(chunkPos); }
  // Insert a worker generated chunk on the main thread. Returns false if a
  // chunk (for example a player edit) or known empty marker covers this pos.
  bool publishGeneratedChunk(Chunk &&chunk);
  void removeChunk(ChunkPos chunkPos) { storedChunks.erase(chunkPos); }
  auto &allChunks() { return storedChunks; }
  const auto &allChunks() const { return storedChunks; }

  // AIR when chunk is missing (treat unloaded as empty for meshing).
  VoxelId getBlock(int worldX, int worldY, int worldZ) const;
  void setBlock(int worldX, int worldY, int worldZ, VoxelId voxelId);
};
