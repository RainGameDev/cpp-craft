#include "voxels/texture_atlas.h"
#include "voxels/voxel_registry.h"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <stb_image.h>
#include <stdexcept>
#include <utility>

namespace {

struct LoadedTile {
  std::string textureName;
  int width = 0;
  int height = 0;
  std::vector<uint8_t> pixels; // RGBA
};

//  tile indices across runs.
std::vector<std::string> collectTextureNames(const VoxelRegistry &registry) {
  std::vector<std::string> orderedNames;
  std::unordered_map<std::string, bool> seenNames;
  for (size_t definitionIndex = 0; definitionIndex < registry.size();
       ++definitionIndex) {
    const VoxelDef &definition = registry.get((VoxelId)definitionIndex);
    for (const std::string &textureName : definition.textureIDs) {
      if (textureName.empty() || seenNames.contains(textureName))
        continue; // empty = air/unset faces, skip them
      seenNames.emplace(textureName, true);
      orderedNames.push_back(textureName);
    }
  }
  return orderedNames;
}

LoadedTile loadTile(const std::string &textureDir,
                    const std::string &textureName, int expectedSize) {
  const std::string filePath = textureDir + "/" + textureName + ".png";
  int width = 0, height = 0;
  uint8_t *rawPixels =
      stbi_load(filePath.c_str(), &width, &height, nullptr, STBI_rgb_alpha);
  if (!rawPixels)
    throw std::runtime_error("atlas: cannot load " + filePath);
  if (width != height || (expectedSize > 0 && width != expectedSize)) {
    stbi_image_free(rawPixels);
    throw std::runtime_error("atlas: tile size mismatch in " + filePath +
                             " (want square " + std::to_string(expectedSize) +
                             ")");
  }
  LoadedTile tile;
  tile.textureName = textureName;
  tile.width = width;
  tile.height = height;
  tile.pixels.assign(rawPixels, rawPixels + (size_t)width * height * 4);
  stbi_image_free(rawPixels);
  return tile;
}

} // namespace

Atlas buildAtlas(const VoxelRegistry &registry, const std::string &textureDir) {
  Atlas atlas;
  std::vector<std::string> orderedNames = collectTextureNames(registry);
  if (orderedNames.empty()) {
    // No textures referenced 1x1 blank
    atlas.tileSize = 16;
    atlas.columns = atlas.rows = 1;
    atlas.width = atlas.height = 16;
    atlas.pixels.assign(16 * 16 * 4, 0);
    return atlas;
  }

  std::vector<LoadedTile> loadedTiles;
  loadedTiles.reserve(orderedNames.size());
  for (const std::string &textureName : orderedNames)
    loadedTiles.push_back(
        loadTile(textureDir, textureName,
                 loadedTiles.empty() ? 0 : loadedTiles.front().width));

  atlas.tileSize = loadedTiles.front().width;
  atlas.columns = (int)std::ceil(std::sqrt((double)loadedTiles.size()));
  atlas.rows = (int)std::ceil((double)loadedTiles.size() / atlas.columns);
  atlas.width = atlas.columns * atlas.tileSize;
  atlas.height = atlas.rows * atlas.tileSize;
  atlas.pixels.assign((size_t)atlas.width * atlas.height * 4, 0);

  for (size_t tileNumber = 0; tileNumber < loadedTiles.size(); ++tileNumber) {
    const LoadedTile &tile = loadedTiles[tileNumber];
    const int cellX = (int)(tileNumber % atlas.columns);
    const int cellY = (int)(tileNumber / atlas.columns);
    atlas.tileIndex[tile.textureName] = (int)tileNumber;
    for (int row = 0; row < atlas.tileSize; ++row) {
      uint8_t *destination =
          atlas.pixels.data() +
          ((size_t)(cellY * atlas.tileSize + row) * atlas.width +
           cellX * atlas.tileSize) *
              4;
      const uint8_t *source =
          tile.pixels.data() + (size_t)row * atlas.tileSize * 4;
      std::memcpy(destination, source, (size_t)atlas.tileSize * 4);
    }
  }
  return atlas;
}

void resolveAtlasCoords(VoxelRegistry &registry, const Atlas &atlas) {
  for (VoxelDef &definition : registry.allDefinitions()) {
    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
      const std::string &textureName = definition.textureIDs[faceIndex];
      if (textureName.empty())
        continue; // air/unset face
      auto foundTile = atlas.tileIndex.find(textureName);
      if (foundTile == atlas.tileIndex.end())
        throw std::runtime_error("atlas: block '" + definition.name +
                                 "' face " + std::to_string(faceIndex) +
                                 " references unknown texture '" + textureName +
                                 "'");
      auto [cellX, cellY] = atlas.tileCell(foundTile->second);
      definition.textureCoords[faceIndex] = {cellX, cellY};
    }
  }
}

int Atlas::tileCount() const { return (int)tileIndex.size(); }

std::pair<int, int> Atlas::tileCell(int index) const {
  return {index % columns, index / columns};
}
