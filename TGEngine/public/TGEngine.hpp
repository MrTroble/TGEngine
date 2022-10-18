#pragma once

#include "Error.hpp"
#include "Module.hpp"
#include "graphics/GameGraphicsModule.hpp"
#include <chrono>
#include <vector>

namespace tge::main {

extern std::vector<Module *> lateModules;

Error init(const graphics::FeatureSet &featureSet = {});

Error start();

Error lastError();

void fireRecreate();

graphics::APILayer *getAPILayer();

graphics::GameGraphicsModule *getGameGraphicsModule();

} // namespace tge::main
