#include "voxels/world.h"

VoxelId World::getBlock(int worldX, int worldY, int worldZ) const {
  ChunkPos chunkPos = toChunkPos(worldX, worldY, worldZ);
  const Chunk *chunk = getChunk(chunkPos);
  if (!chunk)
    return AIR;
  return chunk->getLocal(floorMod(worldX, CW), floorMod(worldY, CH), floorMod(worldZ, CD));
}

void World::markNeighborDirty(ChunkPos chunkPos) {
  Chunk *neighbor = getChunk(chunkPos);
  if (neighbor)
    neighbor->dirty = true;
}

void World::setBlock(int worldX, int worldY, int worldZ, VoxelId voxelId) {
  ChunkPos chunkPos = toChunkPos(worldX, worldY, worldZ);
  Chunk &chunk = getOrCreateChunk(chunkPos);
  int localX = floorMod(worldX, CW), localY = floorMod(worldY, CH), localZ = floorMod(worldZ, CD);
  if (chunk.getLocal(localX, localY, localZ) == voxelId)
    return;
  chunk.setLocal(localX, localY, localZ, voxelId);

  // Faces on the chunk border are shared with the neighbor's mesh.
  if (localX == 0)
    markNeighborDirty({chunkPos.x - 1, chunkPos.y, chunkPos.z});
  if (localX == CW - 1)
    markNeighborDirty({chunkPos.x + 1, chunkPos.y, chunkPos.z});
  if (localY == 0)
    markNeighborDirty({chunkPos.x, chunkPos.y - 1, chunkPos.z});
  if (localY == CH - 1)
    markNeighborDirty({chunkPos.x, chunkPos.y + 1, chunkPos.z});
  if (localZ == 0)
    markNeighborDirty({chunkPos.x, chunkPos.y, chunkPos.z - 1});
  if (localZ == CD - 1)
    markNeighborDirty({chunkPos.x, chunkPos.y, chunkPos.z + 1});
}
