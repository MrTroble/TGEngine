#include "../../public/IO/IOModule.hpp"
#include "../../public/TGEngine.hpp"
#include <iostream>

#ifdef WIN32
#include <Windows.h>
#include <windowsx.h>
#endif // WIN
#ifdef __linux__
#include <X11/Xlib.h>
#endif

namespace tge::io
{

  std::vector<IOModule *> ios;

#ifdef WIN32
  LRESULT CALLBACK callback(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
  {
    switch (Msg)
    {
    case WM_MOUSEMOVE:
    {
      const auto xParam = GET_X_LPARAM(lParam);
      const auto yParam = GET_Y_LPARAM(lParam);
      for (const auto io : ios)
        io->mouseEvent({xParam, yParam, (int)wParam});
    }
    break;
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
    {
      const auto zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
      for (const auto io : ios)
        io->mouseEvent({zDelta, zDelta, SCROLL});
    }
    break;
    case WM_KEYDOWN:
    {
      for (const auto io : ios)
        io->keyboardEvent({(uint32_t)wParam});
    }
    break;
    default:
      break;
    }
    return DefWindowProc(hWnd, Msg, wParam, lParam);
  }
#endif

#ifdef __linux__

  int lastButton = 0;

  int callback(XEvent &event)
  {
    switch (event.type)
    {
    case MotionNotify:
    {
      const auto xParam = event.xmotion.x;
      const auto yParam = event.xmotion.y;
      for (const auto io : ios)
        io->mouseEvent({xParam, yParam, (int)lastButton});
      break;
    }
    case ButtonPress:
    {
      if (event.xbutton.button == Button4)
      {
        for (const auto io : ios)
          io->mouseEvent({event.xbutton.x, event.xbutton.y, (int)tge::io::SCROLL});
        return 0;
      }
      else if (event.xbutton.button == Button5)
      {
        for (const auto io : ios)
          io->mouseEvent({-event.xbutton.x, -event.xbutton.y, (int)tge::io::SCROLL});
        return 0;
      }
      const auto xParam = event.xbutton.x;
      const auto yParam = event.xbutton.y;
      lastButton = event.xbutton.button;
      for (const auto io : ios)
        io->mouseEvent({xParam, yParam, (int)lastButton});
      break;
    }
    case ButtonRelease:
    {
      lastButton = 0;
      break;
    }
    case KeyPress:
    {
      const auto sym = XLookupKeysym(&event.xkey, 0);
      const auto name = std::toupper(*XKeysymToString(sym));
      for (const auto io : ios)
        io->keyboardEvent({(uint32_t)name});
      break;
    }
    }
    return 0;
  }
#endif

  bool funcAdded = false;

  main::Error IOModule::init()
  {
    if (!funcAdded)
    {
      auto win = main::getGameGraphicsModule()->getWindowModule();
      win->customFn.push_back((void *)&callback);
      funcAdded = true;
    }
    ios.push_back(this);
    return main::Error::NONE;
  }

}; // namespace tge::io
