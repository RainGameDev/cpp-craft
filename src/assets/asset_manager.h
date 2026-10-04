#include "asset_loader.h"
#include <string>
#include <vector>

class AssetManager {

private:
  std::vector<std::unique_ptr<IAssetLoader>> loaders;

public:
  /// Registers an assetloader to the manager.
  void addLoader(std::unique_ptr<IAssetLoader> loader) {
    loaders.push_back(std::move(loader));
  }

  /// Loads a specific directories assets.
  /// Eg; loadDirectory("src/../assets/")
  void loadDirectory(const std::string &path) {
    for (auto &loader : loaders)
      loader->loadFiles(path);
  }

  /// Returns all loaded assets as a vec of strings for paths.
  template <typename AssetType> AssetLoader<AssetType> *getLoader() {
    for (auto &loader : loaders) {
      auto *cast = dynamic_cast<AssetLoader<AssetType> *>(loader.get());
      if (cast)
        return cast;
    }
    return nullptr;
  }
};
