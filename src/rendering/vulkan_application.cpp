#include "rendering/vulkan_application.h"
#include "assets/shader_loader.h"
#include "assets/texture_loader.h"
#include "glm/ext/vector_float3.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "voxels/chunk_generator.h"
#include "voxels/raycast.h"
#include "voxels/texture_atlas.h"
#include "voxels/voxel_registry.h"
#include <GLFW/glfw3.h>
#include <cstdint>
#include <memory>
#include <print>

void VulkanApplication::run() {
  initVulkan();
  mainLoop();
  cleanup();
}

void VulkanApplication::setupKeybinds() {
  input.window = window;
  input.addKeybind({GLFW_KEY_W, GLFW_REPEAT, InputDevice::Keyboard},
                   "move_forward");
  input.addKeybind({GLFW_KEY_S, GLFW_REPEAT, InputDevice::Keyboard},
                   "move_back");
  input.addKeybind({GLFW_KEY_A, GLFW_REPEAT, InputDevice::Keyboard},
                   "move_left");
  input.addKeybind({GLFW_KEY_D, GLFW_REPEAT, InputDevice::Keyboard},
                   "move_right");
  input.addKeybind({GLFW_KEY_SPACE, GLFW_REPEAT, InputDevice::Keyboard},
                   "move_up");
  input.addKeybind({GLFW_KEY_LEFT_SHIFT, GLFW_REPEAT, InputDevice::Keyboard},
                   "move_down");
  input.addKeybind({GLFW_KEY_F3, GLFW_PRESS, InputDevice::Keyboard},
                   "toggle_wireframe");
  input.addKeybind({GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, InputDevice::Mouse},
                   "break_block");
  input.addKeybind({GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, InputDevice::Mouse},
                   "place_block");
}

void VulkanApplication::initVulkan() {
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
  glfwSetWindowUserPointer(window, this);
  glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  if (glfwRawMouseMotionSupported())
    glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

  setupKeybinds();

  createInstance();
  setupDebugMessenger();
  createSurface();
  pickPhysicalDevice();
  createLogicalDevice();
  assetManager.addLoader(std::make_unique<ShaderLoader>(device));
  createSwapChain();
  createImageViews();
  createCommandPool();
  createDepthResources();
  createDescriptorSetLayout();
  assetManager.addLoader(std::make_unique<TextureLoader>(
      device, physicalDevice, commandPool, queue, queueIndex));

  voxelRegistry.loadFromDir("assets/blocks");
  std::print("loaded {} voxel defs\n", voxelRegistry.size());
  for (VoxelId i = 0; i < voxelRegistry.size(); ++i) {
    const auto &d = voxelRegistry.get(i);
    auto texts = d.textureIDs;
    auto bt = d.components.contains("BreakTime")
                  ? d.components.at("BreakTime").dump()
                  : "-";
    std::print("  [{}] {} opaque={} render={} BreakTime={}, ids={}\n", (int)i,
               d.name, d.opaque, (int)d.render, bt, texts);
  }

  biomeRegistry.loadFromDir("assets/biomes");
  std::print("loaded {} biomes\n", biomeRegistry.size());

  camera = Camera(glm::vec3(24.0f, 20.0f, -8.0f), glm::vec3(0.0f, 1.0f, 0.0f),
                  108.0f, -35.0f);
  assetManager.loadDirectory("assets/");

  voxelAtlas = buildAtlas(voxelRegistry, "assets/textures");
  resolveAtlasCoords(voxelRegistry, voxelAtlas);
  auto *textureLoader =
      dynamic_cast<TextureLoader *>(assetManager.getLoader<TextureAsset>());
  if (!textureLoader)
    throw std::runtime_error("voxel atlas: texture loader not registered");
  textureLoader->uploadFromPixels(
      voxelAtlas.pixels.data(), voxelAtlas.width, voxelAtlas.height,
      voxelAtlas.tileSize, voxelAtlas.columns, voxelAtlas.rows, "voxel_atlas");

  createDescriptorPool();
  createUniformBuffers();
  createDescriptorSets();
  createGraphicsPipeline();

  for (const auto &chunkEntry : world.allChunks())
    rebuildChunk(chunkEntry.first);
  {
    size_t totalIndices = 0;
    for (const auto &meshEntry : chunkMeshes)
      totalIndices += meshEntry.second.indexCount;
    std::print("uploaded {} chunk meshes ({} indices)\n", chunkMeshes.size(),
               totalIndices);
  }

  createCommandBuffer();
  createSyncObjects();

  initImGui();
}

const uint16_t RENDER_RADIUS = 4;

void VulkanApplication::mainLoop() {
  float lastTime = 0.0f;
  while (!glfwWindowShouldClose(window)) {
    float currentTime = static_cast<float>(glfwGetTime());
    float deltaTime = currentTime - lastTime;
    lastTime = currentTime;

    glm::vec3 playerPos = camera.getPosition();
    ChunkPos playerChunk = World::toChunkPos((int)std::floor(playerPos.x),
                                             (int)std::floor(playerPos.y),
                                             (int)std::floor(playerPos.z));

    if (!streamInitialized || !(playerChunk == lastStreamedChunk)) {
      streamInitialized = true;
      lastStreamedChunk = playerChunk;
      for (int radiusStep = 0; radiusStep <= RENDER_RADIUS; ++radiusStep)
        for (ChunkPos column : spiralAround(playerChunk, radiusStep)) {
          int bottomY = playerChunk.y - 2;
          if (bottomY > 0)
            bottomY = 0;
          for (int columnY = bottomY; columnY <= playerChunk.y; ++columnY) {
            ChunkPos target{column.x, columnY, column.z};
            if (!world.hasChunk(target) && !world.isKnownEmpty(target))
              world.ensureChunk(target, voxelRegistry,
                                biomeRegistry); // generates, dirties neighbors
          }
        }

      for (auto chunkIterator = world.allChunks().begin();
           chunkIterator != world.allChunks().end();) {
        ChunkPos loadedPos = chunkIterator->first;
        if (horizontalDistance(loadedPos, playerChunk) > RENDER_RADIUS + 1) {
          chunkMeshes.erase(loadedPos);
          chunkIterator = world.allChunks().erase(chunkIterator);
          world.evictEmpty(loadedPos);
          for (ChunkPos neighborPos :
               {ChunkPos{loadedPos.x - 1, loadedPos.y, loadedPos.z},
                ChunkPos{loadedPos.x + 1, loadedPos.y, loadedPos.z},
                ChunkPos{loadedPos.x, loadedPos.y - 1, loadedPos.z},
                ChunkPos{loadedPos.x, loadedPos.y + 1, loadedPos.z},
                ChunkPos{loadedPos.x, loadedPos.y, loadedPos.z - 1},
                ChunkPos{loadedPos.x, loadedPos.y, loadedPos.z + 1}}) {
            Chunk *neighborChunk = world.getChunk(neighborPos);
            if (neighborChunk)
              neighborChunk->dirty = true;
          }
        } else {
          ++chunkIterator;
        }
      }
    }

    glfwPollEvents();
    input.update();
    processInput(deltaTime);

    updateUniformBuffer(currentFrame);

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin("Debug", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoInputs |
                     ImGuiWindowFlags_NoFocusOnAppearing |
                     ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground |
                     ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    glm::vec3 position = camera.getPosition();
    ImGui::Text("Position: %.2f, %.2f, %.2f", position.x, position.y,
                position.z);
    ChunkPos overlayChunk = World::toChunkPos((int)std::floor(position.x),
                                              (int)std::floor(position.y),
                                              (int)std::floor(position.z));
    ImGui::Text("Chunk: %d, %d, %d", overlayChunk.x, overlayChunk.y,
                overlayChunk.z);
    ImGui::Text("Chunks: %zu (+%zu empty known)", world.chunkCount(),
                world.knownEmptyCount());
    ImGui::Text("Seed: %i ", world.seedValue());
    ImGui::End();

    ImGui::Render();

    drawFrame();
  }
}

void VulkanApplication::cleanup() {
  cleanupImGui();
  glfwDestroyWindow(window);
  glfwTerminate();
}

void VulkanApplication::processInput(float deltaTime) {
  if (input.isKeybindActive("break_block")) {
    RayHit h =
        raycastVoxel(world, camera.getPosition(), camera.getFront(), 8.0f);
    if (h.hit)
      world.setBlock(h.x, h.y, h.z, AIR);
  }
  if (input.isKeybindActive("place_block")) {
    RayHit h =
        raycastVoxel(world, camera.getPosition(), camera.getFront(), 8.0f);
    if (h.hit)
      world.setBlock(h.prevX, h.prevY, h.prevZ, 1);
  }

  if (input.isKeybindActive("toggle_wireframe"))
    isWirframe = !isWirframe;

  glm::vec3 inputDir =
      input.inputVec3("move_right", "move_left", "move_up", "move_down",
                      "move_forward", "move_back");
  camera.processKeyboard(inputDir, deltaTime);

  camera.processMouseMovement(static_cast<float>(input.mouseDeltaX),
                              static_cast<float>(input.mouseDeltaY));
}

uint32_t VulkanApplication::findMemoryType(uint32_t typeFilter,
                                           vk::MemoryPropertyFlags properties) {
  vk::PhysicalDeviceMemoryProperties memProperties =
      physicalDevice.getMemoryProperties();

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    bool typeSupported = typeFilter & (1 << i);
    bool propertiesSupported =
        (memProperties.memoryTypes[i].propertyFlags & properties) == properties;

    if (typeSupported && propertiesSupported) {
      return i;
    }
  }

  throw std::runtime_error("failed to find suitable memory type!");
}

void VulkanApplication::createDescriptorPool() {
  std::array<vk::DescriptorPoolSize, 2> poolSizes = {
      vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer,
                             maxConcurrentFrames},
      vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler,
                             maxConcurrentFrames}};

  vk::DescriptorPoolCreateInfo poolInfo{
      .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets = maxConcurrentFrames,
      .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
      .pPoolSizes = poolSizes.data()};

  descriptorPool = device.createDescriptorPool(poolInfo);
}
