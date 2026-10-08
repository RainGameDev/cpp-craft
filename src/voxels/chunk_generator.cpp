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
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
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

SurfaceSampler::SurfaceSampler(const VoxelRegistry &registry_,
                               const BiomeRegistry &biomes_,
                               const World &world_)
    : registry(registry_), biomes(biomes_), world(world_) {
  biomeList.reserve(biomes.size());
  for (const auto &entry : biomes.all())
    biomeList.push_back(&entry.second);
  idsPerBiome.reserve(biomeList.size());
  for (auto *biome : biomeList)
    idsPerBiome.push_back({registry.idFromString(biome->topBlock),
                           registry.idFromString(biome->subBlock),
                           registry.idFromString(biome->waterBedBlock),
                           registry.idFromString(biome->waterDeepBlock),
                           registry.idFromString(biome->waterFlatBlock),
                           registry.idFromString(biome->stoneBlock)});
  snowId = registry.idFromString("game:snow");
}

SurfaceInfo SurfaceSampler::at(int worldX, int worldZ) const {
  SurfaceInfo out;
  if (biomeList.empty())
    return out;
  const float fx = (float)worldX;
  const float fz = (float)worldZ;

  // Landform and climate
  const World::Column column = world.sampleColumn(fx, fz);
  const float land = column.land;
  const float detail = column.detail;
  const float temperature = column.temperature;
  const float humidity = column.humidity;

  // biome claim
  std::vector<float> weights(biomeList.size());
  for (size_t index = 0; index < biomeList.size(); ++index)
    weights[index] =
        biomes.weightFor(*biomeList[index], temperature, humidity, land);
  BiomeRegistry::sharpen(weights);

  size_t bestIndex = 0;
  for (size_t index = 1; index < biomeList.size(); ++index)
    if (weights[index] > weights[bestIndex])
      bestIndex = index;
  auto [topId, subId, bedId, deepId, flatId, stoneId] = idsPerBiome[bestIndex];
  const Biome *biome = biomeList[bestIndex];

  // Blend the terrain modifiers
  float heightOffset = 0.0f;
  float roughness = 0.0f;
  float terrace = 0.0f;
  float subDepth = 0.0f;
  for (size_t index = 0; index < biomeList.size(); ++index) {
    const float weight = weights[index];
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

    topId = (surface > SEA_LEVEL - biome->waterBedDepth) ? bedId : deepId;
    subId = topId;
  } else if (surface <= SEA_LEVEL + 3.0f) {
    topId = flatId;
    subId = flatId;
  }
  if (surface >= SNOW_LINE) {
    topId = snowId;
    subId = snowId;
  }

  out.surfaceY = surface;
  out.temperature = temperature;
  out.humidity = humidity;
  out.topId = topId;
  out.subId = subId;
  out.stoneId = stoneId;
  out.subDepth = subDepth;
  out.biome = biomeList[bestIndex];
  out.biomeIndex = bestIndex;
  return out;
}
uint32_t hash2(int x, int z, int seed) {
  uint32_t ux = (uint32_t)(int32_t)x;
  uint32_t uz = (uint32_t)(int32_t)z;
  uint32_t us = (uint32_t)(int32_t)seed;
  uint32_t h = ux * 374761393u + uz * 668265263u + us * 1442695041u;
  h = (h ^ (h >> 13u)) * 1274126177u;
  return h ^ (h >> 16u);
}

float hashU01(uint32_t h) {
  return (float)(h & 0x00FFFFFFu) / (float)0x01000000u;
}

void decorationCandidateForCell(int cellX, int cellZ, int cellSize, int seed,
                                int &outX, int &outZ, uint32_t &outRollHash) {
  uint32_t hx = hash2(cellX, cellZ, seed ^ (int)0x9E3779B9);
  uint32_t hz = hash2(cellX, cellZ, seed ^ (int)0x85EBCA6B);
  outRollHash = hash2(cellX, cellZ, seed ^ (int)0x27D4EB2D);
  int jitterX = (cellSize > 0) ? (int)(hx % (uint32_t)cellSize) : 0;
  int jitterZ = (cellSize > 0) ? (int)(hz % (uint32_t)cellSize) : 0;
  outX = cellX * cellSize + jitterX;
  outZ = cellZ * cellSize + jitterZ;
}

bool isDecorationOrigin(int worldX, int worldZ, int cellSize, int seed,
                        float density) {
  if (cellSize <= 0 || !(density > 0.0f))
    return false;
  if (density >= 1.0f) {
    int cellX = floorDiv(worldX, cellSize);
    int cellZ = floorDiv(worldZ, cellSize);
    int candX = 0, candZ = 0;
    uint32_t roll = 0;
    decorationCandidateForCell(cellX, cellZ, cellSize, seed, candX, candZ,
                               roll);
    return worldX == candX && worldZ == candZ;
  }
  int cellX = floorDiv(worldX, cellSize);
  int cellZ = floorDiv(worldZ, cellSize);
  int candX = 0, candZ = 0;
  uint32_t roll = 0;
  decorationCandidateForCell(cellX, cellZ, cellSize, seed, candX, candZ, roll);
  return worldX == candX && worldZ == candZ && hashU01(roll) < density;
}

namespace {
constexpr int HEIGHT_SEED_XOR = (int)0x1B873593;

constexpr int DEFAULT_TREE_CELL = 8;
constexpr int DEFAULT_CACTUS_CELL = 7;
constexpr int CANOPY_R = 2;

// Effective grid cell for an entry
int effectiveCell(const Decoration &entry) {
  if (entry.cellSize > 0)
    return entry.cellSize;
  return entry.type == "cactus" ? DEFAULT_CACTUS_CELL : DEFAULT_TREE_CELL;
}

void mixStrHash(uint32_t &h, const std::string &text) {
  for (unsigned char c : text) {
    h ^= c;
    h *= 16777619u;
  }
  h ^= 0xFFu;
  h *= 16777619u;
}

uint32_t shapeHash(const TreeShape &shape) {
  uint32_t h = 2166136261u;
  auto mixU32 = [&](uint32_t v) {
    h ^= v;
    h *= 16777619u;
  };
  auto mixFloat = [&](float f) {
    uint32_t v = 0;
    std::memcpy(&v, &f, sizeof(v));
    mixU32(v);
  };
  mixU32((uint32_t)shape.trunkMin);
  mixU32((uint32_t)shape.trunkMax);
  mixFloat(shape.thin);
  for (const std::string &soil : shape.defaultSoils)
    mixStrHash(h, soil);
  for (const CanopyLayer &layer : shape.canopy) {
    mixU32(layer.frustum ? 1u : 0u);
    mixU32((uint32_t)(int32_t)layer.dy);
    mixU32((uint32_t)(int32_t)layer.fromBase);
    mixU32((uint32_t)(int32_t)layer.toTop);
    mixU32((uint32_t)layer.radius);
    mixU32((uint32_t)layer.radiusBottom);
    mixU32((uint32_t)layer.radiusTop);
    mixStrHash(h, layer.mode);
    mixFloat(layer.thin);
  }
  return h;
}

uint32_t decorationSalt(const Decoration &entry, int cell) {
  uint32_t h = 2166136261u;
  mixStrHash(h, entry.type);
  mixStrHash(h, entry.log);
  mixStrHash(h, entry.leaves);
  mixStrHash(h, entry.block);
  h ^= (uint32_t)cell;
  h *= 16777619u;
  h ^= shapeHash(entry.shape);
  h *= 16777619u;
  return h;
}

bool isEntryOrigin(int ox, int oz, int cell, int saltedSeed, float density) {
  if (cell <= 0 || !(density > 0.0f))
    return false;
  const int cellX = floorDiv(ox, cell);
  const int cellZ = floorDiv(oz, cell);
  int candX = 0, candZ = 0;
  uint32_t roll = 0;
  decorationCandidateForCell(cellX, cellZ, cell, saltedSeed, candX, candZ,
                             roll);
  if (candX != ox || candZ != oz)
    return false;
  return density >= 1.0f || hashU01(roll) < density;
}

VoxelId safeId(const VoxelRegistry &registry, const char *name) {
  try {
    return registry.idFromString(name);
  } catch (...) {
    return AIR;
  }
}

// Clipped into this chunk
bool placeClipped(Chunk &chunk, int chunkBaseX, int baseY, int chunkBaseZ,
                  int worldX, int worldY, int worldZ, VoxelId id,
                  bool onlyAir) {
  const int lx = worldX - chunkBaseX;
  if (lx < 0 || lx >= CW)
    return false;
  const int lz = worldZ - chunkBaseZ;
  if (lz < 0 || lz >= CD)
    return false;
  const int ly = worldY - baseY;
  if (ly < 0 || ly >= CH)
    return false;
  if (onlyAir && chunk.getLocal(lx, ly, lz) != AIR)
    return false;
  chunk.setLocal(lx, ly, lz, id);
  return true;
}

bool isLeavesId(VoxelId id, const VoxelId *leaves, int count) {
  for (int i = 0; i < count; ++i)
    if (id == leaves[i])
      return true;
  return false;
}

// Trunk pass
void stampTrunkColumn(Chunk &chunk, int chunkBaseX, int baseY, int chunkBaseZ,
                      int ox, int oz, int base, int height, VoxelId logId,
                      const VoxelId *leaves, int leavesCount) {
  for (int i = 0; i < height; ++i) {
    const int wy = base + i;
    const int lx = ox - chunkBaseX;
    if (lx < 0 || lx >= CW)
      continue;
    const int lz = oz - chunkBaseZ;
    if (lz < 0 || lz >= CD)
      continue;
    const int ly = wy - baseY;
    if (ly < 0 || ly >= CH)
      continue;
    const VoxelId cur = chunk.getLocal(lx, ly, lz);
    if (cur == AIR || isLeavesId(cur, leaves, leavesCount))
      chunk.setLocal(lx, ly, lz, logId);
  }
}

struct TreeSpot {
  int ox = 0, oz = 0;
  int base = 0;
  int height = 0;
  const TreeShape *shape = nullptr;
  VoxelId logId = AIR;
  VoxelId leavesId = AIR;
};

struct CactusSpot {
  int ox = 0, oz = 0;
  int base = 0;
  int height = 0;
  VoxelId cactusId = AIR;
};

inline const std::vector<std::string> &defaultCactusSoils() {
  static const std::vector<std::string> soils = {"game:sand"};
  return soils;
}

void resolveSoilIds(const Decoration &entry,
                    const std::vector<std::string> &defaults,
                    const VoxelRegistry &registry,
                    std::unordered_map<std::string, VoxelId> &cache,
                    std::vector<VoxelId> &out) {
  out.clear();
  const std::vector<std::string> &names =
      entry.soils.empty() ? defaults : entry.soils;
  for (const std::string &name : names) {
    auto found = cache.find(name);
    if (found == cache.end()) {
      const VoxelId id = safeId(registry, name.c_str());
      cache.emplace(name, id);
      out.push_back(id);
    } else {
      out.push_back(found->second);
    }
  }
}

bool slopeOk(const SurfaceSampler &sampler, int ox, int oz) {
  const float sx =
      sampler.at(ox + 1, oz).surfaceY - sampler.at(ox - 1, oz).surfaceY;
  if (sx > 2.5f || sx < -2.5f)
    return false;
  const float sz =
      sampler.at(ox, oz + 1).surfaceY - sampler.at(ox, oz - 1).surfaceY;
  if (sz > 2.5f || sz < -2.5f)
    return false;
  return true;
}

bool validateTreeOrigin(int ox, int oz, const SurfaceInfo &info,
                        const SurfaceSampler &sampler, int seed,
                        const VoxelRegistry &registry, const Decoration &entry,
                        std::unordered_map<std::string, VoxelId> &soilCache,
                        TreeSpot &out) {
  const TreeShape &shape = entry.shape;
  const VoxelId logId = safeId(registry, entry.log.c_str());
  const VoxelId leavesId = safeId(registry, entry.leaves.c_str());
  if (logId == AIR || leavesId == AIR)
    return false;
  const int cell = effectiveCell(entry);
  const int salted = seed ^ (int)decorationSalt(entry, cell);
  if (!isEntryOrigin(ox, oz, cell, salted, entry.density))
    return false;
  const float y = info.surfaceY;
  if (!(y > SEA_LEVEL + 1.0f) || !(y < SNOW_LINE - 2.0f))
    return false;
  if (info.temperature < entry.minTemp || info.temperature > entry.maxTemp ||
      info.humidity < entry.minHumidity || info.humidity > entry.maxHumidity)
    return false;
  std::vector<VoxelId> soils;
  resolveSoilIds(entry, shape.defaultSoils, registry, soilCache, soils);
  bool soilOk = soils.empty();
  for (VoxelId soil : soils)
    if (soil != AIR && soil == info.topId) {
      soilOk = true;
      break;
    }
  if (!soilOk)
    return false;
  if (!slopeOk(sampler, ox, oz))
    return false;

  const uint32_t hh =
      hash2(ox, oz, seed ^ HEIGHT_SEED_XOR ^ (int)decorationSalt(entry, cell));
  const int span = shape.trunkMax - shape.trunkMin + 1;
  const int h = shape.trunkMin + (int)(hh % (uint32_t)span);
  const int base = (int)std::floor(y) + 1;
  if (base + h + 1 >= (int)WORLD_TOP_Y)
    return false;

  out.ox = ox;
  out.oz = oz;
  out.base = base;
  out.height = h;
  out.shape = &shape;
  out.logId = logId;
  out.leavesId = leavesId;
  return true;
}

bool validateCactusOrigin(int ox, int oz, const SurfaceInfo &info,
                          const SurfaceSampler &sampler, int seed,
                          const Decoration &entry,
                          const VoxelRegistry &registry,
                          std::unordered_map<std::string, VoxelId> &soilCache,
                          CactusSpot &out) {
  const VoxelId cactusId = safeId(registry, entry.block.c_str());
  if (cactusId == AIR)
    return false;
  const int cell = effectiveCell(entry);
  const int salted = seed ^ (int)decorationSalt(entry, cell);
  if (!isEntryOrigin(ox, oz, cell, salted, entry.density))
    return false;
  const float y = info.surfaceY;
  if (!(y > SEA_LEVEL + 1.0f) || !(y < SNOW_LINE - 2.0f))
    return false;
  if (info.temperature < entry.minTemp || info.temperature > entry.maxTemp ||
      info.humidity < entry.minHumidity || info.humidity > entry.maxHumidity)
    return false;
  std::vector<VoxelId> soils;
  resolveSoilIds(entry, defaultCactusSoils(), registry, soilCache, soils);
  bool soilOk = soils.empty();
  for (VoxelId soil : soils)
    if (soil != AIR && soil == info.topId) {
      soilOk = true;
      break;
    }
  if (!soilOk)
    return false;
  if (!slopeOk(sampler, ox, oz))
    return false;
  const uint32_t hh = hash2(ox, oz, seed ^ HEIGHT_SEED_XOR);
  const int h = 2 + (int)(hh % 2u); // 2..3
  const int base = (int)std::floor(y) + 1;
  if (base + h >= (int)WORLD_TOP_Y)
    return false;
  out.ox = ox;
  out.oz = oz;
  out.base = base;
  out.height = h;
  out.cactusId = cactusId;
  return true;
}

void gatherOrigins(int chunkBaseX, int chunkBaseZ, int cellSize, int saltedSeed,
                   float density, int pad,
                   std::vector<std::pair<int, int>> &out) {
  if (cellSize <= 0 || !(density > 0.0f))
    return;
  const int minX = chunkBaseX - pad;
  const int maxX = chunkBaseX + CW - 1 + pad;
  const int minZ = chunkBaseZ - pad;
  const int maxZ = chunkBaseZ + CD - 1 + pad;
  const int c0x = floorDiv(minX, cellSize);
  const int c1x = floorDiv(maxX, cellSize);
  const int c0z = floorDiv(minZ, cellSize);
  const int c1z = floorDiv(maxZ, cellSize);
  for (int cx = c0x; cx <= c1x; ++cx) {
    for (int cz = c0z; cz <= c1z; ++cz) {
      int ox = 0, oz = 0;
      uint32_t roll = 0;
      decorationCandidateForCell(cx, cz, cellSize, saltedSeed, ox, oz, roll);
      if (ox < minX || ox > maxX || oz < minZ || oz > maxZ)
        continue;
      if (hashU01(roll) >= density)
        continue;
      out.emplace_back(ox, oz);
    }
  }
}

void stampCanopySlice(Chunk &chunk, int chunkBaseX, int baseY, int chunkBaseZ,
                      int cx, int cy, int cz, int radius,
                      const std::string &mode, float thin, VoxelId leavesId,
                      int seed) {
  if (radius < 0)
    return;
  for (int dx = -radius; dx <= radius; ++dx) {
    for (int dz = -radius; dz <= radius; ++dz) {
      if (radius > 0 && mode == "plus" && dx != 0 && dz != 0)
        continue;
      if (radius > 0 && mode == "rounded" && abs(dx) == radius &&
          abs(dz) == radius)
        continue;
      if (thin > 0.0f) {
        const uint32_t h = hash2(cx + dx + cy * 31, cz + dz - cy * 17, seed);
        if (hashU01(h) < thin)
          continue;
      }
      placeClipped(chunk, chunkBaseX, baseY, chunkBaseZ, cx + dx, cy, cz + dz,
                   leavesId, true);
    }
  }
}
void stampShapeCanopy(Chunk &chunk, int chunkBaseX, int baseY, int chunkBaseZ,
                      const TreeSpot &spot, int seed) {
  const TreeShape &shape = *spot.shape;
  const int top = spot.base + spot.height - 1;
  for (const CanopyLayer &layer : shape.canopy) {
    const float thin = layer.thin >= 0.0f ? layer.thin : shape.thin;
    if (!layer.frustum) {
      stampCanopySlice(chunk, chunkBaseX, baseY, chunkBaseZ, spot.ox,
                       top + layer.dy, spot.oz, layer.radius, layer.mode, thin,
                       spot.leavesId, seed);
    } else {
      const int lo = spot.base + layer.fromBase;
      const int hi = top + layer.toTop;
      for (int wy = lo; wy <= hi; ++wy) {
        const float t = (hi > lo) ? (float)(wy - lo) / (float)(hi - lo) : 0.0f;
        int r = (int)std::round(layer.radiusBottom +
                                (layer.radiusTop - layer.radiusBottom) * t);
        if (r < 0)
          r = 0;
        stampCanopySlice(chunk, chunkBaseX, baseY, chunkBaseZ, spot.ox, wy,
                         spot.oz, r, layer.mode, thin, spot.leavesId, seed);
      }
    }
  }
}

struct DecoGrid {
  std::string type, log, leaves, block;
  int cell = 0;
  uint32_t salt = 0;
  bool operator==(const DecoGrid &) const = default;
};

void decorateChunk(Chunk &chunk, const VoxelRegistry &registry,
                   const BiomeRegistry &biomes, const SurfaceSampler &sampler,
                   const World &world) {
  const int baseY = chunk.pos.y * CH;
  if (baseY + CH - 1 < (int)SEA_LEVEL + 2)
    return;
  if (baseY > (int)SNOW_LINE + 12)
    return;

  const int chunkBaseX = chunk.pos.x * CW;
  const int chunkBaseZ = chunk.pos.z * CD;
  const int seed = world.seedValue();

  // decoration grids over every biomes entries. Candidates are
  // gathered  then each candidate is validated against its own
  // origin column matching ent
  std::vector<DecoGrid> grids;
  for (const auto &[name, biome] : biomes.all()) {
    for (const Decoration &entry : biome.decorations) {
      if (entry.type != "tree" && entry.type != "cactus")
        continue;
      // (no shape registry: geometry lives inline on the entry)
      if (!(entry.density > 0.0f))
        continue;
      const int cell = effectiveCell(entry);
      const DecoGrid grid{entry.type,  entry.log, entry.leaves,
                          entry.block, cell,      decorationSalt(entry, cell)};
      if (std::find(grids.begin(), grids.end(), grid) == grids.end())
        grids.push_back(grid);
    }
  }
  if (grids.empty())
    return;

  std::vector<TreeSpot> treeSpots;
  std::vector<CactusSpot> cactusSpots;
  std::unordered_map<std::string, VoxelId> soilCache;
  std::vector<std::pair<int, int>> candidates;
  for (const DecoGrid &grid : grids) {
    const int salted = seed ^ (int)grid.salt;
    const int pad = (grid.type == "tree") ? CANOPY_R : 0;
    candidates.clear();
    gatherOrigins(chunkBaseX, chunkBaseZ, grid.cell, salted, 1.0f, pad,
                  candidates);
    for (const auto &[ox, oz] : candidates) {
      const SurfaceInfo info = sampler.at(ox, oz);
      if (!info.biome)
        continue;
      const Decoration *entry = nullptr;
      for (const Decoration &candidate : info.biome->decorations) {
        if (candidate.type != grid.type || candidate.log != grid.log ||
            candidate.leaves != grid.leaves || candidate.block != grid.block)
          continue;
        const int cell = effectiveCell(candidate);
        if (cell != grid.cell || decorationSalt(candidate, cell) != grid.salt)
          continue;
        entry = &candidate;
        break;
      }
      if (!entry)
        continue;
      if (grid.type == "tree") {
        TreeSpot spot;
        if (validateTreeOrigin(ox, oz, info, sampler, seed, registry, *entry,
                               soilCache, spot))
          treeSpots.push_back(spot);
      } else {
        CactusSpot spot;
        if (validateCactusOrigin(ox, oz, info, sampler, seed, *entry, registry,
                                 soilCache, spot))
          cactusSpots.push_back(spot);
      }
    }
  }

  // Trunks firstso overlapping canopies never snap a
  // trunk, then canopies
  if (!treeSpots.empty()) {
    VoxelId liveLeaves[16];
    int liveCount = 0;
    for (const TreeSpot &spot : treeSpots) {
      bool known = false;
      for (int i = 0; i < liveCount; ++i)
        if (liveLeaves[i] == spot.leavesId) {
          known = true;
          break;
        }
      if (!known && liveCount < 16)
        liveLeaves[liveCount++] = spot.leavesId;
    }
    for (const TreeSpot &spot : treeSpots)
      stampTrunkColumn(chunk, chunkBaseX, baseY, chunkBaseZ, spot.ox, spot.oz,
                       spot.base, spot.height, spot.logId, liveLeaves,
                       liveCount);
    for (const TreeSpot &spot : treeSpots)
      stampShapeCanopy(chunk, chunkBaseX, baseY, chunkBaseZ, spot, seed);
  }

  for (const CactusSpot &spot : cactusSpots)
    for (int i = 0; i < spot.height; ++i)
      placeClipped(chunk, chunkBaseX, baseY, chunkBaseZ, spot.ox, spot.base + i,
                   spot.oz, spot.cactusId, true);
}
} // namespace

void generateChunkTerrain(Chunk &chunk, const VoxelRegistry &registry,
                          const BiomeRegistry &biomes, const World &world) {

  const int baseY = chunk.pos.y * CH;

  const VoxelId waterId = registry.idFromString("game:water");

  SurfaceSampler sampler(registry, biomes, world);
  if (!sampler.valid())
    return;

  const int seaLocalY = (int)std::floor(SEA_LEVEL) - baseY;
  const int seaClamped = std::min(seaLocalY, CH - 1);

  for (int localZ = 0; localZ < CD; ++localZ) {
    for (int localX = 0; localX < CW; ++localX) {
      const int worldX = chunk.pos.x * CW + localX;
      const int worldZ = chunk.pos.z * CD + localZ;
      const float fx = (float)worldX;
      const float fz = (float)worldZ;

      const SurfaceInfo info = sampler.at(worldX, worldZ);
      const float surface = info.surfaceY;
      const VoxelId topId = info.topId;
      const VoxelId subId = info.subId;
      const VoxelId stoneId = info.stoneId;
      const float subDepth = info.subDepth;

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

  decorateChunk(chunk, registry, biomes, sampler, world);
}
