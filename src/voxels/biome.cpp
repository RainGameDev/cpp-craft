#include "voxels/biome.h"
#include "voxels/voxel_registry.h"
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
    biome.waterBedBlock =
        biomeJson.value("waterBedBlock", std::string("game:sand"));
    biome.waterDeepBlock =
        biomeJson.value("waterDeepBlock", std::string("game:clay"));
    biome.waterBedDepth =
        biomeJson.value("waterBedDepth", biome.waterBedDepth);
    biome.waterFlatBlock =
        biomeJson.value("waterFlatBlock", std::string("game:sand"));

    biome.subDepth = biomeJson.value("subDepth", biome.subDepth);
    biome.stoneBlock = biomeJson.value("stoneBlock", std::string("game:stone"));
    biome.tempCenter = biomeJson.value("tempCenter", biome.tempCenter);
    biome.tempRadius = biomeJson.value("tempRadius", biome.tempRadius);
    biome.humidityCenter =
        biomeJson.value("humidityCenter", biome.humidityCenter);
    biome.humidityRadius =
        biomeJson.value("humidityRadius", biome.humidityRadius);
    biome.elevCenter = biomeJson.value("elevCenter", biome.elevCenter);
    if (biomeJson.contains("decorations")) {
      if (!biomeJson["decorations"].is_array())
        throw std::runtime_error("'decorations' must be an array in " +
                                 filePath);
      for (const auto &decorationJson : biomeJson["decorations"]) {
        if (!decorationJson.is_object())
          throw std::runtime_error("'decorations' entries must be objects in " +
                                   filePath);
        Decoration decoration;
        decoration.type = decorationJson.value("type", std::string("tree"));
        if (decoration.type == "tree") {
          if (!decorationJson.contains("shape") ||
              !decorationJson["shape"].is_object())
            throw std::runtime_error("tree entry needs a 'shape' object in " +
                                     filePath);
          const auto &shapeJson = decorationJson["shape"];
          TreeShape shape;
          if (shapeJson.contains("trunk")) {
            if (!shapeJson["trunk"].is_object())
              throw std::runtime_error("'trunk' must be an object in " +
                                       filePath);
            shape.trunkMin = shapeJson["trunk"].value("min", shape.trunkMin);
            shape.trunkMax = shapeJson["trunk"].value("max", shape.trunkMax);
          }
          if (shape.trunkMin < 1 || shape.trunkMax < shape.trunkMin)
            throw std::runtime_error("bad shape 'trunk' min/max in " +
                                     filePath);
          shape.thin = shapeJson.value("thin", shape.thin);
          if (!(shape.thin >= 0.0f && shape.thin < 1.0f))
            throw std::runtime_error("shape 'thin' must be in [0, 1) in " +
                                     filePath);
          if (shapeJson.contains("defaultSoils")) {
            if (!shapeJson["defaultSoils"].is_array())
              throw std::runtime_error("'defaultSoils' must be an array in " +
                                       filePath);
            for (const auto &soilJson : shapeJson["defaultSoils"]) {
              if (!soilJson.is_string())
                throw std::runtime_error(
                    "'defaultSoils' entries must be strings in " + filePath);
              shape.defaultSoils.push_back(soilJson.get<std::string>());
            }
          }
          if (shapeJson.contains("canopy")) {
            if (!shapeJson["canopy"].is_array())
              throw std::runtime_error("'canopy' must be an array in " +
                                       filePath);
            for (const auto &layerJson : shapeJson["canopy"]) {
              if (!layerJson.is_object())
                throw std::runtime_error(
                    "'canopy' entries must be objects in " + filePath);
              CanopyLayer layer;
              layer.frustum = layerJson.contains("fromBase") ||
                              layerJson.contains("toTop") ||
                              layerJson.contains("radiusBottom") ||
                              layerJson.contains("radiusTop");
              layer.dy = layerJson.value("dy", layer.dy);
              layer.fromBase = layerJson.value("fromBase", layer.fromBase);
              layer.toTop = layerJson.value("toTop", layer.toTop);
              layer.radius = layerJson.value("radius", layer.radius);
              layer.radiusBottom =
                  layerJson.value("radiusBottom", layer.radiusBottom);
              layer.radiusTop = layerJson.value("radiusTop", layer.radiusTop);
              layer.mode = layerJson.value("mode", layer.mode);
              layer.thin = layerJson.value("thin", layer.thin);
              if (layer.mode != "rounded" && layer.mode != "full" &&
                  layer.mode != "plus")
                throw std::runtime_error("unknown canopy mode '" + layer.mode +
                                         "' in " + filePath);
              if (layer.radius < 0 || layer.radiusBottom < 0 ||
                  layer.radiusTop < 0)
                throw std::runtime_error("canopy radius must be >= 0 in " +
                                         filePath);
              if (!(layer.thin < 0.0f ||
                    (layer.thin >= 0.0f && layer.thin < 1.0f)))
                throw std::runtime_error("'thin' must be in [0, 1) in " +
                                         filePath);
              shape.canopy.push_back(std::move(layer));
            }
          }
          decoration.shape = std::move(shape);
        }
        decoration.log = decorationJson.value("log", decoration.log);
        decoration.leaves = decorationJson.value("leaves", decoration.leaves);
        decoration.block = decorationJson.value("block", decoration.block);
        decoration.cellSize =
            decorationJson.value("cellSize", decoration.cellSize);
        decoration.density =
            decorationJson.value("density", decoration.density);
        if (decorationJson.contains("soils")) {
          if (!decorationJson["soils"].is_array())
            throw std::runtime_error("'soils' must be an array in " + filePath);
          for (const auto &soilJson : decorationJson["soils"]) {
            if (!soilJson.is_string())
              throw std::runtime_error("'soils' entries must be strings in " +
                                       filePath);
            decoration.soils.push_back(soilJson.get<std::string>());
          }
        }
        decoration.minTemp =
            decorationJson.value("minTemp", decoration.minTemp);
        decoration.maxTemp =
            decorationJson.value("maxTemp", decoration.maxTemp);
        decoration.minHumidity =
            decorationJson.value("minHumidity", decoration.minHumidity);
        decoration.maxHumidity =
            decorationJson.value("maxHumidity", decoration.maxHumidity);

        if (decoration.type != "tree" && decoration.type != "cactus")
          throw std::runtime_error("unknown decoration type '" +
                                   decoration.type + "' in " + filePath);

        if (decoration.cellSize < 0)
          throw std::runtime_error("'cellSize' must be >= 0 in " + filePath);
        if (!(decoration.density >= 0.0f && decoration.density <= 1.0f))
          throw std::runtime_error("'density' must be in [0, 1] in " +
                                   filePath);
        biome.decorations.push_back(std::move(decoration));
      }
    }
    biome.elevRadius = biomeJson.value("elevRadius", biome.elevRadius);

    if (!byName.emplace(biome.name, std::move(biome)).second)
      throw std::runtime_error(
          "duplicate biome: " + biomeJson.at("name").get<std::string>() +
          " in " + filePath);
  }
}

void BiomeRegistry::validateBlocks(const VoxelRegistry &voxels) const {
  auto require = [&](const std::string &block, const std::string &biome,
                     const std::string &role) {
    try {
      voxels.idFromString(block);
    } catch (...) {
      throw std::runtime_error("biome '" + biome + "': " + role +
                               " names unknown block '" + block + "'");
    }
  };
  for (const auto &[name, biome] : byName) {
    require(biome.topBlock, name, "'topBlock'");
    require(biome.subBlock, name, "'subBlock'");
    require(biome.stoneBlock, name, "'stoneBlock'");
    require(biome.waterBedBlock, name, "'waterBedBlock'");
    require(biome.waterDeepBlock, name, "'waterDeepBlock'");
    require(biome.waterFlatBlock, name, "'waterFlatBlock'");
    for (const Decoration &entry : biome.decorations) {
      if (entry.type == "tree") {
        if (entry.log.empty() || entry.leaves.empty())
          throw std::runtime_error("biome '" + name +
                                   "': tree entry needs 'log' and 'leaves'");
        require(entry.log, name, "'log'");
        require(entry.leaves, name, "'leaves'");
      } else {
        require(entry.block, name, "'block'");
      }
      for (const std::string &soil : entry.soils)
        require(soil, name, "'soils'");
    }
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
