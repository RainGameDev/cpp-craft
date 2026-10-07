#pragma once

#include "utils/thread_pool.h"
#include "voxels/chunk_mesher.h"
#include <cstdint>
#include <deque>
#include <mutex>
#include <queue>
#include <unordered_set>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#pragma once

#include "../assets/asset_manager.h"
#include "../utils/input_manager.h"
#include "../voxels/biome.h"
#include "../voxels/texture_atlas.h"
#include "../voxels/voxel_registry.h"
#include "../voxels/world.h"
#include "camera.h"
#include "mesh.h"
#include "ubo.h"
#include "vertex.h"
#include <unordered_map>

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

struct MeshData;

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 800;

const std::vector<char const *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

class VulkanApplication {
public:
  bool framebufferResized = false;
  void run();

private:
  GLFWwindow *window = nullptr;
  vk::raii::Context context;
  vk::raii::Instance instance = nullptr;
  vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
  vk::raii::PhysicalDevice physicalDevice = nullptr;
  vk::raii::Device device = nullptr;
  vk::PhysicalDeviceFeatures deviceFeatures;
  vk::raii::Queue graphicsQueue = nullptr;
  vk::raii::SurfaceKHR surface = nullptr;
  vk::raii::Queue queue = nullptr;
  uint32_t queueIndex = ~0;

  vk::raii::SwapchainKHR swapChain = nullptr;
  std::vector<vk::Image> swapChainImages;
  vk::SurfaceFormatKHR swapChainSurfaceFormat;
  vk::Extent2D swapChainExtent;
  std::vector<vk::raii::ImageView> swapChainImageViews;
  vk::Format depthFormat = vk::Format::eD32Sfloat;
  vk::raii::Image depthImage = nullptr;
  vk::raii::DeviceMemory depthMemory = nullptr;
  vk::raii::ImageView depthView = nullptr;

  bool isWirframe = false;

  vk::raii::PipelineLayout pipelineLayout = nullptr;
  vk::raii::Pipeline graphicsPipeline = nullptr;
  vk::raii::Pipeline wireframePipeline = nullptr;
  vk::raii::CommandPool commandPool = nullptr;
  vk::raii::CommandBuffer commandBuffer = nullptr;
  vk::raii::Semaphore presentCompleteSemaphore = nullptr;
  vk::raii::Semaphore renderFinishedSemaphore = nullptr;
  vk::raii::Fence drawFence = nullptr;
  std::vector<const char *> requiredDeviceExtension = {
      vk::KHRSwapchainExtensionName};

  std::array<UboBuffer, maxConcurrentFrames> uniformBuffers;

  vk::raii::DescriptorPool descriptorPool = nullptr;
  vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
  vk::raii::DescriptorSets descriptorSets = nullptr;

  vk::raii::DescriptorPool imguiPool = nullptr;

  InputManager input;

  Camera camera;

  uint32_t currentFrame = 0;

  World world;
  Atlas voxelAtlas;
  std::unordered_map<ChunkPos, Mesh, ChunkPosHash> chunkMeshes;
  ChunkPos lastStreamedChunk{};
  bool streamInitialized = false;
  ChunkPos currentPlayerChunk{}; // refreshed every frame in mainLoop

  struct CompletedMesh {
    ChunkPos pos;
    MeshData data;
    uint64_t version = 0;
  };
  std::mutex meshMutex;
  std::deque<CompletedMesh> completedMeshes; // deque allows closest first scan
  std::unordered_set<ChunkPos, ChunkPosHash> meshInFlight;

  struct CompletedGen {
    ChunkPos pos;
    Chunk chunk;
    bool empty = true;
  };
  std::mutex genMutex;
  std::queue<CompletedGen> completedGens;
  std::unordered_set<ChunkPos, ChunkPosHash> genInFlight;

  void initVulkan();
  void mainLoop();
  void cleanup();

  void createInstance();
  std::vector<const char *> getRequiredInstanceExtensions();
  void setupDebugMessenger();
  bool isDeviceSuitable(vk::raii::PhysicalDevice const &physicalDevice);
  void createSurface();
  void pickPhysicalDevice();
  void createLogicalDevice();
  void createSwapChain();
  uint32_t findMemoryType(uint32_t typeFilter,
                          vk::MemoryPropertyFlags properties);

  uint32_t chooseSwapMinImageCount(
      vk::SurfaceCapabilitiesKHR const &surfaceCapabilities);
  vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities);
  vk::PresentModeKHR chooseSwapPresentMode(
      std::vector<vk::PresentModeKHR> const &availablePresentModes);
  vk::SurfaceFormatKHR chooseSwapSurfaceFormat(
      const std::vector<vk::SurfaceFormatKHR> &availableFormats);
  void createImageViews();
  vk::Format findDepthFormat();
  void createDepthResources();
  void createGraphicsPipeline();
  void createCommandPool();
  void createCommandBuffer();
  void recordCommandBuffer(uint32_t imageIndex, uint32_t currentFrame);
  void transition_image_layout(uint32_t imageIndex, vk::ImageLayout old_layout,
                               vk::ImageLayout new_layout,
                               vk::AccessFlags2 src_access_mask,
                               vk::AccessFlags2 dst_access_mask,
                               vk::PipelineStageFlags2 src_stage_mask,
                               vk::PipelineStageFlags2 dst_stage_mask);

  void createUniformBuffers();
  void createDescriptorSetLayout();
  void createDescriptorSets();
  void createDescriptorPool();
  void updateUniformBuffer(uint32_t currentFrame);

  void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                    vk::MemoryPropertyFlags properties,
                    vk::raii::Buffer &buffer,
                    vk::raii::DeviceMemory &bufferMemory);
  void copyBuffer(vk::raii::Buffer &srcBuffer, vk::raii::Buffer &dstBuffer,
                  vk::DeviceSize size);
  Mesh uploadMeshData(const MeshData &meshData);

  void rebuildChunk(const ChunkPos &chunkPos);
  void updateDirtyChunks();
  void kickMeshJobs();
  void drainCompletedMeshes(int budget = 4);
  void dropMeshJobsFor(const ChunkPos &chunkPos);
  void requestGenJobs(const ChunkPos &playerChunk);
  void drainGenJobs(const ChunkPos &playerChunk, int budget = 8);
  void dropGenJobsFor(const ChunkPos &chunkPos);

  void createSyncObjects();
  void drawFrame();

  void initImGui();
  void cleanupImGui();

  void processInput(float deltaTime);
  void setupKeybinds();

  AssetManager assetManager;
  VoxelRegistry voxelRegistry;
  BiomeRegistry biomeRegistry;
  ThreadPool threadPool;

  static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
      vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
      vk::DebugUtilsMessageTypeFlagsEXT type,
      const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData, void *);
};
static void framebufferResizeCallback(GLFWwindow *window, int width,
                                      int height) {
  auto app = static_cast<VulkanApplication *>(glfwGetWindowUserPointer(window));
  app->framebufferResized = true;
}
