#pragma once

#include <vulkan/vulkan.hpp>
#include <string>
#include <vector>
#include <array>
#include <mutex>

namespace tge::shader {

using namespace vk;

inline std::string operator""_str(const char *chr,
                                                  std::size_t size) {
  return std::string(chr, size);
}

struct VulkanShaderPipe {
  std::vector<std::pair<std::vector<uint32_t>, ShaderStageFlagBits>> shader;
  std::vector<VertexInputBindingDescription> vertexInputBindings;
  std::vector<VertexInputAttributeDescription> vertexInputAttributes;
  PipelineVertexInputStateCreateInfo inputStateCreateInfo;
  std::vector<DescriptorSetLayoutBinding> descriptorLayoutBindings;
  std::vector<PushConstantRange> constranges;
  size_t layoutID = INVALID_SIZE_T;
  bool needsDefaultBindings = true;

  std::mutex pipeMutex;
  std::vector<std::pair<ShaderStageFlagBits, ShaderModule>> modules;
  std::vector<std::string> shaderNames;

};

inline uint32_t getSizeFromFormat(const Format format)
{
    switch (format)
    {
    case Format::eR32Uint:
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
