#pragma once

#include <vulkan/vulkan.hpp>
#include <string>
#include <vector>
#include <array>

namespace tge::shader {

using namespace vk;

inline std::string operator""_str(const char *chr,
                                                  std::size_t size) {
  return std::string(chr, size);
}

const std::array shaderNames = {
    std::vector({"assets/testvec4.vert"_str, "assets/test.frag"_str}),
    std::vector({"assets/testUV.vert"_str, "assets/testTexture.frag"_str})};

struct VulkanShaderPipe {
  std::vector<std::pair<std::vector<uint32_t>, ShaderStageFlagBits>> shader;
  std::vector<PipelineShaderStageCreateInfo> pipelineShaderStage;
  std::vector<VertexInputBindingDescription> vertexInputBindings;
  std::vector<VertexInputAttributeDescription> vertexInputAttributes;
  PipelineVertexInputStateCreateInfo inputStateCreateInfo;
  PipelineRasterizationStateCreateInfo rasterization;
  std::vector<DescriptorSetLayoutBinding> descriptorLayoutBindings;
  size_t layoutID = UINT64_MAX;
  bool needsDefaultBindings = true;
};

inline uint32_t getSizeFromFormat(const Format format)
{
    switch (format)
    {
    case Format::eR32Sfloat:
    case Format::eR32Sint:
        return 4;
    case Format::eR32G32Sfloat:
        return 8;
    case Format::eR32G32B32Sfloat:
        return 12;
    case Format::eR32G32B32A32Sfloat:
        return 16;
    default:
        throw std::runtime_error(std::string("Couldn't find size for Format ") +
            to_string(format));
    }
}

} // namespace tge::graphics
