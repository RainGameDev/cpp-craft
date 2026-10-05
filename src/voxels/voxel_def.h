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
  // Face order matches the mesher: 0:+X (right), 1:-X (left), 2:+Y (top),
  // 3:-Y (bottom), 4:+Z (front), 5:-Z (back). "all" fills every face first,
  // then a named slot overwrites just its face.
  std::array<std::string, 6> textureIDs{};
  // Same face order as above; each entry is the {column, row} atlas cell.
  std::array<std::array<int32_t, 2>, 6> textureCoords{};
  std::unordered_map<std::string, nlohmann::json> components;
};
