#include "rendering/vulkan_application.h"

#include <chrono>
#include <glm/ext/matrix_transform.hpp>

#include "assets/texture_loader.h"

void VulkanApplication::createUniformBuffers() {
  vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
  // Create the buffer
  vk::BufferCreateInfo bufferInfo{.size = bufferSize,
                                  .usage =
                                      vk::BufferUsageFlagBits::eUniformBuffer,
                                  .sharingMode = vk::SharingMode::eExclusive};
  for (size_t i = 0; i < maxConcurrentFrames; i++) {
    uniformBuffers[i].buffer = vk::raii::Buffer(device, bufferInfo);

    // Allocate and bind memory
    vk::MemoryRequirements memRequirements =
        uniformBuffers[i].buffer.getMemoryRequirements();

    vk::MemoryAllocateInfo allocInfo{
        .allocationSize = memRequirements.size,
        .memoryTypeIndex =
            findMemoryType(memRequirements.memoryTypeBits,
                           vk::MemoryPropertyFlagBits::eHostVisible |
                               vk::MemoryPropertyFlagBits::eHostCoherent)};

    uniformBuffers[i].memory = vk::raii::DeviceMemory(device, allocInfo);
    uniformBuffers[i].buffer.bindMemory(*uniformBuffers[i].memory, 0);

    // Persistently map the buffer memory
    uniformBuffers[i].mapped =
        uniformBuffers[i].memory.mapMemory(0, bufferSize);
  }
}

void VulkanApplication::createDescriptorSetLayout() {
  vk::DescriptorSetLayoutBinding uboLayoutBinding{
      .binding = 0,
      .descriptorType = vk::DescriptorType::eUniformBuffer,
      .descriptorCount = 1,
      .stageFlags = vk::ShaderStageFlagBits::eVertex,
      .pImmutableSamplers = nullptr};

  vk::DescriptorSetLayoutBinding samplerLayoutBinding{
      .binding = 1,
      .descriptorType = vk::DescriptorType::eCombinedImageSampler,
      .descriptorCount = 1,
      .stageFlags = vk::ShaderStageFlagBits::eFragment,
      .pImmutableSamplers = nullptr};

  std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {
      uboLayoutBinding, samplerLayoutBinding};

  vk::DescriptorSetLayoutCreateInfo layoutInfo{
      .bindingCount = static_cast<uint32_t>(bindings.size()),
      .pBindings = bindings.data()};

  descriptorSetLayout = device.createDescriptorSetLayout(layoutInfo);
}

void VulkanApplication::createDescriptorSets() {
  std::array<vk::DescriptorSetLayout, maxConcurrentFrames> layouts{};
  layouts.fill(*descriptorSetLayout);

  vk::DescriptorSetAllocateInfo allocInfo{.descriptorPool = *descriptorPool,
                                          .descriptorSetCount =
                                              maxConcurrentFrames,
                                          .pSetLayouts = layouts.data()};

  descriptorSets = device.allocateDescriptorSets(allocInfo);

  auto *textures = assetManager.getLoader<TextureAsset>();
  auto &tex = textures->getAsset("dirt_albedo");

  for (size_t i = 0; i < maxConcurrentFrames; i++) {
    vk::DescriptorBufferInfo bufferInfo{.buffer = *uniformBuffers[i].buffer,
                                        .offset = 0,
                                        .range = sizeof(UniformBufferObject)};

    vk::DescriptorImageInfo imageInfo{
        .sampler = *tex.sampler,
        .imageView = *tex.imageView,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};

    std::array<vk::WriteDescriptorSet, 2> descriptorWrites{{
        {.dstSet = descriptorSets[i],
         .dstBinding = 0,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eUniformBuffer,
         .pBufferInfo = &bufferInfo},
        {.dstSet = descriptorSets[i],
         .dstBinding = 1,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eCombinedImageSampler,
         .pImageInfo = &imageInfo},
    }};

    device.updateDescriptorSets(descriptorWrites, nullptr);
  }
}

void VulkanApplication::updateUniformBuffer(uint32_t currentFrame) {
  static auto startTime = std::chrono::high_resolution_clock::now();
  auto currentTime = std::chrono::high_resolution_clock::now();
  float time = std::chrono::duration<float, std::chrono::seconds::period>(
                   currentTime - startTime)
                   .count();

  UniformBufferObject ubo{};

  ubo.model = glm::mat4(1.0f);

  // View matrix: get from our camera
  ubo.view = camera.getViewMatrix();

  // Projection matrix: get from our camera
  ubo.proj = camera.getProjectionMatrix(swapChainExtent.width /
                                        (float)swapChainExtent.height);

  // Vulkan's Y coordinate is inverted compared to OpenGL
  ubo.proj[1][1] *= -1;

  // Copy the data to the uniform buffer for the current frame-in-flight
  memcpy(uniformBuffers[currentFrame].mapped, &ubo, sizeof(ubo));
}
