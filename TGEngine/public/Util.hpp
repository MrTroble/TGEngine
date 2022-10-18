#pragma once

#include <filesystem>
#include <stdint.h>
#include <type_traits>
#include <vector>
#if defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
#define COMPILED_IN_LINUX
#elif defined(__APPLE__) && defined(__MACH__)
#define COMPILED_IN_MACOS
#elif defined(_WIN32) || defined(_WIN64)
#define COMPILED_IN_WINDOWS
#endif

namespace fs = std::filesystem;

namespace tge::util {

template <class T> concept Callable = std::is_invocable_v<T>;
template <Callable C> class OnExit {

  const C call;

public:
  constexpr OnExit(const C cin) : call(cin) {}
  constexpr ~OnExit() { call(); }
};

std::vector<char> wholeFile(const fs::path &path);

extern bool exitRequest;

void requestExit();

} // namespace tge::util