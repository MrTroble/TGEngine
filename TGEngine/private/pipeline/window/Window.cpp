#include "../../../public/pipeline/window/Window.hpp"
#include "../../../public/ui/UISystem.hpp"
#include "../../../public/io/Keyboard.hpp"

namespace tge::win {

	uint8_t states = 0;
	VkSurfaceKHR windowSurface;

	uint32_t mainWindowWidth = 0;
	uint32_t mainWindowHeight = 0;
	uint32_t mainWindowX = 0;
	uint32_t mainWindowY = 0;

	float mouseX = 0;
	float mouseY = 0;
	float mouseHomogeneousX = 0;
	float mouseHomogeneousY = 0;

	bool isDecorated;
	bool isFullscreen;
	bool isConsumingInput;
	bool isCloseRequested;
	bool isFocused;
	bool isMinimized;

#if defined(_WIN32) || defined(_WIN64)
	HMODULE systemModule;
	HWND winHWND;
	HCURSOR winHCURSOR;

	LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		switch (msg) {
		case WM_INPUT:
		{
			uint32_t dwSize = 0;

			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));

			LPBYTE lpb = new BYTE[dwSize];
			if (lpb == NULL)
				return 0;

			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER));

			RAWINPUT* raw = (RAWINPUT*)lpb;

			if (raw->header.dwType == RIM_TYPEKEYBOARD) {
				const uint16_t vkey = raw->data.keyboard.VKey;
				const bool pressed = raw->data.keyboard.Flags;
				switch (vkey) {
				case 'W':
					if (pressed) states &= 0b00001110;
					else states |= 1;
					break;
				case 'S':
					if (pressed) states &= 0b00001101;
					else states |= 2;
					break;
				case 'D':
					if (pressed) states &= 0b00001011;
					else states |= 4;
					break;
				case 'A':
					if (pressed) states &= 0b00000111;
					else states |= 8;
					break;
				}
				tge::io::implKeyUpdate(vkey, pressed);
			}
			else if (raw->header.dwType == RIM_TYPEMOUSE) {
				RECT rect;
				if (GetWindowRect(hwnd, &rect)) {
					mainWindowX = rect.left;
					mainWindowY = rect.top;
				}

				POINT point;
				if (GetCursorPos(&point)) {
					mouseX = (float)(point.x - mainWindowX);
					mouseY = (float)(point.y - mainWindowY);
				}

				tge::ui::checkBoundingBoxes();
			}

			return DefWindowProc(hwnd, msg, wParam, lParam);
		}
		case WM_QUIT:
		case WM_CLOSE:
		case WM_DESTROY:
			isCloseRequested = true;
			break;
		case WM_MOUSELEAVE:
		case WM_NCMOUSELEAVE:
			SetCursor(winHCURSOR);
			break;
		case WM_LBUTTONDOWN:
			tge::io::FIRST_MOUSE_BUTTON = true;
			break;
		case WM_LBUTTONUP:
			tge::io::FIRST_MOUSE_BUTTON = false;
			break;
		case WM_MBUTTONDOWN:
			tge::io::THIRD_MOUSE_BUTTON = true;
			break;
		case WM_MBUTTONUP:
			tge::io::THIRD_MOUSE_BUTTON = false;
			break;
		case WM_RBUTTONDOWN:
			tge::io::SECOND_MOUSE_BUTTON = true;
			break;
		case WM_RBUTTONUP:
			tge::io::SECOND_MOUSE_BUTTON = false;
			break;
		case WM_KILLFOCUS:
			isFocused = false;
			break;
		case WM_SETFOCUS:
			isFocused = true;
			break;
		case WM_SYSCOMMAND:
			if (wParam == SC_MINIMIZE) {
				isMinimized = true;
			}
			else if (wParam == SC_RESTORE) {
				isMinimized = false;
			}
			return DefWindowProc(hwnd, msg, wParam, lParam);
		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
		}
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
#endif

	void createWindow() {
		isFullscreen = tgeproperties.getBoolean("fullscreen");
		isDecorated = isFullscreen ? false : tgeproperties.getBooleanOrDefault("decorated", true);
		isConsumingInput = tgeproperties.getBooleanOrDefault("consumesinput", false);

		if (isFullscreen) {
#ifdef _WIN32
			const HWND hDesktop = GetDesktopWindow();
			RECT desktop;
			GetWindowRect(hDesktop, &desktop);
			mainWindowHeight = desktop.bottom;
			mainWindowWidth = desktop.right;
#endif
		}
		else if (tgeproperties.getBooleanOrDefault("center", true)) {
#ifdef _WIN32
			const HWND hDesktop = GetDesktopWindow();
			RECT desktop;
			GetWindowRect(hDesktop, &desktop);
			mainWindowX = desktop.right / 2 - mainWindowWidth / 2;
			mainWindowY = desktop.bottom / 2 - mainWindowHeight / 2;
#endif
			mainWindowHeight = tgeproperties.getIntOrDefault("height", 400);
			mainWindowWidth = tgeproperties.getIntOrDefault("width", 400);
		}
		else {
			mainWindowHeight = tgeproperties.getIntOrDefault("height", 400);
			mainWindowWidth = tgeproperties.getIntOrDefault("width", 400);
			mainWindowX = tgeproperties.getInt("posx");
			mainWindowY = tgeproperties.getInt("posy");
		}

#ifdef _WIN32 //Windows window creation
		uint32_t size;
		GetRawInputDeviceList(NULL, &size, sizeof(RAWINPUTDEVICELIST));
		RAWINPUTDEVICELIST* list = new RAWINPUTDEVICELIST[size];
		GetRawInputDeviceList(list, &size, sizeof(RAWINPUTDEVICELIST));

		if (isDecorated) {
			//Char unicode conversation
			const char* ch = tgeproperties.getStringOrDefault("app_name", "TGEngine");
#ifdef UNICODE
			size_t conv = strlen(ch) + 1;
			wchar_t* wc = new wchar_t[conv];
			mbstowcs_s(&conv, wc, conv, ch, _TRUNCATE);
			wc[conv] = '\0';
#endif
			DWORD style = WS_CLIPSIBLINGS | WS_CAPTION;
			if (!isConsumingInput) {
				style |= WS_SYSMENU;
			}
			if (tgeproperties.getBooleanOrDefault("minimizeable", true)) {
				style |= WS_MINIMIZEBOX;
			}
			if (tgeproperties.getBooleanOrDefault("maximizable", true)) {
				style |= WS_MAXIMIZEBOX;
			}
			if (tgeproperties.getBooleanOrDefault("resizeable", true)) {
				style |= WS_SIZEBOX;
			}
			winHWND = CreateWindowEx(WS_EX_APPWINDOW, TG_MAIN_WINDOW_HANDLE, 
#ifdef UNICODE
				wc,
#else
				ch,
#endif
				style, win::mainWindowX, win::mainWindowY, win::mainWindowWidth + 16, win::mainWindowHeight + 39, nullptr, nullptr, systemModule, nullptr);
		}
		else {
			winHWND = CreateWindowEx(WS_EX_LEFT, TG_MAIN_WINDOW_HANDLE, nullptr, WS_POPUP | WS_VISIBLE | WS_SYSMENU, win::mainWindowX, win::mainWindowY, win::mainWindowWidth, win::mainWindowHeight, nullptr, nullptr, systemModule, nullptr);
		}

		TGE_ASSERT(!winHWND, TGE_CRASH("Window creation failed, sorry!", -400));

		ShowWindow(winHWND, SW_SHOW);
		UpdateWindow(winHWND);

		winHCURSOR = isConsumingInput ? NULL : LoadCursor(nullptr, IDC_ARROW);
		SetCursor(winHCURSOR);

		RAWINPUTDEVICE Rid[2];
		Rid[0].usUsagePage = 0x01;
		Rid[0].usUsage = 0x02;
		if (isConsumingInput) {
			Rid[0].dwFlags = RIDEV_NOLEGACY | RIDEV_CAPTUREMOUSE;   // adds HID mouse and also ignores legacy mouse messages if it consumes input
		}
		else {
			Rid[0].dwFlags = 0;
		}
		Rid[0].hwndTarget = winHWND;

		Rid[1].usUsagePage = 0x01;
		Rid[1].usUsage = 0x06; // Keyboard usage
		if (isConsumingInput) {
			Rid[1].dwFlags = RIDEV_NOLEGACY;   // adds HID mouse and also ignores legacy mouse messages if it consumes input
		} else {
			Rid[1].dwFlags = 0;
		}
		Rid[1].hwndTarget = winHWND;
		if (RegisterRawInputDevices(Rid, 2, sizeof(Rid[0])) == FALSE) {
			TGE_CRASH("Can not register raw input device!", -401)
		}
#endif
	}

	void createWindowClass() {
#ifdef _WIN32
		TGE_ASSERT(!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_PIN, nullptr, &systemModule), TGE_CRASH("Couldn't get window module handle!", -402));

		WNDCLASSEX  wndclass = {
			sizeof(WNDCLASSEX),
			CS_ENABLE | CS_OWNDC | CS_HREDRAW,
			WndProc,
			0,
			0,
			systemModule,
			NULL,
			NULL,
			NULL,
			NULL,
			TG_MAIN_WINDOW_HANDLE,
			NULL
		};

		TGE_ASSERT(!RegisterClassEx(&wndclass), TGE_CRASH("Couldn't register window class!", -403));
#endif
	}

	void destroyWindows() {
#if defined(_WIN32) || defined(_WIN64)
		DestroyWindow(winHWND);
#endif 
		vkDestroySurfaceKHR(instance, windowSurface, nullptr);
	}

	void createWindowSurfaces() {
#ifdef _WIN32 
		VkWin32SurfaceCreateInfoKHR surface_create_info = {
			VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
			nullptr,
			0,
			systemModule,
			winHWND
		};
		CHECKFAIL(vkCreateWin32SurfaceKHR(instance, &surface_create_info, nullptr, &windowSurface));
#endif 
	}

	void pollEvents() {
#if defined(_WIN32) || defined(_WIN64)
		MSG msg;
		while (PeekMessage(&msg, winHWND, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
#endif
	}

}