#pragma once
#include "voxels/chunk.h"
#include <vector>

class VoxelRegistry;
class World;
int chebyshevDistance(ChunkPos first, ChunkPos second);
int horizontalDistance(ChunkPos first, ChunkPos second);

// Ring of chunk columns at exactly `radius` from `center`Radius 0 returns
// just the center.
std::vector<ChunkPos> spiralAround(ChunkPos center, int radius);
void generateChunkTerrain(Chunk &chunk, const VoxelRegistry &registry);
