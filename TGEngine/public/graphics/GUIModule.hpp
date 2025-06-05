#pragma once

#include <cstddef>

#include "../../public/graphics/WindowModule.hpp"
#include "../Module.hpp"

namespace tge::gui {

class GUIModule : public tge::main::Module {
 public:
  void *pool = nullptr;
  size_t buffer = SIZE_MAX;
  size_t primary = SIZE_MAX;
  void *renderpass = nullptr;
  void *framebuffer = nullptr;

  virtual ~GUIModule() = default;

  main::Error init() override;

  void tick(double deltatime) override;

  void destroy() override;

  void recreate() override;

  virtual void renderGUI() = 0;
};

}  // namespace tge::gui
