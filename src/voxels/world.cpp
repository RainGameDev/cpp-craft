#include "voxels/world.h"
#include "voxels/biome.h"
#include "voxels/chunk_generator.h"
#include "voxels/voxel_registry.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <random>

namespace {
constexpr float fbmRange(int octaves) {
  float range = 0.0f;
  float amplitude = 1.0f;
  for (int octave = 0; octave < octaves; ++octave) {
    range += amplitude;
    amplitude *= 0.5f;
  }
  return range;
}
constexpr float RANGE_3 = fbmRange(3);
constexpr float RANGE_4 = fbmRange(4);

// Smooth gate for province / uplift masks.
inline float smoothstep(float edge0, float edge1, float x) {
  float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}
// Mountain province tuning
constexpr float RANGE_LO = 0.52f; // below this: no ranges, plains survive
constexpr float RANGE_HI = 0.72f; // above this: full uplift, tall chains
// Continental uplift
constexpr float CONT_LO = 0.12f; // coasts stay flat
constexpr float CONT_HI = 0.60f; // interiors take full ridges

// Landform tuning Height is express
constexpr float LAND_SCALE = 48.0f;  // +Y added at the wettest continent
constexpr float OCEAN_SCALE = 24.0f; // -Y dropped at the deepest ocean
constexpr float MOUNTAIN_SCALE =
    165.0f;                          // height of a fully uplifted ridge crest
constexpr float DETAIL_SCALE = 9.0f; // rolling hills and hollows
constexpr float WARP_AMPLITUDE = 180.0f; // how far borders get pushed sideways
constexpr float EROSION_FLAT = 0.15f;    // below this, full relief survives
constexpr float EROSION_WIDTH = 0.40f;   // ramp up to fully worn flat

// Climate tuning
constexpr float CLIMATE_GAIN = 1.2f;
constexpr float LAPSE_RATE = 0.012f;      // Y per unit of cooling
constexpr float SNOW_ALTITUDE = 38.0f;    // cooling starts here
constexpr float OROGRAPHIC_RATE = 0.008f; // drying per unit of altitude
constexpr float DRY_ALTITUDE = 42.0f;
constexpr float CONTINENT_BIAS = 0.60f;
} // namespace

World::World() {

  std::random_device rd;
  std::mt19937 gen(rd());
  int newSeed = gen();
  seed = newSeed;

  auto cSrc = FastNoise::New<FastNoise::Simplex>();
  cSrc->SetScale(90.0f);
  caveNoise = FastNoise::New<FastNoise::FractalFBm>();
  caveNoise->SetSource(cSrc);
  caveNoise->SetOctaveCount(2);

  auto cSrc2 = FastNoise::New<FastNoise::Simplex>();
  cSrc2->SetScale(50.0f);
  caveNoise2 = FastNoise::New<FastNoise::FractalFBm>();
  caveNoise2->SetSource(cSrc2);
  caveNoise2->SetOctaveCount(4);

  auto bSrc = FastNoise::New<FastNoise::Simplex>();
  bSrc->SetScale(25.0f);
  blobNoise = FastNoise::New<FastNoise::FractalFBm>();
  blobNoise->SetSource(bSrc);
  blobNoise->SetOctaveCount(4);

  auto eSrc = FastNoise::New<FastNoise::Simplex>();
  eSrc->SetScale(50.0f);
  entranceNoise = FastNoise::New<FastNoise::FractalFBm>();
  entranceNoise->SetSource(eSrc);
  entranceNoise->SetOctaveCount(2);

  simplex = FastNoise::New<FastNoise::Simplex>();
  simplex->SetScale(250.0f);

  auto wSrc = FastNoise::New<FastNoise::Simplex>();
  wSrc->SetScale(1500.0f);
  warpNoise = FastNoise::New<FastNoise::FractalFBm>();
  warpNoise->SetSource(wSrc);
  warpNoise->SetOctaveCount(3);

  auto kSrc = FastNoise::New<FastNoise::Simplex>();
  kSrc->SetScale(3800.0f);
  continentalNoise = FastNoise::New<FastNoise::FractalFBm>();
  continentalNoise->SetSource(kSrc);
  continentalNoise->SetOctaveCount(3);

  auto rSrc = FastNoise::New<FastNoise::Simplex>();
  rSrc->SetScale(1500.0f);
  erosionNoise = FastNoise::New<FastNoise::FractalFBm>();
  erosionNoise->SetSource(rSrc);
  erosionNoise->SetOctaveCount(3);

  auto pSrc = FastNoise::New<FastNoise::Simplex>();
  pSrc->SetScale(900.0f);
  peakNoise = FastNoise::New<FastNoise::FractalFBm>();
  peakNoise->SetSource(pSrc);
  peakNoise->SetOctaveCount(4);

  auto gSrc = FastNoise::New<FastNoise::Simplex>();
  gSrc->SetScale(2100.0f);
  rangeNoise = FastNoise::New<FastNoise::FractalFBm>();
  rangeNoise->SetSource(gSrc);
  rangeNoise->SetOctaveCount(3);

  auto dSrc = FastNoise::New<FastNoise::Simplex>();
  dSrc->SetScale(180.0f);
  detailNoise = FastNoise::New<FastNoise::FractalFBm>();
  detailNoise->SetSource(dSrc);
  detailNoise->SetOctaveCount(4);

  auto tSrc = FastNoise::New<FastNoise::Simplex>();
  tSrc->SetScale(3000.0f);
  tempNoise = FastNoise::New<FastNoise::FractalFBm>();
  tempNoise->SetSource(tSrc);
  tempNoise->SetOctaveCount(3);

  auto hSrc = FastNoise::New<FastNoise::Simplex>();
  hSrc->SetScale(2400.0f);
  humidityNoise = FastNoise::New<FastNoise::FractalFBm>();
  humidityNoise->SetSource(hSrc);
  humidityNoise->SetOctaveCount(3);
}

void World::warpCoordinates(float x, float z, float &warpedX,
                            float &warpedZ) const {
  warpedX = x + sample(warpNoise, x, z) * WARP_AMPLITUDE;
  warpedZ = z + sample(warpNoise, x + 7919.0f, z + 104729.0f) * WARP_AMPLITUDE;
}

World::Column World::sampleColumn(float x, float z) const {
  float warpedX, warpedZ;
  warpCoordinates(x, z, warpedX, warpedZ);

  // Continentalness decides where the coastline sits
  float continental =
      std::tanh(sample(continentalNoise, warpedX, warpedZ) / RANGE_3 * 1.2f +
                CONTINENT_BIAS);

  float base = SEA_LEVEL + (continental >= 0.0f ? continental * LAND_SCALE
                                                : continental * OCEAN_SCALE);
  base = std::max(base, MIN_SURFACE_Y);

  float erosion =
      sample(erosionNoise, warpedX, warpedZ) / RANGE_3 * 0.5f + 0.5f;
  float worn = std::clamp((erosion - EROSION_FLAT) / EROSION_WIDTH, 0.0f, 1.0f);
  float rugged = 1.0f - (worn * worn * (3.0f - 2.0f * worn));

  float provinceRaw =
      sample(rangeNoise, warpedX, warpedZ) / RANGE_3 * 0.5f + 0.5f;
  float province = smoothstep(RANGE_LO, RANGE_HI, provinceRaw);

  float continentUplift = smoothstep(CONT_LO, CONT_HI, continental);

  float ridge = 1.0f - std::abs(sample(peakNoise, warpedX, warpedZ) / RANGE_4);
  ridge = std::max(ridge, 0.0f);
  ridge = ridge * ridge * ridge;
  float mountains =
      ridge * rugged * province * continentUplift * MOUNTAIN_SCALE;

  Column column;
  column.land = base + mountains;
  column.detail =
      sample(detailNoise, warpedX, warpedZ) / RANGE_4 * DETAIL_SCALE;

  column.temperature = sample(tempNoise, warpedX, warpedZ) * CLIMATE_GAIN;
  column.humidity = sample(humidityNoise, warpedX, warpedZ) * CLIMATE_GAIN;

  column.temperature -=
      std::max(0.0f, column.land - SNOW_ALTITUDE) * LAPSE_RATE;
  column.humidity -=
      std::max(0.0f, column.land - DRY_ALTITUDE) * OROGRAPHIC_RATE;

  column.temperature = std::clamp(column.temperature, -1.0f, 1.0f);
  column.humidity = std::clamp(column.humidity, -1.0f, 1.0f);
  return column;
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
