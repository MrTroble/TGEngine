#pragma once

#include <stdint.h>
#include "ElementHolder.hpp"

namespace tge::shader {
	DEFINE_HOLDER(Shader);
	using ShaderPipe = TShaderHolder;
}

namespace tge::graphics {

enum RenderTarget { NONE = 1, OPAQUE_TARGET = 2, TRANSLUCENT_TARGET = 4, TOOL = 8};

class APILayer;

using Color = float[4];

struct BlendFactorExt;

struct Material {

  Material(shader::ShaderPipe costumShaderData) : costumShaderData(costumShaderData) {}

  Material() = default;

  shader::ShaderPipe costumShaderData{};
  bool doubleSided = false;
  uint32_t primitiveType = INVALID_UINT32;
  bool clockwise = false;
  RenderTarget target = RenderTarget::OPAQUE_TARGET;
  bool depthTest = true;
  std::shared_ptr<BlendFactorExt> blendFactor = nullptr; // API dependent
};
} // namespace tge::graphics
