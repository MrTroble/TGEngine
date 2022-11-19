#pragma once

#ifdef WIN32
#include <Windows.h>
#define VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL 1
#define VK_USE_PLATFORM_WIN32_KHR 1
#undef min
#undef max
#undef ERROR
#endif // WIN32
#ifdef __linux__
#define VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL 1
#define VK_USE_PLATFORM_XLIB_KHR 1
#endif
#include "../../../public/Module.hpp"
#include "../GameGraphicsModule.hpp"
#include "VulkanShaderModule.hpp"
#include "VulkanShaderPipe.hpp"
#include <vector>
#include <vulkan/vulkan.hpp>
#undef None
#undef Bool

namespace tge::graphics {

using namespace vk;

constexpr size_t DATA_ONLY_BUFFER = 0;
constexpr size_t TEXTURE_ONLY_BUFFER = 1;

class VulkanGraphicsModule : public APILayer {

public:
  Instance instance;
  PhysicalDevice physicalDevice;
  Device device;
  SurfaceKHR surface;
  SurfaceFormatKHR format;
  PresentModeKHR presentMode;
  Format depthFormat = Format::eUndefined;
  SwapchainKHR swapchain;
  std::vector<Image> swapchainImages;
  RenderPass renderpass;
  std::vector<ImageView> swapchainImageviews;
  std::vector<Framebuffer> framebuffer;
  CommandPool pool;
  CommandPool secondaryPool;
  CommandPool secondaryBufferPool;
  std::vector<CommandBuffer> cmdbuffer;
  std::vector<CommandBuffer> noneRenderCmdbuffer;
  std::vector<Pipeline> pipelines;
  Queue queue;
  Queue secondaryQueue;
  uint32_t queueFamilyIndex;
  uint32_t queueIndex;
  uint32_t secondaryqueueIndex;
  Semaphore waitSemaphore;
  Semaphore signalSemaphore;
  Fence commandBufferFence;
  Fence secondaryBufferFence;
  std::vector<ShaderModule> shaderModules;
  uint32_t memoryTypeHostVisibleCoherent;
  uint32_t memoryTypeDeviceLocal;
  std::vector<Buffer> bufferList;
  std::vector<size_t> bufferSizeList;
  std::vector<size_t> alignment;
  std::vector<DeviceMemory> bufferMemoryList;
  Viewport viewport;
  std::vector<CommandBuffer> secondaryCommandBuffer;
  std::mutex commandBufferRecording; // protects secondaryCommandBuffer from
                                     // memory invalidation
  std::mutex protectSecondaryData; // protects secondaryCommandBuffer from
  // memory invalidation
  std::vector<Sampler> sampler;
  std::vector<Image> textureImages;
  std::vector<std::tuple<DeviceMemory, size_t>> textureMemorys;
  std::vector<ImageView> textureImageViews;
  std::vector<shader::ShaderPipe> shaderPipes;
  std::vector<CommandBuffer> primary = {CommandBuffer()};
  std::vector<vk::PipelineLayout> materialToLayout;
  std::vector<std::vector<RenderInfo>> renderInfosForRetry;
  std::vector<Material> materialsForRetry;

  size_t firstImage;
  size_t depthImage;
  size_t albedoImage;
  size_t normalImage;
  size_t roughnessMetallicImage;
  size_t position;
  size_t attachmentCount;

  size_t lightData;
  size_t lightPipe = UINT32_MAX;
  size_t lightBindings;
  Material lightMat;

  uint32_t nextImage = 0;

  bool isInitialiazed = false;
  bool exitFailed = false;

  struct Lights {
    Light lights[50];
    int lightCount;
  } lights;

#ifdef DEBUG
  DebugUtilsMessengerEXT debugMessenger;
#endif

  VulkanGraphicsModule() : APILayer(new shader::VulkanShaderModule(this)) {}

  main::Error init() override;

  void tick(double time) override;

  void destroy() override;

  size_t pushMaterials(const size_t materialcount,
                       const Material *materials, const size_t offset = SIZE_MAX) override;

  size_t pushData(const size_t dataCount, void *data, const size_t *dataSizes,
                  const DataType type) override;

  void changeData(const size_t bufferIndex, const void *data,
                  const size_t dataSizes, const size_t offset = 0) override;

  void pushRender(const size_t renderInfoCount, const RenderInfo *renderInfos,
                  const size_t offset = 0) override;

  size_t pushSampler(const SamplerInfo &sampler) override;

  size_t pushTexture(const size_t textureCount,
                     const TextureInfo *textures) override;

  size_t pushLights(const size_t lightCount, const Light *lights,
                    const size_t offset = 0) override;

  void *loadShader(const MaterialType type) override;

  size_t getAligned(const size_t buffer, const size_t toBeAligned=0) const override;

  size_t getAligned(const DataType type) const  override;

  glm::vec2 getRenderExtent() const override;
};

} // namespace tge::graphics