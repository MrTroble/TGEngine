#pragma once

#include <iostream>
#include <string>
#include <vector>

#include "../Error.hpp"
#include "Material.hpp"

namespace tge::shader {

enum class IOType { FLOAT, VEC2, VEC3, VEC4, MAT3, MAT4, SAMPLER2 };

struct ShaderIO {
  std::string name;
  IOType iotype;
  size_t binding;
};

enum class SamplerIOType { SAMPLER, TEXTURE };

struct SamplerIO {
  std::string name;
  SamplerIOType iotype;
  size_t binding;
  size_t size = 1;
};

struct ShaderBindingIO {
  std::string name;
  IOType iotype;
  size_t binding;
  size_t buffer = 0;
};

enum class InstructionType {
  NOOP,
  MULTIPLY,
  ADD,
  TEXTURE,
  SAMPLER,
  SET,
  TEMP,
  VEC4CTR,
  DOT,
  NORMALIZE,
  CROSS,
  MIN,
  MAX,
  CLAMP,
  SUBTRACT,
  DIVIDE
};

struct Instruction {
  std::vector<std::string> inputs;
  IOType outputType;
  InstructionType instruciontType;
  std::string name;
};

enum class ShaderType { VERTEX, FRAGMENT, INVALID };

struct ShaderCreateInfo {
  std::string __code;
  std::vector<ShaderIO> unifromIO;
  std::vector<ShaderBindingIO> inputs;
  std::vector<ShaderIO> outputs;
  std::vector<SamplerIO> samplerIO;
  ShaderType shaderType;
  std::vector<Instruction> instructions;
};

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
  size_t dataID = INVALID_SIZE_T;
  size_t size = INVALID_SIZE_T;
  size_t offset = 0;
};

struct TextureBindingData {
  size_t texture = INVALID_SIZE_T;
  graphics::TSamplerHolder sampler;
};

struct BindingInfo {
  size_t binding = INVALID_SIZE_T;
  size_t bindingSet = INVALID_SIZE_T;
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
};

#ifdef DEBUG
#define DEBUG_EXPECT(input) debugExpect(input)
inline void debugExpect(const bool assertion, const std::string& string) {
  if (!assertion) {
    std::cout << "[DEBUG]: " << string << std::endl;
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
      info.type != BindingType::Sampler || (bool)info.data.texture.sampler,
      "Sampler not correct with sampler binding type!");
  debugExpect(
      (info.type != BindingType::Texture &&
       info.type != BindingType::InputAttachment) ||
          info.data.texture.texture != INVALID_SIZE_T,
      "Texture id not correct with texture or input attachment binding type!");
  debugExpect((info.type != BindingType::UniformBuffer &&
               info.type != BindingType::Storage) ||
                  info.data.buffer.dataID != INVALID_SIZE_T,
              "Data id not correct with buffer binding type!");
  debugExpect((info.type != BindingType::UniformBuffer &&
               info.type != BindingType::Storage) ||
                  info.data.buffer.size != 0,
              "Size is 0 not correct with buffer binding type!");
}
#else
#define DEBUG_EXPECT(input)
#endif

class ShaderAPI {
 public:
  virtual ~ShaderAPI() {}

  _NODISCARD virtual ShaderPipe loadShaderPipeAndCompile(
      const std::vector<std::string>& shadernames) = 0;

  _NODISCARD virtual ShaderPipe compile(
      const std::vector<ShaderInfo>& shadernames) = 0;

  _NODISCARD virtual ShaderPipe createShaderPipe(
      const ShaderCreateInfo* shaderCreateInfo, const size_t shaderCount) = 0;

  _NODISCARD virtual size_t createBindings(ShaderPipe pipe,
                                           const size_t count = 1) = 0;

  virtual void changeInputBindings(const ShaderPipe pipe,
                                   const size_t bindingID,
                                   const size_t buffer) = 0;

  virtual void bindData(const BindingInfo* info, const size_t count) = 0;

  virtual void addToRender(const size_t* bindingID, const size_t size,
                           void* customData) = 0;

  virtual void addToMaterial(const graphics::Material* material,
                             void* customData) = 0;

  virtual void init() = 0;

  virtual void destroy() = 0;
};

}  // namespace tge::shader
