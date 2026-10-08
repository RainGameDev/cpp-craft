#pragma once
#include "FastNoise/Generators/Simplex.h"
#include "voxels/chunk.h"
#include "voxels/voxel_def.h"
#include <FastNoise/FastNoise.h>
#include <array>
#include <cstdint>
#include <vector>

class VoxelRegistry;
class BiomeRegistry;
class World;
struct Biome;

int chebyshevDistance(ChunkPos first, ChunkPos second);
int horizontalDistance(ChunkPos first, ChunkPos second);

// Ring of chunk columns at exactly `radius` from `center`Radius 0 returns
// just the center.
std::vector<ChunkPos> spiralAround(ChunkPos center, int radius);

struct SurfaceInfo {
  float surfaceY = 0.0f;
  float temperature = 0.0f; // climate at the column, for decoration gating
  float humidity = 0.0f;
  VoxelId topId = AIR;
  VoxelId subId = AIR;
  VoxelId stoneId = AIR;
  float subDepth = 0.0f;
  const Biome *biome = nullptr;
  size_t biomeIndex = 0;
};

class SurfaceSampler {
public:
  SurfaceSampler(const VoxelRegistry &registry, const BiomeRegistry &biomes,
                 const World &world);

  bool valid() const { return !biomeList.empty(); }

  SurfaceInfo at(int worldX, int worldZ) const;

private:
  const VoxelRegistry &registry;
  const BiomeRegistry &biomes;
  const World &world;
  std::vector<const Biome *> biomeList;
  std::vector<std::array<VoxelId, 6>> idsPerBiome;
  VoxelId snowId = AIR;
};

uint32_t hash2(int x, int z, int seed);
// Maps hash bits to [0, 1).
float hashU01(uint32_t h);

void decorationCandidateForCell(int cellX, int cellZ, int cellSize, int seed,
                                int &outX, int &outZ, uint32_t &outRollHash);

bool isDecorationOrigin(int worldX, int worldZ, int cellSize, int seed,
                        float density);

void generateChunkTerrain(Chunk &, const VoxelRegistry &, const BiomeRegistry &,
                          const World &);
