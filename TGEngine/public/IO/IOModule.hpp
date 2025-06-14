#pragma once

#include "../Module.hpp"
#include "../graphics/WindowModule.hpp"
#include "../headerlibs/enum.h"

namespace tge::io {

	constexpr auto SCROLL = 0x10000000;
	constexpr auto MIDDLE_MOUSE = 0x00000010;

	enum class PressMode {
		CLICKED,
		RELEASED,
		HOLD,
		SCROLL,
		UNKNOWN
	};

	struct KeyboardEvent {
		unsigned int signal;
		PressMode mode;
	};

	struct MouseEvent {
		int x;
		int y;
		int pressed;
		int additional;
		PressMode pressMode;
	};

	class IOModule : public tge::main::Module {
		tge::graphics::WindowModule* windowModule = nullptr;
	protected:
		double oldX = 0, oldY = 0;
		double deltaX = 0, deltaY = 0;
		double inputX = 0, inputY = 0;

	public:
		main::Error init() override;

		virtual void tick(double delta) override;

		virtual void mouseEvent(const MouseEvent& event) = 0;

		virtual void keyboardEvent(const KeyboardEvent& event) = 0;
	};

} // namespace tge::io

BETTER_ENUM(SpecialKeys, uint32_t, Shift = 340, Ctrl = 341);
