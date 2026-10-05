#pragma once
#include "voxel_def.h"
#include <array>
inline constexpr int CW = 32, CH = 32, CD = 32;

struct ChunkPos {
  int x, y, z;
  bool operator==(const ChunkPos &) const = default;
};
struct ChunkPosHash {
  size_t operator()(ChunkPos chunkPos) const noexcept {
    return (size_t)(chunkPos.x * 73856093 ^ chunkPos.y * 19349663 ^ chunkPos.z * 83492791);
  }
};

struct Chunk {
  ChunkPos pos{};
  std::array<VoxelId, CW * CH * CD> voxels{};
  bool dirty = true;
  bool generated = false;
  static size_t idx(int x, int y, int z) {
    return (size_t)((y * CD + z) * CW + x);
  }
  VoxelId getLocal(int x, int y, int z) const { return voxels[idx(x, y, z)]; }
  void setLocal(int x, int y, int z, VoxelId id) {
    voxels[idx(x, y, z)] = id;
    dirty = true;
  }
};
