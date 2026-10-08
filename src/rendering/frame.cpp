#include "rendering/vulkan_application.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>

void VulkanApplication::createSyncObjects() {
  presentCompleteSemaphores.clear();
  renderFinishedSemaphores.clear();
  drawFences.clear();
  for (uint32_t i = 0; i < maxConcurrentFrames; ++i) {
    presentCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
    renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
    drawFences.emplace_back(
        device,
        vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
  }
}

void VulkanApplication::drawFrame() {
  const uint32_t frameIndex = currentFrame % maxConcurrentFrames;

  // ait for this frame's fence
  auto fenceResult =
      device.waitForFences(*drawFences[frameIndex], vk::True, UINT64_MAX);
  if (fenceResult != vk::Result::eSuccess) {
    throw std::runtime_error("failed to wait for fence!");
  }

  // NOTE: fence is reset only after a successful acquire
  auto [result, imageIndex] = swapChain.acquireNextImage(
      UINT64_MAX, *presentCompleteSemaphores[frameIndex], nullptr);

  if (result == vk::Result::eErrorOutOfDateKHR ||
      result == vk::Result::eSuboptimalKHR) {
    createSwapChain();
    createImageViews();
    createDepthResources();
    updateUniformBuffer(frameIndex);
    return;
  }
  device.resetFences(*drawFences[frameIndex]);

  // UBO is per-frame: safe to write now that our fence is unsignaled-reset
  // (previous use of this slot has completed).
  updateUniformBuffer(frameIndex);
  updateDirtyChunks();

  recordCommandBuffer(commandBuffers[frameIndex], imageIndex, frameIndex);

  vk::PipelineStageFlags waitDestinationStageMask(
      vk::PipelineStageFlagBits::eColorAttachmentOutput);
  vk::Semaphore waitSem = *presentCompleteSemaphores[frameIndex];
  vk::Semaphore signalSem = *renderFinishedSemaphores[frameIndex];
  vk::CommandBuffer rawCmd = *commandBuffers[frameIndex];
  const vk::SubmitInfo submitInfo{.waitSemaphoreCount = 1,
                                  .pWaitSemaphores = &waitSem,
                                  .pWaitDstStageMask =
                                      &waitDestinationStageMask,
                                  .commandBufferCount = 1,
                                  .pCommandBuffers = &rawCmd,
                                  .signalSemaphoreCount = 1,
                                  .pSignalSemaphores = &signalSem};
  queue.submit(submitInfo, *drawFences[frameIndex]);

  const vk::PresentInfoKHR presentInfoKHR{.waitSemaphoreCount = 1,
                                          .pWaitSemaphores = &signalSem,
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
    currentFrame = (currentFrame + 1) % maxConcurrentFrames;
    return;
  }

  switch (result) {
  case vk::Result::eSuccess:
    break;
  case vk::Result::eSuboptimalKHR:
    std::cout
        << "vk::Queue::presentKHR returned vk::Result::eSuboptimalKHR !\n";
    createSwapChain();
    createImageViews();
    createDepthResources();
    break;
  default:
    break;
  }

  currentFrame = (currentFrame + 1) % maxConcurrentFrames;
}
