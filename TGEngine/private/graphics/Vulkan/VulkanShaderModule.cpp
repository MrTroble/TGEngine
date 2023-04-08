#include "../../../public/graphics/vulkan/VulkanShaderModule.hpp"

#include "../../../public/Error.hpp"
#include "../../../public/Util.hpp"
#include "../../../public/graphics/vulkan/VulkanModuleDef.hpp"
#include "../../../public/graphics/vulkan/VulkanShaderPipe.hpp"
#define ENABLE_OPT 1
#include <glslang/MachineIndependent/localintermediate.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <glslang/SPIRV/SpvTools.h>

#include <iostream>
#include <vulkan/vulkan.hpp>
#undef ERROR
#define SPR_NO_GLSL_INCLUDE 1
#include "../../../public/headerlibs/ShaderPermute.hpp"

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

  VertexShaderAnalizer(VulkanShaderPipe* pipe)
      : glslang::TIntermTraverser(false, true, false), shaderPipe(pipe) {
    ioVars.reserve(10);
  }
  std::unordered_set<size_t> ioVars;

  void visitSymbol(glslang::TIntermSymbol* symbol) {
    const auto& qualifier = symbol->getQualifier();
    if (ioVars.contains(symbol->getId())) return;
    ioVars.emplace(symbol->getId());
    if (qualifier.layoutLocation < NO_LAYOUT_GIVEN) {
      if (qualifier.storage == glslang::TStorageQualifier::EvqVaryingIn) {
        const auto bind = qualifier.layoutBinding == NO_BINDING_GIVEN
                              ? 0
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
    std::cout << type.getCompleteString() << std::endl;
    const auto& quali = type.getQualifier();
    if (quali.layoutBinding < NO_BINDING_GIVEN) {
      if (uset.contains(quali.layoutBinding)) return;
      uset.insert(quali.layoutBinding);
      const auto desc = getDescTypeFromELF(type);
      const DescriptorSetLayoutBinding descBinding(
          quali.layoutBinding, desc.first, desc.second, flags);
      shaderPipe->descriptorLayoutBindings.push_back(descBinding);
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
                              const EShLanguage langName) {
  const auto node = interm->getTreeRoot();
  if (langName == EShLangVertex) {
    VertexShaderAnalizer analizer(shaderPipe);
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
    for (auto& str : permute.getContent()) printf("%s\n", str.c_str());
    printf("Error while generating glsl!");
    return std::unique_ptr<glslang::TShader>();
  }
  return std::unique_ptr<glslang::TShader>(
      (glslang::TShader*)permute.getCostumData());
}

VulkanShaderPipe* __implLoadShaderPipeAndCompile(
    const std::vector<ShaderInfo>& vector) {
  if (vector.size() == 0) {
    printf("Wrong shader count!");
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
                             getLangFromShaderLang(pair.language));
  }

  return shaderPipe;
}

ShaderPipe VulkanShaderModule::compile(const std::vector<ShaderInfo>& vector) {
  std::lock_guard guard(this->mutex);
  const auto loadedPipes = __implLoadShaderPipeAndCompile(vector);
  if (loadedPipes) __implCreateDescSets(loadedPipes, this);
  return loadedPipes;
}

ShaderPipe VulkanShaderModule::loadShaderPipeAndCompile(
    const std::vector<std::string>& shadernames) {
  std::vector<ShaderInfo> vector;
  vector.reserve(shadernames.size());
  for (const auto& name : shadernames) {
    const std::string abrivation = name.substr(name.size() - 4);
    const auto& content = util::wholeFile(name);
    if (content.empty()) {
      printf("[WARN]: File [%s] not found!\n", name.c_str());
      continue;
    }
    vector.push_back({getLang(abrivation), content});
  }
  return compile(vector);
}

inline std::string getStringFromIOType(const IOType t) {
  switch (t) {
    case IOType::FLOAT:
      return "float";
    case IOType::VEC2:
      return "vec2";
    case IOType::VEC3:
      return "vec3";
    case IOType::VEC4:
      return "vec4";
    case IOType::MAT3:
      return "mat3";
    case IOType::MAT4:
      return "mat4";
    case IOType::SAMPLER2:
      return "sampler";
    default:
      throw std::runtime_error("Couldn't find string to type!");
  }
}

inline std::string getStringFromSamplerIOType(const SamplerIOType t) {
  switch (t) {
    case SamplerIOType::SAMPLER:
      return "sampler";
    case SamplerIOType::TEXTURE:
      return "texture2D";
    default:
      throw std::runtime_error("Couldn't find string to type!");
  }
}

inline void function(const Instruction& ins,
                     const std::vector<Instruction>& instructions,
                     const std::string fname, std::ostringstream& stream) {
  stream << getStringFromIOType(ins.outputType) << " " << ins.name << " = "
         << fname << "(" << ins.inputs[0];
  for (size_t i = 1; i < ins.inputs.size(); i++) {
    stream << ", " << ins.inputs[i];
  }
  stream << ");" << std::endl;
}

inline void aggragteFunction(const Instruction& ins,
                             const std::vector<Instruction>& instructions,
                             const std::string fname,
                             std::ostringstream& stream) {
  stream << getStringFromIOType(ins.outputType) << " " << ins.name << " = "
         << ins.inputs[0];
  for (size_t i = 1; i < ins.inputs.size(); i++) {
    stream << fname << ins.inputs[i];
  }
  stream << ";" << std::endl;
}

inline void addInstructionsToCode(const std::vector<Instruction>& instructions,
                                  std::ostringstream& stream) {
  for (const auto& ins : instructions) {
    switch (ins.instruciontType) {
      case InstructionType::NOOP:
        break;
      case InstructionType::MULTIPLY:
        aggragteFunction(ins, instructions, " * ", stream);
        break;
      case InstructionType::ADD:
        aggragteFunction(ins, instructions, " + ", stream);
        break;
      case InstructionType::SET:
        stream << ins.name << " = " << ins.inputs[0] << ";" << std::endl;
        break;
      case InstructionType::TEMP:
        stream << getStringFromIOType(ins.outputType) << " " << ins.name
               << " = " << ins.inputs[0] << ";" << std::endl;
        break;
      case InstructionType::TEXTURE:
        function(ins, instructions, "texture", stream);
        break;
      case InstructionType::SAMPLER:
        break;
      case InstructionType::VEC4CTR:
        function(ins, instructions, "vec4", stream);
        break;
      case InstructionType::DOT:
        function(ins, instructions, "dot", stream);
        break;
      case InstructionType::CROSS:
        function(ins, instructions, "cross", stream);
        break;
      case InstructionType::CLAMP:
        function(ins, instructions, "clamp", stream);
        break;
      case InstructionType::MIN:
        function(ins, instructions, "min", stream);
        break;
      case InstructionType::MAX:
        function(ins, instructions, "max", stream);
        break;
      case InstructionType::NORMALIZE:
        function(ins, instructions, "normalize", stream);
        break;
      case InstructionType::SUBTRACT:
        aggragteFunction(ins, instructions, " - ", stream);
        break;
      case InstructionType::DIVIDE:
        aggragteFunction(ins, instructions, " / ", stream);
        break;
      default:
        break;
    }
  }
}

inline uint32_t strideFromIOType(const IOType t) {
  switch (t) {
    case IOType::VEC2:
      return 8;
    case IOType::VEC3:
      return 12;
    case IOType::VEC4:
      return 16;
    default:
      throw std::runtime_error("No stride for IOType!");
  }
}

ShaderPipe VulkanShaderModule::createShaderPipe(
    const ShaderCreateInfo* shaderCreateInfo, const size_t shaderCount) {
  glslang::InitializeProcess();
  util::OnExit e1(glslang::FinalizeProcess);
  auto shaderPipe = new VulkanShaderPipe();
  // IT ISN'T PRETTY BUT IT WORKS - SOMEWHAT
  for (size_t i = 0; i < shaderCount; i++) {
    const ShaderCreateInfo& createInfo = shaderCreateInfo[i];
    ShaderInfo info = {createInfo.shaderType};
    std::ostringstream codebuff;
    codebuff << "#version 450" << std::endl << std::endl;
    for (const auto& input : createInfo.inputs) {
      codebuff << "layout(location=" << input.binding << ") in "
               << getStringFromIOType(input.iotype) << " " << input.name << ";"
               << std::endl;
    }
    codebuff << std::endl;
    for (const auto& out : createInfo.outputs) {
      codebuff << "layout(location=" << out.binding << ") out "
               << getStringFromIOType(out.iotype) << " " << out.name << ";"
               << std::endl;
    }
    codebuff << std::endl;
    size_t ublockID = 0;
    for (const auto& uniform : createInfo.unifromIO) {
      codebuff << "layout(binding=" << uniform.binding << ") uniform UBLOCK_"
               << ublockID << " {" << std::endl
               << getStringFromIOType(uniform.iotype) << " " << uniform.name
               << ";} ublock_" << ublockID << ";" << std::endl;
      ublockID++;
    }
    codebuff << std::endl;
    for (const auto& sampler : createInfo.samplerIO) {
      // layout(binding = 1) uniform texture2D tex;
      codebuff << "layout(binding=" << sampler.binding << ") uniform "
               << getStringFromSamplerIOType(sampler.iotype) << " "
               << sampler.name;
      if (sampler.size > 1) {
        codebuff << "[" << sampler.size << "]";
      }
      codebuff << ";" << std::endl;
    }
    codebuff << std::endl;
    if (createInfo.shaderType == ShaderType::VERTEX) {
      codebuff << "out gl_PerVertex { vec4 gl_Position; };" << std::endl;
    }
    codebuff << "void main() {" << std::endl;
    addInstructionsToCode(createInfo.instructions, codebuff);
    codebuff << std::endl << createInfo.__code << "}" << std::endl;
    std::string code(codebuff.str());
    std::cout << code << std::endl;
    info.code = std::vector(code.begin(), code.end());
    info.code.push_back('\0');
    const auto shader = __implGenerateIntermediate(info);
    glslang::TIntermediate* tint = shader->getIntermediate();
    __implIntermToVulkanPipe(shaderPipe, tint,
                             getLangFromShaderLang(createInfo.shaderType));
    if (createInfo.shaderType == ShaderType::VERTEX) {
      std::sort(shaderPipe->vertexInputAttributes.begin(),
                shaderPipe->vertexInputAttributes.end(),
                [](auto x, auto y) { return x.location < y.location; });
      uint32_t maxBinding = 0;
      for (size_t i = 0; i < shaderPipe->vertexInputAttributes.size(); i++) {
        auto& state = shaderPipe->vertexInputAttributes[i];
        state.binding = createInfo.inputs[i].buffer;
        if (state.binding != 0) state.offset = 0;
        if (maxBinding < state.binding) maxBinding = state.binding;
      }
      shaderPipe->vertexInputBindings.resize(maxBinding + 1);
      for (size_t i = 0; i < shaderPipe->vertexInputBindings.size(); i++) {
        auto& bind = shaderPipe->vertexInputBindings[i];
        const auto& info = createInfo.inputs[i];
        bind.binding = info.buffer;
        bind.stride = strideFromIOType(info.iotype);
      }
      shaderPipe->inputStateCreateInfo = PipelineVertexInputStateCreateInfo(
          {}, shaderPipe->vertexInputBindings,
          shaderPipe->vertexInputAttributes);
    }
  }
  __implCreateDescSets(shaderPipe, this);
  return shaderPipe;
}

size_t VulkanShaderModule::createBindings(ShaderPipe pipe, const size_t count) {
  VulkanShaderPipe* shaderPipe = (VulkanShaderPipe*)pipe;
  const auto layout = shaderPipe->layoutID;
  if (layout == INVALID_SIZE_T) return INVALID_SIZE_T;
  std::vector<DescriptorSetLayout> layouts(count);
  std::fill(layouts.begin(), layouts.end(), this->setLayouts[layout]);
  const DescriptorSetAllocateInfo allocInfo(this->descPools[layout], count,
                                            layouts.data());
  graphics::VulkanGraphicsModule* vgm =
      (graphics::VulkanGraphicsModule*)this->vgm;
  const auto sets = vgm->device.allocateDescriptorSets(allocInfo);
  const auto nextID = this->descSets.size();
  this->descSets.resize(nextID + count);
  std::copy(sets.begin(), sets.end(), this->descSets.begin() + nextID);

  for (size_t i = 0; i < count; i++) {
    pipeInfos.push_back({nextID + i, layout});
  }

  std::vector<BindingInfo> bInfo;
  if (defaultbindings.size() > layout && shaderPipe->needsDefaultBindings) {
    auto& bindings = defaultbindings[layout];
    if (!bindings.empty()) {
      for (size_t i = 0; i < count; i++) {
        const auto nID = nextID + i;
        for (auto& b : bindings) {
          b.bindingSet = nID;
          bInfo.push_back(b);
        }
      }
      BindingInfo binding;
      binding.type = BindingType::Texture;
      binding.binding = 1;
      for (size_t i = 0; i < count; i++) {
        binding.bindingSet = nextID + i;
        for (size_t i = 5; i < vgm->textureImages.size(); i++) {
          binding.arrayID = i;
          bInfo.push_back(binding);
        }
        binding.data.texture.texture =
            vgm->getGraphicsModule()->defaultTextureID;
        for (size_t i = 0; i < 5; i++) {
          binding.arrayID = i;
          bInfo.push_back(binding);
        }
        for (size_t i = vgm->textureImages.size(); i < 255; i++) {
          binding.arrayID = i;
          bInfo.push_back(binding);
        }
      }

      this->bindData(bInfo.data(), bInfo.size());
    }
  }
  return nextID;
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
    switch (cinfo.type) {
      case BindingType::Storage:
      case BindingType::UniformBuffer: {
        const auto& buffI = cinfo.data.buffer;
        const auto buffer = vgm->bufferDataHolder.get(
            vgm->bufferDataHolder.allocation1, buffI.dataID);
        bufferInfo[i] =
            (DescriptorBufferInfo(buffer, buffI.offset, buffI.size));
        set.push_back(
            WriteDescriptorSet(descSets[cinfo.bindingSet], cinfo.binding, 0, 1,
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
            (bool)tex.sampler ? vgm->sampler[tex.sampler.internalHandle]
                              : vk::Sampler(),
            tex.texture == INVALID_SIZE_T ? vk::ImageView()
                                      : vgm->textureImageViews[tex.texture],
            ImageLayout::eShaderReadOnlyOptimal);
        set.push_back(WriteDescriptorSet(
            descSets[cinfo.bindingSet], cinfo.binding, cinfo.arrayID, 1,
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

void VulkanShaderModule::addToRender(const size_t* bids, const size_t size,
                                     void* customData) {
  std::vector<DescriptorSet> sets;
  PipelineLayout pipeLayout;
  for (size_t i = 0; i < size; i++) {
    const auto bindingID = bids[i];
    if (this->pipeInfos.size() > bindingID && bindingID >= 0) {
      const auto& bInfo = pipeInfos[bindingID];
      const auto descSet = descSets[bInfo.descSet];
      pipeLayout = pipeLayouts[bInfo.pipeline];
      sets.push_back(descSet);
    }
  }
  if (!sets.empty()) {
    ((CommandBuffer*)customData)
        ->bindDescriptorSets(PipelineBindPoint::eGraphics, pipeLayout, 0, sets,
                             {});
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
