#pragma once

#include <cstddef>

#include "../../public/graphics/WindowModule.hpp"
#include "../Module.hpp"
#include "../graphics/APILayer.hpp"
#include "../IO/IOModule.hpp"

namespace tge::gui {

class DebugGUIModule : public tge::main::Module {
 public:
  graphics::WindowModule *winModule = nullptr;
  graphics::APILayer* api = nullptr;
  io::IOModule* io = nullptr;
  
  DebugGUIModule(io::IOModule* io) : io(io) {}

  main::Error init() override;

  void tick(double deltatime) override;

  void destroy() override;

  void recreate() override {

  }

  virtual void renderGUI() = 0;
};

}  // namespace tge::gui
