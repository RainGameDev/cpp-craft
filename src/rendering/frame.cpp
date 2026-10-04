#include "rendering/vulkan_application.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>

void VulkanApplication::createSyncObjects() {
  presentCompleteSemaphore =
      vk::raii::Semaphore(device, vk::SemaphoreCreateInfo());
  renderFinishedSemaphore =
      vk::raii::Semaphore(device, vk::SemaphoreCreateInfo());
  drawFence =
      vk::raii::Fence(device, {.flags = vk::FenceCreateFlagBits::eSignaled});
}

void VulkanApplication::drawFrame() {
  auto fenceResult = device.waitForFences(*drawFence, vk::True, UINT64_MAX);
  if (fenceResult != vk::Result::eSuccess) {
    throw std::runtime_error("failed to wait for fence!");
  }
  device.resetFences(*drawFence);

  auto [result, imageIndex] = swapChain.acquireNextImage(
      UINT64_MAX, *presentCompleteSemaphore, nullptr);

  if (result == vk::Result::eErrorOutOfDateKHR ||
      result == vk::Result::eSuboptimalKHR) {
    createSwapChain();
    createImageViews();
    createDepthResources();
    updateUniformBuffer(currentFrame % maxConcurrentFrames);
    return;
  }

  updateDirtyChunks();

  recordCommandBuffer(imageIndex, currentFrame % maxConcurrentFrames);

  queue.waitIdle();
  vk::PipelineStageFlags waitDestinationStageMask(
      vk::PipelineStageFlagBits::eColorAttachmentOutput);
  const vk::SubmitInfo submitInfo{
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &*presentCompleteSemaphore,
      .pWaitDstStageMask = &waitDestinationStageMask,
      .commandBufferCount = 1,
      .pCommandBuffers = &*commandBuffer,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &*renderFinishedSemaphore};
  queue.submit(submitInfo, *drawFence);

  const vk::PresentInfoKHR presentInfoKHR{.waitSemaphoreCount = 1,
                                          .pWaitSemaphores =
                                              &*renderFinishedSemaphore,
                                          .swapchainCount = 1,
                                          .pSwapchains = &*swapChain,
                                          .pImageIndices = &imageIndex};

  result = queue.presentKHR(presentInfoKHR);

  if (result == vk::Result::eErrorOutOfDateKHR ||
      result == vk::Result::eSuboptimalKHR || framebufferResized) {
    framebufferResized = false;
    createSwapChain();
    createImageViews();
    createDepthResources();
    updateUniformBuffer(currentFrame % maxConcurrentFrames);
    currentFrame = (currentFrame + 1) % maxConcurrentFrames;
    return;
  }

  currentFrame = (currentFrame + 1) % maxConcurrentFrames;

  switch (result) {
  case vk::Result::eSuccess:
    break;
  case vk::Result::eSuboptimalKHR:
    std::cout
        << "vk::Queue::presentKHR returned vk::Result::eSuboptimalKHR !\n";
    createSwapChain();
    createImageViews();
    createDepthResources();
    updateUniformBuffer(currentFrame % maxConcurrentFrames);
    break;
  default:
    break;
  }

  currentFrame = (currentFrame + 1) % maxConcurrentFrames;
}
