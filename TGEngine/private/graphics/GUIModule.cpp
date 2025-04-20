#include "../../public/graphics/GUIModule.hpp"

#include <vulkan/vulkan.hpp>

#include "../../public/TGEngine.hpp"
#include "../../public/graphics/WindowModule.hpp"
#include "../../public/graphics/vulkan/VulkanModuleDef.hpp"
//
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
//

namespace tge::gui {

using namespace vk;

inline void render(gui::GUIModule* gmod) {
  const RenderPass pass((VkRenderPass)gmod->renderpass);
  auto vgm = (graphics::VulkanGraphicsModule*)main::getAPILayer()->backend();
  const CommandBuffer buffer = vgm->cmdbuffer[gmod->buffer + vgm->nextImage];
  const Framebuffer frame = ((Framebuffer*)gmod->framebuffer)[vgm->nextImage];

  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();

  ImGui::NewFrame();

  gmod->renderGUI();

  ImGui::Render();
  ImDrawData* draw_data = ImGui::GetDrawData();

  const CommandBufferBeginInfo beginInfo;
  auto guard = vgm->primarySync->begin(buffer, beginInfo);

  constexpr std::array clearColor = {1.0f, 1.0f, 1.0f, 1.0f};

  const std::array clearValue = {ClearValue(clearColor),
                                 ClearValue(ClearDepthStencilValue(0.0f, 0))};

  const RenderPassBeginInfo renderPassBeginInfo(
      pass, frame,
      {{0, 0}, {(uint32_t)vgm->viewport.width, (uint32_t)vgm->viewport.height}},
      clearValue);
  buffer.beginRenderPass(renderPassBeginInfo, {});
  ImGui_ImplVulkan_RenderDrawData(draw_data, (VkCommandBuffer)buffer);
  buffer.endRenderPass();

  vgm->primary[gmod->primary] = buffer;
  vgm->primarySync->end(buffer, std::move(guard));
}

main::Error GUIModule::init() {
  auto winModule = main::getGameGraphicsModule()->getWindowModule();
  auto api = main::getAPILayer();
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  ImGui::StyleColorsDark();
  const auto vmod = (graphics::VulkanGraphicsModule*)api->backend();

  const std::array attachments = {AttachmentDescription(
      {}, vmod->format.format, SampleCountFlagBits::e1, AttachmentLoadOp::eLoad,
      AttachmentStoreOp::eStore, AttachmentLoadOp::eDontCare,
      AttachmentStoreOp::eDontCare, ImageLayout::ePresentSrcKHR,
      ImageLayout::ePresentSrcKHR)};

  constexpr std::array colorAttachments = {
      AttachmentReference(0, ImageLayout::eColorAttachmentOptimal)};

  const std::array subpassDescriptions = {SubpassDescription(
      {}, PipelineBindPoint::eGraphics, {}, colorAttachments)};

  const std::array subpassDependencies = {SubpassDependency(
      VK_SUBPASS_EXTERNAL, 0, PipelineStageFlagBits::eColorAttachmentOutput,
      PipelineStageFlagBits::eColorAttachmentOutput, (AccessFlagBits)0,
      AccessFlagBits::eColorAttachmentWrite |
          AccessFlagBits::eColorAttachmentRead)};

  const RenderPassCreateInfo renderPassCreateInfo(
      {}, attachments, subpassDescriptions, subpassDependencies);
  this->renderpass =
      (void*)VkRenderPass(vmod->device.createRenderPass(renderPassCreateInfo));

  GUIModule::recreate();

  VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};
  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
  pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
  pool_info.pPoolSizes = pool_sizes;
  const auto result = vkCreateDescriptorPool(
      (VkDevice)vmod->device, &pool_info, nullptr, ((VkDescriptorPool*)&pool));
  if (result != VK_SUCCESS) return main::Error::VULKAN_ERROR;

  ImGui_ImplVulkan_InitInfo instinfo = {VK_API_VERSION_1_0,
                                        (VkInstance)vmod->instance,
                                        (VkPhysicalDevice)vmod->physicalDevice,
                                        (VkDevice)vmod->device,
                                        vmod->queueFamilyIndex,
                                        (VkQueue)vmod->primarySync->queue,
                                        (VkDescriptorPool)pool,
                                        (VkRenderPass)this->renderpass,
                                        3,
                                        3,
                                        VK_SAMPLE_COUNT_1_BIT,
                                        nullptr,
                                        0,
                                        0,
                                        false,
                                        {},
                                        nullptr,
                                        [](VkResult rslt) {
                                          if (rslt != VK_SUCCESS)
                                            printf("ERROR IN VK");
                                        },
                                        1024 * 1024};

  if (!ImGui_ImplVulkan_Init(&instinfo))
    return main::Error::COULD_NOT_CREATE_WINDOW_CLASS;
  
  if(!ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)winModule->hWnd, true))
      return main::Error::COULD_NOT_CREATE_WINDOW;

  const CommandPoolCreateInfo poolCreateInfo(
      CommandPoolCreateFlagBits::eResetCommandBuffer, vmod->queueFamilyIndex);
  vmod->guiPool = vmod->device.createCommandPool(poolCreateInfo);

  const CommandBufferAllocateInfo info(vmod->guiPool,
                                       CommandBufferLevel::ePrimary, 1);
  const auto sCmd = vmod->device.allocateCommandBuffers(info).back();
  const auto beginInfo =
      CommandBufferBeginInfo(CommandBufferUsageFlagBits::eOneTimeSubmit);
  sCmd.begin(beginInfo);
  ImGui_ImplVulkan_CreateFontsTexture();
  sCmd.end();

  const auto submitInfo = SubmitInfo({}, {}, sCmd, {});
  vmod->primarySync->submitAndWait(submitInfo);

  vmod->device.waitIdle();

  const auto allocInfo =
      CommandBufferAllocateInfo(vmod->guiPool, CommandBufferLevel::ePrimary,
                                vmod->swapchainImages.size());
  this->buffer = vmod->cmdbuffer.size();
  for (const auto buffer : vmod->device.allocateCommandBuffers(allocInfo)) {
    vmod->cmdbuffer.push_back(buffer);
  }

  this->primary = vmod->primary.size();
  vmod->primary.push_back(vmod->cmdbuffer[this->buffer]);

  return main::Error::NONE;
}

void GUIModule::tick(double deltatime) { render(this); }

void GUIModule::destroy() {
  const auto vmod =
      (graphics::VulkanGraphicsModule*)main::getAPILayer()->backend();
  vmod->device.waitIdle();
  ImGui_ImplVulkan_Shutdown();
  vmod->device.destroyDescriptorPool(
      vk::DescriptorPool((VkDescriptorPool)pool));
  vmod->device.destroyRenderPass(
      vk::RenderPass((VkRenderPass)this->renderpass));
  for (size_t i = 0; i < vmod->swapchainImageviews.size(); i++) {
    vmod->device.destroyFramebuffer(((Framebuffer*)framebuffer)[i]);
  }
  delete[] (Framebuffer*)framebuffer;
}

void GUIModule::recreate() {
  const auto vmod =
      (graphics::VulkanGraphicsModule*)main::getAPILayer()->backend();

  if (framebuffer != nullptr) {
    for (size_t i = 0; i < vmod->swapchainImageviews.size(); i++) {
      vmod->device.destroyFramebuffer(((Framebuffer*)framebuffer)[i]);
    }
    delete[] (Framebuffer*)framebuffer;
  }
  framebuffer = new Framebuffer[vmod->swapchainImageviews.size()];

  for (size_t i = 0; i < vmod->swapchainImageviews.size(); i++) {
    const auto imview = vmod->swapchainImageviews[i];
    const FramebufferCreateInfo framebufferCreateInfo(
        {}, vk::RenderPass((VkRenderPass)renderpass), imview,
        vmod->viewport.width, vmod->viewport.height, 1);
    ((Framebuffer*)framebuffer)[i] =
        vmod->device.createFramebuffer(framebufferCreateInfo);
  }
}

}  // namespace tge::gui
