#pragma once

#ifdef WIN32
#include <Windows.h>
#define VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL 1
#define VK_USE_PLATFORM_WIN32_KHR 1
#undef min
#undef max
#undef ERROR
#endif  // WIN32
#ifdef __linux__
#define VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL 1
#define VK_USE_PLATFORM_XLIB_KHR 1
#endif
#include <chrono>
#include <mutex>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "../../../public/Module.hpp"
#include "../../DataHolder.hpp"
#include "../GameGraphicsModule.hpp"
#include "VulkanShaderModule.hpp"
#include "VulkanShaderPipe.hpp"
#undef None
#undef Bool

#define VERROR(rslt)                                               \
  if (rslt != Result::eSuccess) {                                  \
    verror = rslt;                                                 \
    main::error = main::Error::VULKAN_ERROR;                       \
    std::string s = to_string(verror);                             \
    const auto file = __FILE__;                                    \
    const auto line = __LINE__;                                    \
    printf("Vulkan error %s in %s L%d!\n", s.c_str(), file, line); \
  }  // namespace tge::graphics

namespace tge::graphics {
using namespace vk;
extern Result verror;

struct QueueSync {
  Device device;
  Fence fence;
  Queue queue;
  bool armed = false;
  std::mutex handle;

  QueueSync() : device(nullptr), fence(nullptr), queue(nullptr) {}

  QueueSync(const Device device, const Queue queue)
      : device(device), queue(queue) {
    FenceCreateInfo info{};
    fence = device.createFence(info);
  }

  void internalWaitStopWithoutUnlock() {
    handle.lock();
    if (armed) {
      const auto result2 = device.waitForFences(fence, true, INVALID_SIZE_T);
      VERROR(result2);
      device.resetFences(fence);
      armed = false;
    }
  }

  void waitAndDisarm() {
    internalWaitStopWithoutUnlock();
    handle.unlock();
  }

  void begin(const CommandBuffer buffer, const CommandBufferBeginInfo &info) {
    internalWaitStopWithoutUnlock();
    buffer.begin(info);
  }

  void end(const CommandBuffer buffer) {
    buffer.end();
    handle.unlock();
  }

  void submit(const vk::ArrayProxy<SubmitInfo> &submitInfos) {
    internalWaitStopWithoutUnlock();
    queue.submit(submitInfos, fence);
    armed = true;
    handle.unlock();
  }

  void submitAndWait(const vk::ArrayProxy<SubmitInfo> &submitInfos) {
    internalWaitStopWithoutUnlock();
    queue.submit(submitInfos, fence);
    const auto result = device.waitForFences(fence, true, INVALID_SIZE_T);
    VERROR(result);
    device.resetFences(fence);
    handle.unlock();
  }

  void endSubmitAndWait(const vk::ArrayProxy<SubmitInfo> &submitInfos) {
    for (const auto &submit : submitInfos) {
      for (size_t i = 0; i < submit.commandBufferCount; i++) {
        submit.pCommandBuffers[i].end();
      }
    }
    if (armed) {
      const auto result = device.waitForFences(fence, true, INVALID_SIZE_T);
      VERROR(result);
      device.resetFences(fence);
    }
    queue.submit(submitInfos, fence);
    const auto result = device.waitForFences(fence, true, INVALID_SIZE_T);
    VERROR(result);
    device.resetFences(fence);
    handle.unlock();
  }

  Fence getFence() { return fence; }

  void destroy() {
    std::lock_guard guard(handle);
    device.destroyFence(fence);
  }
};

constexpr size_t DATA_ONLY_BUFFER = 0;
constexpr size_t TEXTURE_ONLY_BUFFER = 1;
constexpr size_t DATA_CHANGE_ONLY_BUFFER = 2;

struct InternalImageInfo {
  Format format;
  Extent2D ex;
  ImageUsageFlags usage = ImageUsageFlagBits::eColorAttachment;
  SampleCountFlagBits sampleCount = SampleCountFlagBits::e1;
};

using BufferHolderType =
    DataHolder5<Buffer, DeviceMemory, size_t, size_t, size_t>;

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
  CommandPool guiPool;
  std::vector<CommandBuffer> cmdbuffer;
  std::vector<CommandBuffer> noneRenderCmdbuffer;
  std::vector<Pipeline> pipelines;
  uint32_t queueFamilyIndex;
  uint32_t queueIndex;
  uint32_t secondaryqueueIndex;
  Semaphore waitSemaphore;
  Semaphore signalSemaphore;
  QueueSync *primarySync = nullptr;
  QueueSync *secondarySync = nullptr;
  std::vector<ShaderModule> shaderModules;
  uint32_t memoryTypeHostVisibleCoherent;
  uint32_t memoryTypeDeviceLocal;
  vk::PhysicalDeviceLimits deviceLimits;
  BufferHolderType bufferDataHolder;

  Viewport viewport;
  std::vector<CommandBuffer> secondaryCommandBuffer;
  std::mutex commandBufferRecording;  // protects secondaryCommandBuffer from
                                      // memory invalidation
  std::vector<vk::Sampler> sampler;
  std::vector<Image> textureImages;
  std::vector<std::tuple<DeviceMemory, size_t>> textureMemorys;
  std::vector<ImageView> textureImageViews;
  std::vector<shader::ShaderPipe> shaderPipes;
  std::vector<CommandBuffer> primary = {CommandBuffer()};
  std::vector<vk::PipelineLayout> materialToLayout;
  std::vector<std::vector<RenderInfo>> renderInfosForRetry;
  std::vector<Material> materialsForRetry;
  std::vector<InternalImageInfo> internalimageInfos;

  size_t firstImage;
  size_t depthImage;
  size_t albedoImage;
  size_t normalImage;
  size_t roughnessMetallicImage;
  size_t position;
  size_t attachmentCount;

  size_t lightData;
  size_t lightPipe = INVALID_UINT32;
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

  void removeRender(const size_t renderInfoCount,
                    const TRenderHolder*renderIDs) override;

  std::vector<PipelineHolder> pushMaterials(
      const size_t materialcount, const Material *materials,
      const size_t offset = INVALID_SIZE_T) override;

  size_t pushData(const size_t dataCount, void *data, const size_t *dataSizes,
                  const DataType type) override;

  void changeData(const size_t bufferIndex, const void *data,
                  const size_t dataSizes, const size_t offset = 0) override;

  TRenderHolder pushRender(const size_t renderInfoCount,
                           const RenderInfo *renderInfos,
                    const size_t offset = 0) override;

  TSamplerHolder pushSampler(const SamplerInfo &sampler) override;

  std::vector<TTextureHolder> pushTexture(const size_t textureCount,
                     const TextureInfo *textures) override;

  size_t pushLights(const size_t lightCount, const Light *lights,
                    const size_t offset = 0) override;

  size_t getAligned(const size_t buffer,
                    const size_t toBeAligned = 0) const override;

  size_t getAligned(const DataType type) const override;

  glm::vec2 getRenderExtent() const override;

  std::vector<char> getImageData(const size_t imageId,
                                 CacheIndex *cacheIndex = nullptr) override;
};

}  // namespace tge::graphics