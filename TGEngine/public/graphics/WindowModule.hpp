#pragma once

#include <mutex>
#include <thread>
#include <vector>

#include "../Module.hpp"
#include <vulkan/vulkan.hpp>

namespace tge::graphics {

struct WindowProperties {
  bool centered = false;    // Not implemented
  int x = 0;                // Ignored if centered
  int y = 0;                // Ignored if centered
  char fullscreenmode = 0;  // Not implemented
  int width = 1000;         // Ignored if fullscreenmode != 0
  int height = 1000;        // Ignored if fullscreenmode != 0
};

struct WindowBounds {
  int x;
  int y;
  int width;
  int height;
};

class WindowModule : public main::Module {
 public:
  void* hInstance;
  void* hWnd;
  void* root;
  std::vector<void*> customFn;
  bool closeRequest = false;
  bool closing = false;
  std::thread osThread;
  std::mutex osMutex;
  std::mutex exitMutex;
  std::mutex resizeMutex;

  main::Error init() override;

  void tick(double deltatime) override;

  void destroy() override;

  WindowProperties getWindowProperties();

  WindowBounds getBounds();

  std::vector<const char*> getExtensionRequirements();

  vk::SurfaceKHR getVulkanSurface(const vk::Instance instance);

  bool isMinimized();
};

}  // namespace tge::graphics
