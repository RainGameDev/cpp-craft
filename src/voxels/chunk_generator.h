#pragma once
#include "FastNoise/Generators/Simplex.h"
#include "voxels/chunk.h"
#include <FastNoise/FastNoise.h>
#include <vector>

class VoxelRegistry;
class BiomeRegistry;
class World;
int chebyshevDistance(ChunkPos first, ChunkPos second);
int horizontalDistance(ChunkPos first, ChunkPos second);

// Ring of chunk columns at exactly `radius` from `center`Radius 0 returns
// just the center.
std::vector<ChunkPos> spiralAround(ChunkPos center, int radius);

void generateChunkTerrain(Chunk &, const VoxelRegistry &, const BiomeRegistry &,
                          const World &);
