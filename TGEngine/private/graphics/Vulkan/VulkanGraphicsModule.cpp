#include "../../../public/graphics/vulkan/VulkanGraphicsModule.hpp"

#include <array>
#include <iostream>
#include <mutex>
#include <ranges>
#include <unordered_set>

#define DEBUG 1

#include "../../../public/Error.hpp"
#include "../../../public/Util.hpp"
#include "../../../public/graphics/WindowModule.hpp"
#define VULKAN_HPP_HAS_SPACESHIP_OPERATOR

#include "../../../public/Error.hpp"
#include "../../../public/TGEngine.hpp"
#include "../../../public/graphics/vulkan/VulkanModuleDef.hpp"
#include "../../../public/graphics/vulkan/VulkanShaderModule.hpp"

namespace tge::graphics {

using namespace tge::shader;

constexpr std::array layerToEnable = {"VK_LAYER_KHRONOS_validation"};

constexpr std::array extensionToEnable = {VK_KHR_SURFACE_EXTENSION_NAME
#ifdef WIN32
                                          ,
                                          VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#endif  // WIN32
#ifdef __linux__
                                          ,
                                          VK_KHR_XLIB_SURFACE_EXTENSION_NAME
#endif  // __linux__
#ifdef DEBUG
                                          ,
                                          VK_EXT_DEBUG_UTILS_EXTENSION_NAME
#endif

};

using namespace vk;

Result verror = Result::eSuccess;

inline void waitForImageTransition(
    const CommandBuffer& curBuffer, const ImageLayout oldLayout,
    const ImageLayout newLayout, const Image image,
    const ImageSubresourceRange& subresource,
    const PipelineStageFlags srcFlags = PipelineStageFlagBits::eTopOfPipe,
    const AccessFlags srcAccess = AccessFlagBits::eNoneKHR,
    const PipelineStageFlags dstFlags = PipelineStageFlagBits::eAllGraphics,
    const AccessFlags dstAccess = AccessFlagBits::eNoneKHR) {
  const ImageMemoryBarrier imageMemoryBarrier(
      srcAccess, dstAccess, oldLayout, newLayout, VK_QUEUE_FAMILY_IGNORED,
      VK_QUEUE_FAMILY_IGNORED, image, subresource);
  curBuffer.pipelineBarrier(srcFlags, dstFlags, DependencyFlagBits::eByRegion,
                            {}, {}, imageMemoryBarrier);
}

#define EXPECT(assertion)                                                 \
  if (!this->isInitialiazed || !(assertion)) {                            \
    throw std::runtime_error(std::string("Debug assertion failed! ") +    \
                             __FILE__ + " L" + std::to_string(__LINE__)); \
  }

size_t VulkanGraphicsModule::getAligned(const DataType type) const {
  const auto properties = this->physicalDevice.getProperties();
  switch (type) {
    case DataType::All:
    case DataType::Uniform:
      return properties.limits.minUniformBufferOffsetAlignment;
    case DataType::VertexData:
    case DataType::IndexData:
    case DataType::VertexIndexData:
      return 0;
    default:
      break;
  }
  throw std::runtime_error("Not implemented!");
}

size_t VulkanGraphicsModule::getAligned(const TDataHolder buffer,
                                        const size_t toBeAligned) {
  const auto align = bufferDataHolder.get<4>(buffer);
  const auto rest = toBeAligned % align;
  return toBeAligned + (align - rest);
}

inline void getOrCreate(
    VulkanGraphicsModule* module, VulkanShaderPipe* shaderPipe,
    std::vector<PipelineShaderStageCreateInfo>& currentStages) {
  if (shaderPipe->modules.empty()) {
    std::lock_guard guard(shaderPipe->pipeMutex);
    for (size_t index = 0; index < shaderPipe->shader.size(); index++) {
      const auto& shaderPair = shaderPipe->shader[index];
      const auto& shaderData = shaderPair.first;

      const ShaderModuleCreateInfo shaderModuleCreateInfo(
          {}, shaderData.size() * sizeof(uint32_t), shaderData.data());
      const auto shaderModule =
          module->device.createShaderModule(shaderModuleCreateInfo);
      module->shaderModules.push_back(shaderModule);
      shaderPipe->modules.push_back(
          std::make_pair(shaderPair.second, shaderModule));
    }
  }

  for (const auto [level, module] : shaderPipe->modules) {
    currentStages.push_back(
        PipelineShaderStageCreateInfo({}, level, module, "main"));
  }
}

std::vector<TPipelineHolder> VulkanGraphicsModule::pushMaterials(
    const size_t materialcount, const Material* materials) {
  EXPECT(materialcount != 0 && materials != nullptr);

  const Rect2D scissor({0, 0},
                       {(uint32_t)viewport.width, (uint32_t)viewport.height});
  const PipelineViewportStateCreateInfo pipelineViewportCreateInfo({}, viewport,
                                                                   scissor);

  const PipelineMultisampleStateCreateInfo multisampleCreateInfo(
      {}, SampleCountFlagBits::e1, false, 1);

  const auto pDefaultState = PipelineColorBlendAttachmentState(
      true, BlendFactor::eSrcAlpha, BlendFactor::eOneMinusSrcAlpha,
      BlendOp::eAdd, BlendFactor::eOne, BlendFactor::eZero, BlendOp::eAdd,
      (ColorComponentFlags)FlagTraits<ColorComponentFlagBits>::allFlags);

  const auto pOverrideState = PipelineColorBlendAttachmentState(
      true, BlendFactor::eOne, BlendFactor::eZero, BlendOp::eAdd,
      BlendFactor::eOne, BlendFactor::eZero, BlendOp::eAdd,
      (ColorComponentFlags)FlagTraits<ColorComponentFlagBits>::allFlags);

  const std::array blendAttachment = {pDefaultState, pOverrideState,
                                      pOverrideState, pOverrideState};

  const PipelineColorBlendStateCreateInfo colorBlendState(
      {}, false, LogicOp::eClear, blendAttachment);

  const PipelineDepthStencilStateCreateInfo pipeDepthState(
      {}, true, true, CompareOp::eLessOrEqual, false, false, {}, {}, 0, 1);

  std::vector<GraphicsPipelineCreateInfo> pipelineCreateInfos;
  pipelineCreateInfos.reserve(materialcount);

  std::vector<PipelineInputAssemblyStateCreateInfo> input;
  input.resize(materialcount);

  std::vector<PipelineRasterizationStateCreateInfo> rasterizationInfos(
      materialcount);

  std::vector<std::vector<PipelineShaderStageCreateInfo>> shaderStages(
      materialcount);

  for (size_t i = 0; i < materialcount; i++) {
    const auto& material = materials[i];

    const auto shaderPipe = (VulkanShaderPipe*)material.costumShaderData;

    auto& currentStages = shaderStages[i];
    currentStages.reserve(shaderPipe->shader.size());
    getOrCreate(this, shaderPipe, currentStages);

    rasterizationInfos[i] = PipelineRasterizationStateCreateInfo(
        {}, false, false, {},
        material.doubleSided ? CullModeFlagBits::eNone
                             : CullModeFlagBits::eFront,
        FrontFace::eCounterClockwise, false, 0, 0, 0, 1.0f);

    input[i] = PipelineInputAssemblyStateCreateInfo(
        {},
        material.primitiveType == INVALID_UINT32
            ? PrimitiveTopology::eTriangleList
            : (PrimitiveTopology)(material.primitiveType),
        false);

    GraphicsPipelineCreateInfo gpipeCreateInfo(
        {}, shaderStages[i], &shaderPipe->inputStateCreateInfo, &input[i], {},
        &pipelineViewportCreateInfo, &rasterizationInfos[i],
        &multisampleCreateInfo, &pipeDepthState, &colorBlendState, {}, {},
        renderpass, 0);
    shaderAPI->addToMaterial(&material, &gpipeCreateInfo);
    pipelineCreateInfos.push_back(gpipeCreateInfo);
    shaderPipes.push_back(shaderPipe);
  }

  const auto piperesult =
      device.createGraphicsPipelines({}, pipelineCreateInfos);
  VERROR(piperesult.result);

  std::vector<TPipelineHolder> holder(materialcount);
  {
    auto output = this->materialHolder.allocate(materialcount);
    std::apply(
        [&](auto pipeline, auto layout, auto material) {
          for (size_t i = 0; i < materialcount; i++) {
            holder[i] = TPipelineHolder(i + output.beginIndex);
            *pipeline = piperesult.value[i];
            *layout = pipelineCreateInfos[i].layout;
            *material = materials[i];
            pipeline++;
            layout++;
            material++;
          }
        },
        output.iterator);
  }

  return holder;
}

inline vk::ShaderStageFlagBits shaderToVulkan(shader::ShaderType type) {
  switch (type) {
    case shader::ShaderType::VERTEX:
      return vk::ShaderStageFlagBits::eVertex;
    case shader::ShaderType::FRAGMENT:
      return vk::ShaderStageFlagBits::eFragment;
    default:
      break;
  }
  throw std::runtime_error("Error shader translation not implemented!");
}

void VulkanGraphicsModule::removeData(
    const std::span<const TDataHolder> dataHolder, bool instant) {
  if (this->bufferDataHolder.erase(dataHolder)) {
    if (instant || this->bufferDataHolder.size() / 2 >=
                       this->bufferDataHolder.translationTable.size()) {
      const auto compactation = this->bufferDataHolder.compact();
      const auto& buffers = std::get<0>(compactation);
      for (const auto buffer : buffers) {
        device.destroy(buffer);
      }
      const std::lock_guard guard(this->bufferDataHolder.mutex);
      const auto& memorys = std::get<1>(compactation);
      const auto& allMemorys =
          std::get<1>(this->bufferDataHolder.internalValues);
      std::vector<vk::DeviceMemory> uniqueMemory;
      uniqueMemory.reserve(memorys.size());
      std::ranges::unique_copy(memorys, std::back_inserter(uniqueMemory));
      for (const auto memory : uniqueMemory) {
        if (std::ranges::none_of(allMemorys, [&](auto memoryIn) {
              return memory == memoryIn;
            })) {
          device.freeMemory(memory);
        }
      }
    }
  }
}

void VulkanGraphicsModule::removeTextures(
    const std::span<const TTextureHolder> textureHolder, bool instant) {
  if (this->textureImageHolder.erase(textureHolder)) {
    if (instant || this->textureImageHolder.size() / 2 >=
                       this->textureImageHolder.translationTable.size()) {
      const auto compactation = this->textureImageHolder.compact();
      const auto& images = std::get<0>(compactation);
      for (const auto image : images) {
        device.destroy(image);
      }
      const auto& views = std::get<1>(compactation);
      for (const auto view : views) {
        device.destroy(view);
      }
      const std::lock_guard guard(this->textureImageHolder.mutex);
      const auto& memorys = std::get<2>(compactation);
      const auto& allMemorys =
          std::get<2>(this->textureImageHolder.internalValues);
      std::vector<vk::DeviceMemory> uniqueMemory;
      uniqueMemory.reserve(memorys.size());
      std::ranges::unique_copy(memorys, std::back_inserter(uniqueMemory));
      for (const auto memory : uniqueMemory) {
        if (std::ranges::none_of(allMemorys, [&](auto memoryIn) {
              return memory == memoryIn;
            })) {
          device.freeMemory(memory);
        }
      }
    }
  }
}

void VulkanGraphicsModule::removeSampler(
    const std::span<const TSamplerHolder> samplerHolder, bool instant) {}

void VulkanGraphicsModule::removeMaterials(
    const std::span<const TPipelineHolder> pipelineHolder, bool instant) {}

TRenderHolder VulkanGraphicsModule::pushRender(const size_t renderInfoCount,
                                               const RenderInfo* renderInfos,
                                               const TRenderHolder toOverride,
                                               const RenderTarget target) {
  EXPECT(renderInfoCount != 0 && renderInfos != nullptr);

  std::unique_lock<std::mutex> lockGuard;
  vk::CommandBuffer commandBuffer;
  TRenderHolder nextHolder = toOverride;
  std::unique_lock<std::mutex> generalLock;
  auto& secondaryCommandBuffer = this->secondaryCommandBuffer;
  if (!toOverride) {
    const CommandBufferAllocateInfo commandBufferAllocate(
        secondaryBufferPool, CommandBufferLevel::eSecondary, 1);
    commandBuffer = device.allocateCommandBuffers(commandBufferAllocate)[0];
  } else {
    generalLock = primarySync->waitAndGet();
    lockGuard =
        std::unique_lock(*secondaryCommandBuffer.get<2>(toOverride).get());
    commandBuffer = secondaryCommandBuffer.get<0>(toOverride);

    auto retryChanges = secondaryCommandBuffer.change<1>(toOverride);
    auto& retry = retryChanges.data;
    retry.resize(renderInfoCount);
    std::copy(renderInfos, renderInfos + renderInfoCount, retry.begin());
  }

  const CommandBufferInheritanceInfo inheritance(renderpass, 0);
  const CommandBufferBeginInfo beginInfo(
      CommandBufferUsageFlagBits::eRenderPassContinue, &inheritance);
  commandBuffer.begin(beginInfo);
  for (size_t i = 0; i < renderInfoCount; i++) {
    auto& info = renderInfos[i];

    const std::vector<Buffer> vertexBuffer =
        bufferDataHolder.get<0>(std::span(info.vertexBuffer));

    if (!vertexBuffer.empty()) {
      if (info.vertexOffsets.size() == 0) {
        std::vector<DeviceSize> offsets(vertexBuffer.size());
        std::fill(offsets.begin(), offsets.end(), 0);
        commandBuffer.bindVertexBuffers(0, vertexBuffer, offsets);
      } else {
        TGE_EXPECT(vertexBuffer.size() == info.vertexOffsets.size(),
                   "Size is not equal!", {});
        commandBuffer.bindVertexBuffers(0, vertexBuffer.size(),
                                        vertexBuffer.data(),
                                        (DeviceSize*)info.vertexOffsets.data());
      }
    }

    if (!info.bindingID) {
      const auto binding = shaderAPI->createBindings(
          shaderPipes[info.materialId.internalHandle]);
      shaderAPI->addToRender(binding, (void*)&commandBuffer);
    } else {
      shaderAPI->addToRender(std::span(&info.bindingID, 1),
                             (void*)&commandBuffer);
    }

    commandBuffer.bindPipeline(
        PipelineBindPoint::eGraphics,
        this->materialHolder.get<0>(info.materialId.internalHandle));

    for (const auto& range : info.constRanges) {
      commandBuffer.pushConstants(
          this->materialHolder.get<1>(info.materialId.internalHandle),
          shaderToVulkan(range.type), 0, range.pushConstData.size(),
          range.pushConstData.data());
    }

    if (info.indexSize != IndexSize::NONE) [[likely]] {
      commandBuffer.bindIndexBuffer(bufferDataHolder.get<0>(info.indexBuffer),
                                    info.indexOffset,
                                    (IndexType)info.indexSize);

      commandBuffer.drawIndexed(info.indexCount, info.instanceCount, 0, 0,
                                info.firstInstance);
    } else {
      commandBuffer.draw(info.indexCount, info.instanceCount, 0, 0);
    }
  }
  commandBuffer.end();

  if (!toOverride) {
    auto allocation = secondaryCommandBuffer.allocate(1);
    auto& [buffer, retry, mutex, dataHolder, pipeline, targetOut] =
        allocation.iterator;
    *targetOut = target;
    *mutex = std::make_unique<std::mutex>();
    lockGuard = std::unique_lock(*mutex->get());
    *buffer = commandBuffer;
    retry->resize(renderInfoCount);
    std::copy(renderInfos, renderInfos + renderInfoCount, retry->begin());
    nextHolder = TRenderHolder(allocation.beginIndex);
    std::lock_guard guard(renderInfosForRetryHolder);
    renderInfosForRetry.push_back(nextHolder);
    std::vector<TDataHolder>& dataHolderToAdd = *dataHolder;
    dataHolderToAdd.reserve(renderInfoCount * 100);
    std::vector<TPipelineHolder>& pipelineHolder = *pipeline;
    pipelineHolder.reserve(renderInfoCount);
    for (size_t i = 0; i < renderInfoCount; i++) {
      const auto& render = renderInfos[i];
      pipelineHolder.push_back(render.materialId);
      dataHolderToAdd.push_back(render.indexBuffer);
      for (const auto moreHolder : render.vertexBuffer) {
        dataHolderToAdd.push_back(moreHolder);
      }
    }
  }
  return nextHolder;
}

inline BufferUsageFlags getUsageFlagsFromDataType(const DataType type) {
  switch (type) {
    case DataType::VertexIndexData:
      return BufferUsageFlagBits::eVertexBuffer |
             BufferUsageFlagBits::eIndexBuffer;
    case DataType::Uniform:
      return BufferUsageFlagBits::eUniformBuffer;
    case DataType::VertexData:
      return BufferUsageFlagBits::eVertexBuffer;
    case DataType::IndexData:
      return BufferUsageFlagBits::eIndexBuffer;
    case DataType::All:
      return BufferUsageFlagBits::eVertexBuffer |
             BufferUsageFlagBits::eIndexBuffer |
             BufferUsageFlagBits::eUniformBuffer |
             BufferUsageFlagBits::eStorageBuffer;
    default:
      throw std::runtime_error("Couldn't find usage flag");
  }
}

inline size_t aligned(const size_t size, const size_t align) {
  const auto alignmentOffset = size % align;
  return size + (align - alignmentOffset) % align;
}

struct OutputBuffer {
  vk::Buffer buffer;
  size_t alignedSize;
  size_t alignedOffset;
};

template <bool perma = true>
inline std::tuple<std::vector<OutputBuffer>, vk::DeviceMemory, size_t>
internalBuffer(VulkanGraphicsModule* vgm, const size_t dataCount,
               const BufferCreateInfo* bufferInfo, bool hostVisible = false) {
  std::vector<OutputBuffer> tempBuffer;
  tempBuffer.reserve(dataCount);

  auto lockguard = vgm->bufferDataHolder.allocate(perma ? dataCount : 0);
  auto [bufferList, bufferMemoryList, bufferSizeList, bufferOffset, alignment] =
      lockguard.iterator;

  size_t tempMemory = 0;

  for (size_t i = 0; i < dataCount; i++) {
    const auto& info = bufferInfo[i];
    const auto intermBuffer = vgm->device.createBuffer(info);
    const auto memRequ = vgm->device.getBufferMemoryRequirements(intermBuffer);
    const auto tempOffsetedSize = aligned(memRequ.size, memRequ.alignment);
    tempBuffer.push_back({intermBuffer, tempOffsetedSize, tempMemory});
    if constexpr (perma) {
      *bufferOffset++ = tempMemory;
      *bufferList++ = intermBuffer;
      *bufferSizeList++ = tempOffsetedSize;
      *alignment++ = memRequ.alignment;
    }
    tempMemory += tempOffsetedSize;
  }

  const MemoryAllocateInfo allocLocalInfo(
      tempMemory, hostVisible ? vgm->memoryTypeHostVisibleCoherent
                              : vgm->memoryTypeDeviceLocal);
  const auto memory = vgm->device.allocateMemory(allocLocalInfo);
  if constexpr (perma) {
    for (size_t i = 0; i < dataCount; i++) {
      bufferMemoryList[i] = memory;
    }
  }
  for (size_t i = 0; i < dataCount; i++) {
    const auto& buffer = tempBuffer[i];
    vgm->device.bindBufferMemory(buffer.buffer, memory, buffer.alignedOffset);
  }
  return std::make_tuple(tempBuffer, memory, lockguard.beginIndex);
}

std::vector<TDataHolder> VulkanGraphicsModule::pushData(
    const size_t dataCount, const BufferInfo* bufferInfo) {
  EXPECT(dataCount != 0 && bufferInfo != nullptr);

  std::vector<Buffer> tempBuffer;
  tempBuffer.reserve(dataCount);
  std::vector<size_t> tempBufferAlignedSize;
  tempBufferAlignedSize.resize(dataCount);
  std::vector<size_t> bufferAlignedSize;
  bufferAlignedSize.resize(dataCount);
  size_t tempMemory = 0;
  size_t actualMemory = 0;

  size_t returnIndex = 0;
  {
    auto bufferAllocate = bufferDataHolder.allocate(dataCount);
    auto [bufferList, bufferMemoryList, bufferSizeList, bufferOffset,
          alignment] = bufferAllocate.iterator;
    returnIndex = bufferAllocate.beginIndex;

    for (size_t i = 0; i < dataCount; i++) {
      const auto& info = bufferInfo[i];
      const BufferUsageFlags bufferUsage = getUsageFlagsFromDataType(info.type);

      const BufferCreateInfo bufferCreateInfo({}, info.size,
                                              BufferUsageFlagBits::eTransferSrc,
                                              SharingMode::eExclusive);
      const auto intermBuffer = device.createBuffer(bufferCreateInfo);
      tempBuffer.push_back(intermBuffer);
      const auto memRequ = device.getBufferMemoryRequirements(intermBuffer);
      const auto tempOffsetedSize = aligned(memRequ.size, memRequ.alignment);
      tempMemory += tempOffsetedSize;
      tempBufferAlignedSize[i] = tempOffsetedSize;

      const BufferCreateInfo bufferLocalCreateInfo(
          {}, info.size,
          BufferUsageFlagBits::eTransferDst |
              BufferUsageFlagBits::eTransferSrc | bufferUsage,
          SharingMode::eExclusive);
      const auto localBuffer = device.createBuffer(bufferLocalCreateInfo);
      const auto memRequLocal = device.getBufferMemoryRequirements(localBuffer);

      const auto offsetedSize =
          aligned(memRequLocal.size, memRequLocal.alignment);
      bufferAlignedSize[i] = offsetedSize;
      const auto old = actualMemory;
      actualMemory = aligned(actualMemory, memRequLocal.alignment);
      const auto difference = actualMemory - old;
      if (difference > 0 && i > 0) bufferAlignedSize[i - 1] += difference;
      actualMemory += offsetedSize;

      *(bufferOffset++) = actualMemory;
      *(bufferList + i) = localBuffer;
      *(bufferSizeList++) = offsetedSize;
      *(alignment++) = memRequLocal.alignment;
    }
  }

  const MemoryAllocateInfo allocLocalInfo(actualMemory, memoryTypeDeviceLocal);
  const auto localMem = device.allocateMemory(allocLocalInfo);
  this->bufferDataHolder.fill_adjacent<1>(returnIndex, localMem, dataCount);

  const MemoryAllocateInfo allocInfo(tempMemory, memoryTypeHostVisibleCoherent);
  const auto hostVisibleMemory = device.allocateMemory(allocInfo);

  const auto& cmdBuf = noneRenderCmdbuffer[DATA_ONLY_BUFFER];

  const CommandBufferBeginInfo beginInfo(
      CommandBufferUsageFlagBits::eOneTimeSubmit);
  auto guard = secondarySync->begin(cmdBuf, beginInfo);

  size_t currentOffset = 0;
  size_t tempCurrentOffset = 0;
  for (size_t i = 0; i < dataCount; i++) {
    const auto& info = bufferInfo[i];
    const auto dataptr = (const uint8_t*)info.data;

    const auto tempBuf = tempBuffer[i];
    device.bindBufferMemory(tempBuf, hostVisibleMemory, tempCurrentOffset);

    const auto tempCurrentSize = tempBufferAlignedSize[i];
    const auto mappedHandle =
        device.mapMemory(hostVisibleMemory, tempCurrentOffset, tempCurrentSize);
    memcpy(mappedHandle, dataptr, info.size);
    device.unmapMemory(hostVisibleMemory);
    tempCurrentOffset += tempCurrentSize;

    const auto currentBuffer = this->bufferDataHolder.get<0>(i + returnIndex);
    device.bindBufferMemory(currentBuffer, localMem, currentOffset);
    currentOffset += bufferAlignedSize[i];

    const BufferCopy copyInfo(0, 0, info.size);
    cmdBuf.copyBuffer(tempBuf, currentBuffer, copyInfo);
  }

  const SubmitInfo info({}, {}, cmdBuf);
  secondarySync->endSubmitAndWait(info, std::move(guard));

  device.freeMemory(hostVisibleMemory);
  for (const auto buf : tempBuffer) device.destroyBuffer(buf);

  std::vector<TDataHolder> dataHolders(dataCount);
  for (size_t i = 0; i < dataCount; i++) {
    dataHolders[i] = TDataHolder(returnIndex + i);
  }
  return dataHolders;
}

void VulkanGraphicsModule::changeData(const size_t sizes,
                                      const BufferChange* changeInfos) {
  EXPECT(sizes >= 0 && changeInfos != nullptr);

  std::vector<BufferCreateInfo> createInfo;
  createInfo.resize(sizes);
  for (size_t i = 0; i < sizes; i++) {
    createInfo[i] = BufferCreateInfo({}, changeInfos[i].size,
                                     BufferUsageFlagBits::eTransferSrc);
  }
  const auto& [bufferList, memory, returnID] =
      internalBuffer<false>(this, sizes, createInfo.data(), true);

  const auto mappedHandle = device.mapMemory(memory, 0, VK_WHOLE_SIZE);

  for (size_t i = 0; i < sizes; i++) {
    const auto& change = changeInfos[i];
    memcpy(((uint8_t*)mappedHandle) + bufferList[i].alignedOffset, change.data,
           change.size);
  }

  device.unmapMemory(memory);

  const auto& cmdBuf = noneRenderCmdbuffer[DATA_ONLY_BUFFER];

  const CommandBufferBeginInfo beginInfo(
      CommandBufferUsageFlagBits::eOneTimeSubmit);
  auto guard = secondarySync->begin(cmdBuf, beginInfo);

  for (size_t i = 0; i < sizes; i++) {
    const auto& change = changeInfos[i];

    const BufferCopy copyRegion(0, change.offset, change.size);

    const auto currentBuffer = this->bufferDataHolder.get<0>(change.holder);
    cmdBuf.copyBuffer(bufferList[i].buffer, currentBuffer, copyRegion);
  }

  const SubmitInfo info({}, {}, cmdBuf);
  secondarySync->endSubmitAndWait(info, std::move(guard));

  device.freeMemory(memory);
  for (const auto& buffer : bufferList) {
    device.destroyBuffer(buffer.buffer);
  }
}

TSamplerHolder VulkanGraphicsModule::pushSampler(const SamplerInfo& sampler) {
  const auto position = this->sampler.size();
  const auto anisotropy =
      std::min((float)sampler.anisotropy, deviceLimits.maxSamplerAnisotropy);
  const SamplerCreateInfo samplerCreateInfo(
      {}, (Filter)sampler.minFilter, (Filter)sampler.magFilter,
      SamplerMipmapMode::eLinear, (SamplerAddressMode)sampler.uMode,
      (SamplerAddressMode)sampler.vMode, (SamplerAddressMode)sampler.vMode, 0,
      anisotropy != 0, anisotropy);
  const auto smplr = device.createSampler(samplerCreateInfo);
  this->sampler.push_back(smplr);
  return TSamplerHolder(position);
}

inline std::vector<TTextureHolder> createInternalImages(
    VulkanGraphicsModule* vgm,
    const std::vector<InternalImageInfo>& internalImageInfos) {
  std::vector<std::tuple<ImageViewCreateInfo, size_t>> memoryAndOffsets;
  memoryAndOffsets.reserve(internalImageInfos.size());
  size_t wholeSize = 0;

  for (const auto& imageInfo : internalImageInfos) {
    const ImageCreateInfo depthImageCreateInfo(
        {}, ImageType::e2D, imageInfo.format,
        {imageInfo.extent.width, imageInfo.extent.height, 1}, 1, 1,
        imageInfo.sampleCount, ImageTiling::eOptimal, imageInfo.usage);
    const auto depthImage = vgm->device.createImage(depthImageCreateInfo);

    const MemoryRequirements memoryRequirements =
        vgm->device.getImageMemoryRequirements(depthImage);

    const ImageAspectFlags aspect =
        ((imageInfo.usage & ImageUsageFlagBits::eColorAttachment ||
          imageInfo.usage & ImageUsageFlagBits::eSampled)
             ? ImageAspectFlagBits::eColor
             : (ImageAspectFlagBits)0) |
        (imageInfo.usage & ImageUsageFlagBits::eDepthStencilAttachment
             ? ImageAspectFlagBits::eDepth
             : (ImageAspectFlagBits)0);
    const ImageSubresourceRange subresourceRange(aspect, 0, 1, 0, 1);

    const ImageViewCreateInfo depthImageViewCreateInfo(
        {}, depthImage, ImageViewType::e2D, imageInfo.format, {},
        subresourceRange);

    const auto rest = wholeSize % memoryRequirements.alignment;
    if (rest != 0) {
      wholeSize += memoryRequirements.alignment - rest;
    }

    memoryAndOffsets.push_back(
        std::make_tuple(depthImageViewCreateInfo, wholeSize));
    wholeSize += memoryRequirements.size;
  }

  const MemoryAllocateInfo allocationInfo(wholeSize,
                                          vgm->memoryTypeDeviceLocal);
  const auto imageMemory = vgm->device.allocateMemory(allocationInfo);

  std::vector<TTextureHolder> internalTexture;
  internalTexture.reserve(internalImageInfos.size());

  const auto output =
      vgm->textureImageHolder.allocate(internalImageInfos.size());
  auto [imageItr, viewItr, memoryItr, offsetItr, internalItr] = output.iterator;
  for (const auto& [image, offset] : memoryAndOffsets) {
    vgm->device.bindImageMemory(image.image, imageMemory, offset);

    const auto imageView = vgm->device.createImageView(image);
    *(imageItr++) = image.image;
    *(viewItr++) = imageView;
    *(memoryItr++) = imageMemory;
    *(offsetItr++) = offset;
    internalTexture.emplace_back(internalTexture.size() + output.beginIndex);
  }
  std::copy(internalImageInfos.begin(), internalImageInfos.end(), internalItr);
  return internalTexture;
}

std::vector<TTextureHolder> VulkanGraphicsModule::pushTexture(
    const size_t textureCount, const TextureInfo* textures) {
  EXPECT(textureCount != 0 && textures != nullptr);

  std::vector<Buffer> bufferList;
  std::vector<DeviceMemory> memoryList;
  bufferList.reserve(textureCount);
  memoryList.reserve(textureCount);

  util::OnExit exitHandle([&] {
    for (auto memory : memoryList) device.freeMemory(memory);
    for (auto buffer : bufferList) device.destroyBuffer(buffer);
  });

  const auto& commandBuffer = noneRenderCmdbuffer[TEXTURE_ONLY_BUFFER];

  const CommandBufferBeginInfo beginInfo(
      CommandBufferUsageFlagBits::eOneTimeSubmit, {});
  auto guard = secondarySync->begin(commandBuffer, beginInfo);

  constexpr ImageSubresourceRange range = {ImageAspectFlagBits::eColor, 0, 1, 0,
                                           1};
  std::vector<InternalImageInfo> imagesIn(textureCount);
  for (size_t i = 0; i < textureCount; i++) {
    const TextureInfo& tex = textures[i];
    const Format format = (Format)tex.internalFormatOverride;
    const Extent2D ext = {tex.width, tex.height};
    imagesIn[i] = {
        format, ext,
        ImageUsageFlagBits::eTransferDst | ImageUsageFlagBits::eSampled};
  }

  const auto internalImageHolder = createInternalImages(this, imagesIn);

  for (size_t i = 0; i < textureCount; i++) {
    const TextureInfo& textureInfo = textures[i];
    const Extent3D extent = {textureInfo.width, textureInfo.height, 1};

    const BufferCreateInfo bufferCreateInfo({}, textureInfo.size,
                                            BufferUsageFlagBits::eTransferSrc,
                                            SharingMode::eExclusive, {});
    const auto buffer = device.createBuffer(bufferCreateInfo);
    bufferList.push_back(buffer);
    const auto memoryRequirements = device.getBufferMemoryRequirements(buffer);
    const MemoryAllocateInfo allocationInfo(memoryRequirements.size,
                                            memoryTypeHostVisibleCoherent);
    const auto memory = device.allocateMemory(allocationInfo);
    memoryList.push_back(memory);
    device.bindBufferMemory(buffer, memory, 0);
    const auto handle = device.mapMemory(memory, 0, VK_WHOLE_SIZE, {});
    std::memcpy(handle, textureInfo.data, textureInfo.size);
    device.unmapMemory(memory);

    const auto holderInformation = internalImageHolder[i];
    const auto currentImage =
        textureImageHolder.get<0>(holderInformation.internalHandle);

    waitForImageTransition(
        commandBuffer, ImageLayout::eUndefined,
        ImageLayout::eTransferDstOptimal, currentImage, range,
        PipelineStageFlagBits::eTopOfPipe, AccessFlagBits::eNoneKHR,
        PipelineStageFlagBits::eTransfer, AccessFlagBits::eTransferWrite);

    commandBuffer.copyBufferToImage(buffer, currentImage,
                                    ImageLayout::eTransferDstOptimal,
                                    {{0,
                                      textureInfo.width,
                                      textureInfo.height,
                                      {ImageAspectFlagBits::eColor, 0, 0, 1},
                                      {},
                                      extent}});

    waitForImageTransition(
        commandBuffer, ImageLayout::eTransferDstOptimal,
        ImageLayout::eShaderReadOnlyOptimal, currentImage, range,
        PipelineStageFlagBits::eTransfer, AccessFlagBits::eTransferWrite,
        PipelineStageFlagBits::eFragmentShader, AccessFlagBits::eShaderRead);
  }

  const SubmitInfo info({}, {}, commandBuffer);
  secondarySync->endSubmitAndWait(info, std::move(guard));
  return internalImageHolder;
}

size_t VulkanGraphicsModule::pushLights(const size_t lightCount,
                                        const Light* lights,
                                        const size_t offset) {
  EXPECT(lightCount + offset < 50 && lights != nullptr);
  this->lights.lightCount = offset + lightCount;
  std::copy(lights, lights + lightCount, this->lights.lights + offset);
  BufferChange changeInfo;
  changeInfo.data = &this->lights;
  changeInfo.holder = lightData;
  changeInfo.size = sizeof(this->lights);
  changeData(1, &changeInfo);
  return this->lights.lightCount;
}

#ifdef DEBUG
VkBool32 debugMessage(DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                      DebugUtilsMessageTypeFlagsEXT messageTypes,
                      const DebugUtilsMessengerCallbackDataEXT* pCallbackData,
                      void* pUserData) {
  plog::Severity severity = plog::info;
  switch (messageSeverity) {
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
      severity = plog::verbose;
      break;
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
      severity = plog::info;
      break;
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
      severity = plog::warning;
      break;
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
      severity = plog::error;
      break;
    default:
      break;
  }

  PLOG(severity) << pCallbackData->pMessage;
  return VK_FALSE;
}
#endif

inline void updateDescriptors(VulkanGraphicsModule* vgm,
                              shader::ShaderAPI* sapi) {
  std::array<BindingInfo, 5> bindingInfos;
  for (size_t i = 0; i < 4; i++) {
    bindingInfos[i].type = BindingType::InputAttachment;
    bindingInfos[i].bindingSet = vgm->lightBindings;
    bindingInfos[i].binding = i;
    bindingInfos[i].data.texture.texture = vgm->internalImageData[i + 1];
    bindingInfos[i].data.texture.sampler = TSamplerHolder();
  }
  bindingInfos[4].type = BindingType::UniformBuffer;
  bindingInfos[4].bindingSet = vgm->lightBindings;
  bindingInfos[4].binding = 4;
  bindingInfos[4].data.buffer.dataID = vgm->lightData;
  bindingInfos[4].data.buffer.size = VK_WHOLE_SIZE;
  bindingInfos[4].data.buffer.offset = 0;
  sapi->bindData(bindingInfos.data(), bindingInfos.size());
}

inline void createLightPass(VulkanGraphicsModule* vgm) {
  const auto sapi = vgm->getShaderAPI();

  const Rect2D sic = {
      {0, 0}, {(uint32_t)vgm->viewport.width, (uint32_t)vgm->viewport.height}};

  const PipelineVertexInputStateCreateInfo visci;
  const PipelineViewportStateCreateInfo vsci({}, vgm->viewport, sic);
  const PipelineRasterizationStateCreateInfo rsci(
      {}, false, false, {}, {}, {}, false, 0.0f, 0.0f, 0.0f, 1.0f);

  const PipelineMultisampleStateCreateInfo msci;

  constexpr std::array blendAttachment = {PipelineColorBlendAttachmentState(
      true, BlendFactor::eSrcAlpha, BlendFactor::eOneMinusSrcAlpha,
      BlendOp::eAdd, BlendFactor::eOne, BlendFactor::eZero, BlendOp::eAdd,
      (ColorComponentFlags)FlagTraits<ColorComponentFlagBits>::allFlags)};

  const PipelineColorBlendStateCreateInfo colorBlendState(
      {}, false, LogicOp::eOr, blendAttachment);

  const PipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo(
      {}, PrimitiveTopology::eTriangleList, false);

  const auto states = {DynamicState::eViewport, DynamicState::eScissor};

  const PipelineDynamicStateCreateInfo dynamicStateInfo({}, states);

  GraphicsPipelineCreateInfo graphicsPipeline(
      {}, vgm->lightCreateInfos, &visci, &inputAssemblyCreateInfo, {}, &vsci,
      &rsci, &msci, {}, &colorBlendState, &dynamicStateInfo, nullptr,
      vgm->renderpass, 1);
  sapi->addToMaterial(&vgm->lightMat, &graphicsPipeline);

  const auto gp = vgm->device.createGraphicsPipeline({}, graphicsPipeline);
  VERROR(gp.result)

  auto output = vgm->materialHolder.allocate(1);
  vgm->lightPipe = TPipelineHolder(output.beginIndex);
  std::get<0>(output.iterator)[0] = gp.value;
  std::get<1>(output.iterator)[0] = graphicsPipeline.layout;
}

inline void oneTimeWait(VulkanGraphicsModule* vgm,
                        const CommandBuffer commandBuffer,
                        QueueSync* synchronizer) {
  const CommandBufferBeginInfo beginInfo(
      CommandBufferUsageFlagBits::eOneTimeSubmit, {});
  auto guard = synchronizer->begin(commandBuffer, beginInfo);

  waitForImageTransition(
      commandBuffer, ImageLayout::eUndefined, ImageLayout::eGeneral,
      vgm->textureImageHolder.get<0>(vgm->internalImageData[0]),
      {ImageAspectFlagBits::eDepth, 0, 1, 0, 1});

  constexpr ImageSubresourceRange range = {ImageAspectFlagBits::eColor, 0, 1, 0,
                                           1};
  for (size_t i = 1; i < vgm->internalImageData.size(); i++) {
    waitForImageTransition(
        commandBuffer, ImageLayout::eUndefined,
        ImageLayout::eShaderReadOnlyOptimal,
        vgm->textureImageHolder.get<0>(vgm->internalImageData[i]), range);
  }

  for (const auto& image : vgm->swapchainImages) {
    waitForImageTransition(commandBuffer, ImageLayout::eUndefined,
                           ImageLayout::eGeneral, image, range);
  }

  const SubmitInfo info({}, {}, commandBuffer);
  synchronizer->endSubmitAndWait(info, std::move(guard));
}

inline void createSwapchain(VulkanGraphicsModule* vgm) {
  vgm->device.waitIdle();

  for (const auto frame : vgm->framebuffer) {
    vgm->device.destroyFramebuffer(frame);
  }
  vgm->framebuffer.clear();

  const auto capabilities =
      vgm->physicalDevice.getSurfaceCapabilitiesKHR(vgm->surface);
  vgm->viewport = Viewport(0, 0, capabilities.currentExtent.width,
                           capabilities.currentExtent.height, 0, 1.0f);

  const SwapchainCreateInfoKHR swapchainCreateInfo(
      {}, vgm->surface, 3, vgm->format.format, vgm->format.colorSpace,
      capabilities.currentExtent, 1, ImageUsageFlagBits::eColorAttachment,
      SharingMode::eExclusive, 0, nullptr,
      SurfaceTransformFlagBitsKHR::eIdentity,
      CompositeAlphaFlagBitsKHR::eOpaque, vgm->presentMode, true,
      vgm->swapchain);

  auto oldChain = vgm->swapchain;
  vgm->swapchain = vgm->device.createSwapchainKHR(swapchainCreateInfo);
  vgm->swapchainImages = vgm->device.getSwapchainImagesKHR(vgm->swapchain);

  const Extent2D extent = {(uint32_t)vgm->viewport.width,
                           (uint32_t)vgm->viewport.height};
  const std::vector<InternalImageInfo> internalImageInfos = {
      {vgm->depthFormat, extent, ImageUsageFlagBits::eDepthStencilAttachment},
      {vgm->format.format, extent,
       ImageUsageFlagBits::eColorAttachment |
           ImageUsageFlagBits::eInputAttachment |
           ImageUsageFlagBits::eTransferSrc},
      {Format::eR8G8B8A8Snorm, extent,
       ImageUsageFlagBits::eColorAttachment |
           ImageUsageFlagBits::eInputAttachment |
           ImageUsageFlagBits::eTransferSrc},
      {Format::eR32Sfloat, extent,
       ImageUsageFlagBits::eColorAttachment |
           ImageUsageFlagBits::eInputAttachment |
           ImageUsageFlagBits::eTransferSrc},
      {Format::eR32Sfloat, extent,
       ImageUsageFlagBits::eColorAttachment |
           ImageUsageFlagBits::eInputAttachment |
           ImageUsageFlagBits::eTransferSrc},
      {vgm->format.format, extent,
       ImageUsageFlagBits::eColorAttachment |
           ImageUsageFlagBits::eInputAttachment |
           ImageUsageFlagBits::eTransferSrc}};

  const auto oldImages = vgm->internalImageData;
  const auto data = createInternalImages(vgm, internalImageInfos);
  std::copy(data.begin(), data.end(), vgm->internalImageData.begin());

  oneTimeWait(vgm, vgm->noneRenderCmdbuffer[TEXTURE_ONLY_BUFFER],
              vgm->secondarySync);

  for (const auto view : vgm->swapchainImageviews) {
    vgm->device.destroy(view);
  }
  vgm->swapchainImageviews.reserve(vgm->attachmentCount);
  vgm->swapchainImageviews.clear();

  for (auto im : vgm->swapchainImages) {
    const ImageViewCreateInfo imageviewCreateInfo(
        {}, im, ImageViewType::e2D, vgm->format.format, ComponentMapping(),
        ImageSubresourceRange(ImageAspectFlagBits::eColor, 0, 1, 0, 1));

    const auto imview = vgm->device.createImageView(imageviewCreateInfo);
    vgm->swapchainImageviews.push_back(imview);

    std::vector<ImageView> images =
        vgm->textureImageHolder.get<1, TTextureHolder>(vgm->internalImageData);
    images.resize(vgm->attachmentCount);
    images.back() = imview;

    const FramebufferCreateInfo framebufferCreateInfo(
        {}, vgm->renderpass, images, vgm->viewport.width, vgm->viewport.height,
        1);
    vgm->framebuffer.push_back(
        vgm->device.createFramebuffer(framebufferCreateInfo));
  }
  if (!(!vgm->lightPipe)) {
    const auto sapi = vgm->getShaderAPI();
    updateDescriptors(vgm, sapi);
  }

  vgm->device.waitIdle();
  if (oldChain) {
    vgm->device.destroy(oldChain);
  }
  vgm->removeTextures(oldImages, true);
}

inline bool checkAndRecreate(VulkanGraphicsModule* vgm, const Result result) {
  if (result == Result::eErrorOutOfDateKHR ||
      result == Result::eSuboptimalKHR) {
    createSwapchain(vgm);
    const auto oldValues = vgm->materialHolder.clear();
    for (auto pipeline : std::get<0>(oldValues)) {
      vgm->device.destroy(pipeline);
    }
    vgm->materialHolder.currentIndex = 0;
    createLightPass(vgm);
    const auto& materialCopy = std::get<2>(oldValues);
    vgm->pushMaterials(materialCopy.size() - 1, materialCopy.data() + 1);

    {
      std::unique_lock lock(vgm->renderInfosForRetryHolder);
      for (const TRenderHolder renderHolder : vgm->renderInfosForRetry) {
        const auto& currentVector =
            vgm->secondaryCommandBuffer.get<1>(renderHolder.internalHandle);
        if (currentVector.empty()) continue;
        vgm->pushRender(currentVector.size(), currentVector.data(),
                        renderHolder);
      }
    }
    tge::main::fireRecreate();
    return true;
  } else {
    VERROR(result);
  }
  return false;
}

main::Error VulkanGraphicsModule::init() {
  FeatureSet& features = getGraphicsModule()->features;
#pragma region Instance
  const ApplicationInfo applicationInfo(APPLICATION_NAME, APPLICATION_VERSION,
                                        ENGINE_NAME, ENGINE_VERSION,
                                        VK_API_VERSION_1_0);

  const auto layerInfos = enumerateInstanceLayerProperties();
  std::vector<const char*> layerEnabled;
  for (const auto& layer : layerInfos) {
    const auto lname = layer.layerName.data();
    const auto enditr = layerToEnable.end();
    if (std::find_if(layerToEnable.begin(), enditr,
                     [&](auto in) { return strcmp(lname, in) == 0; }) != enditr)
      layerEnabled.push_back(lname);
  }

  const auto extensionInfos = enumerateInstanceExtensionProperties();
  std::vector<const char*> extensionEnabled;
  for (const auto& extension : extensionInfos) {
    const auto lname = extension.extensionName.data();
    const auto enditr = extensionToEnable.end();
    if (std::find_if(extensionToEnable.begin(), enditr, [&](auto in) {
          return strcmp(lname, in) == 0;
        }) != enditr) {
      extensionEnabled.push_back(lname);
    }
  }

  const InstanceCreateInfo createInfo(
      {}, &applicationInfo, (uint32_t)layerEnabled.size(), layerEnabled.data(),
      (uint32_t)extensionEnabled.size(), extensionEnabled.data());
  this->instance = createInstance(createInfo);

#ifdef DEBUG
  if (std::find_if(begin(extensionEnabled), end(extensionEnabled), [](auto x) {
        return strcmp(x, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0;
      }) != end(extensionEnabled)) {
    PLOG_DEBUG << "Create debug utils!";

    DispatchLoaderDynamic stat;
    stat.vkCreateDebugUtilsMessengerEXT =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            instance, "vkCreateDebugUtilsMessengerEXT");
    const DebugUtilsMessengerCreateInfoEXT debugUtilsMsgCreateInfo(
        {},
        (DebugUtilsMessageSeverityFlagsEXT)
            FlagTraits<DebugUtilsMessageSeverityFlagBitsEXT>::allFlags,
        (DebugUtilsMessageTypeFlagsEXT)
            FlagTraits<DebugUtilsMessageTypeFlagBitsEXT>::allFlags,
        (PFN_vkDebugUtilsMessengerCallbackEXT)debugMessage);
    debugMessenger = instance.createDebugUtilsMessengerEXT(
        debugUtilsMsgCreateInfo, nullptr, stat);
  }
#endif
#pragma endregion

#pragma region Device
  constexpr auto getScore = [](auto physDevice) {
    const auto properties = physDevice.getProperties();
    return properties.limits.maxImageDimension2D +
           (properties.deviceType == PhysicalDeviceType::eDiscreteGpu ? 1000
                                                                      : 0);
  };

  const auto physicalDevices = this->instance.enumeratePhysicalDevices();
  this->physicalDevice = *std::max_element(
      physicalDevices.begin(), physicalDevices.end(),
      [&](auto p1, auto p2) { return getScore(p1) < getScore(p2); });

  // just one queue for now
  const auto queueFamilys = this->physicalDevice.getQueueFamilyProperties();
  const auto bgnitr = queueFamilys.begin();
  const auto enditr = queueFamilys.end();
  const auto queueFamilyItr = std::find_if(bgnitr, enditr, [](auto queue) {
    return queue.queueFlags & QueueFlagBits::eGraphics;
  });
  if (queueFamilyItr == enditr) return main::Error::NO_GRAPHIC_QUEUE_FOUND;

  queueFamilyIndex = (uint32_t)std::distance(bgnitr, queueFamilyItr);
  const auto& queueFamily = *queueFamilyItr;
  std::vector<float> priorities(queueFamily.queueCount);
  std::fill(priorities.begin(), priorities.end(), 0.0f);
  queueIndex = 0;
  secondaryqueueIndex = queueFamily.queueCount > 1 ? 1 : 0;
  const DeviceQueueCreateInfo queueCreateInfo(
      {}, queueIndex, secondaryqueueIndex + 1, priorities.data());

  const auto devextensions =
      physicalDevice.enumerateDeviceExtensionProperties();
  const auto devextEndItr = devextensions.end();
  const auto fndDevExtItr = std::find_if(
      devextensions.begin(), devextEndItr, [](ExtensionProperties prop) {
        return strcmp(prop.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
      });
  if (fndDevExtItr == devextEndItr) return main::Error::SWAPCHAIN_EXT_NOT_FOUND;

  this->deviceLimits = this->physicalDevice.getProperties().limits;
  const auto vkFeatures = this->physicalDevice.getFeatures();
  if (features.wideLines) features.wideLines = vkFeatures.wideLines;
  if (!vkFeatures.samplerAnisotropy) features.anisotropicfiltering = 0;
  if (!vkFeatures.independentBlend)
    return main::Error::INDEPENDENT_BLEND_NOT_SUPPORTED;
  vk::PhysicalDeviceFeatures enabledFeatures{};
  enabledFeatures.wideLines = features.wideLines;
  enabledFeatures.samplerAnisotropy = features.anisotropicfiltering != 0;
  enabledFeatures.independentBlend = VK_TRUE;

  const char* name = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
  const DeviceCreateInfo deviceCreateInfo({}, 1, &queueCreateInfo, 0, {}, 1,
                                          &name, &enabledFeatures);
  this->device = this->physicalDevice.createDevice(deviceCreateInfo);

  const auto c4Props =
      this->physicalDevice.getFormatProperties(Format::eR8G8B8A8Unorm);
  if (!(c4Props.optimalTilingFeatures &
        FormatFeatureFlagBits::eColorAttachment))
    return main::Error::FORMAT_NOT_SUPPORTED;

  const auto float32Props =
      this->physicalDevice.getFormatProperties(Format::eR32Sfloat);
  if (!(float32Props.optimalTilingFeatures &
        FormatFeatureFlagBits::eColorAttachment))
    return main::Error::FORMAT_NOT_SUPPORTED;

#pragma endregion

#pragma region Vulkan Mutex
  this->primarySync = new QueueSync(
      this->device, device.getQueue(queueFamilyIndex, queueIndex));
  if (queueFamily.queueCount > 1) {
    this->secondarySync = new QueueSync(
        this->device, device.getQueue(queueFamilyIndex, secondaryqueueIndex));
  } else {
    this->secondarySync = this->primarySync;
  }

  const SemaphoreCreateInfo semaphoreCreateInfo;
  waitSemaphore = device.createSemaphore(semaphoreCreateInfo);
  signalSemaphore = device.createSemaphore(semaphoreCreateInfo);
#pragma endregion

#pragma region Queue, Surface, Prepipe, MemTypes
  const auto winM = graphicsModule->getWindowModule();
#ifdef WIN32
  Win32SurfaceCreateInfoKHR surfaceCreateInfo({}, (HINSTANCE)winM->hInstance,
                                              (HWND)winM->hWnd);
  surface = instance.createWin32SurfaceKHR(surfaceCreateInfo);
#endif  // WIN32
#ifdef __linux__
  XlibSurfaceCreateInfoKHR surfaceCreateInfo({}, (Display*)winM->hInstance,
                                             (Window)winM->hWnd);
  surface = instance.createXlibSurfaceKHR(surfaceCreateInfo);
#endif

  if (!physicalDevice.getSurfaceSupportKHR(queueIndex, surface))
    return main::Error::NO_SURFACE_SUPPORT;

  const auto surfaceFormat = physicalDevice.getSurfaceFormatsKHR(surface);
  const auto surfEndItr = surfaceFormat.end();
  const auto surfBeginItr = surfaceFormat.begin();
  const auto fitr =
      std::find_if(surfBeginItr, surfEndItr, [](SurfaceFormatKHR format) {
        return format.format == Format::eB8G8R8A8Unorm;
      });
  if (fitr == surfEndItr) return main::Error::FORMAT_NOT_FOUND;
  format = *fitr;

  const auto memoryProperties = physicalDevice.getMemoryProperties();
  const auto memBeginItr = memoryProperties.memoryTypes.begin();
  const auto memEndItr = memoryProperties.memoryTypes.end();

  const auto findMemoryIndex = [&](auto prop) {
    const auto findItr = std::find_if(memBeginItr, memEndItr, [&](auto& type) {
      return type.propertyFlags & (prop);
    });
    return std::distance(memBeginItr, findItr);
  };

  memoryTypeDeviceLocal = findMemoryIndex(MemoryPropertyFlagBits::eDeviceLocal);
  memoryTypeHostVisibleCoherent =
      findMemoryIndex(MemoryPropertyFlagBits::eHostVisible |
                      MemoryPropertyFlagBits::eHostCoherent);

  const auto capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
  viewport = Viewport(0, 0, capabilities.currentExtent.width,
                      capabilities.currentExtent.height, 0, 1.0f);

  const auto presentModes = physicalDevice.getSurfacePresentModesKHR(surface);
  const auto presentModesEndItr = presentModes.end();
  const auto presentModesBeginItr = presentModes.begin();
  auto fndPresentMode = std::find(presentModesBeginItr, presentModesEndItr,
                                  PresentModeKHR::eMailbox);
  if (fndPresentMode == presentModesEndItr) {
    fndPresentMode = std::find(presentModesBeginItr, presentModesEndItr,
                               PresentModeKHR::eImmediate);
    if (fndPresentMode == presentModesEndItr)
      fndPresentMode = presentModesBeginItr;
  }
  this->presentMode = *fndPresentMode;
#pragma endregion

#pragma region Depth and Output Attachments
  constexpr std::array potentialDepthFormat = {
      Format::eD32Sfloat, Format::eD16Unorm, Format::eD32SfloatS8Uint,
      Format::eD24UnormS8Uint, Format::eD16UnormS8Uint};
  for (const Format pDF : potentialDepthFormat) {
    const FormatProperties fProp = physicalDevice.getFormatProperties(pDF);
    if (fProp.optimalTilingFeatures &
        FormatFeatureFlagBits::eDepthStencilAttachment) {
      depthFormat = pDF;
      break;
    }
  }
  if (depthFormat == Format::eUndefined) return main::Error::FORMAT_NOT_FOUND;
#pragma endregion

#pragma region Renderpass

  const std::array attachments = {
      AttachmentDescription(
          {}, depthFormat, SampleCountFlagBits::e1, AttachmentLoadOp::eClear,
          AttachmentStoreOp::eStore, AttachmentLoadOp::eDontCare,
          AttachmentStoreOp::eDontCare, ImageLayout::eUndefined,
          ImageLayout::eDepthStencilAttachmentOptimal),
      AttachmentDescription(
          {}, format.format, SampleCountFlagBits::e1, AttachmentLoadOp::eClear,
          AttachmentStoreOp::eStore, AttachmentLoadOp::eDontCare,
          AttachmentStoreOp::eDontCare, ImageLayout::eUndefined,
          ImageLayout::eShaderReadOnlyOptimal),
      AttachmentDescription(
          {}, Format::eR8G8B8A8Snorm, SampleCountFlagBits::e1,
          AttachmentLoadOp::eClear, AttachmentStoreOp::eStore,
          AttachmentLoadOp::eDontCare, AttachmentStoreOp::eDontCare,
          ImageLayout::eUndefined, ImageLayout::eShaderReadOnlyOptimal),
      AttachmentDescription(
          {}, Format::eR32Sfloat, SampleCountFlagBits::e1,
          AttachmentLoadOp::eClear, AttachmentStoreOp::eStore,
          AttachmentLoadOp::eDontCare, AttachmentStoreOp::eDontCare,
          ImageLayout::eUndefined, ImageLayout::eShaderReadOnlyOptimal),
      AttachmentDescription(
          {}, Format::eR32Sfloat, SampleCountFlagBits::e1,
          AttachmentLoadOp::eClear, AttachmentStoreOp::eStore,
          AttachmentLoadOp::eDontCare, AttachmentStoreOp::eDontCare,
          ImageLayout::eUndefined, ImageLayout::eShaderReadOnlyOptimal),
      AttachmentDescription(
          {}, format.format, SampleCountFlagBits::e1, AttachmentLoadOp::eClear,
          AttachmentStoreOp::eStore, AttachmentLoadOp::eDontCare,
          AttachmentStoreOp::eDontCare, ImageLayout::eUndefined,
          ImageLayout::ePresentSrcKHR)};
  attachmentCount = attachments.size();

  constexpr std::array colorAttachments = {
      AttachmentReference(1, ImageLayout::eColorAttachmentOptimal),
      AttachmentReference(2, ImageLayout::eColorAttachmentOptimal),
      AttachmentReference(3, ImageLayout::eColorAttachmentOptimal),
      AttachmentReference(4, ImageLayout::eColorAttachmentOptimal)};

  constexpr std::array inputAttachments = {
      AttachmentReference(1, ImageLayout::eShaderReadOnlyOptimal),
      AttachmentReference(2, ImageLayout::eShaderReadOnlyOptimal),
      AttachmentReference(3, ImageLayout::eShaderReadOnlyOptimal),
      AttachmentReference(4, ImageLayout::eShaderReadOnlyOptimal)};

  constexpr std::array colorAttachmentsSubpass1 = {
      AttachmentReference(5, ImageLayout::eColorAttachmentOptimal)};

  constexpr AttachmentReference depthAttachment(
      0, ImageLayout::eDepthStencilAttachmentOptimal);

  const std::array subpassDescriptions = {
      SubpassDescription({}, PipelineBindPoint::eGraphics, {}, colorAttachments,
                         {}, &depthAttachment),
      SubpassDescription({}, PipelineBindPoint::eGraphics, inputAttachments,
                         colorAttachmentsSubpass1)};

  constexpr auto frag1 = PipelineStageFlagBits::eColorAttachmentOutput |
                         PipelineStageFlagBits::eLateFragmentTests |
                         PipelineStageFlagBits::eEarlyFragmentTests;

  constexpr auto frag2 = AccessFlagBits::eColorAttachmentWrite |
                         AccessFlagBits::eColorAttachmentRead |
                         AccessFlagBits::eDepthStencilAttachmentRead |
                         AccessFlagBits::eDepthStencilAttachmentWrite;

  const std::array subpassDependencies = {
      SubpassDependency(0, 1, frag1, frag1, frag2, frag2),
      SubpassDependency(
          1, VK_SUBPASS_EXTERNAL, PipelineStageFlagBits::eColorAttachmentOutput,
          PipelineStageFlagBits::eColorAttachmentOutput,
          AccessFlagBits::eColorAttachmentWrite, (AccessFlagBits)0)};

  const RenderPassCreateInfo renderPassCreateInfo(
      {}, attachments, subpassDescriptions, subpassDependencies);
  renderpass = device.createRenderPass(renderPassCreateInfo);
#pragma endregion

#pragma region CommandBuffer
  const CommandPoolCreateInfo commandPoolCreateInfo(
      CommandPoolCreateFlagBits::eResetCommandBuffer, queueFamilyIndex);
  pool = device.createCommandPool(commandPoolCreateInfo);
  secondaryPool = device.createCommandPool(commandPoolCreateInfo);
  secondaryBufferPool = device.createCommandPool(commandPoolCreateInfo);

  const CommandBufferAllocateInfo cmdBufferAllocInfo(
      pool, CommandBufferLevel::ePrimary, (uint32_t)3);
  cmdbuffer = device.allocateCommandBuffers(cmdBufferAllocInfo);

  const CommandBufferAllocateInfo cmdBufferAllocInfoSecond(
      secondaryPool, CommandBufferLevel::ePrimary, (uint32_t)3);
  noneRenderCmdbuffer = device.allocateCommandBuffers(cmdBufferAllocInfoSecond);
  createSwapchain(this);
#pragma endregion

  this->isInitialiazed = true;
  this->shaderAPI->init();
  device.waitIdle();

  const auto pipe = (VulkanShaderPipe*)shaderAPI->loadShaderPipeAndCompile(
      {"assets/lightPass.vert", "assets/lightPass.frag"});
  shaderPipes.push_back(pipe);
  lightBindings = shaderAPI->createBindings(pipe)[0];
  getOrCreate(this, pipe, lightCreateInfos);
  lightMat = Material(pipe);

  BufferInfo bufferInfo;
  bufferInfo.data = &this->lights;
  bufferInfo.size = sizeof(this->lights);
  bufferInfo.type = DataType::Uniform;
  this->lightData = this->pushData(1, &bufferInfo)[0];

  updateDescriptors(this, this->shaderAPI);

  createLightPass(this);

  auto nextimage =
      device.acquireNextImageKHR(swapchain, INVALID_SIZE_T, waitSemaphore, {});
  VERROR(nextimage.result);
  this->nextImage = nextimage.value;

  return main::Error::NONE;
}

void VulkanGraphicsModule::tick(double time) {
  if (exitFailed) return;

  if (this->nextImage > cmdbuffer.size()) {
    PLOG(plog::fatal) << "Size greater command buffer size!";
  }

  const auto currentBuffer = cmdbuffer[this->nextImage];

  if (1) {  // For now rerecord every tick
    constexpr std::array clearColor = {1.0f, 1.0f, 1.0f, 1.0f};
    const std::array clearValue = {ClearValue(ClearDepthStencilValue(1.0f, 0)),
                                   ClearValue(clearColor),
                                   ClearValue(clearColor),
                                   ClearValue(clearColor),
                                   ClearValue(clearColor),
                                   ClearValue(clearColor)};

    const CommandBufferBeginInfo cmdBufferBeginInfo({}, nullptr);
    currentBuffer.begin(cmdBufferBeginInfo);

    const RenderPassBeginInfo renderPassBeginInfo(
        renderpass, framebuffer[this->nextImage],
        {{0, 0}, {(uint32_t)viewport.width, (uint32_t)viewport.height}},
        clearValue);
    currentBuffer.beginRenderPass(renderPassBeginInfo,
                                  SubpassContents::eSecondaryCommandBuffers);
    {
      std::lock_guard lg(secondaryCommandBuffer.mutex);
      const auto& bufferToExecute =
          std::get<0>(secondaryCommandBuffer.internalValues);
      if (!bufferToExecute.empty()) {
        currentBuffer.executeCommands(bufferToExecute);
      }
    }

    currentBuffer.nextSubpass(SubpassContents::eInline);

    currentBuffer.setViewport(0, this->viewport);

    const Rect2D scissor({},
                         {(uint32_t)viewport.width, (uint32_t)viewport.height});
    currentBuffer.setScissor(0, scissor);

    currentBuffer.bindPipeline(PipelineBindPoint::eGraphics,
                               materialHolder.get<0>(lightPipe.internalHandle));

    const std::array lights = {lightBindings};
    getShaderAPI()->addToRender(lights, (CommandBuffer*)&currentBuffer);

    currentBuffer.draw(3, 1, 0, 0);

    currentBuffer.endRenderPass();

    waitForImageTransition(currentBuffer, ImageLayout::eUndefined,
                           ImageLayout::eGeneral,
                           textureImageHolder.get<0>(internalImageData[0]),
                           {ImageAspectFlagBits::eDepth, 0, 1, 0, 1});

    currentBuffer.end();
  }

  primary[0] = currentBuffer;

  constexpr PipelineStageFlags stageFlag =
      PipelineStageFlagBits::eColorAttachmentOutput |
      PipelineStageFlagBits::eLateFragmentTests;
  const SubmitInfo submitInfo(waitSemaphore, stageFlag, primary,
                              signalSemaphore);
  primarySync->submit(submitInfo);

  const PresentInfoKHR presentInfo(signalSemaphore, swapchain, this->nextImage,
                                   nullptr);
  const Result result = primarySync->queue.presentKHR(&presentInfo);
  if (checkAndRecreate(this, result)) {
    currentBuffer.reset();
    auto nextimage = device.acquireNextImageKHR(swapchain, INVALID_SIZE_T,
                                                waitSemaphore, {});
    this->nextImage = nextimage.value;
    return;
  }

  while (true) {
    auto nextimage = device.acquireNextImageKHR(swapchain, INVALID_SIZE_T,
                                                waitSemaphore, {});
    this->nextImage = nextimage.value;
    if (!checkAndRecreate(this, nextimage.result)) break;
  }
}

void VulkanGraphicsModule::destroy() {
  this->isInitialiazed = false;
  device.waitIdle();
  this->shaderAPI->destroy();
  delete primarySync;
  if (primarySync != secondarySync) delete secondarySync;
  device.destroySemaphore(waitSemaphore);
  device.destroySemaphore(signalSemaphore);
  auto [imageList, viewList, memoryList, _u1, _u2] = textureImageHolder.clear();
  for (auto image : imageList) device.destroy(image);
  for (auto view : viewList) device.destroy(view);
  const auto imageMemoryEnd = std::unique(memoryList.begin(), memoryList.end());
  for (auto iterator = memoryList.begin(); iterator != imageMemoryEnd;
       iterator++)
    device.freeMemory(*iterator);
  for (const auto samp : sampler) device.destroySampler(samp);
  for (const auto memory :
       std::ranges::unique(std::get<1>(bufferDataHolder.internalValues)))
    device.freeMemory(memory);
  for (const auto buf : std::get<0>(bufferDataHolder.internalValues))
    device.destroyBuffer(buf);
  const auto pipeList = std::get<0>(materialHolder.clear());
  for (const auto pipe : pipeList) device.destroyPipeline(pipe);
  for (const auto shader : shaderModules) device.destroyShaderModule(shader);
  device.destroyCommandPool(pool);
  device.destroyCommandPool(secondaryPool);
  device.destroyCommandPool(secondaryBufferPool);
  for (const auto framebuff : framebuffer) device.destroyFramebuffer(framebuff);
  for (const auto imv : swapchainImageviews) device.destroyImageView(imv);
  device.destroyRenderPass(renderpass);
  device.destroySwapchainKHR(swapchain);
  device.destroy();
  instance.destroySurfaceKHR(surface);
#ifdef DEBUG
  if (debugMessenger) {
    DispatchLoaderDynamic stat;
    stat.vkDestroyDebugUtilsMessengerEXT =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            instance, "vkDestroyDebugUtilsMessengerEXT");
    instance.destroyDebugUtilsMessengerEXT(debugMessenger, nullptr, stat);
  }
#endif
  instance.destroy();
  delete shaderAPI;
}

void VulkanGraphicsModule::removeRender(const size_t renderInfoCount,
                                        const TRenderHolder* renderIDs) {
  std::vector<size_t> toErase;
  toErase.reserve(renderInfoCount);
  for (size_t i = 0; i < renderInfoCount; i++) {
    TRenderHolder holder = renderIDs[i];
    if (!holder) continue;
    toErase.push_back(holder.internalHandle);
    std::lock_guard guard(renderInfosForRetryHolder);
    const auto iterator = std::find(renderInfosForRetry.begin(),
                                    renderInfosForRetry.end(), holder);
    if (iterator != renderInfosForRetry.end())
      renderInfosForRetry.erase(iterator);
  }
  if (!toErase.empty()) {
    secondaryCommandBuffer.erase(toErase);
    const auto tuple = secondaryCommandBuffer.compact();
    const auto& lostBuffer = std::get<0>(tuple);
    if (!lostBuffer.empty()) {
      auto guard = this->primarySync->waitAndGet();
      device.freeCommandBuffers(secondaryBufferPool, lostBuffer);
    }
  }
}

glm::vec2 VulkanGraphicsModule::getRenderExtent() const {
  return glm::vec2(this->viewport.width, this->viewport.height);
}

APILayer* getNewVulkanModule() { return new VulkanGraphicsModule(); }

std::pair<std::vector<char>, TDataHolder> VulkanGraphicsModule::getImageData(
    const TTextureHolder imageId, const TDataHolder cache) {
  const auto currentImage = textureImageHolder.get<0>(imageId);
  const auto requireMents = device.getImageMemoryRequirements(currentImage);

  Buffer dataBuffer(nullptr);
  DeviceMemory memoryBuffer(nullptr);
  TDataHolder dataHolder = cache;
  if (!dataHolder) {
    const BufferCreateInfo bufferCreateInfo({}, requireMents.size,
                                            BufferUsageFlagBits::eTransferDst);
    const auto& [output, memory, returnID] =
        internalBuffer<true>(this, 1, &bufferCreateInfo, true);
    dataBuffer = output.back().buffer;
    memoryBuffer = memory;
    dataHolder = TDataHolder(returnID);
  } else {
    dataBuffer = bufferDataHolder.get<0>(dataHolder);
    memoryBuffer = bufferDataHolder.get<1>(dataHolder);
  }

  const auto buffer = noneRenderCmdbuffer[DATA_ONLY_BUFFER];
  static constexpr CommandBufferBeginInfo info;
  std::unique_lock<std::mutex> lockGuard;
  if (secondarySync != primarySync) {
    lockGuard = std::unique_lock(secondarySync->handle);
  }
  auto guard = primarySync->begin(buffer, info);

  constexpr ImageSubresourceRange range(ImageAspectFlagBits::eColor, 0, 1, 0,
                                        1);

  waitForImageTransition(buffer, ImageLayout::eShaderReadOnlyOptimal,
                         ImageLayout::eTransferSrcOptimal, currentImage, range);

  const auto oldInfo = this->textureImageHolder.get<4>(imageId);
  constexpr ImageSubresourceLayers layers(ImageAspectFlagBits::eColor, 0, 0, 1);
  const BufferImageCopy imageInfo(
      0, 0, 0, layers, {}, {oldInfo.extent.width, oldInfo.extent.height, 1});
  buffer.copyImageToBuffer(currentImage, ImageLayout::eTransferSrcOptimal,
                           dataBuffer, imageInfo);

  waitForImageTransition(buffer, ImageLayout::eTransferSrcOptimal,
                         ImageLayout::eShaderReadOnlyOptimal, currentImage,
                         range);

  const SubmitInfo submit({}, {}, buffer, {});
  primarySync->endSubmitAndWait(submit, std::move(guard));

  std::vector<char> vector(requireMents.size);
  const char* readMemory =
      (char*)device.mapMemory(memoryBuffer, 0, requireMents.size);
  std::copy(readMemory, (readMemory + requireMents.size), vector.begin());
  device.unmapMemory(memoryBuffer);

  return std::make_pair(vector, dataHolder);
}

}  // namespace tge::graphics
