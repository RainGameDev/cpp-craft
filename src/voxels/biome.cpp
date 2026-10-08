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
    biome.heightOffset = biomeJson.value("heightOffset", biome.heightOffset);
    biome.terrace = biomeJson.value("terrace", biome.terrace);
    biome.roughness = biomeJson.value("roughness", biome.roughness);
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
    biome.elevCenter = biomeJson.value("elevCenter", biome.elevCenter);
    biome.elevRadius = biomeJson.value("elevRadius", biome.elevRadius);

    if (!byName.emplace(biome.name, std::move(biome)).second)
      throw std::runtime_error(
          "duplicate biome: " + biomeJson.at("name").get<std::string>() +
          " in " + filePath);
  }
}

void BiomeRegistry::sharpen(std::vector<float> &weights) {
  float total = 0.0f;
  for (float weight : weights)
    total += weight;
  if (!(total > 0.0f))
    return;

  // Normalis
  float sharpenedTotal = 0.0f;
  for (float &weight : weights) {
    weight = std::pow(weight / total, BIOME_SHARPNESS);
    sharpenedTotal += weight;
  }
  if (!(sharpenedTotal > 0.0f))
    return;
  for (float &weight : weights)
    weight /= sharpenedTotal;
}

float BiomeRegistry::weightFor(const Biome &biome, float temp, float humidity,
                               float elevation) const {
  float tempRadius = biome.tempRadius;
  float humidityRadius = biome.humidityRadius;
  if (!(tempRadius > 1e-4f))
    tempRadius = 1e-4f;
  if (!(humidityRadius > 1e-4f))
    humidityRadius = 1e-4f;
  float elevRadius = biome.elevRadius;
  if (!(elevRadius > 1e-4f))
    elevRadius = 1e4f;

  float tempDist = (temp - biome.tempCenter) / tempRadius;
  float humidityDist = (humidity - biome.humidityCenter) / humidityRadius;
  float elevDist = (elevation - biome.elevCenter) / elevRadius;
  return std::exp(-(tempDist * tempDist + humidityDist * humidityDist +
                    elevDist * elevDist));
}

std::vector<std::pair<const Biome *, float>>
BiomeRegistry::weights(float temp, float humidity, float elevation) const {
  std::vector<std::pair<const Biome *, float>> result;
  result.reserve(byName.size());
  std::vector<float> raw;
  raw.reserve(byName.size());
  for (const auto &entry : byName) {
    result.emplace_back(&entry.second, 0.0f);
    raw.push_back(weightFor(entry.second, temp, humidity, elevation));
  }
  sharpen(raw);
  for (size_t index = 0; index < result.size(); ++index)
    result[index].second = raw[index];
  return result;
}

const Biome &BiomeRegistry::pick(float temp, float humidity,
                                 float elevation) const {
  if (byName.empty())
    throw std::runtime_error("no biomes loaded");
  const Biome *best = nullptr;
  float bestWeight = -1.0f;
  for (const auto &entry : byName) {
    float w = weightFor(entry.second, temp, humidity, elevation);
    if (w > bestWeight) {
      bestWeight = w;
      best = &entry.second;
    }
  }
  return *best;
}
