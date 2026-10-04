#pragma once

#include <array>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

using VoxelId = uint16_t;
constexpr VoxelId AIR = 0;

struct VoxelDef {
  VoxelId id;         // assigned on load, 1,2,3...
  std::string name;   // "game:voxel:dirt"
  bool opaque = true; // false for glass/leaves/water -> disables face-culling
  bool solid = true;  // collision
  bool tickable = false;
  enum RenderMode { Cube, Cutout, Transparent, None } render = Cube;
  // "all" takes last priority, eg if all is set then a"left" is defined, left
  // overwrites the all texture for that face. 0: top, 1: left, 2: right, 3:
  // bottom, 4: front, 5: back
  std::array<std::string, 6> textureIDs{};
  // 0: top, 1: left, 2: right, 3: bottom, 4: front, 5: back
  std::array<std::array<int32_t, 2>, 6> textureCoords;
  std::unordered_map<std::string, nlohmann::json> components;
};
