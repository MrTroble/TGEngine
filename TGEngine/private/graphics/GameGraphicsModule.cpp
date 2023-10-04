#include "../../public/graphics/GameGraphicsModule.hpp"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <array>
#include <glm/gtx/transform.hpp>
#include <iostream>

#include "../../public/Util.hpp"
#include "../../public/graphics/GameShaderModule.hpp"
#include "../../public/graphics/vulkan/VulkanShaderPipe.hpp"
#include "../../public/headerlibs/ddspp.h"
#include "../../public/headerlibs/tiny_gltf.h"
#include "BGAL.h"

namespace tge::graphics {

using namespace tinygltf;

inline AddressMode gltfToAPI(int in, AddressMode def) {
  switch (in) {
    case TINYGLTF_TEXTURE_WRAP_REPEAT:
      return AddressMode::REPEAT;
    case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
      return AddressMode::CLAMP_TO_EDGE;
    case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
      return AddressMode::MIRROR_REPEAT;
  }
  return def;
}

inline FilterSetting gltfToAPI(int in, FilterSetting def) {
  switch (in) {
    case TINYGLTF_TEXTURE_FILTER_LINEAR:
      return FilterSetting::LINEAR;
    case TINYGLTF_TEXTURE_FILTER_NEAREST:
      return FilterSetting::NEAREST;
    default:
      return def;
  }
}

inline vk::Format getFormatFromStride(uint32_t stride) {
  switch (stride) {
    case 4:
      return vk::Format::eR32Sfloat;
    case 8:
      return vk::Format::eR32G32Sfloat;
    case 12:
      return vk::Format::eR32G32B32Sfloat;
    case 16:
      return vk::Format::eR32G32B32A32Sfloat;
    default:
      throw std::runtime_error("Couldn't find format");
  }
}

inline std::vector<TSamplerHolder> loadSampler(const Model &model,
                                               APILayer *apiLayer) {
  std::vector<TSamplerHolder> samplerHolder;
  for (const auto &smplr : model.samplers) {
    const SamplerInfo samplerInfo = {
        gltfToAPI(smplr.minFilter, FilterSetting::LINEAR),
        gltfToAPI(smplr.minFilter, FilterSetting::LINEAR),
        gltfToAPI(smplr.wrapS, AddressMode::REPEAT),
        gltfToAPI(smplr.wrapT, AddressMode::REPEAT)};
    samplerHolder.push_back(apiLayer->pushSampler(samplerInfo));
  }

  if (!model.images.empty()) {
    if (model.samplers.empty()) {  // default sampler
      const SamplerInfo samplerInfo = {
          FilterSetting::LINEAR, FilterSetting::LINEAR, AddressMode::REPEAT,
          AddressMode::REPEAT};
      samplerHolder.push_back(apiLayer->pushSampler(samplerInfo));
    }
  }
  return samplerHolder;
}

inline std::vector<TTextureHolder> loadTexturesFM(const Model &model,
                                                  APILayer *apiLayer) {
  std::vector<TextureInfo> textureInfos;
  textureInfos.reserve(model.images.size());
  for (const auto &img : model.images) {
    if (!img.image.empty()) [[likely]] {
      const TextureInfo info{(uint8_t *)img.image.data(),
                             (uint32_t)img.image.size(), (uint32_t)img.width,
                             (uint32_t)img.height, (uint32_t)img.component};
      textureInfos.push_back(info);
    } else {
      throw std::runtime_error("Not implemented!");
    }
  }
  if (!textureInfos.empty())
    return apiLayer->pushTexture(textureInfos.size(), textureInfos.data());
  return {};
}

inline std::vector<TDataHolder> loadDataBuffers(const Model &model,
                                                APILayer *apiLayer) {
  std::vector<BufferInfo> infoBuffer;
  infoBuffer.reserve(model.buffers.size());
  for (const auto &buffer : model.buffers) {
    infoBuffer.push_back({(uint8_t *)buffer.data.data(), buffer.data.size(),
                          DataType::VertexIndexData});
  }
  return apiLayer->pushData(infoBuffer.size(), infoBuffer.data());
}

inline void pushRender(const Model &model, APILayer *apiLayer,
                       const std::vector<TDataHolder> &dataId,
                       const std::vector<TPipelineHolder> &materialId,
                       const std::vector<TNodeHolder> nodeID,
                       GameGraphicsModule *ggm) {
  std::vector<RenderInfo> renderInfos;
  renderInfos.reserve(1000);
  const auto &bindings = ggm->getBinding(nodeID);
  for (size_t i = 0; i < model.meshes.size(); i++) {
    const auto &mesh = model.meshes[i];
    const auto bID = bindings[i];
    for (const auto &prim : mesh.primitives) {
      std::vector<std::tuple<int, TDataHolder, int>> strides;
      strides.reserve(prim.attributes.size());

      for (const auto &attr : prim.attributes) {
        const auto &vertAccesor = model.accessors[attr.second];
        const auto &vertView = model.bufferViews[vertAccesor.bufferView];
        const auto bufferID = dataId[vertView.buffer];
        const auto vertOffset = vertView.byteOffset + vertAccesor.byteOffset;
        strides.push_back(
            std::make_tuple(vertAccesor.type, bufferID, vertOffset));
      }

      std::sort(strides.rbegin(), strides.rend(),
                [](auto x, auto y) { return std::get<0>(x) < std::get<0>(y); });
      std::vector<TDataHolder> bufferIndicies;
      bufferIndicies.reserve(strides.size());
      std::vector<size_t> bufferOffsets;
      bufferOffsets.reserve(bufferIndicies.capacity());
      for (auto &stride : strides) {
        bufferIndicies.push_back(std::get<1>(stride));
        bufferOffsets.push_back(std::get<2>(stride));
      }

      if (prim.indices >= 0) [[likely]] {
        const auto &indexAccesor = model.accessors[prim.indices];
        const auto &indexView = model.bufferViews[indexAccesor.bufferView];
        const auto indexOffset = indexView.byteOffset + indexAccesor.byteOffset;
        const IndexSize indextype =
            indexView.byteStride == 4 ? IndexSize::UINT32 : IndexSize::UINT16;
        const RenderInfo renderInfo = {
            bufferIndicies,
            dataId[indexView.buffer],
            materialId[prim.material == -1 ? 0 : prim.material],
            indexAccesor.count,
            1,
            indexOffset,
            indextype,
            bufferOffsets,
            bID};
        renderInfos.push_back(renderInfo);
      } else {
        const auto accessorID = prim.attributes.begin()->second;
        const auto &vertAccesor = model.accessors[accessorID];
        const RenderInfo renderInfo = {
            bufferIndicies,
            dataId[0],
            materialId[prim.material == -1 ? 0 : prim.material],
            0,
            1,
            vertAccesor.count,
            IndexSize::NONE,
            bufferOffsets,
            bID};
        renderInfos.push_back(renderInfo);
      }
    }
  }

  apiLayer->pushRender(renderInfos.size(), renderInfos.data());
}

inline std::vector<TNodeHolder> loadNodes(
    const Model &model, APILayer *apiLayer, GameGraphicsModule *ggm,
    const std::vector<shader::ShaderPipe> &created) {
  std::vector<NodeInfo> nodeInfos = {};
  const auto amount = model.nodes.size();
  nodeInfos.resize(amount + 1);
  if (amount != 0) [[likely]] {
    for (size_t i = 0; i < amount; i++) {
      const auto &node = model.nodes[i];
      const auto infoID = i + 1;
      auto &info = nodeInfos[infoID];
      if (!node.translation.empty()) {
        info.transforms.translation.x = (float)node.translation[0];
        info.transforms.translation.y = (float)node.translation[1];
        info.transforms.translation.z = (float)node.translation[2];
      }
      if (!node.scale.empty()) {
        info.transforms.scale.x = (float)node.scale[0];
        info.transforms.scale.y = (float)node.scale[1];
        info.transforms.scale.z = (float)node.scale[2];
      }
      if (!node.rotation.empty()) {
        info.transforms.rotation =
            glm::quat((float)node.rotation[3], (float)node.rotation[0],
                      (float)node.rotation[1], (float)node.rotation[2]);
      }
      for (const auto id : node.children) {
        nodeInfos[id + 1].parent = infoID;
      }
      if (node.mesh >= 0 && created.size() > node.mesh) [[likely]] {
        info.bindingID =
            apiLayer->getShaderAPI()->createBindings(created[node.mesh]);
      } else {
        info.bindingID =
            apiLayer->getShaderAPI()->createBindings(ggm->defaultPipe);
      }
    }
    for (auto &nInfo : nodeInfos) {
      if (nInfo.parent == INVALID_SIZE_T) {
        nInfo.parent = 0;
      }
    }
  } else {
    const auto startID =
        apiLayer->getShaderAPI()->createBindings(ggm->defaultPipe);
    nodeInfos[0].bindingID = startID;
  }
  return ggm->addNode(nodeInfos.data(), nodeInfos.size());
}

GameGraphicsModule::GameGraphicsModule(APILayer *apiLayer,
                                       WindowModule *winModule,
                                       const FeatureSet &features) {
  this->features = features;
  const auto prop = winModule->getWindowProperties();
  this->apiLayer = apiLayer;
  this->windowModule = winModule;
  // TODO Cleanup
  this->projectionMatrix =
      glm::perspective(glm::radians(45.0f),
                       (float)prop.width / (float)prop.height, 0.1f, 100.0f);
  this->projectionMatrix[1][1] *= -1;
  this->viewMatrix = glm::lookAt(glm::vec3(0, 0.5f, 1), glm::vec3(0, 0, 0),
                                 glm::vec3(0, 1, 0));
}

std::vector<TNodeHolder> GameGraphicsModule::loadModel(
    const std::vector<char> &data, const bool binary,
    const std::string &baseDir, void *shaderPipe) {
  TinyGLTF loader;
  std::string error;
  std::string warning;
  Model model;

  const bool rst =
      binary ? loader.LoadBinaryFromMemory(&model, &error, &warning,
                                           (const uint8_t *)data.data(),
                                           data.size(), baseDir)
             : loader.LoadASCIIFromString(&model, &error, &warning, data.data(),
                                          data.size(), baseDir);
  if (!rst) {
    PLOG_ERROR << "Loading failed\n" << error << std::endl << warning;
    return {};
  }

  if (!warning.empty()) {
    PLOG_WARNING << warning;
  }

  const auto samplerId = loadSampler(model, apiLayer);

  const auto textureId = loadTexturesFM(model, apiLayer);

  const auto dataId = loadDataBuffers(model, apiLayer);

  std::vector<shader::ShaderPipe> createdShader;
  std::vector<TPipelineHolder> materials(
      model.materials.size());  // TODO fix this
  std::fill(begin(materials), end(materials), defaultMaterial);

  const auto nId = loadNodes(model, apiLayer, this, createdShader);

  pushRender(model, apiLayer, dataId, materials, nId, this);

  return nId;
}

inline glm::mat4 calculateMatrixSingle(const NodeTransform &transform) {
  const auto rotationMatrix = glm::toMat4(transform.rotation);
  return glm::translate(transform.translation) * glm::scale(transform.scale) *
         rotationMatrix;
}

inline glm::mat4 calculateMatrixNormals(const NodeTransform &transform) {
  return glm::toMat4(transform.rotation);
}

constexpr uint8_t AMOUNT_OF_DATA = 2;

inline glm::mat4 adjoint(const glm::mat4 &matrix) {
  glm::mat4 output;
  for (size_t x = 0; x < 4; x++) {
    for (size_t y = 0; y < 4; y++) {
      glm::mat3 mat3;
      size_t nextX = 0;
      for (size_t x2 = 0; x2 < 4; x2++) {
        if (x == x2) continue;
        size_t nextY = 0;
        for (size_t y2 = 0; y2 < 4; y2++) {
          if (y == y2) continue;
          mat3[nextX][nextY++] = matrix[x2][y2];
        }
        nextX++;
      }
      output[x][y] = std::pow(-1, x + y) * glm::determinant(mat3);
    }
  }
  return glm::transpose(output);
}

main::Error GameGraphicsModule::init() {
  assetResolver.push_back(&util::wholeFile);
  glm::mat4 projView = this->projectionMatrix * this->viewMatrix;
  std::vector<BufferInfo> bufferInfos = {
      BufferInfo{&projView, sizeof(glm::mat4), DataType::Uniform}};

  const auto &bufferList =
      apiLayer->pushData(bufferInfos.size(), bufferInfos.data());
  projection = bufferList[0];
  defaultPipe = apiLayer->getShaderAPI()->loadShaderPipeAndCompile(
      {"assets/testvec4.vert", "assets/test.frag"});
  const Material defMat(defaultPipe);
  defaultMaterial = apiLayer->pushMaterials(1, &defMat)[0];

  TextureInfo info;
  info.width = BGAL::width;
  info.height = BGAL::height;
  info.channel = 4;
  info.size = info.width * info.height * info.channel;
  auto data = BGAL::header_data;
  info.data = new uint8_t[info.size];
  for (size_t i = 0; i < info.width * info.height; i++) {
    std::array<uint8_t, 3> color;
    HEADER_PIXEL(data, color);
    info.data[i * 4 + 0] = color[0];
    info.data[i * 4 + 1] = color[1];
    info.data[i * 4 + 2] = color[2];
    info.data[i * 4 + 3] = 255;
  }
  defaultTextureID = apiLayer->pushTexture(1, &info)[0];
  bufferChange.reserve(128);
  bufferChange.push_back({projection, &projectionView, sizeof(glm::mat4), 0});
  return main::Error::NONE;
}

void GameGraphicsModule::tick(double time) {
  const auto size = nodeHolder.size();
  std::lock_guard guard(nodeHolder.mutex);
  projectionView = this->projectionMatrix * this->viewMatrix;
  bool status = false;
  bufferChange.resize(1);
  const auto &parents = std::get<2>(nodeHolder.internalValues);
  const auto &dataHolder = std::get<0>(nodeHolder.internalValues);
  const auto &transforms = std::get<1>(nodeHolder.internalValues);
  auto &statuses = std::get<4>(nodeHolder.internalValues);
  auto &modelMatrices = std::get<5>(nodeHolder.internalValues);
  for (size_t i = 0; i < size; i++) {
    const auto parentID = nodeHolder.translationTable[parents[i]];
    if (statuses[i] == 1 || (parentID < size && statuses[parentID])) {
      status = true;
      if (parentID < i) {
        modelMatrices[i] =
            modelMatrices[parentID] * calculateMatrixSingle(transforms[i]);
      } else {
        modelMatrices[i] = calculateMatrixSingle(transforms[i]);
      }
      bufferChange.emplace_back(dataHolder[i], &modelMatrices[i],
                                sizeof(glm::mat4));
    }
  }
  apiLayer->changeData(bufferChange.size(), bufferChange.data());
  if (status) std::fill(begin(statuses), end(statuses), 0);
}

void GameGraphicsModule::destroy() {}

std::vector<TextureInfo> loadSTBI(const std::vector<std::vector<char>> &data) {
  std::vector<TextureInfo> textureInfos;
  textureInfos.reserve(data.size());
  for (const auto &dataIn : data) {
    if (dataIn.empty()) {
      PLOG_FATAL << "Found empty texture!";
      exit(-1);
      continue;
    }
    TextureInfo info;
    info.data = stbi_load_from_memory((stbi_uc *)dataIn.data(), dataIn.size(),
                                      (int *)&info.width, (int *)&info.height,
                                      (int *)&info.channel, 0);
    info.size = info.width * info.height * info.channel;
    if (info.channel == 3) {
      PLOG_FATAL << "Texture with 3 channels not supported!";
      exit(-1);
      continue;
    }
    textureInfos.push_back(info);
  }
  return textureInfos;
}

inline size_t fromDXGI(ddspp::DXGIFormat format) {
  switch (format) {
    case ddspp::UNKNOWN:
      return (size_t)vk::Format::eUndefined;
    case ddspp::R32G32B32A32_TYPELESS:
      break;
    case ddspp::R32G32B32A32_FLOAT:
      break;
    case ddspp::R32G32B32A32_UINT:
      break;
    case ddspp::R32G32B32A32_SINT:
      break;
    case ddspp::R32G32B32_TYPELESS:
      break;
    case ddspp::R32G32B32_FLOAT:
      break;
    case ddspp::R32G32B32_UINT:
      break;
    case ddspp::R32G32B32_SINT:
      break;
    case ddspp::R16G16B16A16_TYPELESS:
      break;
    case ddspp::R16G16B16A16_FLOAT:
      break;
    case ddspp::R16G16B16A16_UNORM:
      break;
    case ddspp::R16G16B16A16_UINT:
      break;
    case ddspp::R16G16B16A16_SNORM:
      break;
    case ddspp::R16G16B16A16_SINT:
      break;
    case ddspp::R32G32_TYPELESS:
      break;
    case ddspp::R32G32_FLOAT:
      break;
    case ddspp::R32G32_UINT:
      break;
    case ddspp::R32G32_SINT:
      break;
    case ddspp::R32G8X24_TYPELESS:
      break;
    case ddspp::D32_FLOAT_S8X24_UINT:
      break;
    case ddspp::R32_FLOAT_X8X24_TYPELESS:
      break;
    case ddspp::X32_TYPELESS_G8X24_UINT:
      break;
    case ddspp::R10G10B10A2_TYPELESS:
      break;
    case ddspp::R10G10B10A2_UNORM:
      break;
    case ddspp::R10G10B10A2_UINT:
      break;
    case ddspp::R11G11B10_FLOAT:
      break;
    case ddspp::R8G8B8A8_TYPELESS:
      break;
    case ddspp::R8G8B8A8_UNORM:
      break;
    case ddspp::R8G8B8A8_UNORM_SRGB:
      break;
    case ddspp::R8G8B8A8_UINT:
      break;
    case ddspp::R8G8B8A8_SNORM:
      break;
    case ddspp::R8G8B8A8_SINT:
      break;
    case ddspp::R16G16_TYPELESS:
      break;
    case ddspp::R16G16_FLOAT:
      break;
    case ddspp::R16G16_UNORM:
      break;
    case ddspp::R16G16_UINT:
      break;
    case ddspp::R16G16_SNORM:
      break;
    case ddspp::R16G16_SINT:
      break;
    case ddspp::R32_TYPELESS:
      break;
    case ddspp::D32_FLOAT:
      break;
    case ddspp::R32_FLOAT:
      break;
    case ddspp::R32_UINT:
      break;
    case ddspp::R32_SINT:
      break;
    case ddspp::R24G8_TYPELESS:
      break;
    case ddspp::D24_UNORM_S8_UINT:
      break;
    case ddspp::R24_UNORM_X8_TYPELESS:
      break;
    case ddspp::X24_TYPELESS_G8_UINT:
      break;
    case ddspp::R8G8_TYPELESS:
      break;
    case ddspp::R8G8_UNORM:
      break;
    case ddspp::R8G8_UINT:
      break;
    case ddspp::R8G8_SNORM:
      break;
    case ddspp::R8G8_SINT:
      break;
    case ddspp::R16_TYPELESS:
      break;
    case ddspp::R16_FLOAT:
      break;
    case ddspp::D16_UNORM:
      break;
    case ddspp::R16_UNORM:
      break;
    case ddspp::R16_UINT:
      break;
    case ddspp::R16_SNORM:
      break;
    case ddspp::R16_SINT:
      break;
    case ddspp::R8_TYPELESS:
      break;
    case ddspp::R8_UNORM:
      break;
    case ddspp::R8_UINT:
      break;
    case ddspp::R8_SNORM:
      break;
    case ddspp::R8_SINT:
      break;
    case ddspp::A8_UNORM:
      break;
    case ddspp::R1_UNORM:
      break;
    case ddspp::R9G9B9E5_SHAREDEXP:
      break;
    case ddspp::R8G8_B8G8_UNORM:
      break;
    case ddspp::G8R8_G8B8_UNORM:
      break;
    case ddspp::BC1_TYPELESS:
    case ddspp::BC1_UNORM:
      return (size_t)vk::Format::eBc1RgbaUnormBlock;
    case ddspp::BC1_UNORM_SRGB:
      return (size_t)vk::Format::eBc1RgbUnormBlock;
    case ddspp::BC2_TYPELESS:
      break;
    case ddspp::BC2_UNORM:
      break;
    case ddspp::BC2_UNORM_SRGB:
      break;
    case ddspp::BC3_TYPELESS:
    case ddspp::BC3_UNORM:
      return (size_t)vk::Format::eBc3UnormBlock;
    case ddspp::BC3_UNORM_SRGB:
      return (size_t)vk::Format::eBc3SrgbBlock;
    case ddspp::BC4_TYPELESS:
      break;
    case ddspp::BC4_UNORM:
      break;
    case ddspp::BC4_SNORM:
      break;
    case ddspp::BC5_TYPELESS:
      break;
    case ddspp::BC5_UNORM:
      break;
    case ddspp::BC5_SNORM:
      break;
    case ddspp::B5G6R5_UNORM:
      break;
    case ddspp::B5G5R5A1_UNORM:
      break;
    case ddspp::B8G8R8A8_UNORM:
      break;
    case ddspp::B8G8R8X8_UNORM:
      break;
    case ddspp::R10G10B10_XR_BIAS_A2_UNORM:
      break;
    case ddspp::B8G8R8A8_TYPELESS:
      break;
    case ddspp::B8G8R8A8_UNORM_SRGB:
      break;
    case ddspp::B8G8R8X8_TYPELESS:
      break;
    case ddspp::B8G8R8X8_UNORM_SRGB:
      break;
    case ddspp::BC6H_TYPELESS:
      break;
    case ddspp::BC6H_UF16:
      break;
    case ddspp::BC6H_SF16:
      break;
    case ddspp::BC7_TYPELESS:
      break;
    case ddspp::BC7_UNORM:
      break;
    case ddspp::BC7_UNORM_SRGB:
      break;
    case ddspp::AYUV:
      break;
    case ddspp::Y410:
      break;
    case ddspp::Y416:
      break;
    case ddspp::NV12:
      break;
    case ddspp::P010:
      break;
    case ddspp::P016:
      break;
    case ddspp::OPAQUE_420:
      break;
    case ddspp::YUY2:
      break;
    case ddspp::Y210:
      break;
    case ddspp::Y216:
      break;
    case ddspp::NV11:
      break;
    case ddspp::AI44:
      break;
    case ddspp::IA44:
      break;
    case ddspp::P8:
      break;
    case ddspp::A8P8:
      break;
    case ddspp::B4G4R4A4_UNORM:
      break;
    case ddspp::P208:
      break;
    case ddspp::V208:
      break;
    case ddspp::V408:
      break;
    case ddspp::ASTC_4X4_TYPELESS:
      break;
    case ddspp::ASTC_4X4_UNORM:
      break;
    case ddspp::ASTC_4X4_UNORM_SRGB:
      break;
    case ddspp::ASTC_5X4_TYPELESS:
      break;
    case ddspp::ASTC_5X4_UNORM:
      break;
    case ddspp::ASTC_5X4_UNORM_SRGB:
      break;
    case ddspp::ASTC_5X5_TYPELESS:
      break;
    case ddspp::ASTC_5X5_UNORM:
      break;
    case ddspp::ASTC_5X5_UNORM_SRGB:
      break;
    case ddspp::ASTC_6X5_TYPELESS:
      break;
    case ddspp::ASTC_6X5_UNORM:
      break;
    case ddspp::ASTC_6X5_UNORM_SRGB:
      break;
    case ddspp::ASTC_6X6_TYPELESS:
      break;
    case ddspp::ASTC_6X6_UNORM:
      break;
    case ddspp::ASTC_6X6_UNORM_SRGB:
      break;
    case ddspp::ASTC_8X5_TYPELESS:
      break;
    case ddspp::ASTC_8X5_UNORM:
      break;
    case ddspp::ASTC_8X5_UNORM_SRGB:
      break;
    case ddspp::ASTC_8X6_TYPELESS:
      break;
    case ddspp::ASTC_8X6_UNORM:
      break;
    case ddspp::ASTC_8X6_UNORM_SRGB:
      break;
    case ddspp::ASTC_8X8_TYPELESS:
      break;
    case ddspp::ASTC_8X8_UNORM:
      break;
    case ddspp::ASTC_8X8_UNORM_SRGB:
      break;
    case ddspp::ASTC_10X5_TYPELESS:
      break;
    case ddspp::ASTC_10X5_UNORM:
      break;
    case ddspp::ASTC_10X5_UNORM_SRGB:
      break;
    case ddspp::ASTC_10X6_TYPELESS:
      break;
    case ddspp::ASTC_10X6_UNORM:
      break;
    case ddspp::ASTC_10X6_UNORM_SRGB:
      break;
    case ddspp::ASTC_10X8_TYPELESS:
      break;
    case ddspp::ASTC_10X8_UNORM:
      break;
    case ddspp::ASTC_10X8_UNORM_SRGB:
      break;
    case ddspp::ASTC_10X10_TYPELESS:
      break;
    case ddspp::ASTC_10X10_UNORM:
      break;
    case ddspp::ASTC_10X10_UNORM_SRGB:
      break;
    case ddspp::ASTC_12X10_TYPELESS:
      break;
    case ddspp::ASTC_12X10_UNORM:
      break;
    case ddspp::ASTC_12X10_UNORM_SRGB:
      break;
    case ddspp::ASTC_12X12_TYPELESS:
      break;
    case ddspp::ASTC_12X12_UNORM:
      break;
    case ddspp::ASTC_12X12_UNORM_SRGB:
      break;
    case ddspp::FORCE_UINT:
      break;
    default:
      break;
  }
  throw std::runtime_error("Translation table not found for DXGI!");
}

std::vector<TextureInfo> loadDDS(const std::vector<std::vector<char>> &data) {
  std::vector<TextureInfo> textureInfos;
  textureInfos.reserve(data.size());
  for (const auto &ddsVec : data) {
    if (ddsVec.empty()) {
      PLOG_FATAL << "Found empty texture";
      exit(-1);
      continue;
    }
    uint8_t *ddsData = (uint8_t *)ddsVec.data();
    ddspp::Descriptor desc;
    ddspp::decode_header(ddsData, desc);
    TextureInfo info;
    info.data = ddsData + desc.headerSize;
    info.width = desc.width;
    info.height = desc.height;
    info.size = ddsVec.size() - desc.headerSize;
    info.internalFormatOverride = fromDXGI(desc.format);
    textureInfos.push_back(info);
  }
  return textureInfos;
}

std::vector<TTextureHolder> GameGraphicsModule::loadTextures(
    const std::vector<std::vector<char>> &data, const LoadType type) {
  if (data.empty()) return {};
  std::vector<TextureInfo> textureInfos;

  util::OnExit onExit([tinfos = &textureInfos, type = type] {
    if (type != LoadType::STBI) return;
    for (const auto &tex : *tinfos)
      if (tex.data != nullptr) free(tex.data);
  });

  if (type == LoadType::STBI) {
    textureInfos = loadSTBI(data);
  } else if (type == LoadType::DDSPP) {
    textureInfos = loadDDS(data);
  } else {
    throw std::runtime_error("Wrong load type!");
  }

  return apiLayer->pushTexture(textureInfos.size(), textureInfos.data());
}

std::vector<TTextureHolder> GameGraphicsModule::loadTextures(
    const std::vector<std::string> &names, const LoadType type) {
  const auto amount = names.size();
  std::vector<TTextureHolder> localtextureIDs(amount);
  std::vector<std::vector<char>> data;
  data.reserve(amount);
  for (size_t i = 0; i < amount; i++) {
    const auto &name = names[i];
    {
      std::lock_guard guard(protectTexture);
      auto found = textureMap.find(name);
      if (found != std::end(textureMap)) {
        localtextureIDs[i] = found->second;
        continue;
      }
    }
    std::vector<char> file;
    for (auto &function : assetResolver) {
      file = function(name);
      if (!file.empty()) break;
    }
    if (file.empty()) {
      localtextureIDs[i] = defaultTextureID;
      textureMap[names[i]] = defaultTextureID;
      PLOG_ERROR << "Couldn't find asset: " << name << "!";
      continue;
    }
    data.push_back(file);
    localtextureIDs[i] = TTextureHolder();
  }
  if (!data.empty()) {
    const auto startTexture = loadTextures(data, type);
    size_t nameID = 0;
    for (size_t i = 0; i < names.size(); i++) {
      if (!textureMap.contains(names[i])) {
        textureMap[names[i]] = startTexture[nameID];
        localtextureIDs[i] = startTexture[nameID];
        nameID++;
      }
    }
  }
  return localtextureIDs;
}

std::vector<TNodeHolder> GameGraphicsModule::addNode(const NodeInfo *nodeInfos,
                                                     const size_t count) {
  std::vector<shader::BindingInfo> bindings;
  bindings.reserve(count);
  std::vector<BufferInfo> bufferInfos(2 * count);
  auto allocation = nodeHolder.allocate(count);
  auto [dataHolder, transform, parent, binding, status, cache, cacheNormal] =
      allocation.iterator;
  std::fill(status, status + count, 1);
  for (size_t i = 0; i < count; i++) {
    const NodeInfo &nodeInfo = nodeInfos[i];
    binding[i] = nodeInfo.bindingID;
    transform[i] = nodeInfo.transforms;
    parent[i] = !nodeInfo.parentHolder ? allocation.beginIndex + nodeInfo.parent
                                       : nodeInfo.parentHolder.internalHandle;
    bufferInfos[i] =
        BufferInfo{&cache[i], sizeof(glm::mat4), DataType::Uniform};
    bufferInfos[i + count] =
        BufferInfo{&cacheNormal[i], sizeof(glm::mat4), DataType::Uniform};
  }
  const auto allData = apiLayer->pushData(bufferInfos);
  std::vector<TNodeHolder> nodeHolder(count);
  for (size_t i = 0; i < count; i++) {
    const NodeInfo &nodeInfo = nodeInfos[i];
    nodeHolder[i] = TNodeHolder(allocation.beginIndex + i);
    dataHolder[i] = allData[i];
    if (nodeInfo.bindingID != INVALID_SIZE_T) [[likely]] {
      shader::BindingInfo binding;
      binding.bindingSet = nodeInfo.bindingID;
      binding.type = shader::BindingType::UniformBuffer;
      binding.data.buffer.size = sizeof(glm::mat4);
      binding.data.buffer.dataID = allData[i];
      binding.data.buffer.offset = 0;
      binding.binding = 2;
      bindings.push_back(binding);
      binding.binding = 3;
      binding.data.buffer.size = sizeof(glm::mat4);
      binding.data.buffer.dataID = projection;
      binding.data.buffer.offset = 0;
      bindings.push_back(binding);
    }
  }

  apiLayer->getShaderAPI()->bindData(bindings.data(), bindings.size());
  return nodeHolder;
}

}  // namespace tge::graphics