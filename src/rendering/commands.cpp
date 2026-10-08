#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "rendering/vulkan_application.h"

#include <array>
#include <cstdint>

void VulkanApplication::createCommandPool() {
  vk::CommandPoolCreateInfo poolInfo{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = queueIndex};
  commandPool = vk::raii::CommandPool(device, poolInfo);
}

void VulkanApplication::createCommandBuffer() {
  commandBuffers.clear();
  vk::CommandBufferAllocateInfo allocInfo{
      .commandPool = *commandPool,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = maxConcurrentFrames};
  auto allocated = vk::raii::CommandBuffers(device, allocInfo);
  for (auto &b : allocated)
    commandBuffers.emplace_back(std::move(b));
}

void VulkanApplication::recordCommandBuffer(vk::raii::CommandBuffer &cmd,
                                            uint32_t imageIndex,
                                            uint32_t frameIndex) {
  cmd.reset();
  cmd.begin({});
  transition_image_layout(cmd, imageIndex, vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eColorAttachmentOptimal, {},
                          vk::AccessFlagBits2::eColorAttachmentWrite,
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput);

  vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
  vk::RenderingAttachmentInfo attachmentInfo = {
      .imageView = swapChainImageViews[imageIndex],
      .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eStore,
      .clearValue = clearColor};

  vk::ClearValue depthClear = vk::ClearDepthStencilValue{1.0f, 0};
  vk::RenderingAttachmentInfo depthAttachmentInfo = {
      .imageView = *depthView,
      .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eStore,
      .clearValue = depthClear};

  vk::RenderingInfo renderingInfo = {
      .renderArea = {.offset = {0, 0}, .extent = swapChainExtent},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &attachmentInfo,
      .pDepthAttachment = &depthAttachmentInfo};

  cmd.beginRendering(renderingInfo);

  cmd.bindPipeline(vk::PipelineBindPoint::eGraphics,
                   isWirframe ? wireframePipeline : *graphicsPipeline);

  cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0,
                         {*descriptorSets[frameIndex]}, nullptr);

  cmd.setViewport(
      0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width),
                      static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
  cmd.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));

  auto proj = camera.getProjectionMatrix(swapChainExtent.width /
                                         (float)swapChainExtent.height);
  proj[1][1] *= -1;
  glm::mat4 vp = proj * camera.getViewMatrix();
  auto row = [&](int r) {
    return glm::vec4(vp[0][r], vp[1][r], vp[2][r], vp[3][r]);
  };
  glm::vec4 planes[6] = {row(3) + row(0), row(3) - row(0), row(3) + row(1),
                         row(3) - row(1), row(3) + row(2), row(3) - row(2)};
  for (auto &pl : planes)
    pl /= glm::length(glm::vec3(pl));

  const AtlasLayout atlas = voxelAtlas.layout();
  constexpr float pad = 2.0f;
  for (auto &kv : chunkMeshes) {
    const ChunkPos &p = kv.first;
    glm::vec3 mn(p.x * CW - pad, p.y * CH - pad, p.z * CD - pad);
    glm::vec3 mx(p.x * CW + CW + pad, p.y * CH + CH + pad, p.z * CD + CD + pad);
    bool inside = true;
    for (auto &pl : planes) {
      glm::vec3 pv(pl.x > 0 ? mx.x : mn.x, pl.y > 0 ? mx.y : mn.y,
                   pl.z > 0 ? mx.z : mn.z);
      if (glm::dot(glm::vec3(pl), pv) + pl.w < 0) {
        inside = false;
        break;
      }
    }
    if (!inside)
      continue;
    Mesh &m = kv.second;
    if (!*m.buffer || m.indexCount == 0) continue;
    const ChunkPush pc{.ox = p.x * CW,
                       .oy = p.y * CH,
                       .oz = p.z * CD,
                       .cols = atlas.columns,
                       .rows = atlas.rows};
    cmd.pushConstants<uint32_t>(*pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0,
                      std::array<uint32_t, 5>{
                          (uint32_t)pc.ox, (uint32_t)pc.oy, (uint32_t)pc.oz,
                          (uint32_t)pc.cols, (uint32_t)pc.rows});
    cmd.bindVertexBuffers(0, {*m.buffer}, {m.vertexOffset});
    cmd.bindIndexBuffer(*m.buffer, m.indexOffset, vk::IndexType::eUint32);
    cmd.drawIndexed(m.indexCount, 1, 0, 0, 0);
  }

  // render imgui
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *cmd, VK_NULL_HANDLE);

  cmd.endRendering();

  transition_image_layout(cmd, imageIndex,
                          vk::ImageLayout::eColorAttachmentOptimal,
                          vk::ImageLayout::ePresentSrcKHR,
                          vk::AccessFlagBits2::eColorAttachmentWrite, {},
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                          vk::PipelineStageFlagBits2::eBottomOfPipe);

  cmd.end();
}

void VulkanApplication::transition_image_layout(
    vk::raii::CommandBuffer &cmd, uint32_t imageIndex,
    vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
    vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask,
    vk::PipelineStageFlags2 srcStageMask,
    vk::PipelineStageFlags2 dstStageMask) {
  vk::ImageMemoryBarrier2 barrier = {
      .srcStageMask = srcStageMask,
      .srcAccessMask = srcAccessMask,
      .dstStageMask = dstStageMask,
      .dstAccessMask = dstAccessMask,
      .oldLayout = oldLayout,
      .newLayout = newLayout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = swapChainImages[imageIndex],
      .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1}};
  vk::DependencyInfo dependencyInfo = {.dependencyFlags = {},
                                       .imageMemoryBarrierCount = 1,
                                       .pImageMemoryBarriers = &barrier};
  cmd.pipelineBarrier2(dependencyInfo);
}
