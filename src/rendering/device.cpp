#include "rendering/vulkan_application.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

bool VulkanApplication::isDeviceSuitable(
    vk::raii::PhysicalDevice const &physicalDevice) {
  bool supportsVulkan1_3 =
      physicalDevice.getProperties().apiVersion >= vk::ApiVersion13;

  auto queueFamilies = physicalDevice.getQueueFamilyProperties();
  bool supportsGraphics =
      std::ranges::any_of(queueFamilies, [](auto const &qfp) {
        return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics);
      });

  auto availableDeviceExtensions =
      physicalDevice.enumerateDeviceExtensionProperties();
  bool supportsAllRequiredExtensions = std::ranges::all_of(
      requiredDeviceExtension,
      [&availableDeviceExtensions](auto const &requiredDeviceExtension) {
        return std::ranges::any_of(
            availableDeviceExtensions,
            [requiredDeviceExtension](auto const &availableDeviceExtension) {
              return strcmp(availableDeviceExtension.extensionName,
                            requiredDeviceExtension) == 0;
            });
      });

  auto features = physicalDevice.template getFeatures2<
      vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
      vk::PhysicalDeviceVulkan13Features,
      vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
  bool supportsRequiredFeatures =
      features.template get<vk::PhysicalDeviceVulkan11Features>()
          .shaderDrawParameters &&
      features.template get<vk::PhysicalDeviceVulkan13Features>()
          .dynamicRendering &&
      features.template get<vk::PhysicalDeviceVulkan13Features>()
          .synchronization2 &&
      features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>()
          .extendedDynamicState;

  return supportsVulkan1_3 && supportsGraphics &&
         supportsAllRequiredExtensions && supportsRequiredFeatures;
}

void VulkanApplication::createSurface() {
  VkSurfaceKHR _surface;
  if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0) {
    throw std::runtime_error("failed to create window surface!");
  }
  surface = vk::raii::SurfaceKHR(instance, _surface);
}

void VulkanApplication::pickPhysicalDevice() {
  std::vector<vk::raii::PhysicalDevice> physicalDevices =
      instance.enumeratePhysicalDevices();
  auto const devIter =
      std::ranges::find_if(physicalDevices, [&](auto const &physicalDevice) {
        return isDeviceSuitable(physicalDevice);
      });
  if (devIter == physicalDevices.end()) {
    throw std::runtime_error("failed to find a suitable GPU!");
  }
  physicalDevice = *devIter;
}

void VulkanApplication::createLogicalDevice() {
  std::vector<vk::QueueFamilyProperties> queueFamilyProperties =
      physicalDevice.getQueueFamilyProperties();

  for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size();
       qfpIndex++) {
    if ((queueFamilyProperties[qfpIndex].queueFlags &
         vk::QueueFlagBits::eGraphics) &&
        physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface)) {
      queueIndex = qfpIndex;
      break;
    }
  }

  if (queueIndex == ~0u) {
    throw std::runtime_error(
        "Could not find a queue for graphics and present -> terminating");
  }

  vk::StructureChain<vk::PhysicalDeviceFeatures2,
                     vk::PhysicalDeviceVulkan11Features,
                     vk::PhysicalDeviceVulkan13Features,
                     vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
      featureChain = {{.features = {.samplerAnisotropy = true}},
                      {.shaderDrawParameters = true},
                      {.synchronization2 = true, .dynamicRendering = true},
                      {.extendedDynamicState = true}};

  float queuePriority = 0.5f;
  vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
      .queueFamilyIndex = queueIndex,
      .queueCount = 1,
      .pQueuePriorities = &queuePriority};

  vk::DeviceCreateInfo deviceCreateInfo{
      .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &deviceQueueCreateInfo,
      .enabledExtensionCount =
          static_cast<uint32_t>(requiredDeviceExtension.size()),
      .ppEnabledExtensionNames = requiredDeviceExtension.data()};

  device = vk::raii::Device(physicalDevice, deviceCreateInfo);
  queue = vk::raii::Queue(device, queueIndex, 0);
}
