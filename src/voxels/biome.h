#pragma once
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct Biome {
  std::string name;

  float baseHeight = 10.0f;
  float amplitude = 17.0f;
  float frequency = 0.05f;
  std::string topBlock, subBlock; // grass, dirt
  int subDepth = 3;
  std::string stoneBlock;

  // Climate niche. Weight falls off with distance from
  // (tempCenter, humidityCenter). Larger radius = wider/smoother transitions.
  float tempCenter = 0.0f;
  float tempRadius = 1.0f;
  float humidityCenter = 0.0f;
  float humidityRadius = 1.0f;
};

class BiomeRegistry {
  std::unordered_map<std::string, Biome> byName;

public:
  void loadFromDir(const std::string &dir); // parse *.json
  const Biome &get(const std::string &name) const { return byName.at(name); }
  // Dominant biome (max weight). Kept for blocktype choice.
  const Biome &pick(float temp, float humidity) const;
  // Unnormalized gaussian weight for one biome.
  float weightFor(const Biome &biome, float temp, float humidity) const;
  // Normalized weights over all biomes, sums to 1. Empty when no biomes.
  std::vector<std::pair<const Biome *, float>> weights(float temp,
                                                       float humidity) const;
  const std::unordered_map<std::string, Biome> &all() const { return byName; }
  size_t size() const { return byName.size(); }
};
