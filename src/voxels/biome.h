#pragma once
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

inline constexpr float BIOME_SHARPNESS = 2.0f;

struct Biome {
  std::string name;

  float heightOffset = 0.0f;      // added to the landform height, in world Y
  float terrace = 0.0f;           // 0..1, quantises the surface into flat steps
  float roughness = 1.0f;         // scales the fine surface detail
  std::string topBlock, subBlock; // grass, dirt
  int subDepth = 3;
  std::string stoneBlock;

  float tempCenter = 0.0f;
  float tempRadius = 1.0f;
  float humidityCenter = 0.0f;
  float humidityRadius = 1.0f;
  float elevCenter = 0.0f;
  float elevRadius = 1e4f; // effectively ignores altitude when left huge
};

class BiomeRegistry {
  std::unordered_map<std::string, Biome> byName;

public:
  void loadFromDir(const std::string &dir); // parse *.json
  // Normalises raw niche weights and raises them to BIOME_SHARPNESS so
  // one biome wins the column. Shared so generation and queries agree.
  static void sharpen(std::vector<float> &weights);
  const Biome &get(const std::string &name) const { return byName.at(name); }
  // Dominant biome
  const Biome &pick(float temp, float humidity, float elevation) const;
  // Unnormalized weight for one biome.
  float weightFor(const Biome &biome, float temp, float humidity,
                  float elevation) const;
  // Sharp, normalized weights over all biomes, sums to 1. Empty when no
  // biomes.
  std::vector<std::pair<const Biome *, float>>
  weights(float temp, float humidity, float elevation) const;
  const std::unordered_map<std::string, Biome> &all() const { return byName; }
  size_t size() const { return byName.size(); }
};
