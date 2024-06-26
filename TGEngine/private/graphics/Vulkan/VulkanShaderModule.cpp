#include "../../../public/graphics/vulkan/VulkanShaderModule.hpp"

#include <glslang/MachineIndependent/localintermediate.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>

#include <iostream>
#include <vulkan/vulkan.hpp>

#include "../../../public/Error.hpp"
#include "../../../public/Util.hpp"
#include "../../../public/graphics/vulkan/VulkanModuleDef.hpp"
#include "../../../public/graphics/vulkan/VulkanShaderPipe.hpp"
#undef ERROR
#define SPR_NO_DEBUG_OUTPUT 1
#define SPR_NO_GLSL_INCLUDE 1
#define SPR_NO_STATIC 1
#define SPR_STATIC extern
#include "../../../public/headerlibs/ShaderPermute.hpp"

namespace permute {
std::map<std::string, int> lookupCounter;
permute::lookup glslLookup = {{"next", next}};
}  // namespace permute

namespace tge::shader {

using namespace vk;

inline ShaderStageFlagBits getStageFromLang(const EShLanguage lang) {
  switch (lang) {
    case EShLanguage::EShLangVertex:
      return ShaderStageFlagBits::eVertex;
    case EShLanguage::EShLangFragment:
      return ShaderStageFlagBits::eFragment;
    default:
      throw std::runtime_error(
          std::string("Couldn't find ShaderStageFlagBits for EShLanguage ") +
          std::to_string(lang));
  }
}

inline EShLanguage getLangFromShaderLang(const ShaderType type) {
  switch (type) {
    case ShaderType::FRAGMENT:
      return EShLangFragment;
    case ShaderType::VERTEX:
      return EShLangVertex;
    default:
      throw std::runtime_error("Not implemented!");
  };
}

inline ShaderType getLang(const std::string& str) {
  if (str.compare("vert") == 0) return ShaderType::VERTEX;
  if (str.compare("frag") == 0) return ShaderType::FRAGMENT;
  throw std::runtime_error(std::string("Couldn't find EShLanguage for ") + str);
}

inline Format getFormatFromElf(const glslang::TType& format) {
  if (format.isVector() &&
      format.getBasicType() == glslang::TBasicType::EbtFloat) {
    switch (format.getVectorSize()) {
      case 1:
        return Format::eR32Sint;
      case 2:
        return Format::eR32G32Sfloat;
      case 3:
        return Format::eR32G32B32Sfloat;
      case 4:
        return Format::eR32G32B32A32Sfloat;
      default:
        break;
    }
  } else {
    switch (format.getBasicType()) {
      case glslang::TBasicType::EbtFloat:
        return Format::eR32Sfloat;
      case glslang::TBasicType::EbtInt:
        return Format::eR32Sint;
      case glslang::TBasicType::EbtUint:
        return Format::eR32Uint;
      default:
        break;
    }
  }
  throw std::runtime_error(std::string("Couldn't find Format for TType" +
                                       format.getCompleteString()));
}

#define NO_BINDING_GIVEN 65535
#define NO_LAYOUT_GIVEN 4095

struct VertexShaderAnalizer : public glslang::TIntermTraverser {
  VulkanShaderPipe* shaderPipe;
  const ShaderCreateInfo& createInfo;

  VertexShaderAnalizer(VulkanShaderPipe* pipe,
                       const ShaderCreateInfo& createInfo)
      : glslang::TIntermTraverser(false, true, false),
        shaderPipe(pipe),
        createInfo(createInfo) {
    ioVars.reserve(10);
  }
  std::unordered_set<size_t> ioVars;

  void visitSymbol(glslang::TIntermSymbol* symbol) {
    const auto& qualifier = symbol->getQualifier();
    if (ioVars.contains(symbol->getId())) return;
    ioVars.emplace(symbol->getId());
    if (qualifier.layoutLocation < NO_LAYOUT_GIVEN) {
      if (qualifier.storage == glslang::TStorageQualifier::EvqVaryingIn) {
        const uint32_t bind = qualifier.layoutBinding == NO_BINDING_GIVEN
                                  ? (uint32_t)createInfo.inputLayoutTranslation(
                                        qualifier.layoutLocation)
                                  : qualifier.layoutBinding;
        shaderPipe->vertexInputAttributes.push_back(
            VertexInputAttributeDescription(
                qualifier.layoutLocation, bind,
                getFormatFromElf(symbol->getType())));
      }
    }
  }

  void post() {
    const auto beginItr = shaderPipe->vertexInputAttributes.begin();
    const auto endItr = shaderPipe->vertexInputAttributes.end();

    for (auto& vert : shaderPipe->vertexInputAttributes) {
      for (auto itr = beginItr; itr != endItr; itr++) {
        if (itr->location < vert.location && itr->binding == vert.binding)
          vert.offset += getSizeFromFormat(itr->format);
      }

      auto beginItrBinding = shaderPipe->vertexInputBindings.begin();
      auto endItrBinding = shaderPipe->vertexInputBindings.end();
      auto fitr = std::find_if(
          beginItrBinding, endItrBinding,
          [bind = vert.binding](auto c) { return c.binding == bind; });
      if (fitr == endItrBinding) {
        const auto index = shaderPipe->vertexInputBindings.size();
        shaderPipe->vertexInputBindings.push_back(
            VertexInputBindingDescription(vert.binding, 0));
        fitr = shaderPipe->vertexInputBindings.begin() + index;
      }
      fitr->stride += getSizeFromFormat(vert.format);
    }

    shaderPipe->inputStateCreateInfo = PipelineVertexInputStateCreateInfo(
        {}, shaderPipe->vertexInputBindings.size(),
        shaderPipe->vertexInputBindings.data(),
        shaderPipe->vertexInputAttributes.size(),
        shaderPipe->vertexInputAttributes.data());
  }
};

inline std::pair<DescriptorType, uint32_t> getDescTypeFromELF(
    const glslang::TType& type) {
  const auto count = type.isArray() ? type.getOuterArraySize() : 1;
  if (type.getBasicType() == glslang::TBasicType::EbtSampler) {
    const auto sampler = type.getSampler();
    if (sampler.isImageClass() && sampler.isSubpass())
      return std::pair(DescriptorType::eInputAttachment, count);
    if (sampler.isPureSampler() || sampler.isCombined())
      return std::pair(DescriptorType::eSampler, count);
    return std::pair(DescriptorType::eSampledImage, count);
  } else if (type.getBasicType() == glslang::TBasicType::EbtBlock) {
    return std::pair(DescriptorType::eUniformBuffer, count);
  }
  std::cout << type.getQualifier().layoutAttachment << " <- Attachment";
  throw std::runtime_error("Descriptor could not be found for: " +
                           std::string(type.getCompleteString()));
}

struct GeneralShaderAnalizer : public glslang::TIntermTraverser {
  VulkanShaderPipe* shaderPipe;
  const ShaderStageFlags flags;
  std::unordered_set<uint32_t> uset;
  bool pushConst = false;

  GeneralShaderAnalizer(VulkanShaderPipe* pipe, ShaderStageFlags flags)
      : glslang::TIntermTraverser(false, true, false),
        shaderPipe(pipe),
        flags(flags) {
    shaderPipe->descriptorLayoutBindings.reserve(10);
    uset.reserve(10);
  }

  void visitSymbol(glslang::TIntermSymbol* symbol) {
    const auto& type = symbol->getType();
    const auto& quali = type.getQualifier();
    if (quali.layoutBinding < NO_BINDING_GIVEN) {
      if (uset.contains(quali.layoutBinding)) return;
      uset.insert(quali.layoutBinding);
      const auto desc = getDescTypeFromELF(type);
      auto foundDesc = std::ranges::find_if(shaderPipe->descriptorLayoutBindings, [&](auto& value) {
          return value.binding == quali.layoutBinding;
          });
      if (foundDesc == std::end(shaderPipe->descriptorLayoutBindings)) {
          const DescriptorSetLayoutBinding descBinding(
              quali.layoutBinding, desc.first, desc.second, flags);
          shaderPipe->descriptorLayoutBindings.push_back(descBinding);
      }
      else {
          foundDesc->stageFlags |= flags;
      }
    } else if (quali.isPushConstant() && !pushConst) {
      pushConst = true;
      const auto& structure = *type.getStruct();
      uint32_t size = 0;
      for (const auto& type : structure) {
        size += getSizeFromFormat(getFormatFromElf(*type.type));
      }
      const PushConstantRange range(flags, 0, size);
      shaderPipe->constranges.push_back(range);
    }
  }
};

void __implIntermToVulkanPipe(VulkanShaderPipe* shaderPipe,
                              const glslang::TIntermediate* interm,
                              const EShLanguage langName,
                              const ShaderCreateInfo& createInfo) {
  const auto node = interm->getTreeRoot();
  if (langName == EShLangVertex) {
    VertexShaderAnalizer analizer(shaderPipe, createInfo);
    node->traverse(&analizer);
    analizer.post();
  }
  const auto flags = getStageFromLang(langName);
  GeneralShaderAnalizer generalAnalizer(shaderPipe, flags);
  node->traverse(&generalAnalizer);

  shaderPipe->shader.push_back(std::pair(std::vector<uint32_t>(), flags));
  glslang::GlslangToSpv(*interm, shaderPipe->shader.back().first);
}

std::string next(const std::string& param) { return permute::next(param); }

void __implCreateDescSets(VulkanShaderPipe* shaderPipe,
                          VulkanShaderModule* vsm) {
  graphics::VulkanGraphicsModule* vgm =
      (graphics::VulkanGraphicsModule*)vsm->vgm;

  if (!shaderPipe->descriptorLayoutBindings.empty()) {
    const DescriptorSetLayoutCreateInfo layoutCreate(
        {}, shaderPipe->descriptorLayoutBindings);
    const auto descLayout = vgm->device.createDescriptorSetLayout(layoutCreate);
    vsm->setLayouts.push_back(descLayout);
    std::vector<DescriptorPoolSize> descPoolSizes;
    constexpr auto limit = 100000;
    for (const auto& binding : shaderPipe->descriptorLayoutBindings) {
      descPoolSizes.push_back(
          {binding.descriptorType, binding.descriptorCount * limit});
    }
    const DescriptorPoolCreateInfo descPoolCreateInfo({}, limit, descPoolSizes);
    const auto descPool = vgm->device.createDescriptorPool(descPoolCreateInfo);
    vsm->descPools.push_back(descPool);

    const auto layoutCreateInfo =
        PipelineLayoutCreateInfo({}, descLayout, shaderPipe->constranges);
    const auto pipeLayout = vgm->device.createPipelineLayout(layoutCreateInfo);
    vsm->pipeLayouts.push_back(pipeLayout);
    shaderPipe->layoutID = vsm->pipeLayouts.size() - 1;
  } else {
    shaderPipe->layoutID = INVALID_SIZE_T;
  }
}

std::unique_ptr<glslang::TShader> __implGenerateIntermediate(
    const ShaderInfo& pair) noexcept {
  const auto& additional = pair.additionalCode;

  std::string code(begin(pair.code), end(pair.code));
  const nlohmann::json json = nlohmann::json::parse(code);
  auto permute = permute::fromJson<permute::PermuteGLSL>(json);
  if (!permute.generate(additional)) {
    for (auto& str : permute.getContent()) PLOG_INFO << str;
    PLOG_ERROR << "Error while generating glsl!";
    return std::unique_ptr<glslang::TShader>();
  }
  return std::unique_ptr<glslang::TShader>(
      (glslang::TShader*)permute.getCostumData());
}

VulkanShaderPipe* __implLoadShaderPipeAndCompile(
    const std::vector<ShaderInfo>& vector, const ShaderCreateInfo& createInfo) {
  if (vector.size() == 0) {
    PLOG_ERROR << "Wrong shader count!";
    return nullptr;
  }
  VulkanShaderPipe* shaderPipe = new VulkanShaderPipe();
  glslang::InitializeProcess();
  util::OnExit e1(glslang::FinalizeProcess);
  shaderPipe->shader.reserve(vector.size());

  for (auto& pair : vector) {
    if (pair.code.empty()) {
      delete shaderPipe;
      return nullptr;
    }
    const auto shader = __implGenerateIntermediate(pair);
    if (!shader) {
      delete shaderPipe;
      return nullptr;
    }
    __implIntermToVulkanPipe(shaderPipe, shader->getIntermediate(),
                             getLangFromShaderLang(pair.language), createInfo);
    shaderPipe->shaderNames.push_back(pair.debug);
  }

  return shaderPipe;
}

ShaderPipe VulkanShaderModule::compile(const std::vector<ShaderInfo>& vector,
                                       const ShaderCreateInfo& createInfo) {
  std::lock_guard guard(this->mutex);
  const auto loadedPipes = __implLoadShaderPipeAndCompile(vector, createInfo);
  if (loadedPipes) __implCreateDescSets(loadedPipes, this);
  if (loadedPipes) this->shaderPipes.push_back(loadedPipes);
  return loadedPipes;
}

ShaderPipe VulkanShaderModule::loadShaderPipeAndCompile(
    const std::vector<std::string>& shadernames,
    const ShaderCreateInfo& createInfo) {
  std::vector<ShaderInfo> vector;
  vector.reserve(shadernames.size());
  for (const auto& name : shadernames) {
    const std::string abrivation = name.substr(name.size() - 4);
    const auto& content = util::wholeFile(name);
    if (content.empty()) {
      PLOG_WARNING << "File [" << name << "] not found";
      continue;
    }
    vector.push_back({ getLang(abrivation), content, {}, name });
  }
  return compile(vector, createInfo);
}

std::vector<TBindingHolder> VulkanShaderModule::createBindings(
    ShaderPipe pipe, const size_t count) {
  VulkanShaderPipe* shaderPipe = (VulkanShaderPipe*)pipe;
  const auto layout = shaderPipe->layoutID;
  if (layout == INVALID_SIZE_T) return {};
  auto output = bindingHolder.allocate(count);
  auto [descriptorSets, layoutsOut, pipeLayouts, status, expected] =
      output.iterator;
  std::fill(layoutsOut, layoutsOut + count, this->setLayouts[layout]);
#ifdef DEBUG
  std::fill(status, status + count, 0);
  size_t allBindings = 0;
  for (const auto &x : shaderPipe->descriptorLayoutBindings) {
    allBindings |= 1 << x.binding;
  }
  std::fill(expected, expected + count, allBindings);
  std::fill(status, status + count, 0);
#endif  // DEBUG

  const DescriptorSetAllocateInfo allocInfo(this->descPools[layout], count,
                                            &layoutsOut[0]);
  graphics::VulkanGraphicsModule* vgm =
      (graphics::VulkanGraphicsModule*)this->vgm;
  const auto sets = vgm->device.allocateDescriptorSets(allocInfo);
  std::ranges::copy(sets, descriptorSets);
  std::fill(pipeLayouts, pipeLayouts + count, this->pipeLayouts[layout]);
  return output.generateOutputArray<TBindingHolder>(count);
}

void VulkanShaderModule::changeInputBindings(const ShaderPipe pipe,
                                             const size_t bindingID,
                                             const size_t buffer) {}

void VulkanShaderModule::bindData(const BindingInfo* info, const size_t count) {
  graphics::VulkanGraphicsModule* vgm =
      (graphics::VulkanGraphicsModule*)this->vgm;

  std::vector<WriteDescriptorSet> set;
  set.reserve(count);
  std::vector<DescriptorBufferInfo> bufferInfo;
  bufferInfo.resize(count);
  std::vector<DescriptorImageInfo> imgInfo;
  imgInfo.resize(count);
  for (size_t i = 0; i < count; i++) {
    const auto& cinfo = info[i];
    DEBUG_EXPECT(cinfo);
    const auto descriptorSet = bindingHolder.get<0>(cinfo.bindingSet);
#ifdef DEBUG
    const auto old = bindingHolder.get<3>(cinfo.bindingSet);
    { bindingHolder.change<3>(cinfo.bindingSet) = old | (1 << cinfo.binding); }
#endif  // DEBUG
    switch (cinfo.type) {
      case BindingType::Storage:
      case BindingType::UniformBuffer: {
        const auto& bufferBindingInfo = cinfo.data.buffer;
        const auto buffer =
            vgm->bufferDataHolder.get<0>(bufferBindingInfo.dataID);
        bufferInfo[i] = (DescriptorBufferInfo(buffer, bufferBindingInfo.offset,
                                              bufferBindingInfo.size));
        set.push_back(
            WriteDescriptorSet(descriptorSet, cinfo.binding, 0, 1,
                               cinfo.type == BindingType::UniformBuffer
                                   ? DescriptorType::eUniformBuffer
                                   : DescriptorType::eStorageBuffer,
                               nullptr, bufferInfo.data() + i));
      } break;
      case BindingType::Texture:
      case BindingType::Sampler:
      case BindingType::InputAttachment: {
        const auto& tex = cinfo.data.texture;
        imgInfo[i] = DescriptorImageInfo(
            !tex.sampler ? vk::Sampler()
                         : vgm->sampler[tex.sampler.internalHandle],
            !tex.texture ? vk::ImageView()
                         : vgm->textureImageHolder.get<1>(tex.texture),
            ImageLayout::eShaderReadOnlyOptimal);
        set.push_back(WriteDescriptorSet(
            descriptorSet, cinfo.binding, cinfo.arrayID, 1,
            cinfo.type == BindingType::Texture ? DescriptorType::eSampledImage
            : (cinfo.type == BindingType::InputAttachment)
                ? DescriptorType::eInputAttachment
                : DescriptorType::eSampler,
            imgInfo.data() + i));
      } break;
      default:
        throw std::runtime_error("Can not find descriptor type in bind data!");
    }
  }
  vgm->device.updateDescriptorSets(set, {});
  return;
}

void VulkanShaderModule::addToRender(
    const std::span<const TBindingHolder> bindings, void* customData) {
  for (const auto binding : bindings) {
    const auto pipeLayout = bindingHolder.get<2>(binding);
    const auto descriptorSet = bindingHolder.get<0>(binding);
#ifdef DEBUG
    auto bitset = bindingHolder.get<3>(binding);
    auto expected = bindingHolder.get<4>(binding);
    debugExpect(bitset == expected, "Binding not updatet at index [" +
                                        std::to_string(bitset ^ expected) + "] !");
#endif  // DEBUG

    ((CommandBuffer*)customData)
        ->bindDescriptorSets(PipelineBindPoint::eGraphics, pipeLayout, 0,
                             descriptorSet, {});
  }
}

void VulkanShaderModule::addToMaterial(const graphics::Material* material,
                                       void* customData) {
  using namespace tge::graphics;
  const auto vkPipe = ((VulkanShaderPipe*)material->costumShaderData);
  const auto layOut = vkPipe->layoutID;
  if (layOut != INVALID_SIZE_T) [[likely]] {
    ((GraphicsPipelineCreateInfo*)customData)->setLayout(pipeLayouts[layOut]);
  } else {
    ((GraphicsPipelineCreateInfo*)customData)->setLayout(defaultLayout);
  }
}

void VulkanShaderModule::init() {
  graphics::VulkanGraphicsModule* vgm =
      (graphics::VulkanGraphicsModule*)this->vgm;
  if (!vgm->isInitialiazed)
    throw std::runtime_error(
        "Vulkan module not initalized, Vulkan Shader Module cannot be used!");

  const DescriptorSetLayoutCreateInfo layoutCreate({}, {});
  defaultDescLayout = vgm->device.createDescriptorSetLayout(layoutCreate);

  const auto layoutCreateInfo = PipelineLayoutCreateInfo({}, defaultDescLayout);
  defaultLayout = vgm->device.createPipelineLayout(layoutCreateInfo);
}

void VulkanShaderModule::destroy() {
  graphics::VulkanGraphicsModule* vgm =
      (graphics::VulkanGraphicsModule*)this->vgm;
  vgm->device.destroyDescriptorSetLayout(defaultDescLayout);
  vgm->device.destroyPipelineLayout(defaultLayout);
  for (auto pool : descPools) vgm->device.destroyDescriptorPool(pool);
  for (auto dscLayout : setLayouts)
    vgm->device.destroyDescriptorSetLayout(dscLayout);
  for (auto pipeLayout : pipeLayouts)
    vgm->device.destroyPipelineLayout(pipeLayout);
}

}  // namespace tge::shader
