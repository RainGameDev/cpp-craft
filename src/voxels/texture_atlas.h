#pragma once

#include "voxels/voxel_registry.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct AtlasLayout {
  int columns = 1;
  int rows = 1;
};

struct Atlas {
  // TODO: i want this to be defined in voxels if possible
  int tileSize = 16; // all tiles must be this size
  int columns = 0, rows = 0;
  int width = 0, height = 0;
  std::vector<uint8_t> pixels;                    // RGBA, width*height*4
  std::unordered_map<std::string, int> tileIndex; // "dirt" -> 3
  int tileCount() const;
  std::pair<int, int> tileCell(int index) const; // -> {col,row}

  AtlasLayout layout() const { return {columns, rows}; }
};

Atlas buildAtlas(const VoxelRegistry &registry, const std::string &textureDir);
void resolveAtlasCoords(VoxelRegistry &registry, const Atlas &atlas);
