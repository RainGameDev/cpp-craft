#pragma once
#include "asset_loader.h"
#include <fstream>
#include <vector>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

struct ShaderAsset {
  std::vector<char> code;
  vk::raii::ShaderModule module = nullptr;
};

class ShaderLoader : public AssetLoader<ShaderAsset> {
  vk::raii::Device &device;

public:
  ShaderLoader(vk::raii::Device &device) : device(device) {}

  std::vector<std::string> extensions() const override { return {".spv"}; }

  ShaderAsset loadAsset(const std::string &path) override {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
      throw std::runtime_error("failed to open shader: " + path);

    ShaderAsset shader;
    shader.code.resize(file.tellg());
    file.seekg(0);
    file.read(shader.code.data(), shader.code.size());

    vk::ShaderModuleCreateInfo createInfo{
        .codeSize = shader.code.size() * sizeof(char),
        .pCode =
            reinterpret_cast<const uint32_t *>(shader.code.data())};
    shader.module = vk::raii::ShaderModule(device, createInfo);

    return shader;
  }
};
