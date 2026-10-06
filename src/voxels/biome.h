#pragma once
#include <string>
#include <unordered_map>

struct Biome {
  std::string name;

  float baseHeight = 10.0f;
  float amplitude = 17.0f;
  float frequency = 0.05f;
  std::string topBlock, subBlock; // grass, dirt
  int subDepth = 3;
  std::string stoneBlock;
};

class BiomeRegistry {
  std::unordered_map<std::string, Biome> byName;

public:
  void loadFromDir(const std::string &dir); // parse *.json
  const Biome &get(const std::string &name) const { return byName.at(name); }
  const Biome &pick(float temp, float humidity) const;
  size_t size() const { return byName.size(); }
};
