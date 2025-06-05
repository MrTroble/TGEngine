#include "../../public/graphics/GUIModule.hpp"

#include <vulkan/vulkan.hpp>

#include "../../public/TGEngine.hpp"
#include "../../public/graphics/WindowModule.hpp"
#include "../../public/graphics/vulkan/VulkanModuleDef.hpp"
//
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
//

namespace tge::gui {

using namespace vk;

main::Error DebugGUIModule::init() {
  auto winModule = main::getGameGraphicsModule()->getWindowModule();
  auto api = main::getAPILayer();
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  ImGui::StyleColorsDark();

  api->initDebugGUI();

  if(!ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)winModule->hWnd, true))
      return main::Error::COULD_NOT_CREATE_WINDOW;
  return main::Error::NONE;
}

void DebugGUIModule::tick(double deltatime) { 
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    ImGui::NewFrame();

    renderGUI();

    ImGui::Render();
 }

void DebugGUIModule::destroy() {
  const auto vmod =
      (graphics::VulkanGraphicsModule*)main::getAPILayer()->backend();
  vmod->device.waitIdle();
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

}  // namespace tge::gui
