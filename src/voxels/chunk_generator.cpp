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
  std::vector<const Biome *> biomeList;
  biomeList.reserve(biomes.size());
  for (const auto &entry : biomes.all())
    biomeList.push_back(&entry.second);
  std::vector<float> weightBuf(biomeList.size(), 0.0f);

  std::vector<std::array<VoxelId, 3>> idsPerBiome;
  idsPerBiome.reserve(biomeList.size());
  for (auto *b : biomeList)
    idsPerBiome.push_back({registry.idFromString(b->topBlock),
                           registry.idFromString(b->subBlock),
                           registry.idFromString(b->stoneBlock)});

  // Snapshot biome
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
      size_t bestIdx = 0;
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
          bestIdx = i;
        }
      }
      if (!dominant)
        continue;
      auto [topId, subId, stoneId] = idsPerBiome[bestIdx];
      float subBottom = surface - (1.0f + blendedSubDepth);
      const int topLocalY = (int)std::floor(surface) - baseY;
      const int subBotLocalY = (int)std::floor(subBottom) - baseY;
      const int topClamped =
          topLocalY < -1 ? -1 : (topLocalY > CH - 1 ? CH - 1 : topLocalY);
      const float fx = (float)worldX, fz = (float)worldZ;
      for (int localY = 0; localY <= topClamped; ++localY) {
        const int worldY = baseY + localY;
        VoxelId id = (localY == topLocalY)
                         ? topId
                         : (localY > subBotLocalY ? subId : stoneId);
        if (worldY < surface - 3) {
          const float fy = (float)worldY;
          if (std::abs(world.caveAt(fx, fy * 1.8f, fz)) < 0.08f &&
              std::abs(world.caveAt2(fx, fy * 1.8f, fz)) < 0.08f)
            id = AIR;
          else if (worldY < surface - 6 &&
                   world.blobAt(fx, fy * 1.2f, fz) > 0.80f)
            id = AIR;
        }
        chunk.setLocal(localX, localY, localZ, id);
      }
    }
  }
}
