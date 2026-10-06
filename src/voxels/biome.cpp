#include "voxels/biome.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

void BiomeRegistry::loadFromDir(const std::string &directory) {
  namespace fs = std::filesystem;
  if (!fs::exists(directory))
    throw std::runtime_error("biome dir not found: " + directory);

  for (auto &directoryEntry : fs::recursive_directory_iterator(directory)) {
    if (!directoryEntry.is_regular_file())
      continue;
    if (directoryEntry.path().extension() != ".json")
      continue;

    const std::string filePath = directoryEntry.path().string();
    std::ifstream inputFile(filePath);
    if (!inputFile)
      throw std::runtime_error("cannot open " + filePath);

    nlohmann::json biomeJson;
    try {
      inputFile >> biomeJson;
    } catch (const std::exception &parseError) {
      throw std::runtime_error("json parse failed in " + filePath + ": " +
                               parseError.what());
    }

    if (!biomeJson.contains("name") || !biomeJson["name"].is_string())
      throw std::runtime_error("missing string 'name' in " + filePath);

    Biome biome;
    biome.name = biomeJson.at("name").get<std::string>();
    biome.baseHeight = biomeJson.value("baseHeight", biome.baseHeight);
    biome.amplitude = biomeJson.value("amplitude", biome.amplitude);
    biome.frequency = biomeJson.value("frequency", biome.frequency);
    biome.topBlock = biomeJson.value("topBlock", std::string("game:grass"));
    biome.subBlock = biomeJson.value("subBlock", std::string("game:dirt"));
    biome.subDepth = biomeJson.value("subDepth", biome.subDepth);
    biome.stoneBlock = biomeJson.value("stoneBlock", std::string("game:stone"));

    if (!byName.emplace(biome.name, std::move(biome)).second)
      throw std::runtime_error("duplicate biome: " + biomeJson.at("name").get<std::string>() +
                               " in " + filePath);
  }
}

const Biome &BiomeRegistry::pick(float temp, float /*humidity*/) const {
  // tempNoise is ~[-1,1]. Mountains on warm side, plains elsewhere.
  const char *wanted = temp > 0.2f ? "game:mountains" : "game:plains";
  if (auto found = byName.find(wanted); found != byName.end())
    return found->second;
  return byName.begin()->second;
}
