#pragma once

#include <stdint.h>

#include <functional>
#include <glm/glm.hpp>
#include <mutex>
#include <vector>

#include "../Module.hpp"
#include "ElementHolder.hpp"
#include "Material.hpp"

namespace tge::shader {
class ShaderAPI;
enum class ShaderType;
}  // namespace tge::shader

namespace tge::graphics {

class GameGraphicsModule;

enum class IndexSize { UINT16, UINT32, NONE };

struct CacheIndex {
  size_t buffer = INVALID_SIZE_T;
};

struct PushConstRanges {
  std::vector<char> pushConstData;
  shader::ShaderType type;
};

struct RenderInfo {
  std::vector<size_t> vertexBuffer;
  size_t indexBuffer;
  PipelineHolder materialId;
  size_t indexCount;
  size_t instanceCount = 1;
  size_t indexOffset = 0;
  IndexSize indexSize = IndexSize::UINT32;
  std::vector<size_t> vertexOffsets;
  size_t bindingID = INVALID_SIZE_T;
  size_t firstInstance = 0;
  std::vector<PushConstRanges> constRanges;
};

struct TextureInfo {
  uint8_t* data = nullptr;
  uint32_t size;
  uint32_t width;
  uint32_t height;
  uint32_t channel;
  size_t internalFormatOverride = 37;
};

enum class FilterSetting { NEAREST, LINEAR };

enum class AddressMode {
  REPEAT,
  MIRROR_REPEAT,
  CLAMP_TO_EDGE,
  CLAMP_TO_BORDER,
  MIRROR_CLAMP_TO_EDGE
};

struct SamplerInfo {
  FilterSetting minFilter;
  FilterSetting magFilter;
  AddressMode uMode;
  AddressMode vMode;
  int anisotropy = 0;
};

struct Light {
  glm::vec3 pos;
  float __alignment = 0;
  glm::vec3 color;
  float intensity;

  Light() = default;

  Light(glm::vec3 pos, glm::vec3 color, float intensity)
      : pos(pos), color(color), intensity(intensity) {}
};

enum class DataType { IndexData, VertexData, VertexIndexData, Uniform, All };

class APILayer : public main::Module {  // Interface
 protected:
  GameGraphicsModule* graphicsModule = nullptr;
  shader::ShaderAPI* shaderAPI;
  std::vector<size_t> referenceCounter;
  std::mutex referenceCounterMutex;

 public:
  [[nodiscard]] size_t nextCounter() {
    std::lock_guard lg(referenceCounterMutex);
    const auto currentCount = referenceCounter.size();
    referenceCounter.push_back(1);
    return currentCount;
  }

  void setGameGraphicsModule(GameGraphicsModule* graphicsModule) {
    this->graphicsModule = graphicsModule;
  }

  APILayer(shader::ShaderAPI* shaderAPI) : shaderAPI(shaderAPI) {}

  virtual ~APILayer() {}

  [[nodiscard]] virtual void* loadShader(
      const MaterialType type) = 0;  // Legacy support

  [[nodiscard]] virtual std::vector<PipelineHolder> pushMaterials(
      const size_t materialcount, const Material* materials,
      const size_t offset = INVALID_SIZE_T) = 0;

  [[nodiscard]] virtual size_t pushData(const size_t dataCount, void* data,
                                        const size_t* dataSizes,
                                        const DataType type) = 0;

  virtual void changeData(const size_t bufferIndex, const void* data,
                          const size_t dataSizes, const size_t offset = 0) = 0;

  void changeData(const size_t bufferIndex, void* data, const size_t dataSizes,
                  const size_t offset = 0) {
    changeData(bufferIndex, (const void*)data, dataSizes, offset);
  }

  virtual void removeRender(const size_t renderInfoCount,
                            const TRenderHolder* renderIDs) = 0;

  [[nodiscard]] virtual TRenderHolder pushRender(const size_t renderInfoCount,
                                                 const RenderInfo* renderInfos,
                                                 const size_t offset = 0) = 0;

  [[nodiscard]] virtual TSamplerHolder pushSampler(const SamplerInfo& sampler) = 0;

  [[nodiscard]] virtual size_t pushTexture(const size_t textureCount,
                                           const TextureInfo* textures) = 0;

  [[nodiscard]] virtual size_t pushLights(const size_t lightCount,
                                          const Light* lights,
                                          const size_t offset = 0) = 0;

  [[nodiscard]] virtual size_t getAligned(const size_t buffer,
                                          const size_t toBeAligned) const = 0;

  [[nodiscard]] virtual size_t getAligned(const DataType type) const = 0;

  [[nodiscard]] GameGraphicsModule* getGraphicsModule() const {
    return graphicsModule;
  };

  [[nodiscard]] shader::ShaderAPI* getShaderAPI() const {
    return this->shaderAPI;
  }

  [[nodiscard]] virtual glm::vec2 getRenderExtent() const = 0;

  [[nodiscard]] virtual std::vector<char> getImageData(
      const size_t imageId, CacheIndex* = nullptr) = 0;

  [[nodiscard]] virtual APILayer* backend() { return this; }
};

}  // namespace tge::graphics
