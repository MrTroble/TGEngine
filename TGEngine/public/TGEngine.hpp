#pragma once

#include "Error.hpp"
#include "Module.hpp"
#include "graphics/GameGraphicsModule.hpp"
#include <chrono>
#include <vector>

#ifdef WIN32
constexpr auto END_CHARACTER = '\\';
#else
#ifdef __linux__
constexpr auto END_CHARACTER = '/';
#endif
#endif

namespace tge::main {

extern std::vector<Module *> lateModules;

Error init(const graphics::FeatureSet &featureSet = {});

Error start();

Error lastError();

void fireRecreate();

graphics::APILayer *getAPILayer();

graphics::GameGraphicsModule *getGameGraphicsModule();

void* getMainWindowHandle();

} // namespace tge::main
