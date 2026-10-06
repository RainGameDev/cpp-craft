#include "voxels/biome.h"
#include <cmath>
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
    biome.tempCenter = biomeJson.value("tempCenter", biome.tempCenter);
    biome.tempRadius = biomeJson.value("tempRadius", biome.tempRadius);
    biome.humidityCenter =
        biomeJson.value("humidityCenter", biome.humidityCenter);
    biome.humidityRadius =
        biomeJson.value("humidityRadius", biome.humidityRadius);

    if (!byName.emplace(biome.name, std::move(biome)).second)
      throw std::runtime_error(
          "duplicate biome: " + biomeJson.at("name").get<std::string>() +
          " in " + filePath);
  }
}

float BiomeRegistry::weightFor(const Biome &biome, float temp,
                               float humidity) const {
  float tempRadius = biome.tempRadius;
  float humidityRadius = biome.humidityRadius;
  if (!(tempRadius > 1e-4f))
    tempRadius = 1e-4f;
  if (!(humidityRadius > 1e-4f))
    humidityRadius = 1e-4f;
  float tempDist = (temp - biome.tempCenter) / tempRadius;
  float humidityDist = (humidity - biome.humidityCenter) / humidityRadius;
  return std::exp(-(tempDist * tempDist + humidityDist * humidityDist));
}

std::vector<std::pair<const Biome *, float>>
BiomeRegistry::weights(float temp, float humidity) const {
  std::vector<std::pair<const Biome *, float>> result;
  result.reserve(byName.size());
  float total = 0.0f;
  for (const auto &entry : byName) {
    float w = weightFor(entry.second, temp, humidity);
    result.emplace_back(&entry.second, w);
    total += w;
  }
  if (total > 0.0f) {
    for (auto &entry : result)
      entry.second /= total;
  }
  return result;
}

const Biome &BiomeRegistry::pick(float temp, float humidity) const {
  if (byName.empty())
    throw std::runtime_error("no biomes loaded");
  const Biome *best = nullptr;
  float bestWeight = -1.0f;
  for (const auto &entry : byName) {
    float w = weightFor(entry.second, temp, humidity);
    if (w > bestWeight) {
      bestWeight = w;
      best = &entry.second;
    }
  }
  return *best;
}
