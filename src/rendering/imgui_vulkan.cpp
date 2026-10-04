#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "rendering/vulkan_application.h"

void VulkanApplication::initImGui() {
  // Let ImGui backend create its own descriptor pool (needs SAMPLED_IMAGE + SAMPLER since 2026-04-22 breaking change).
  imguiPool.clear();

  // Create imgui context
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGui_ImplGlfw_InitForVulkan(window, true);

  // Create imgui vulkan
  ImGui_ImplVulkan_InitInfo initInfo{};
  initInfo.ApiVersion = VK_API_VERSION_1_3;
  initInfo.Instance = *instance;
  initInfo.PhysicalDevice = *physicalDevice;
  initInfo.Device = *device;
  initInfo.QueueFamily = queueIndex;
  initInfo.Queue = *queue;
  initInfo.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE;
  initInfo.MinImageCount = 2;
  initInfo.ImageCount = static_cast<uint32_t>(swapChainImages.size());
  initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  initInfo.UseDynamicRendering = true;

  VkFormat colorFormat = static_cast<VkFormat>(swapChainSurfaceFormat.format);

  VkPipelineRenderingCreateInfoKHR pipelineRenderingInfo{};
  pipelineRenderingInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
  pipelineRenderingInfo.colorAttachmentCount = 1;
  pipelineRenderingInfo.pColorAttachmentFormats = &colorFormat;

  initInfo.UseDynamicRendering = true;
  initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = pipelineRenderingInfo;

  ImGui_ImplVulkan_Init(&initInfo);
}

void VulkanApplication::cleanupImGui() {
  device.waitIdle();
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}
