#include "voxels/world.h"
#include "voxels/biome.h"
#include "voxels/chunk_generator.h"
#include "voxels/voxel_registry.h"
#include <cstdlib>
#include <random>

World::World() {

  std::random_device rd;
  std::mt19937 gen(rd());
  int newSeed = gen();
  seed = newSeed;

  auto cSrc = FastNoise::New<FastNoise::Simplex>();
  cSrc->SetScale(40.0f);
  caveNoise = FastNoise::New<FastNoise::FractalFBm>();
  caveNoise->SetSource(cSrc);
  caveNoise->SetOctaveCount(2);

  auto cSrc2 = FastNoise::New<FastNoise::Simplex>();
  cSrc2->SetScale(20.0f);
  caveNoise2 = FastNoise::New<FastNoise::FractalFBm>();
  caveNoise2->SetSource(cSrc2);
  caveNoise2->SetOctaveCount(4);

  auto bSrc = FastNoise::New<FastNoise::Simplex>();
  bSrc->SetScale(10.0f);
  blobNoise = FastNoise::New<FastNoise::FractalFBm>();
  blobNoise->SetSource(bSrc);
  blobNoise->SetOctaveCount(4);

  auto eSrc = FastNoise::New<FastNoise::Simplex>();
  eSrc->SetScale(20.0f);
  entranceNoise = FastNoise::New<FastNoise::FractalFBm>();
  entranceNoise->SetSource(eSrc);
  entranceNoise->SetOctaveCount(2);

  auto heightSrc = FastNoise::New<FastNoise::Simplex>();
  simplex = FastNoise::New<FastNoise::Simplex>();
  simplex->SetScale(100.0f);

  auto tSrc = FastNoise::New<FastNoise::Simplex>();
  tSrc->SetScale(800.0f);
  tempNoise = FastNoise::New<FastNoise::FractalFBm>();
  tempNoise->SetSource(tSrc);
  tempNoise->SetOctaveCount(3);

  auto hSrc = FastNoise::New<FastNoise::Simplex>();
  hSrc->SetScale(800.0f);
  humidityNoise = FastNoise::New<FastNoise::FractalFBm>();
  humidityNoise->SetSource(hSrc);
  humidityNoise->SetOctaveCount(3);
}

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
    neighbor->markDirty();
}

Chunk *World::ensureChunk(ChunkPos chunkPos, const VoxelRegistry &registry,
                          const BiomeRegistry &biomes) {
  if (Chunk *existingChunk = getChunk(chunkPos))
    return existingChunk;
  if (knownEmptyChunks.contains(chunkPos))
    return nullptr;

  Chunk &chunk = getOrCreateChunk(chunkPos);
  generateChunkTerrain(chunk, registry, biomes, *this);
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
  chunk.markDirty();
  markNeighborDirty({chunkPos.x - 1, chunkPos.y, chunkPos.z});
  markNeighborDirty({chunkPos.x + 1, chunkPos.y, chunkPos.z});
  markNeighborDirty({chunkPos.x, chunkPos.y - 1, chunkPos.z});
  markNeighborDirty({chunkPos.x, chunkPos.y + 1, chunkPos.z});
  markNeighborDirty({chunkPos.x, chunkPos.y, chunkPos.z - 1});
  markNeighborDirty({chunkPos.x, chunkPos.y, chunkPos.z + 1});
  return &chunk;
}

bool World::publishGeneratedChunk(Chunk &&chunk) {
  ChunkPos chunkPos = chunk.pos;
  if (storedChunks.contains(chunkPos) || knownEmptyChunks.contains(chunkPos))
    return false;
  bool anySolid = false;
  for (VoxelId voxelId : chunk.voxels) {
    if (voxelId != AIR) {
      anySolid = true;
      break;
    }
  }
  if (!anySolid) {
    knownEmptyChunks.insert(chunkPos);
    return false;
  }
  auto [it, inserted] = storedChunks.try_emplace(chunkPos, std::move(chunk));
  if (!inserted)
    return false;
  it->second.generated = true;
  it->second.markDirty();
  markNeighborDirty({chunkPos.x - 1, chunkPos.y, chunkPos.z});
  markNeighborDirty({chunkPos.x + 1, chunkPos.y, chunkPos.z});
  markNeighborDirty({chunkPos.x, chunkPos.y - 1, chunkPos.z});
  markNeighborDirty({chunkPos.x, chunkPos.y + 1, chunkPos.z});
  markNeighborDirty({chunkPos.x, chunkPos.y, chunkPos.z - 1});
  markNeighborDirty({chunkPos.x, chunkPos.y, chunkPos.z + 1});
  return true;
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
  chunk.markDirty();

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
