#pragma once
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

inline constexpr float BIOME_SHARPNESS = 2.0f;

struct CanopyLayer {
  bool frustum = false;
  int dy = 0;
  int fromBase = 0, toTop = 0;
  int radius = 1, radiusBottom = 1, radiusTop = 1;
  std::string mode = "rounded";
  float thin = -1.0f;
  bool operator==(const CanopyLayer &) const = default;
};

struct TreeShape {
  int trunkMin = 4, trunkMax = 6;
  float thin = 0.06f;
  std::vector<std::string> defaultSoils;
  std::vector<CanopyLayer> canopy;
  bool operator==(const TreeShape &) const = default;
};

struct Decoration {
  std::string type; // "tree" | "cactus"

  TreeShape shape;
  std::string log;                   // tree trunk block, e.g. "game:oaklog"
  std::string leaves;                // tree canopy block, e.g. "game:oakleaves"
  std::string block = "game:cactus"; // cactus block for type == "cactus"
  int cellSize = 0;     // decoration grid cell; 0 = engine default per type
  float density = 0.0f; // fraction of grid candidates that grow (0..1)
  std::vector<std::string> soils; // allowed top blocks; empty = default
  float minTemp = -2.0f, maxTemp = 2.0f;
  float minHumidity = -2.0f, maxHumidity = 2.0f;
};

struct Biome {
  std::string name;

  float heightOffset = 0.0f;      // added to the landform height, in world Y
  float terrace = 0.0f;           // 0..1, quantises the surface into flat steps
  float roughness = 1.0f;         // scales the fine surface detail
  std::string topBlock, subBlock; // grass, dirt
  // Water beds (surface below sea level) and near-water flats (beaches).
  // Beds use waterBedBlock down to waterBedDepth below sea level and
  // waterDeepBlock beneath that; flats take waterFlatBlock for top and sub.
  // Lets cold biomes keep snowy shores over sandy beds, swamps go muddy, etc.
  std::string waterBedBlock, waterDeepBlock, waterFlatBlock;
  float waterBedDepth = 6.0f;
  int subDepth = 3;
  std::string stoneBlock;

  std::vector<Decoration> decorations;

  float tempCenter = 0.0f;
  float tempRadius = 1.0f;
  float humidityCenter = 0.0f;
  float humidityRadius = 1.0f;
  float elevCenter = 0.0f;
  float elevRadius = 1e4f; // effectively ignores altitude when left huge
};

class VoxelRegistry;

class BiomeRegistry {
  std::unordered_map<std::string, Biome> byName;

public:
  void loadFromDir(const std::string &dir); // parse *.json
  void validateBlocks(const VoxelRegistry &voxels) const;
  static void sharpen(std::vector<float> &weights);
  const Biome &get(const std::string &name) const { return byName.at(name); }
  // Dominant biome
  const Biome &pick(float temp, float humidity, float elevation) const;
  // Unnormalized weight for one biome.
  float weightFor(const Biome &biome, float temp, float humidity,
                  float elevation) const;
  std::vector<std::pair<const Biome *, float>>
  weights(float temp, float humidity, float elevation) const;
  const std::unordered_map<std::string, Biome> &all() const { return byName; }
  size_t size() const { return byName.size(); }
};
