#pragma once

#include <concepts>
#include <functional>

#include "../Error.hpp"

namespace tge::graphics {

struct EntryHolder {
  size_t internalHandle = INVALID_SIZE_T;

  EntryHolder() = default;

  EntryHolder(const size_t internalHandle) : internalHandle(internalHandle) {}

  [[nodiscard]] inline bool operator!() const noexcept {
    return internalHandle == INVALID_SIZE_T;
  }

  [[nodiscard]] inline bool operator==(
      const EntryHolder& holder) const noexcept {
    return this->internalHandle == holder.internalHandle;
  }
};

#define DEFINE_HOLDER(name)                                    \
  struct T##name##Holder : public tge::graphics::EntryHolder { \
    T##name##Holder() = default;                               \
    using tge::graphics::EntryHolder::EntryHolder;             \
  }

DEFINE_HOLDER(Pipeline);
DEFINE_HOLDER(Render);
DEFINE_HOLDER(Sampler);
DEFINE_HOLDER(Texture);
DEFINE_HOLDER(Data);

}  // namespace tge::graphics

namespace tge::shader {
DEFINE_HOLDER(Binding);
}

namespace std {
template <std::derived_from<tge::graphics::EntryHolder> T>
struct std::hash<T> {
  [[nodiscard]] inline std::size_t operator()(
      const tge::graphics::EntryHolder& s) const noexcept {
    const std::hash<std::size_t> test;
    return test(s.internalHandle);
  }
};
}  // namespace std