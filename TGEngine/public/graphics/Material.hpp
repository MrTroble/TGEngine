#pragma once

#include <stdint.h>
#include "ElementHolder.hpp"

namespace tge::graphics {

class APILayer;

using Color = float[4];

struct Material {

  Material(void *costumShaderData) : costumShaderData(costumShaderData) {}

  Material() = default;

  void *costumShaderData = nullptr; // API dependent
  bool doubleSided = false;
  uint32_t primitiveType = INVALID_UINT32;
  bool clockwise = false;
};
} // namespace tge::graphics
