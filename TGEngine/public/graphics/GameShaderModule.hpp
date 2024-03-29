#pragma once

#include <iostream>
#include <span>
#include <string>
#include <vector>

#include "../Error.hpp"
#include "../Util.hpp"
#include "Material.hpp"

namespace tge::shader {

enum class SamplerIOType { SAMPLER, TEXTURE };

enum class ShaderType { VERTEX, FRAGMENT, INVALID };

using ShaderPipe = void*;

enum BindingType {
  UniformBuffer,
  Texture,
  Sampler,
  InputAttachment,
  Storage,
  Invalid
};

struct BufferBindingData {
  graphics::TDataHolder dataID;
  size_t size = INVALID_SIZE_T;
  size_t offset = 0;
};

struct TextureBindingData {
  graphics::TTextureHolder texture;
  graphics::TSamplerHolder sampler;
};

struct BindingInfo {
  size_t binding = INVALID_SIZE_T;
  TBindingHolder bindingSet{};
  BindingType type = BindingType::Invalid;
  union BindingData {
    TextureBindingData texture;
    BufferBindingData buffer;
    BindingData() {}
  } data;
  size_t arrayID = 0;

  BindingInfo() {}
};

struct ShaderInfo {
  ShaderType language = ShaderType::INVALID;
  std::vector<char> code;
  std::vector<std::string> additionalCode;
  std::string debug;
};

#ifdef DEBUG
#define DEBUG_EXPECT(input) debugExpect(input)
inline void debugExpect(const bool assertion, const std::string& string) {
  if (!assertion) {
    PLOG_DEBUG << string;
    throw std::runtime_error(string);
  }
}

inline void debugExpect(const ShaderInfo& info) {
  debugExpect(info.language == ShaderType::INVALID, "Shader type invalid!");
  debugExpect(!info.code.empty(), "Shader code is empty (no code provided)!");
}

inline void debugExpect(const BindingInfo& info) {
  debugExpect(info.type != BindingType::Invalid, "BindingType not set!");
  debugExpect(info.bindingSet != INVALID_SIZE_T, "BindingSet not set!");
  debugExpect(info.binding != INVALID_SIZE_T, "Binding not set!");
  debugExpect(
      info.type != BindingType::Sampler || !(!info.data.texture.sampler),
      "Sampler not correct with sampler binding type!");
  debugExpect(
      (info.type != BindingType::Texture &&
       info.type != BindingType::InputAttachment) ||
          !(!info.data.texture.texture),
      "Texture id not correct with texture or input attachment binding type!");
  debugExpect((info.type != BindingType::UniformBuffer &&
               info.type != BindingType::Storage) ||
                  !(!info.data.buffer.dataID),
              "Data id not correct with buffer binding type!");
  debugExpect((info.type != BindingType::UniformBuffer &&
               info.type != BindingType::Storage) ||
                  info.data.buffer.size != 0,
              "Size is 0 not correct with buffer binding type!");
}
#else
#define DEBUG_EXPECT(input)
#endif

struct ShaderCreateInfo {
  std::function<size_t(size_t)> inputLayoutTranslation = [](auto) { return 0; };
};

class ShaderAPI {
 public:
  virtual ~ShaderAPI() {}

  [[nodiscard]] virtual ShaderPipe loadShaderPipeAndCompile(
      const std::vector<std::string>& shadernames,
      const ShaderCreateInfo& createInfo = {}) = 0;

  [[nodiscard]] virtual ShaderPipe compile(
      const std::vector<ShaderInfo>& shadernames,
      const ShaderCreateInfo& createInfo = {}) = 0;

  [[nodiscard]] virtual std::vector<TBindingHolder> createBindings(
      ShaderPipe pipe, const size_t count = 1) = 0;

  virtual void changeInputBindings(const ShaderPipe pipe,
                                   const size_t bindingID,
                                   const size_t buffer) = 0;

  virtual void bindData(const BindingInfo* info, const size_t count) = 0;

  virtual void bindData(const std::span<const BindingInfo> infos) {
    bindData(infos.data(), infos.size());
  }

  virtual void addToRender(const std::span<const TBindingHolder> bindings,
                           void* customData) = 0;

  virtual void addToMaterial(const graphics::Material* material,
                             void* customData) = 0;

  virtual void init() = 0;

  virtual void destroy() = 0;
};

}  // namespace tge::shader
