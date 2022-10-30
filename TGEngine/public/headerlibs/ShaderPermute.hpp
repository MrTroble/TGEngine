/*
   Copyright 2021 MrTroble

   Licensed under the Apache License,
   Version 2.0(the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#pragma once

#define SPR_VERSION_MAJOR 1
#define SPR_VERSION_MINOR 0
#define SPR_VERSION_PATCH 4

#ifdef SPR_USE_FORMAT_LIB
#include <format>
#else
#include <sstream>
#endif // SPR_USE_FORMAT_LIB

#include <functional>
#include <map>
#include <string>
#include <vector>

#ifndef SPR_NO_FSTREAM
#include <fstream>
#endif

#ifndef SPR_NO_JSON_HPP_INCLUDE
#include "json.hpp"
#endif

#if defined(_NODISCARD) && !(defined(SPR_DISABLE_NODISCARD))
#define SPR_NODISCARD _NODISCARD
#elif !(defined(SPR_NODISCARD))
#define SPR_NODISCARD
#endif

#define SPR_OPTIONAL_FROM(v1)                                                  \
  const auto eItr##v1 = end(nlohmann_json_j);                                  \
  const auto itr##v1 = nlohmann_json_j.find(#v1);                              \
  if (eItr##v1 != itr##v1)                                                     \
    (*itr##v1).get_to(nlohmann_json_t.v1);

#define SPR_OPTIONAL_TO(v1)                                                    \
  if (!((bool)nlohmann_json_t.v1)) {                                           \
    NLOHMANN_JSON_TO(v1);                                                      \
  }

#define SPR_OPTIONAL_TO_L(v1)                                                  \
  if (!((bool)nlohmann_json_t.v1.empty())) {                                   \
    NLOHMANN_JSON_TO(v1);                                                      \
  }

#if !defined(SPR_NO_GLSL) && !defined(SPR_NO_GLSL_INCLUDE)
#include <SPIRV/GlslangToSpv.h>
#include <glslang/Public/ShaderLang.h>
#endif

#if defined(SPR_MATERIALX) && !defined(SPR_NO_MATERIALX_INCLUDE)
#include <MaterialXGenGlsl/GlslShaderGenerator.h>
#endif

#ifndef SPR_NO_STATIC
#define SPR_STATIC static
#endif

#ifndef SPR_NO_GLSL
NLOHMANN_JSON_SERIALIZE_ENUM(EShLanguage,
                             {{EShLangVertex, "vertex"},
                              {EShLangTessControl, "tesslation-control"},
                              {EShLangTessEvaluation, "tesslation-evaluation"},
                              {EShLangGeometry, "geometry"},
                              {EShLangFragment, "fragment"},
                              {EShLangCompute, "compute"},
                              {EShLangRayGen, "raygen"},
                              {EShLangIntersect, "intersect"},
                              {EShLangAnyHit, "anyhit"},
                              {EShLangClosestHit, "closehit"},
                              {EShLangMiss, "miss"},
                              {EShLangCallable, "callable"},
                              {EShLangTaskNV, "tasknv"},
                              {EShLangMeshNV, "meshnv"}

                             });

namespace glslang {
NLOHMANN_JSON_SERIALIZE_ENUM(EShClient, {{EShClientNone, "none"},
                                         {EShClientVulkan, "vulkan"},
                                         {EShClientOpenGL, "opengl"}});
NLOHMANN_JSON_SERIALIZE_ENUM(EShTargetClientVersion,
                             {{EShTargetVulkan_1_0, "vulkan_1_0"},
                              {EShTargetVulkan_1_1, "vulkan_1_1"},
                              {EShTargetVulkan_1_2, "vulkan_1_2"},
                              {EShTargetOpenGL_450, "opengl_450"}});
NLOHMANN_JSON_SERIALIZE_ENUM(EShTargetLanguage,
                             {{EShTargetNone, "none"}, {EShTargetSpv, "spv"}});
NLOHMANN_JSON_SERIALIZE_ENUM(EShTargetLanguageVersion,
                             {
                                 {EShTargetSpv_1_0, "spv_1_0"},
                                 {EShTargetSpv_1_1, "spv_1_1"},
                                 {EShTargetSpv_1_2, "spv_1_2"},
                                 {EShTargetSpv_1_3, "spv_1_3"},
                                 {EShTargetSpv_1_4, "spv_1_4"},
                                 {EShTargetSpv_1_5, "spv_1_5"},
                             });
} // namespace glslang
#endif

namespace permute {

using lookup =
    std::map<std::string, std::function<std::string(const std::string &)>>;

enum class ShaderCodeFlags { NONE = 0, REQUIRED = 1 };

NLOHMANN_JSON_SERIALIZE_ENUM(ShaderCodeFlags,
                             {{ShaderCodeFlags::NONE, "none"},
                              {ShaderCodeFlags::REQUIRED, "required"}})

enum class OutputType { ERROR, TEXT, BINARY };

SPR_NODISCARD inline bool isRequired(const ShaderCodeFlags flag) {
  return (int)flag & (int)ShaderCodeFlags::REQUIRED;
}

template <class T>
SPR_NODISCARD inline bool isInDependency(T &dependency, T &dependsOn) {
  const auto endItr = end(dependency);
  for (auto target : dependsOn) {
    auto itr = begin(dependency);
    if (std::find(itr, endItr, target) == endItr)
      return false;
  }
  return true;
}

struct ShaderCodes {
  std::vector<std::string> code;
  ShaderCodeFlags flags = ShaderCodeFlags::NONE;
  std::vector<std::string> dependsOn;

  friend void to_json(nlohmann::json &nlohmann_json_j,
                      const ShaderCodes &nlohmann_json_t) {
    NLOHMANN_JSON_TO(code);
    SPR_OPTIONAL_TO(flags);
    SPR_OPTIONAL_TO_L(dependsOn);
  }

  friend void from_json(const nlohmann::json &nlohmann_json_j,
                        ShaderCodes &nlohmann_json_t) {
    NLOHMANN_JSON_FROM(code);
    SPR_OPTIONAL_FROM(flags);
    SPR_OPTIONAL_FROM(dependsOn);
  }
};

struct GenerateInput {
  const std::vector<ShaderCodes> &codes;
  const std::vector<std::string> &dependencies;
  const nlohmann::json &settings;
};

struct GenerateOutput {
  std::vector<std::string> output;
  OutputType type = OutputType::ERROR;
  std::vector<unsigned int> data;
  void *costumData = nullptr;
};

class PermuteText {
public:
  SPR_NODISCARD inline static GenerateOutput
  generate(const GenerateInput input) {
    std::vector<std::string> buffer;
    buffer.reserve(input.codes.size());
    for (const auto &code : input.codes) {
      if (isRequired(code.flags) || code.dependsOn.empty() ||
          isInDependency(input.dependencies, code.dependsOn)) {
        for (const auto &codePart : code.code)
          buffer.push_back(codePart);
      }
    }
    return {buffer, OutputType::TEXT};
  }
};

inline std::string postProcess(std::string &codePart, const lookup &callback) {
  if (callback.empty())
    return codePart;
  auto eItr = end(codePart);
  auto startWordItr = eItr;
  auto paramStartItr = eItr;
  lookup::value_type::second_type func(nullptr);
  for (auto itr = begin(codePart); itr != eItr; itr++) {
    if (*itr == '$') {
      startWordItr = itr + 1;
      continue;
    }
    if (startWordItr != eItr && *itr == '_') {
      const auto word = std::string(startWordItr, itr);
      const auto fncItr = callback.find(word);
      if (fncItr != end(callback)) {
        func = fncItr->second;
        paramStartItr = itr + 1;
      } else {
        startWordItr = eItr;
      }
      continue;
    }
    if (startWordItr != eItr && *itr == ' ') {
      const std::string param(paramStartItr, itr);
      const auto replace = func(param);
      const auto distance = std::distance(startWordItr, itr);
      codePart = codePart.replace(startWordItr - 1, itr, replace);
      eItr = end(codePart);
      itr = begin(codePart) + distance;
      startWordItr = eItr;
    }
  }
  return codePart;
}

inline void postProcess(std::vector<std::string> &codePart,
                        const lookup &callback) {
  for (size_t i = 0; i < codePart.size(); i++) {
    codePart[i] = postProcess(codePart[i], callback) + "\n";
  }
}

#ifndef SPR_NO_GLSL
struct GlslSettings {
  EShLanguage shaderType;
  glslang::EShClient targetClient = glslang::EShClient::EShClientVulkan;
  glslang::EShTargetClientVersion targetVersion =
      glslang::EShTargetClientVersion::EShTargetVulkan_1_0;
  glslang::EShTargetLanguage targetLanguage =
      glslang::EShTargetLanguage::EShTargetSpv;
  glslang::EShTargetLanguageVersion targetLanguageVersion =
      glslang::EShTargetLanguageVersion::EShTargetSpv_1_0;

  friend void to_json(nlohmann::json &nlohmann_json_j,
                      const GlslSettings &nlohmann_json_t) {
    NLOHMANN_JSON_TO(shaderType);
    SPR_OPTIONAL_TO(targetClient);
    SPR_OPTIONAL_TO(targetVersion);
    SPR_OPTIONAL_TO(targetLanguage);
    SPR_OPTIONAL_TO(targetLanguageVersion);
  }

  friend void from_json(const nlohmann::json &nlohmann_json_j,
                        GlslSettings &nlohmann_json_t) {
    NLOHMANN_JSON_FROM(shaderType);
    SPR_OPTIONAL_FROM(targetClient);
    SPR_OPTIONAL_FROM(targetVersion);
    SPR_OPTIONAL_FROM(targetLanguage);
    SPR_OPTIONAL_FROM(targetLanguageVersion);
  }
};

SPR_STATIC std::map<std::string, int> lookupCounter;

SPR_STATIC std::string next(const std::string &input) {
  const auto id = lookupCounter[input];
  lookupCounter[input]++;
#ifdef SPR_USE_FORMAT_LIB
  if (input == "ublock")
    return std::format("layout(binding={}) uniform BLOCK{}", id, id);
  return std::format("layout(location={}) {}", id, input);
#else
  std::stringstream strStream;
  if (input == "ublock") {
    strStream << "layout(binding=" << id << ") uniform BLOCK" << id;
  } else {
    strStream << "layout(location=" << id << ") " << input;
  }
  return strStream.str();
#endif
}

SPR_STATIC lookup glslLookup
#ifndef SPR_NO_STATIC
    = {{"next", next}}
#endif // SPR_STATIC
;

class ShaderTraverser;
static std::vector<permute::ShaderTraverser *> traverser;

class ShaderTraverser {
public:
  ShaderTraverser() { permute::traverser.push_back(this); }

  ~ShaderTraverser() {
    permute::traverser.erase(
        std::remove(begin(permute::traverser), end(permute::traverser), this));
  }

  virtual void visitSymbol(glslang::TIntermSymbol *) {}
  virtual void visitConstantUnion(glslang::TIntermConstantUnion *) {}
  virtual bool visitBinary(glslang::TVisit, glslang::TIntermBinary *) {
    return true;
  }
  virtual bool visitUnary(glslang::TVisit, glslang::TIntermUnary *) {
    return true;
  }
  virtual bool visitSelection(glslang::TVisit, glslang::TIntermSelection *) {
    return true;
  }
  virtual bool visitAggregate(glslang::TVisit, glslang::TIntermAggregate *) {
    return true;
  }
  virtual bool visitLoop(glslang::TVisit, glslang::TIntermLoop *) {
    return true;
  }
  virtual bool visitBranch(glslang::TVisit, glslang::TIntermBranch *) {
    return true;
  }
  virtual bool visitSwitch(glslang::TVisit, glslang::TIntermSwitch *) {
    return true;
  }
  virtual void postProcess() {}
  virtual bool isValid(const GlslSettings &settings) = 0;
};

namespace impl {

class ShaderTraverser : public glslang::TIntermTraverser {
public:
  permute::ShaderTraverser *traverser;

  ShaderTraverser(permute::ShaderTraverser *traverser) : traverser(traverser) {}

  virtual void visitSymbol(glslang::TIntermSymbol *s) {
    traverser->visitSymbol(s);
  }

  virtual void visitConstantUnion(glslang::TIntermConstantUnion *s) {
    traverser->visitConstantUnion(s);
  }

  virtual bool visitBinary(glslang::TVisit v, glslang::TIntermBinary *s) {
    return traverser->visitBinary(v, s);
  }
  virtual bool visitUnary(glslang::TVisit v, glslang::TIntermUnary *s) {
    return traverser->visitUnary(v, s);
  }
  virtual bool visitSelection(glslang::TVisit v, glslang::TIntermSelection *s) {
    return traverser->visitSelection(v, s);
  }
  virtual bool visitAggregate(glslang::TVisit v, glslang::TIntermAggregate *s) {
    return traverser->visitAggregate(v, s);
  }
  virtual bool visitLoop(glslang::TVisit v, glslang::TIntermLoop *s) {
    return traverser->visitLoop(v, s);
  }
  virtual bool visitBranch(glslang::TVisit v, glslang::TIntermBranch *s) {
    return traverser->visitBranch(v, s);
  }
  virtual bool visitSwitch(glslang::TVisit v, glslang::TIntermSwitch *s) {
    return traverser->visitSwitch(v, s);
  }
};

} // namespace impl

#ifdef SPR_MATERIALX
class PermuteMaterialX {
public:
  SPR_NODISCARD inline static GenerateOutput
  generate(const GenerateInput input) {
    auto generator = MaterialX::GlslShaderGenerator::create();
    auto doc = MaterialX::Document::createDocument();
    generator->registerShaderMetadata() auto shader = generator->generate("", );
  }
};
#endif // SPR_NO_MATERIALX

inline TBuiltInResource InitResources()
{
    TBuiltInResource Resources{};
    Resources.maxLights = 32;
    Resources.maxClipPlanes = 6;
    Resources.maxTextureUnits = 32;
    Resources.maxTextureCoords = 32;
    Resources.maxVertexAttribs = 64;
    Resources.maxVertexUniformComponents = 4096;
    Resources.maxVaryingFloats = 64;
    Resources.maxVertexTextureImageUnits = 32;
    Resources.maxCombinedTextureImageUnits = 80;
    Resources.maxTextureImageUnits = 32;
    Resources.maxFragmentUniformComponents = 4096;
    Resources.maxDrawBuffers = 32;
    Resources.maxVertexUniformVectors = 128;
    Resources.maxVaryingVectors = 8;
    Resources.maxFragmentUniformVectors = 16;
    Resources.maxVertexOutputVectors = 16;
    Resources.maxFragmentInputVectors = 15;
    Resources.minProgramTexelOffset = -8;
    Resources.maxProgramTexelOffset = 7;
    Resources.maxClipDistances = 8;
    Resources.maxComputeWorkGroupCountX = 65535;
    Resources.maxComputeWorkGroupCountY = 65535;
    Resources.maxComputeWorkGroupCountZ = 65535;
    Resources.maxComputeWorkGroupSizeX = 1024;
    Resources.maxComputeWorkGroupSizeY = 1024;
    Resources.maxComputeWorkGroupSizeZ = 64;
    Resources.maxComputeUniformComponents = 1024;
    Resources.maxComputeTextureImageUnits = 16;
    Resources.maxComputeImageUniforms = 8;
    Resources.maxComputeAtomicCounters = 8;
    Resources.maxComputeAtomicCounterBuffers = 1;
    Resources.maxVaryingComponents = 60;
    Resources.maxVertexOutputComponents = 64;
    Resources.maxGeometryInputComponents = 64;
    Resources.maxGeometryOutputComponents = 128;
    Resources.maxFragmentInputComponents = 128;
    Resources.maxImageUnits = 8;
    Resources.maxCombinedImageUnitsAndFragmentOutputs = 8;
    Resources.maxCombinedShaderOutputResources = 8;
    Resources.maxImageSamples = 0;
    Resources.maxVertexImageUniforms = 0;
    Resources.maxTessControlImageUniforms = 0;
    Resources.maxTessEvaluationImageUniforms = 0;
    Resources.maxGeometryImageUniforms = 0;
    Resources.maxFragmentImageUniforms = 8;
    Resources.maxCombinedImageUniforms = 8;
    Resources.maxGeometryTextureImageUnits = 16;
    Resources.maxGeometryOutputVertices = 256;
    Resources.maxGeometryTotalOutputComponents = 1024;
    Resources.maxGeometryUniformComponents = 1024;
    Resources.maxGeometryVaryingComponents = 64;
    Resources.maxTessControlInputComponents = 128;
    Resources.maxTessControlOutputComponents = 128;
    Resources.maxTessControlTextureImageUnits = 16;
    Resources.maxTessControlUniformComponents = 1024;
    Resources.maxTessControlTotalOutputComponents = 4096;
    Resources.maxTessEvaluationInputComponents = 128;
    Resources.maxTessEvaluationOutputComponents = 128;
    Resources.maxTessEvaluationTextureImageUnits = 16;
    Resources.maxTessEvaluationUniformComponents = 1024;
    Resources.maxTessPatchComponents = 120;
    Resources.maxPatchVertices = 32;
    Resources.maxTessGenLevel = 64;
    Resources.maxViewports = 16;
    Resources.maxVertexAtomicCounters = 0;
    Resources.maxTessControlAtomicCounters = 0;
    Resources.maxTessEvaluationAtomicCounters = 0;
    Resources.maxGeometryAtomicCounters = 0;
    Resources.maxFragmentAtomicCounters = 8;
    Resources.maxCombinedAtomicCounters = 8;
    Resources.maxAtomicCounterBindings = 1;
    Resources.maxVertexAtomicCounterBuffers = 0;
    Resources.maxTessControlAtomicCounterBuffers = 0;
    Resources.maxTessEvaluationAtomicCounterBuffers = 0;
    Resources.maxGeometryAtomicCounterBuffers = 0;
    Resources.maxFragmentAtomicCounterBuffers = 1;
    Resources.maxCombinedAtomicCounterBuffers = 1;
    Resources.maxAtomicCounterBufferSize = 16384;
    Resources.maxTransformFeedbackBuffers = 4;
    Resources.maxTransformFeedbackInterleavedComponents = 64;
    Resources.maxCullDistances = 8;
    Resources.maxCombinedClipAndCullDistances = 8;
    Resources.maxSamples = 4;
    Resources.maxMeshOutputVerticesNV = 256;
    Resources.maxMeshOutputPrimitivesNV = 512;
    Resources.maxMeshWorkGroupSizeX_NV = 32;
    Resources.maxMeshWorkGroupSizeY_NV = 1;
    Resources.maxMeshWorkGroupSizeZ_NV = 1;
    Resources.maxTaskWorkGroupSizeX_NV = 32;
    Resources.maxTaskWorkGroupSizeY_NV = 1;
    Resources.maxTaskWorkGroupSizeZ_NV = 1;
    Resources.maxMeshViewCountNV = 4;

    Resources.limits.nonInductiveForLoops = 1;
    Resources.limits.whileLoops = 1;
    Resources.limits.doWhileLoops = 1;
    Resources.limits.generalUniformIndexing = 1;
    Resources.limits.generalAttributeMatrixVectorIndexing = 1;
    Resources.limits.generalVaryingIndexing = 1;
    Resources.limits.generalSamplerIndexing = 1;
    Resources.limits.generalVariableIndexing = 1;
    Resources.limits.generalConstantMatrixVectorIndexing = 1;

    return Resources;
}
const TBuiltInResource DefaultTBuiltInResource = InitResources();


class PermuteGLSL {
public:
  SPR_NODISCARD inline static GenerateOutput
  generate(const GenerateInput input) {
    lookupCounter.clear();
    auto output = PermuteText::generate(input);
    if (output.type == OutputType::ERROR)
      return output;
    try {
      postProcess(output.output, glslLookup);
#if defined(DEBUG) && !defined(SPR_NO_DEBUG_OUTPUT)
      for (auto &str : output.output) {
        printf("%s\n", str.c_str());
      }
#endif // DEBUG
      auto stringPtr = output.output;
      std::vector<const char *> cstrings;
      cstrings.resize(stringPtr.size());
      for (size_t i = 0; i < stringPtr.size(); i++) {
        cstrings[i] = stringPtr[i].c_str();
      }
      const GlslSettings settings = input.settings.get<GlslSettings>();
      auto shader = new glslang::TShader(settings.shaderType);
      shader->setStrings(cstrings.data(), cstrings.size());
      shader->setEnvInput(glslang::EShSourceGlsl, settings.shaderType,
                          settings.targetClient, 100);
      shader->setEnvClient(settings.targetClient, settings.targetVersion);
      shader->setEnvTarget(settings.targetLanguage,
                           settings.targetLanguageVersion);
      if (!shader->parse(&DefaultTBuiltInResource, 450, EProfile::ENoProfile, false, false,
                         EShMessages::EShMsgVulkanRules)) {
        return {{shader->getInfoLog()}, OutputType::ERROR};
      }
      const auto interm = shader->getIntermediate();
      const auto node = interm->getTreeRoot();
      for (const auto travPtr : traverser) {
        if (!travPtr->isValid(settings))
          continue;
        impl::ShaderTraverser trav(travPtr);
        node->traverse(&trav);
        travPtr->postProcess();
      }
      std::vector<unsigned int> outputData;
      glslang::GlslangToSpv(*interm, outputData);
      return {std::move(output.output), OutputType::BINARY,
              std::move(outputData), shader};
    } catch (const std::exception &e) {
      return {{"Could not parse glsl settings!", std::string(e.what())},
              OutputType::ERROR};
    }
    return {{"Undefined error!"},
            OutputType::ERROR};
  }
};

#endif

template <class T> class Permute {

private:
  std::vector<ShaderCodes> codes;
  nlohmann::json settings;
  GenerateOutput output;

public:
  SPR_NODISCARD inline bool
  generate(const std::vector<std::string> &dependencies = {}) {
    const GenerateInput input = {codes, dependencies, settings};
    output = T::generate(input);
    return success();
  }

  SPR_NODISCARD inline bool success() const {
    return output.type != OutputType::ERROR;
  }

  SPR_NODISCARD inline std::vector<std::string> getContent() const {
    return output.output;
  }

  SPR_NODISCARD inline std::vector<unsigned int> getBinary() const {
    if (output.type != OutputType::BINARY)
      return {};
    return output.data;
  }

  SPR_NODISCARD inline nlohmann::json getSettings() const { return settings; }

  SPR_NODISCARD inline void *getCostumData() const { return output.costumData; }

  template <class Settings> SPR_NODISCARD inline Settings getSettings() const {
    return settings.get<Settings>();
  }

  inline void toBinaryFile(const std::string &path) const {
    std::ofstream output(path, std::ios_base::binary);
    const auto &data = this->output.data;
    output.write((char *)data.data(), data.size() * sizeof(unsigned int));
  }

  friend void to_json(nlohmann::json &nlohmann_json_j,
                      const Permute &nlohmann_json_t) {
    NLOHMANN_JSON_TO(codes);
    SPR_OPTIONAL_TO_L(settings);
  }

  friend void from_json(const nlohmann::json &nlohmann_json_j,
                        Permute &nlohmann_json_t) {
    NLOHMANN_JSON_FROM(codes);
    SPR_OPTIONAL_FROM(settings);
  }
};

template <class T> inline Permute<T> fromJson(const nlohmann::json &json) {
  return json.get<Permute<T>>();
}

#ifndef SPR_NO_FSTREAM
template <class T> inline Permute<T> fromFile(const std::string &path) {
  std::ifstream inputfile(path);
  if (!inputfile.good())
    throw std::runtime_error("File not found!");
  nlohmann::json json;
  inputfile >> json;
  return fromJson<T>(json);
}
#endif

} // namespace permute
