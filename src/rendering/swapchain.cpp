#include "rendering/vulkan_application.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>

void VulkanApplication::createSwapChain() {
  device.waitIdle();
  vk::SurfaceCapabilitiesKHR surfaceCapabilities =
      physicalDevice.getSurfaceCapabilitiesKHR(*surface);
  swapChainExtent = chooseSwapExtent(surfaceCapabilities);
  uint32_t minImageCount = chooseSwapMinImageCount(surfaceCapabilities);

  std::vector<vk::SurfaceFormatKHR> availableFormats =
      physicalDevice.getSurfaceFormatsKHR(*surface);
  swapChainSurfaceFormat = chooseSwapSurfaceFormat(availableFormats);

  std::vector<vk::PresentModeKHR> availablePresentModes =
      physicalDevice.getSurfacePresentModesKHR(*surface);
  vk::PresentModeKHR presentMode = chooseSwapPresentMode(availablePresentModes);

  vk::SwapchainCreateInfoKHR swapChainCreateInfo{
      .surface = *surface,
      .minImageCount = minImageCount,
      .imageFormat = swapChainSurfaceFormat.format,
      .imageColorSpace = swapChainSurfaceFormat.colorSpace,
      .imageExtent = swapChainExtent,
      .imageArrayLayers = 1,
      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
      .imageSharingMode = vk::SharingMode::eExclusive,
      .preTransform = surfaceCapabilities.currentTransform,
      .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
      .presentMode = presentMode,
      .clipped = true,
      .oldSwapchain = *swapChain};

  swapChain = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
  swapChainImages = swapChain.getImages();
}

uint32_t VulkanApplication::chooseSwapMinImageCount(
    vk::SurfaceCapabilitiesKHR const &surfaceCapabilities) {
  auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
  if ((0 < surfaceCapabilities.maxImageCount) &&
      (surfaceCapabilities.maxImageCount < minImageCount)) {
    minImageCount = surfaceCapabilities.maxImageCount;
  }
  return minImageCount;
}

vk::Extent2D VulkanApplication::chooseSwapExtent(
    vk::SurfaceCapabilitiesKHR const &capabilities) {
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  }
  int width, height;
  glfwGetFramebufferSize(window, &width, &height);

  return {std::clamp<uint32_t>(width, capabilities.minImageExtent.width,
                               capabilities.maxImageExtent.width),
          std::clamp<uint32_t>(height, capabilities.minImageExtent.height,
                               capabilities.maxImageExtent.height)};
}

vk::PresentModeKHR VulkanApplication::chooseSwapPresentMode(
    std::vector<vk::PresentModeKHR> const &availablePresentModes) {
  assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) {
    return presentMode == vk::PresentModeKHR::eFifo;
  }));
  return std::ranges::any_of(availablePresentModes,
                             [](const vk::PresentModeKHR value) {
                               return vk::PresentModeKHR::eMailbox == value;
                             })
             ? vk::PresentModeKHR::eMailbox
             : vk::PresentModeKHR::eFifo;
}

vk::SurfaceFormatKHR VulkanApplication::chooseSwapSurfaceFormat(
    const std::vector<vk::SurfaceFormatKHR> &availableFormats) {
  const auto formatIt =
      std::ranges::find_if(availableFormats, [](const auto &format) {
        return format.format == vk::Format::eB8G8R8A8Srgb &&
               format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
      });
  return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
}

void VulkanApplication::createImageViews() {
  swapChainImageViews.clear();

  vk::ImageViewCreateInfo imageViewCreateInfo{
      .viewType = vk::ImageViewType::e2D,
      .format = swapChainSurfaceFormat.format,
      .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
  imageViewCreateInfo.components = {
      vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity,
      vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity};
  imageViewCreateInfo.subresourceRange = {.aspectMask =
                                              vk::ImageAspectFlagBits::eColor,
                                          .levelCount = 1,
                                          .layerCount = 1};
  for (auto &image : swapChainImages) {
    imageViewCreateInfo.image = image;
    swapChainImageViews.emplace_back(device, imageViewCreateInfo);
  }
}
