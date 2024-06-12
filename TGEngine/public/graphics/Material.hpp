#pragma once

#include <stdint.h>
#include "ElementHolder.hpp"

namespace tge::graphics {

enum RenderTarget { NONE = 1, OPAQUE_TARGET = 2, TRANSLUCENT_TARGET = 4 };

class APILayer;

using Color = float[4];

struct Material {

  Material(void *costumShaderData) : costumShaderData(costumShaderData) {}

  Material() = default;

  void *costumShaderData = nullptr; // API dependent
  bool doubleSided = false;
  uint32_t primitiveType = INVALID_UINT32;
  bool clockwise = false;
  RenderTarget target = RenderTarget::OPAQUE_TARGET;
};
} // namespace tge::graphics
