#pragma once

#include <functional>
#define GLM_ENABLE_EXPERIMENTAL 1
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../public/DataHolder.hpp"
#include "../../public/Error.hpp"
#include "../../public/Module.hpp"
#include "APILayer.hpp"
#include "GameShaderModule.hpp"
#include "Material.hpp"
#include "WindowModule.hpp"

namespace tge::graphics {

DEFINE_HOLDER(Node);

struct NodeTransform {
  glm::vec3 translation = glm::vec3(0.0f, 0.0f, 0.0f);
  glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f);
  glm::quat rotation = glm::quat(0.0f, 0.0f, 0.0f, 0.0f);
};

struct NodeDebugInfo {
  std::string name;
  void* data;
};

struct NodeInfo {
  shader::TBindingHolder bindingID{};
  NodeTransform transforms = {};
  size_t parent = INVALID_SIZE_T;
  TNodeHolder parentHolder;
  std::shared_ptr<NodeDebugInfo> debugInfo;
};

struct FeatureSet {
  uint32_t wideLines = false;
  uint32_t anisotropicfiltering = INT_MAX;
  uint32_t mipMapLevels = 4;
};

struct TextureLoadInternal {
  std::vector<char> textureInfo;
  std::string debugName{};
};

enum class LoadType { STBI, DDSPP };

struct ValueSystem {
  glm::mat4 model = glm::mat4(1.0f);
  glm::mat4 normalModel = glm::mat4(1.0f);
  glm::vec4 color = glm::vec4(0);
  size_t offset = 0;
  char padding[104];
};
static_assert(sizeof(ValueSystem) == 256);

class GameGraphicsModule : public main::Module {
  APILayer* apiLayer;
  WindowModule* windowModule;
  glm::mat4 projectionView;
  glm::mat4 projectionMatrix;
  glm::mat4 viewMatrix;
  size_t nextNode = 0;
  TDataHolder projection;
  std::vector<BufferChange> bufferChange;
  std::vector<std::function<std::vector<char>(const std::string&)>>
      assetResolver;

 public:
  DataHolder<TDataHolder, NodeTransform, size_t, shader::TBindingHolder, char,
             ValueSystem, std::vector<size_t>, std::shared_ptr<NodeDebugInfo>>
      nodeHolder;

  TTextureHolder defaultTextureID;
  std::mutex protectTexture;
  std::unordered_map<std::string, TTextureHolder> textureMap;
  TPipelineHolder defaultMaterial;
  std::vector<char> vertexShader;
  std::vector<char> fragmentShader;
  tge::shader::ShaderPipe defaultPipe;
  tge::shader::ShaderPipe glbWidget;
  FeatureSet features;

  GameGraphicsModule(APILayer* apiLayer, WindowModule* winModule,
                     const FeatureSet& set = {});

  inline std::vector<shader::TBindingHolder> getBinding(
      std::span<const TNodeHolder> holders) {
    return nodeHolder.get<3>(holders);
  }

  void addAssetResolver(
      std::function<std::vector<char>(const std::string&)>&& function) {
    assetResolver.push_back(function);
  }

  [[nodiscard]] std::vector<TNodeHolder> loadModel(
      const std::vector<char>& data, const bool binary,
      const std::string& baseDir = "", void* shaderPipe = nullptr);

  std::vector<TTextureHolder> loadTextures(
      const std::vector<TextureLoadInternal>& data,
      const LoadType type = LoadType::STBI);

  std::vector<TTextureHolder> loadTextures(
      const std::vector<std::string>& names,
      const LoadType type = LoadType::STBI);

  [[nodiscard]] std::vector<TNodeHolder> addNode(
      const NodeInfo* nodeInfos, const size_t count,
      const std::string& debugInfo = "UnknownNode");

  [[nodiscard]] std::vector<TNodeHolder> addNode(
      const std::span<const NodeInfo> nodeInfos,
      const std::string& debugInfo = "UnknownNode") {
    return addNode(nodeInfos.data(), nodeInfos.size(), debugInfo);
  }

  void removeNode(std::span<const TNodeHolder> holder,
                  const bool instand = false) {
    if (nodeHolder.erase(holder)) {
      if (instand ||
          nodeHolder.size() > 2 * nodeHolder.translationTable.size()) {
        const auto compacted = nodeHolder.compact();
        apiLayer->removeData(std::get<0>(compacted), instand);
      }
    }
  }

  void updateTransform(const TNodeHolder nodeID,
                       const NodeTransform& transform) {
    { nodeHolder.change<1>(nodeID) = transform; }
    { nodeHolder.change<4>(nodeID) = 1; }
  }

  void updateViewMatrix(const glm::mat4 matrix) {
    this->projectionMatrix = matrix;
  }

  void updateCameraMatrix(const glm::mat4 matrix) { this->viewMatrix = matrix; }

  main::Error init() override;

  void tick(double time) override;

  void destroy() override;

  [[nodiscard]] APILayer* getAPILayer() { return apiLayer; }

  [[nodiscard]] WindowModule* getWindowModule() { return windowModule; }
};

}  // namespace tge::graphics