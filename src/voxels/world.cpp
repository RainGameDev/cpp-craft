#include "voxels/world.h"
#include "voxels/chunk_generator.h"
#include "voxels/voxel_registry.h"

VoxelId World::getBlock(int worldX, int worldY, int worldZ) const {
  ChunkPos chunkPos = toChunkPos(worldX, worldY, worldZ);
  const Chunk *chunk = getChunk(chunkPos);
  if (!chunk)
    return AIR;
  return chunk->getLocal(floorMod(worldX, CW), floorMod(worldY, CH),
                         floorMod(worldZ, CD));
}

void World::markNeighborDirty(ChunkPos chunkPos) {
  Chunk *neighbor = getChunk(chunkPos);
  if (neighbor)
    neighbor->dirty = true;
}

Chunk *World::ensureChunk(ChunkPos chunkPos, const VoxelRegistry &registry) {
  if (Chunk *existingChunk = getChunk(chunkPos))
    return existingChunk;
  if (knownEmptyChunks.contains(chunkPos))
    return nullptr;

  Chunk &chunk = getOrCreateChunk(chunkPos);
  generateChunkTerrain(chunk, registry);
  chunk.generated = true;

  bool anySolid = false;
  for (VoxelId voxelId : chunk.voxels) {
    if (voxelId != AIR) {
      anySolid = true;
      break;
    }
  }

  if (!anySolid) {
    storedChunks.erase(chunkPos);
    knownEmptyChunks.insert(chunkPos);
    return nullptr;
  }

  // mark neighbours as dirty
  chunk.dirty = true;
  markNeighborDirty({chunkPos.x - 1, chunkPos.y, chunkPos.z});
  markNeighborDirty({chunkPos.x + 1, chunkPos.y, chunkPos.z});
  markNeighborDirty({chunkPos.x, chunkPos.y - 1, chunkPos.z});
  markNeighborDirty({chunkPos.x, chunkPos.y + 1, chunkPos.z});
  markNeighborDirty({chunkPos.x, chunkPos.y, chunkPos.z - 1});
  markNeighborDirty({chunkPos.x, chunkPos.y, chunkPos.z + 1});
  return &chunk;
}

void World::evictEmptyBeyond(ChunkPos playerChunk, int radius) {
  for (auto emptyIterator = knownEmptyChunks.begin();
       emptyIterator != knownEmptyChunks.end();) {
    if (horizontalDistance(*emptyIterator, playerChunk) > radius)
      emptyIterator = knownEmptyChunks.erase(emptyIterator);
    else
      ++emptyIterator;
  }
}

void World::setBlock(int worldX, int worldY, int worldZ, VoxelId voxelId) {
  ChunkPos chunkPos = toChunkPos(worldX, worldY, worldZ);
  knownEmptyChunks.erase(chunkPos);
  Chunk &chunk = getOrCreateChunk(chunkPos);
  int localX = floorMod(worldX, CW), localY = floorMod(worldY, CH),
      localZ = floorMod(worldZ, CD);
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
