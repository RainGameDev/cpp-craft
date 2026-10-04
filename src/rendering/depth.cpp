#include "rendering/vulkan_application.h"

vk::Format VulkanApplication::findDepthFormat() {
  const vk::Format candidates[] = {vk::Format::eD32Sfloat,
                                  vk::Format::eD32SfloatS8Uint,
                                  vk::Format::eD24UnormS8Uint};
  for (vk::Format candidate : candidates) {
    vk::FormatProperties properties =
        physicalDevice.getFormatProperties(candidate);
    if ((properties.optimalTilingFeatures &
         vk::FormatFeatureFlagBits::eDepthStencilAttachment) ==
        vk::FormatFeatureFlagBits::eDepthStencilAttachment)
      return candidate;
  }
  throw std::runtime_error("no supported depth format");
}

void VulkanApplication::createDepthResources() {
  device.waitIdle();
  depthFormat = findDepthFormat();

  vk::ImageCreateInfo imageInfo{
      .imageType = vk::ImageType::e2D,
      .format = depthFormat,
      .extent = {swapChainExtent.width, swapChainExtent.height, 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = vk::SampleCountFlagBits::e1,
      .tiling = vk::ImageTiling::eOptimal,
      .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
      .sharingMode = vk::SharingMode::eExclusive,
      .initialLayout = vk::ImageLayout::eUndefined};
  depthImage = vk::raii::Image(device, imageInfo);

  vk::MemoryRequirements memoryRequirements = depthImage.getMemoryRequirements();
  vk::MemoryAllocateInfo allocInfo{
      .allocationSize = memoryRequirements.size,
      .memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits,
                                       vk::MemoryPropertyFlagBits::eDeviceLocal)};
  depthMemory = vk::raii::DeviceMemory(device, allocInfo);
  depthImage.bindMemory(*depthMemory, 0);

  vk::ImageViewCreateInfo viewInfo{
      .image = *depthImage,
      .viewType = vk::ImageViewType::e2D,
      .format = depthFormat,
      .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eDepth,
                           .levelCount = 1,
                           .layerCount = 1}};
  depthView = vk::raii::ImageView(device, viewInfo);

  vk::CommandBufferAllocateInfo commandAllocInfo{
      .commandPool = *commandPool,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = 1};
  auto transitionCommands = vk::raii::CommandBuffers(device, commandAllocInfo);
  auto transitionCommand = std::move(transitionCommands.front());
  transitionCommand.begin(
      {.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

  vk::ImageMemoryBarrier2 barrier{
      .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
      .srcAccessMask = vk::AccessFlagBits2::eNone,
      .dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests,
      .dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
      .oldLayout = vk::ImageLayout::eUndefined,
      .newLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = *depthImage,
      .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eDepth,
                           .levelCount = 1,
                           .layerCount = 1}};
  vk::DependencyInfo dependencyInfo{.imageMemoryBarrierCount = 1,
                                    .pImageMemoryBarriers = &barrier};
  transitionCommand.pipelineBarrier2(dependencyInfo);
  transitionCommand.end();

  vk::SubmitInfo submitInfo{.commandBufferCount = 1,
                            .pCommandBuffers = &*transitionCommand};
  queue.submit(submitInfo, nullptr);
  queue.waitIdle();
}
