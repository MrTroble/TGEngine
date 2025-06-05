#include <array>
#include <iostream>
#include <string>

#include "../../public/Util.hpp"
#include "../../public/graphics/WindowModule.hpp"

#ifndef APPLICATION_NAME
#define APPLICATION_NAME "unknown"
#endif

#ifndef APPLICATION_VERSION
#define APPLICATION_VERSION VK_MAKE_VERSION(1, 0, 0)
#endif

#define VERROR(rslt)                                               \
  if (rslt != vk::Result::eSuccess) {                              \
    std::string s = to_string(rslt);                               \
    const auto file = __FILE__;                                    \
    const auto line = __LINE__;                                    \
    printf("Vulkan error %s in %s L%d!\n", s.c_str(), file, line); \
  }  // namespace tge::graphics

#include <GLFW/glfw3.h>

namespace tge::graphics {

    main::Error init(WindowModule* winModule) {
        if (!glfwInit()) return main::Error::COULD_NOT_CREATE_WINDOW_CLASS;
        const auto windowProperties = winModule->getWindowProperties();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_DECORATED, true);
        glfwWindowHint(GLFW_RESIZABLE, true);
        glfwWindowHint(GLFW_RESIZABLE, true);

        GLFWwindow* window = glfwCreateWindow(windowProperties.width,
            windowProperties.height, APPLICATION_NAME, NULL, NULL);
        if (!window) return main::Error::COULD_NOT_CREATE_WINDOW;
        if (!windowProperties.centered) {
            glfwSetWindowPos(window, windowProperties.x,
                windowProperties.y);
        }

        winModule->hWnd = window;
        return main::Error::NONE;
    }

    void pool(WindowModule* winModule) {
        glfwPollEvents();
    }

    void destroy(WindowModule* winModule) {
        glfwDestroyWindow((GLFWwindow*)winModule->hWnd);
        glfwTerminate();
    }

    main::Error WindowModule::init() { return tge::graphics::init(this); }

    void WindowModule::tick(double deltatime) { tge::graphics::pool(this); }

    void WindowModule::destroy() {
        this->closing = true;
        tge::graphics::destroy(this);
    }

    WindowProperties WindowModule::getWindowProperties() {
        return WindowProperties();
    }

    WindowBounds WindowModule::getBounds() {
        WindowBounds bounds;
        const auto window = (GLFWwindow*)this->hWnd;
        glfwGetWindowPos(window, &bounds.x, &bounds.y);
        glfwGetWindowSize(window, &bounds.width, &bounds.height);
        return bounds;
    }

    std::vector<const char*> WindowModule::getExtensionRequirements() {
        uint32_t count;
        const char** extensions = glfwGetRequiredInstanceExtensions(&count);
        return std::vector(extensions, extensions + count);
    }

    vk::SurfaceKHR WindowModule::getVulkanSurface(const vk::Instance instance) {
        VkSurfaceKHR surface;
        const auto window = (GLFWwindow*)this->hWnd;
        vk::Result err{ glfwCreateWindowSurface(instance, window, NULL, &surface) };
        VERROR(err);
        return surface;
    }

    bool WindowModule::isMinimized() {
        const auto window = (GLFWwindow*)this->hWnd;
        return glfwGetWindowAttrib(window, GLFW_ICONIFIED);
    }

}  // namespace tge::graphics
