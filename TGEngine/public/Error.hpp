#pragma once

#include <stdint.h>

#include <cstddef>
#include <limits>

static constexpr auto forceSemicolon = 0;
static constexpr size_t INVALID_SIZE_T = std::numeric_limits<size_t>().max();
static constexpr uint32_t INVALID_UINT32 =
    std::numeric_limits<uint32_t>().max();

#ifdef DEBUG
#define TGE_EXPECT(statement, message, rv) \
  if (!(statement)) {                      \
    PLOG_ERROR << message;                 \
    return rv;                             \
  }                                        \
  forceSemicolon

#define TGE_EXPECT_N(statement, message) \
  if (!(statement)) {                    \
    PLOG_ERROR << message;               \
    return;                              \
  }                                      \
  forceSemicolon
#else
#define TGE_EXPECT(statement, message, rv) forceSemicolon
#define TGE_EXPECT_N(statement, message) forceSemicolon
#endif  // DEBUG

namespace tge::main {

enum class Error {
  NONE,
  ALREADY_RUNNING,
  ALREADY_INITIALIZED,
  NOT_INITIALIZED,
  NO_GRAPHIC_QUEUE_FOUND,
  SURFACECREATION_FAILED,
  COULD_NOT_CREATE_WINDOW,
  COULD_NOT_CREATE_WINDOW_CLASS,
  NO_MODULE_HANDLE,
  FORMAT_NOT_FOUND,
  SWAPCHAIN_EXT_NOT_FOUND,
  NO_SURFACE_SUPPORT,
  VULKAN_ERROR,
  GLTF_LOADER_ERROR,
  FORMAT_NOT_SUPPORTED,
  INDEPENDENT_BLEND_NOT_SUPPORTED
};

extern Error error;

}  // namespace tge::main
