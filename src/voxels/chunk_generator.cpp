#include "voxels/chunk_generator.h"
#include "FastNoise/Generators/Simplex.h"
#include "voxels/chunk.h"
#include "voxels/voxel_registry.h"
#include <FastNoise/FastNoise.h>

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
                          FastNoise::SmartNode<FastNoise::Simplex> &simplex) {

  simplex.get()->SetScale(5);
  VoxelId dirtId = registry.idFromString("game:dirt");
  VoxelId grassId = registry.idFromString("game:grass");
  VoxelId stoneId = registry.idFromString("game:stone");
  const int baseY = chunk.pos.y * CH;
  for (int localY = 0; localY < CH; ++localY) {
    for (int localZ = 0; localZ < CD; ++localZ) {
      for (int localX = 0; localX < CW; ++localX) {
        int worldX = chunk.pos.x * CD + localX;
        int worldZ = chunk.pos.z * CW + localZ;
        int worldY = baseY + localY;

        float heightVariation =
            simplex.get()->GenSingle2D(worldX * 0.05, worldZ * 0.05, 1) * 7.0;

        if (worldY > (10.0 + heightVariation))
          chunk.setLocal(localX, localY, localZ, 0);
        else
          chunk.setLocal(localX, localY, localZ, grassId);
      }
    }
  }
}
