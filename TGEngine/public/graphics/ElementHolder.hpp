#pragma once

#include "../Error.hpp"
#include <functional>

namespace tge::graphics {
class APILayer;

struct EntryHolder {
  uint32_t internalHandle = INVALID_UINT32;
  uint32_t referenceID = INVALID_UINT32;

  EntryHolder() = default;

  EntryHolder(APILayer* api, const size_t internalHandle);

  [[nodiscard]] inline bool operator!() const noexcept {
    return internalHandle == INVALID_UINT32;
  }

  [[nodiscard]] inline bool operator==(
      const EntryHolder& holder) const noexcept {
    return this->internalHandle == holder.internalHandle;
  }
};

struct PipelineHolder : public EntryHolder {
  using EntryHolder::EntryHolder;
};

#define DEFINE_HOLDER(name)                     \
  struct T##name##Holder : public EntryHolder { \
    T##name##Holder() = default;                \
    using EntryHolder::EntryHolder;             \
  }

DEFINE_HOLDER(Render);
DEFINE_HOLDER(Sampler);
DEFINE_HOLDER(Texture);
DEFINE_HOLDER(Data);

}  // namespace tge::graphics

namespace std {
template <>
struct std::hash<tge::graphics::EntryHolder> {
  [[nodiscard]] inline std::size_t operator()(
      tge::graphics::EntryHolder const& s) const noexcept {
    return std::hash<size_t>{}(s.internalHandle);
  }
};
}  // namespace std