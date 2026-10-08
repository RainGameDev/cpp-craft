#include "voxels/chunk_generator.h"
#include "voxels/biome.h"
#include "voxels/chunk.h"
#include "voxels/voxel_registry.h"
#include "voxels/world.h"
#include <FastNoise/FastNoise.h>
#include <algorithm>
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

namespace {
float terraceSurface(float surface, float strength) {
  constexpr float terraceStep = 7.0f;
  float terraced = std::floor(surface / terraceStep) * terraceStep;
  return surface + (terraced - surface) * strength;
}
} // namespace

void generateChunkTerrain(Chunk &chunk, const VoxelRegistry &registry,
                          const BiomeRegistry &biomes, const World &world) {

  const int baseY = chunk.pos.y * CH;

  const VoxelId sandId = registry.idFromString("game:sand");
  const VoxelId waterId = registry.idFromString("game:water");
  const VoxelId clayId = registry.idFromString("game:clay");
  const VoxelId snowId = registry.idFromString("game:snow");

  std::vector<const Biome *> biomeList;
  biomeList.reserve(biomes.size());
  for (const auto &entry : biomes.all())
    biomeList.push_back(&entry.second);
  if (biomeList.empty())
    return;
  std::vector<float> weightBuf(biomeList.size(), 0.0f);

  std::vector<std::array<VoxelId, 3>> idsPerBiome;
  idsPerBiome.reserve(biomeList.size());
  for (auto *biome : biomeList)
    idsPerBiome.push_back({registry.idFromString(biome->topBlock),
                           registry.idFromString(biome->subBlock),
                           registry.idFromString(biome->stoneBlock)});

  const int seaLocalY = (int)std::floor(SEA_LEVEL) - baseY;
  const int seaClamped = std::min(seaLocalY, CH - 1);

  for (int localZ = 0; localZ < CD; ++localZ) {
    for (int localX = 0; localX < CW; ++localX) {
      const int worldX = chunk.pos.x * CW + localX;
      const int worldZ = chunk.pos.z * CD + localZ;
      const float fx = (float)worldX;
      const float fz = (float)worldZ;

      //  Landform and climate
      const World::Column column = world.sampleColumn(fx, fz);
      const float land = column.land;
      const float detail = column.detail;
      const float temperature = column.temperature;
      const float humidity = column.humidity;

      // biome claims
      for (size_t index = 0; index < biomeList.size(); ++index)
        weightBuf[index] =
            biomes.weightFor(*biomeList[index], temperature, humidity, land);
      BiomeRegistry::sharpen(weightBuf);

      size_t bestIndex = 0;
      for (size_t index = 1; index < biomeList.size(); ++index)
        if (weightBuf[index] > weightBuf[bestIndex])
          bestIndex = index;
      auto [topId, subId, stoneId] = idsPerBiome[bestIndex];

      // Blend the terrain modifiers
      float heightOffset = 0.0f;
      float roughness = 0.0f;
      float terrace = 0.0f;
      float subDepth = 0.0f;
      for (size_t index = 0; index < biomeList.size(); ++index) {
        const float weight = weightBuf[index];
        heightOffset += weight * biomeList[index]->heightOffset;
        roughness += weight * biomeList[index]->roughness;
        terrace += weight * biomeList[index]->terrace;
        subDepth += weight * (float)biomeList[index]->subDepth;
      }

      float surface = land + heightOffset + detail * roughness;
      if (terrace > 0.0f)
        surface = terraceSurface(surface, terrace);

      surface = std::max(surface, MIN_SURFACE_Y);

      if (surface < SEA_LEVEL) {
        // A shallow shelf reads as sand; the deeps are bare clay.
        topId = (surface > SEA_LEVEL - 6.0f) ? sandId : clayId;
        subId = topId;
      } else if (surface <= SEA_LEVEL + 3.0f) {
        topId = sandId;
        subId = sandId;
      }
      if (surface >= SNOW_LINE) {
        topId = snowId;
        subId = snowId;
      }

      const int topLocalY = (int)std::floor(surface) - baseY;
      const float subBottom = surface - (1.0f + subDepth);
      const int subBotLocalY = (int)std::floor(subBottom) - baseY;
      const int solidTop = std::clamp(topLocalY, -1, CH - 1);

      for (int localY = 0; localY <= solidTop; ++localY) {
        const int worldY = baseY + localY;
        VoxelId id = (localY == topLocalY)
                         ? topId
                         : (localY > subBotLocalY ? subId : stoneId);
        // Carving starts a few blocks down so the surface keeps a solid roof.
        if (worldY < surface - 3.0f) {
          const float fy = (float)worldY;
          if (std::abs(world.caveAt(fx, fy * 1.8f, fz)) < 0.08f &&
              std::abs(world.caveAt2(fx, fy * 1.8f, fz)) < 0.08f)
            id = AIR;
          else if (worldY < surface - 6.0f &&
                   world.blobAt(fx, fy * 1.2f, fz) > 0.80f)
            id = AIR;
        }
        chunk.setLocal(localX, localY, localZ, id);
      }

      //
      const int waterBottom = std::max(0, topLocalY + 1);
      for (int localY = waterBottom; localY <= seaClamped; ++localY)
        chunk.setLocal(localX, localY, localZ, waterId);
    }
  }
}
