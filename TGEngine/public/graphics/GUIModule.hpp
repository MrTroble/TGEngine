#pragma once

#include "../Module.hpp"
#include <cstddef>

namespace tge::gui {

class GUIModule : public tge::main::Module {

public:

  void *pool;
  size_t buffer;
  size_t primary;
  void *renderpass;
  void *framebuffer = nullptr;

  main::Error init() override;

  void tick(double deltatime) override;

  void destroy() override;

  void recreate() override;

  virtual void renderGUI() = 0;
};

} // namespace tge::gui
