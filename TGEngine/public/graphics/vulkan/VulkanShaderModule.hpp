#pragma once

#include <mutex>
#include <thread>
#include <vulkan/vulkan.hpp>

#include "../GameShaderModule.hpp"
#include "VulkanShaderPipe.hpp"
#include "../../DataHolder.hpp"

namespace tge::shader {

struct BindingPipeInfo {
  size_t descSet;
  size_t pipeline;
};

class VulkanShaderModule : public tge::shader::ShaderAPI {
 public:
  explicit VulkanShaderModule(void* vgm) : vgm(vgm) {}

  void* vgm;
  std::vector<vk::DescriptorPool> descPools;
  std::vector<vk::PipelineLayout> pipeLayouts;
  std::vector<vk::DescriptorSetLayout> setLayouts;
  std::vector<vk::DescriptorSet> descSets;
  std::vector<tge::shader::VulkanShaderPipe*> shaderPipes;

  tge::DataHolder<vk::DescriptorSet, vk::DescriptorSetLayout,
                  vk::PipelineLayout, size_t, size_t>
      bindingHolder;

  DescriptorSetLayout defaultDescLayout;
  PipelineLayout defaultLayout;
  std::mutex mutex;
  // Legacy support
  std::vector<std::vector<BindingInfo>> defaultbindings;

  ShaderPipe loadShaderPipeAndCompile(
      const std::vector<std::string>& shadernames,
      const ShaderCreateInfo& createInfo = {}) override;

  ShaderPipe compile(const std::vector<ShaderInfo>& shadernames,
                     const ShaderCreateInfo& createInfo = {}) override;

  std::vector<TBindingHolder> createBindings(ShaderPipe pipe,
                                             const size_t count = 1) override;

  void changeInputBindings(const ShaderPipe pipe, const size_t bindingID,
                           const size_t buffer) override;

  void bindData(const BindingInfo* info, const size_t count) override;

  void addToRender(const std::span<const TBindingHolder> bindings,
                   void* customData) override;

  void addToMaterial(const graphics::Material* material,
                     void* customData) override;

  void init() override;

  void destroy() override;
};

}  // namespace tge::shader
