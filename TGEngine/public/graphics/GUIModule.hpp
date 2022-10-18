#pragma once

#include "../Module.hpp"

namespace tge::gui {

class GUIModule : public tge::main::Module {

public:

  void *pool;
  void *buffer;
  void *renderpass;
  void *framebuffer = nullptr;

  main::Error init() override;

  void tick(double deltatime) override;

  void destroy() override;

  void recreate() override;

  virtual void renderGUI() = 0;
};

} // namespace tge::gui
