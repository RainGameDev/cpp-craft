#include "voxels/chunk_generator.h"
#include "voxels/biome.h"
#include "voxels/chunk.h"
#include "voxels/voxel_registry.h"
#include "voxels/world.h"
#include <FastNoise/FastNoise.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <unordered_map>
#include <vector>

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

  // Snapshot biome
  std::vector<const Biome *> biomeList;
  biomeList.reserve(biomes.size());
  for (const auto &entry : biomes.all())
    biomeList.push_back(&entry.second);
  std::vector<float> weightBuf(biomeList.size(), 0.0f);

  for (int localZ = 0; localZ < CD; ++localZ) {
    for (int localX = 0; localX < CW; ++localX) {
      int worldX = chunk.pos.x * CW + localX;
      int worldZ = chunk.pos.z * CD + localZ;
      float temp = world.tempAt((float)worldX, (float)worldZ);
      float humidity = world.humidityAt((float)worldX, (float)worldZ);

      // weights, normalized to sum to 1.
      float total = 0.0f;
      for (size_t i = 0; i < biomeList.size(); ++i) {
        float w = biomes.weightFor(*biomeList[i], temp, humidity);
        weightBuf[i] = w;
        total += w;
      }
      if (total > 0.0f) {
        for (float &w : weightBuf)
          w /= total;
      }

      // Blended heigh
      float surface = 0.0f;
      float blendedSubDepth = 0.0f;
      const Biome *dominant = biomeList.empty() ? nullptr : biomeList[0];
      float bestWeight = -1.0f;
      for (size_t i = 0; i < biomeList.size(); ++i) {
        const Biome *b = biomeList[i];
        float w = weightBuf[i];
        float scaleK = b->frequency * 100.0f;
        float height =
            b->baseHeight +
            world.pointAt(worldX * scaleK, worldZ * scaleK) * b->amplitude;
        surface += w * height;
        blendedSubDepth += w * (float)b->subDepth;
        if (w > bestWeight) {
          bestWeight = w;
          dominant = b;
        }
      }
      if (!dominant)
        continue;
      auto [topId, subId, stoneId] = idsFor(*dominant);
      float subBottom = surface - (1.0f + blendedSubDepth);
      for (int localY = 0; localY < CH; ++localY) {
        int worldY = baseY + localY;
        VoxelId id;

        if (worldY > surface)
          id = AIR;
        else if (worldY > surface - 1.0f)
          id = topId;
        else if (worldY > subBottom)
          id = subId;
        else
          id = stoneId;

        if (id != AIR && worldY < surface - 3) {
          float fx = (float)worldX;
          float fy = (float)worldY;
          float fz = (float)worldZ;
          float n1 = world.caveAt(fx, fy * 1.8f, fz);
          float n2 = world.caveAt2(fx, fy * 1.8f, fz);
          constexpr float tunnelW = 0.07f;
          bool tunnel = std::abs(n1) < tunnelW && std::abs(n2) < tunnelW;
          if (tunnel) {
            id = AIR;
          } else if (worldY < surface - 6) {
            float n3 = world.blobAt(fx, fy * 1.2f, fz);
            if (n3 > 0.62f)
              id = AIR;
          }
        }
        chunk.setLocal(localX, localY, localZ, id);
      }
    }
  }
}
