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

namespace std {
    template<>
    struct hash<vk::DeviceMemory> {
        [[nodiscard]] inline std::size_t operator()(
            const vk::DeviceMemory& s) const noexcept {
            const std::hash<std::size_t> test;
            return test((size_t)(VkDeviceMemory)s);
        }
    };
}

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

  [[nodiscard]] std::unique_lock<std::mutex> waitAndGet() {
    std::unique_lock guard(handle);
    if (armed) {
      const auto result2 = device.waitForFences(fence, true, INVALID_SIZE_T);
      VERROR(result2);
      device.resetFences(fence);
      armed = false;
    }
    return std::move(guard);
  }

  void waitAndDisarm() { auto guard = waitAndGet(); }

  [[nodiscard]] std::unique_lock<std::mutex> begin(
      const CommandBuffer buffer, const CommandBufferBeginInfo &info) {
    auto guard = waitAndGet();
    buffer.begin(info);
    return std::move(guard);
  }

  void end(const CommandBuffer buffer, std::unique_lock<std::mutex> &&lock) {
    buffer.end();
  }

  void submit(const vk::ArrayProxy<SubmitInfo> &submitInfos) {
    auto guard = waitAndGet();
    queue.submit(submitInfos, fence);
    armed = true;
  }

  void submitAndWait(const vk::ArrayProxy<SubmitInfo> &submitInfos) {
    auto guard = waitAndGet();
    queue.submit(submitInfos, fence);
    const auto result = device.waitForFences(fence, true, INVALID_SIZE_T);
    VERROR(result);
    device.resetFences(fence);
  }

  void endSubmitAndWait(const vk::ArrayProxy<SubmitInfo> &submitInfos,
                        std::unique_lock<std::mutex> &&lock) {
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
  }

  Fence getFence() { return fence; }

  void destroy() {
    std::lock_guard guard(handle);
    device.destroyFence(fence);
    fence = nullptr;
  }
};

constexpr size_t DATA_ONLY_BUFFER = 0;
constexpr size_t TEXTURE_ONLY_BUFFER = 1;
constexpr size_t DATA_CHANGE_ONLY_BUFFER = 2;

struct InternalImageInfo {
  Format format = Format::eUndefined;
  Extent2D extent;
  ImageUsageFlags usage = ImageUsageFlagBits::eColorAttachment;
  SampleCountFlagBits sampleCount = SampleCountFlagBits::e1;
  size_t mipmapCount = 1;
  std::string debugInfo;
};

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
  std::vector<char> needsRefresh;

  DataHolder<vk::Pipeline, vk::PipelineLayout, Material> materialHolder;

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
  DataHolder<vk::Buffer, vk::DeviceMemory, size_t, size_t, size_t>
      bufferDataHolder;

  std::unordered_map<vk::DeviceMemory, size_t> memoryCounter;

  DataHolder<vk::CommandBuffer, std::vector<RenderInfo>,
             std::shared_ptr<std::mutex>, std::vector<TDataHolder>,
             std::vector<TPipelineHolder>, RenderTarget>
      secondaryCommandBuffer;

  DataHolder<vk::Image, vk::ImageView, vk::DeviceMemory, size_t,
             InternalImageInfo>
      textureImageHolder;

  std::vector<vk::Sampler> sampler;
  Viewport viewport;
  std::vector<shader::ShaderPipe> shaderPipes;
  std::vector<CommandBuffer> primary = {CommandBuffer()};

  std::vector<TRenderHolder> renderInfosForRetry;
  std::mutex renderInfosForRetryHolder;

  std::array<TTextureHolder, 6> internalImageData;

  size_t attachmentCount;

  TDataHolder lightData;
  TPipelineHolder lightPipe;
  shader::TBindingHolder lightBindings{};
  Material lightMat;
  std::vector<PipelineShaderStageCreateInfo> lightCreateInfos;

  uint32_t nextImage = 0;

#ifdef DEBUG
  DebugUtilsMessengerEXT debugMessenger;
  bool debugEnabled = false;
  std::unordered_map<vk::DeviceMemory, std::string> memoryDebugTags;
#endif
  detail::DispatchLoaderDynamic dynamicLoader;

  bool isInitialiazed = false;
  bool exitFailed = false;

  struct Lights {
    Light lights[50];
    int lightCount;
  } lights;


  VulkanGraphicsModule() : APILayer(new shader::VulkanShaderModule(this)) {}

  main::Error init() override;

  void tick(double time) override;

  void destroy() override;

  void removeRender(const size_t renderInfoCount,
                    const TRenderHolder *renderIDs) override;

  void removeData(const std::span<const TDataHolder> dataHolder,
                  bool instant = false) override;

  void removeTextures(const std::span<const TTextureHolder> textureHolder,
                      bool instant = false) override;

  void removeSampler(const std::span<const TSamplerHolder> samplerHolder,
                     bool instant = false) override;

  void removeMaterials(const std::span<const TPipelineHolder> pipelineHolder,
                       bool instant = false) override;

  void hideRender(const std::span<const TRenderHolder> renderIDs, bool hide) override;

  std::vector<TPipelineHolder> pushMaterials(
      const size_t materialcount, const Material *materials) override;

  std::vector<TDataHolder> pushData(const size_t dataCount,
                                    const BufferInfo *bufferInfo,
                                    const std::string &debugTag = "Unknown") override;

  void changeData(const size_t sizes, const BufferChange *changeInfos) override;

  TRenderHolder pushRender(const size_t renderInfoCount,
                           const RenderInfo *renderInfos,
                           const TRenderHolder toOverride = TRenderHolder(),
                           const RenderTarget target = RenderTarget::OPAQUE_TARGET) override;

  TSamplerHolder pushSampler(const SamplerInfo &sampler) override;

  std::vector<TTextureHolder> pushTexture(const size_t textureCount,
                                          const TextureInfo *textures) override;

  size_t pushLights(const size_t lightCount, const Light *lights,
                    const size_t offset = 0) override;

  size_t getAligned(const TDataHolder buffer,
                    const size_t toBeAligned = 0) override;

  size_t getAligned(const DataType type) const override;

  glm::vec2 getRenderExtent() const override;

  virtual std::pair<std::vector<char>, TDataHolder> getImageData(
      const TTextureHolder imageId,
      const TDataHolder cache = TDataHolder()) override;
};

}  // namespace tge::graphics