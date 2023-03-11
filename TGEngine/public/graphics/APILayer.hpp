#pragma once

#include <stdint.h>

#include <glm/glm.hpp>
#include <vector>
#include <mutex>

#include "../Module.hpp"
#include "Material.hpp"

namespace tge::shader {
class ShaderAPI;
enum class ShaderType;
}  // namespace tge::shader

namespace tge::graphics {

class GameGraphicsModule;
class APILayer;

struct EntryHolder {
  size_t internalHandle;
  size_t referenceID;

  EntryHolder() : internalHandle(SIZE_MAX), referenceID(SIZE_MAX) {}

  EntryHolder(APILayer* api, const size_t internalHandle);
};

struct PipelineHolder : public EntryHolder {
    using EntryHolder::EntryHolder;
};

enum class IndexSize { UINT16, UINT32, NONE };

struct CacheIndex {
  size_t buffer = SIZE_MAX;
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
  size_t bindingID = UINT64_MAX;
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
  size_t nextCounter() {
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

  _NODISCARD virtual void* loadShader(
      const MaterialType type) = 0;  // Legacy support

  _NODISCARD virtual std::vector<PipelineHolder> pushMaterials(
      const size_t materialcount, const Material* materials,
      const size_t offset = SIZE_MAX) = 0;

  _NODISCARD virtual size_t pushData(const size_t dataCount, void* data,
                                     const size_t* dataSizes,
                                     const DataType type) = 0;

  virtual void changeData(const size_t bufferIndex, const void* data,
                          const size_t dataSizes, const size_t offset = 0) = 0;

  void changeData(const size_t bufferIndex, void* data, const size_t dataSizes,
                  const size_t offset = 0) {
    changeData(bufferIndex, (const void*)data, dataSizes, offset);
  }

  virtual size_t removeRender(const size_t renderInfoCount,
                              const size_t* renderIDs) = 0;

  virtual size_t pushRender(const size_t renderInfoCount,
                            const RenderInfo* renderInfos,
                            const size_t offset = 0) = 0;

  _NODISCARD virtual size_t pushSampler(const SamplerInfo& sampler) = 0;

  _NODISCARD virtual size_t pushTexture(const size_t textureCount,
                                        const TextureInfo* textures) = 0;

  _NODISCARD virtual size_t pushLights(const size_t lightCount,
                                       const Light* lights,
                                       const size_t offset = 0) = 0;

  _NODISCARD virtual size_t getAligned(const size_t buffer,
                                       const size_t toBeAligned) const = 0;

  _NODISCARD virtual size_t getAligned(const DataType type) const = 0;

  _NODISCARD GameGraphicsModule* getGraphicsModule() const {
    return graphicsModule;
  };

  _NODISCARD shader::ShaderAPI* getShaderAPI() const { return this->shaderAPI; }

  _NODISCARD virtual glm::vec2 getRenderExtent() const = 0;

  _NODISCARD virtual std::vector<char> getImageData(const size_t imageId,
                                                    CacheIndex* = nullptr) = 0;

  virtual APILayer* backend() { return this; }
};

}  // namespace tge::graphics
