#include "../../public/IO/IOModule.hpp"

#include <iostream>

#include "../../public/TGEngine.hpp"
#include <GLFW/glfw3.h>

namespace tge::io {

	std::vector<IOModule*> ios;

	static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
		const PressMode mode = action == GLFW_RELEASE ? PressMode::RELEASED : (action == GLFW_PRESS ? PressMode::CLICKED : PressMode::UNKNOWN);
		const KeyboardEvent event{ key, mode };
		for (auto io : ios)	io->keyboardEvent(event);
	};

	static void mouseCallback(GLFWwindow* window, int button, int action, int mods) {
		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);
		const PressMode mode = action == GLFW_RELEASE ? PressMode::RELEASED : (action == GLFW_PRESS ? PressMode::CLICKED : PressMode::UNKNOWN);
		const MouseEvent event{ (int)xpos, (int)ypos, button + 1, mods, mode };
		for (auto io : ios)	io->mouseEvent(event);
	}

	bool isInit = false;

	main::Error IOModule::init() {
		ios.push_back(this);
		this->windowModule = main::getGameGraphicsModule()->getWindowModule();
		if (isInit) return main::Error::NONE;
		GLFWwindow* window = (GLFWwindow*)this->windowModule->hWnd;
		glfwSetKeyCallback(window, &keyCallback);
		glfwSetMouseButtonCallback(window, &mouseCallback);
		isInit = true;
		return main::Error::NONE;
	}

	void IOModule::tick(double delta) {
		GLFWwindow* window = (GLFWwindow*)this->windowModule->hWnd;
		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);
		deltaX = xpos - oldX;
		deltaY = ypos - oldY;
		inputX = deltaX * delta;
		inputY = deltaY * delta;
		for (size_t i = 0; i < GLFW_MOUSE_BUTTON_LAST; i++)
		{
			const auto pressed = glfwGetMouseButton(window, i);
			if (pressed == GLFW_PRESS) {
				const PressMode mode = PressMode::HOLD;
				const MouseEvent event{ (int)xpos, (int)ypos, i + 1, 0, mode };
				mouseEvent(event);
			}
		}
	}

};  // namespace tge::io
