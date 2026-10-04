#pragma once
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

class IAssetLoader {
public:
  virtual ~IAssetLoader() = default;
  virtual void loadFiles(const std::string &directory) = 0;
};

template <typename AssetType> class AssetLoader : public IAssetLoader {
protected:
  std::unordered_map<std::string, AssetType> assets;

public:
  /// What extensions are valid for this asset
  virtual std::vector<std::string> extensions() const = 0;
  /// How does the asset get loaded
  virtual AssetType loadAsset(const std::string &path) = 0;

  /// Returns all the assets loaded
  const std::unordered_map<std::string, AssetType> &getAssets() const {
    return assets;
  }

  /// Gets asset by name (e.g. "sdr_default_model.vert.spv")
  AssetType &getAsset(const std::string &name) { return assets[name]; }

  /// Gets asset by name, returns nullptr if not found
  AssetType *tryGetAsset(const std::string &name) {
    auto it = assets.find(name);
    return it != assets.end() ? &it->second : nullptr;
  }

  void loadFiles(const std::string &directory) override {
    for (const auto &entry :
         std::filesystem::recursive_directory_iterator(directory)) {
      if (!entry.is_regular_file())
        continue;

      std::string ext = entry.path().extension().string();
      for (const auto &supported : extensions()) {
        if (ext == supported) {
          std::string name = entry.path().stem().string();
          assets[name] = loadAsset(entry.path().string());
        }
      }
    }
  }
};
