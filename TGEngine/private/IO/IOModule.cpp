#include "../../public/IO/IOModule.hpp"

#include <iostream>

#include "../../public/TGEngine.hpp"

#ifdef WIN32
#include <Windows.h>
#include <windowsx.h>
#endif  // WIN
#ifdef __linux__
#include <X11/Xlib.h>
#endif

namespace tge::io {

std::vector<IOModule *> ios;

template <PressMode mode>
inline void dispatchInputs(WPARAM lParam, int additional, int buttonID) {
  const auto xParam = GET_X_LPARAM(lParam);
  const auto yParam = GET_Y_LPARAM(lParam);
  MouseEvent mouseevent = {xParam, yParam, buttonID, additional, mode};
  for (const auto io : ios) io->mouseEvent(mouseevent);
}

#ifdef WIN32
LRESULT CALLBACK callback(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
  switch (Msg) {
    case WM_LBUTTONUP:
      dispatchInputs<PressMode::RELEASED>(lParam, 0, 1);
      break;
    case WM_MBUTTONUP:
      dispatchInputs<PressMode::RELEASED>(lParam, (int)wParam, 3);
      break;
    case WM_RBUTTONUP:
      dispatchInputs<PressMode::RELEASED>(lParam, (int)wParam, 2);
      break;
    case WM_LBUTTONDOWN:
      dispatchInputs<PressMode::CLICKED>(lParam, (int)wParam, 1);
      break;
    case WM_MBUTTONDOWN:
      dispatchInputs<PressMode::CLICKED>(lParam, (int)wParam, 3);
      break;
    case WM_RBUTTONDOWN:
      dispatchInputs<PressMode::CLICKED>(lParam, (int)wParam, 2);
      break;
    case WM_MOUSEMOVE:
      dispatchInputs<PressMode::HOLD>(lParam, 0, (int)wParam);
      break;
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL: {
      const auto zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
      for (const auto io : ios) io->mouseEvent({zDelta, zDelta, SCROLL});
    } break;
    case WM_KEYDOWN: {
      for (const auto io : ios) io->keyboardEvent({(uint32_t)wParam, PressMode::CLICKED});
    } break;
    case WM_KEYUP: {
      for (const auto io : ios) io->keyboardEvent({ (uint32_t)wParam, PressMode::RELEASED });
    } break;
    default:
      break;
  }
  return DefWindowProc(hWnd, Msg, wParam, lParam);
}
#endif

#ifdef __linux__

int lastButton = 0;

int callback(XEvent &event) {
  switch (event.type) {
    case MotionNotify: {
      const auto xParam = event.xmotion.x;
      const auto yParam = event.xmotion.y;
      for (const auto io : ios)
        io->mouseEvent({xParam, yParam, (int)lastButton});
      break;
    }
    case ButtonPress: {
      if (event.xbutton.button == Button4) {
        for (const auto io : ios)
          io->mouseEvent(
              {event.xbutton.x, event.xbutton.y, (int)tge::io::SCROLL});
        return 0;
      } else if (event.xbutton.button == Button5) {
        for (const auto io : ios)
          io->mouseEvent(
              {-event.xbutton.x, -event.xbutton.y, (int)tge::io::SCROLL});
        return 0;
      }
      const auto xParam = event.xbutton.x;
      const auto yParam = event.xbutton.y;
      lastButton = event.xbutton.button;
      for (const auto io : ios)
        io->mouseEvent({xParam, yParam, (int)lastButton});
      break;
    }
    case ButtonRelease: {
      lastButton = 0;
      break;
    }
    case KeyPress: {
      const auto sym = XLookupKeysym(&event.xkey, 0);
      uint32_t name = (int)sym;
      if (sym <= 'z' && sym >= 'a') {
        name -= 32;
      } else if (sym >= 'A' && sym <= 'Z') {
        name += 32;
      }
      for (const auto io : ios) io->keyboardEvent({name});
      break;
    }
  }
  return 0;
}
#endif

bool funcAdded = false;

main::Error IOModule::init() {
  if (!funcAdded) {
    auto win = main::getGameGraphicsModule()->getWindowModule();
    win->customFn.push_back((void *)&callback);
    funcAdded = true;
  }
  ios.push_back(this);
  return main::Error::NONE;
}

};  // namespace tge::io
