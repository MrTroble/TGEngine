#pragma once

#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../public/Error.hpp"
#include "../../public/Module.hpp"
#include "APILayer.hpp"
#include "GameShaderModule.hpp"
#include "Material.hpp"
#include "WindowModule.hpp"

namespace tge::graphics {

struct NodeTransform {
  glm::vec3 translation = glm::vec3(0.0f, 0.0f, 0.0f);
  glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f);
  glm::quat rotation = glm::quat(0.0f, 0.0f, 0.0f, 0.0f);
};

struct NodeInfo {
  size_t bindingID = INVALID_SIZE_T;
  NodeTransform transforms = {};
  size_t parent = INVALID_SIZE_T;
};

struct FeatureSet {
  bool wideLines = false;
  int anisotropicfiltering = INT_MAX;
};

enum class LoadType { STBI, DDSPP };

class GameGraphicsModule : public main::Module {
  APILayer *apiLayer;
  WindowModule *windowModule;
  glm::mat4 projectionView;

  glm::mat4 projectionMatrix;
  glm::mat4 viewMatrix;
  size_t nextNode = 0;
  std::vector<glm::mat4> modelMatrices;
  std::vector<NodeTransform> node;
  std::vector<size_t> parents;
  std::vector<size_t> bindingID;
  std::vector<char> status;
  TDataHolder projection;
  TDataHolder modelHolder;
  uint32_t alignment = 1;
  std::vector<BufferChange> bufferChange;

 public:
  TTextureHolder defaultTextureID;
  std::mutex protectTexture;
  std::unordered_map<std::string, TTextureHolder> textureMap;
  PipelineHolder defaultMaterial;
  tge::shader::ShaderPipe defaultPipe;
  FeatureSet features;

  GameGraphicsModule(APILayer *apiLayer, WindowModule *winModule,
                     const FeatureSet &set = {});

  _NODISCARD size_t loadModel(const std::vector<char> &data, const bool binary,
                              const std::string &baseDir,
                              void *shaderPipe = nullptr);

  _NODISCARD size_t loadModel(const std::vector<char> &data,
                              const bool binary) {
    return loadModel(data, binary, "");
  }

  [[nodiscard]] std::vector<TTextureHolder> loadTextures(
      const std::vector<std::vector<char>> &data,
      const LoadType type = LoadType::STBI);

  [[nodiscard]] std::vector<TTextureHolder> loadTextures(
      const std::vector<std::string> &names,
      const LoadType type = LoadType::STBI);

  _NODISCARD size_t addNode(const NodeInfo *nodeInfos, const size_t count);

  _NODISCARD size_t nextNodeID() { return node.size(); }

  void updateTransform(const size_t nodeID, const NodeTransform &transform);

  void updateViewMatrix(const glm::mat4 matrix) {
    this->projectionMatrix = matrix;
  }

  void updateCameraMatrix(const glm::mat4 matrix) { this->viewMatrix = matrix; }

  void updateScale(const size_t nodeID, const glm::vec3 scale) {
    this->node[nodeID].scale = scale;
    this->status[nodeID] = 1;
  }

  main::Error init() override;

  void tick(double time) override;

  void destroy() override;

  _NODISCARD APILayer *getAPILayer() { return apiLayer; }

  _NODISCARD WindowModule *getWindowModule() { return windowModule; }
};

}  // namespace tge::graphics