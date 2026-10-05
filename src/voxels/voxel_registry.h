#pragma once
#include "voxel_def.h"
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

class VoxelRegistry {
  std::vector<VoxelDef> byId; // byId[id], hot path
  std::unordered_map<std::string, VoxelId> byName;

public:
  VoxelRegistry() { // guarantee 0 = air
    VoxelDef air;
    air.id = 0;
    air.name = "game:air";
    air.render = VoxelDef::RenderMode::None;
    air.opaque = false;
    air.solid = false;
    byId.push_back(std::move(air));
    byName["game:air"] = 0;
  }
  std::vector<VoxelDef> &allDefinitions() { return byId; }
  void loadFromDir(const std::string &dir); // parse *.json, assign ids
  const VoxelDef &get(VoxelId id) const { return byId[id]; }
  VoxelId idFromString(const std::string &blockName) const {
    return byName.at(blockName);
  }
  size_t size() const { return byId.size(); }
};
