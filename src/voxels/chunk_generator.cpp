#include "voxels/chunk_generator.h"
#include "voxels/biome.h"
#include "voxels/chunk.h"
#include "voxels/voxel_registry.h"
#include "voxels/world.h"
#include <FastNoise/FastNoise.h>
#include <array>
#include <unordered_map>

int horizontalDistance(ChunkPos first, ChunkPos second) {
  int deltaX = first.x - second.x;
  int deltaZ = first.z - second.z;
  if (deltaX < 0)
    deltaX = -deltaX;
  if (deltaZ < 0)
    deltaZ = -deltaZ;
  return deltaX > deltaZ ? deltaX : deltaZ;
}

int chebyshevDistance(ChunkPos first, ChunkPos second) {
  int deltaX = first.x - second.x;
  int deltaY = first.y - second.y;
  int deltaZ = first.z - second.z;
  if (deltaX < 0)
    deltaX = -deltaX;
  if (deltaY < 0)
    deltaY = -deltaY;
  if (deltaZ < 0)
    deltaZ = -deltaZ;
  int widest = deltaX;
  if (deltaY > widest)
    widest = deltaY;
  if (deltaZ > widest)
    widest = deltaZ;
  return widest;
}

std::vector<ChunkPos> spiralAround(ChunkPos center, int radius) {
  std::vector<ChunkPos> ring;
  if (radius == 0) {
    ring.push_back(center);
    return ring;
  }
  ring.reserve((size_t)radius * 8);
  for (int deltaX = -radius; deltaX <= radius; ++deltaX) {
    for (int deltaZ = -radius; deltaZ <= radius; ++deltaZ) {
      int widest = deltaX < 0 ? -deltaX : deltaX;
      int depth = deltaZ < 0 ? -deltaZ : deltaZ;
      if (depth > widest)
        widest = depth;
      if (widest != radius)
        continue;
      ring.push_back({center.x + deltaX, center.y, center.z + deltaZ});
    }
  }
  return ring;
}

void generateChunkTerrain(Chunk &chunk, const VoxelRegistry &registry,
                          const BiomeRegistry &biomes, const World &world) {
  const int baseY = chunk.pos.y * CH;
  std::unordered_map<std::string, std::array<VoxelId, 3>> idCache;
  auto idsFor = [&](const Biome &b) {
    if (auto f = idCache.find(b.name); f != idCache.end())
      return f->second;
    std::array<VoxelId, 3> ids = {registry.idFromString(b.topBlock),
                                  registry.idFromString(b.subBlock),
                                  registry.idFromString(b.stoneBlock)};
    idCache.emplace(b.name, ids);
    return ids;
  };
  for (int localZ = 0; localZ < CD; ++localZ) {
    for (int localX = 0; localX < CW; ++localX) {
      int worldX = chunk.pos.x * CW + localX;
      int worldZ = chunk.pos.z * CD + localZ;
      const Biome &biome = biomes.pick(world.tempAt(worldX, worldZ),
                                       world.humidityAt(worldX, worldZ));
      float scaleK = biome.frequency * 100.0f;
      float surface =
          biome.baseHeight +
          world.pointAt(worldX * scaleK, worldZ * scaleK) * biome.amplitude;
      auto [topId, subId, stoneId] = idsFor(biome);
      for (int localY = 0; localY < CH; ++localY) {
        int worldY = baseY + localY;
        VoxelId id;
        if (worldY > surface)
          id = AIR;
        else if (worldY > surface - 1.0f)
          id = topId;
        else if (worldY > surface - (1.0f + biome.subDepth))
          id = subId;
        else
          id = stoneId;
        chunk.setLocal(localX, localY, localZ, id);
      }
    }
  }
}
