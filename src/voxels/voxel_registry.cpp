#include "voxels/voxel_registry.h"
#include "voxels/voxel_def.h"
#include <filesystem>
#include <fstream>
#include <print>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
VoxelDef::RenderMode parseRender(const std::string &modeName,
                                 const std::string &filePath) {
  if (modeName == "cube")
    return VoxelDef::Cube;
  if (modeName == "cutout")
    return VoxelDef::Cutout;
  if (modeName == "transparent")
    return VoxelDef::Transparent;
  if (modeName == "none")
    return VoxelDef::None;
  throw std::runtime_error("unknown render mode '" + modeName + "' in " +
                           filePath);
}
} // namespace

void VoxelRegistry::loadFromDir(const std::string &directory) {
  namespace fs = std::filesystem;
  if (!fs::exists(directory))
    throw std::runtime_error("block dir not found: " + directory);

  for (auto &directoryEntry : fs::recursive_directory_iterator(directory)) {
    if (!directoryEntry.is_regular_file())
      continue;
    if (directoryEntry.path().extension() != ".json")
      continue;

    const std::string filePath = directoryEntry.path().string();
    std::ifstream inputFile(filePath);
    if (!inputFile)
      throw std::runtime_error("cannot open " + filePath);

    nlohmann::json definitionJson;
    try {
      inputFile >> definitionJson;
    } catch (const std::exception &parseError) {
      throw std::runtime_error("json parse failed in " + filePath + ": " +
                               parseError.what());
    }

    if (!definitionJson.contains("name") || !definitionJson["name"].is_string())
      throw std::runtime_error("missing string 'name' in " + filePath);

    VoxelDef definition;
    definition.id = static_cast<VoxelId>(byId.size());
    definition.name = definitionJson.at("name").get<std::string>();
    definition.opaque = definitionJson.value("opaque", true);
    definition.solid = definitionJson.value("solid", true);
    definition.tickable = definitionJson.value("tickable", false);
    definition.render = parseRender(
        definitionJson.value("render", std::string("cube")), filePath);

    // Face convention matches the mesher: 0:+X, 1:-X, 2:+Y, 3:-Y, 4:+Z, 5:-Z.
    if (definitionJson.contains("textures")) {
      for (auto &[textureType, textureName] :
           definitionJson["textures"].items()) {
        std::string type = textureType;
        std::string name = textureName.get<std::string>();

        if (type == "all") {
          definition.textureIDs.fill(name);
        } else if (type == "top") {
          definition.textureIDs[2] = name;
        } else if (type == "bottom") {
          definition.textureIDs[3] = name;
        } else if (type == "left") {
          definition.textureIDs[1] = name;
        } else if (type == "right") {
          definition.textureIDs[0] = name;
        } else if (type == "front") {
          definition.textureIDs[4] = name;
        } else if (type == "back") {
          definition.textureIDs[5] = name;
        } else {
          throw std::runtime_error("unknown texture slot '" + type + "' in " +
                                   filePath);
        }
      }
    }
    for (std::string &textureName : definition.textureIDs) {
      if (textureName.empty())
        textureName = "missing";
    }
    // definition.components[componentName] = componentValue;

    if (definitionJson.contains("components")) {
      if (!definitionJson["components"].is_object())
        throw std::runtime_error("'components' must be an object in " +
                                 filePath);
      for (auto &[componentName, componentValue] :
           definitionJson["components"].items())
        definition.components[componentName] = componentValue;
    }
    if (!byName.emplace(definition.name, definition.id).second)
      throw std::runtime_error("duplicate block: " + definition.name + " in " +
                               filePath);
    byId.push_back(std::move(definition));
  }
}
