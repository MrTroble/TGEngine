#pragma once

#include <stdint.h>
#include "ElementHolder.hpp"

namespace tge::graphics {

class APILayer;

using Color = float[4];

enum class MaterialType { None, TextureOnly };
constexpr MaterialType MAX_TYPE = MaterialType::TextureOnly;

struct Material {

  Material(void *costumShaderData) : costumShaderData(costumShaderData) {}

  Material() = default;

  MaterialType type = MaterialType::None;
  void *costumShaderData = nullptr; // API dependent
  bool doubleSided = false;
  uint32_t primitiveType = INVALID_UINT32;
};
} // namespace tge::graphics
