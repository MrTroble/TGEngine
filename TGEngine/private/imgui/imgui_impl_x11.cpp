// dear imgui: Platform Binding for Linux (standard X11 API for 32 and 64 bits applications)
// This needs to be used along with a Renderer (e.g. OpenGL3, Vulkan..)

// https://www.uninformativ.de/blog/postings/2017-04-02/0/POSTING-en.html
// https://stackoverflow.com/questions/27378318/c-get-string-from-clipboard-on-linux

// Implemented features:
//  [X] Platform: Clipboard support
//  [ ] Platform: Mouse cursor shape and visibility. Disable with XK_io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange'.
//  [X] Platform: Keyboard arrays indexed using
//  [ ] Platform: Gamepad support. Enabled with XK_io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad'.

#include "../../public/imgui/imgui_impl_x11.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>

#include <cstdlib>
#include <climits>
#include <ctime>
#include <cstdint>

//#include <iostream>

// CHANGELOG
// (minor and older changes stripped away, please see git history for details)
//  2022-12-08: Update to ImGUi 1.88
//  2021-15-06: Clipboard support.
//  2019-08-31: Initial X11 implementation.

struct ImGui_ImplX11_Data
{
    Display*             hDisplay;
    Window               hWindow;
    bool                 MouseTracked;
    int                  MouseButtonsDown;
    uint64_t             Time;
    uint64_t             TicksPerSecond;
    ImGuiMouseCursor     LastMouseCursor;
    bool                 HasGamepad;
    bool                 WantUpdateHasGamepad;

    Atom                 BufId;
    Atom                 PropId;
    Atom                 FmtIdUtf8String;
    Atom                 IncrId;

    char*                ClipboardBuffer;
    size_t               ClipboardBufferLength;
    size_t               ClipboardBufferSize;
    bool                 ClipboardOwned;

    ImGui_ImplX11_Data()      { memset((void*)this, 0, sizeof(*this)); }
};

// Backend data stored in io.BackendPlatformUserData to allow support for multiple Dear ImGui contexts
// It is STRONGLY preferred that you use docking branch with multi-viewports (== single Dear ImGui context + multiple windows) instead of multiple Dear ImGui contexts.
// FIXME: multi-context support is not well tested and probably dysfunctional in this backend.
// FIXME: some shared resources (mouse cursor shape, gamepad) are mishandled when using multi-context.
static ImGui_ImplX11_Data* ImGui_ImplX11_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplX11_Data*)ImGui::GetIO().BackendPlatformUserData : NULL;
}

static bool GetKeyState(Display* hDisplay, int keysym, char keys[32])
{
    int keycode = XKeysymToKeycode(hDisplay, keysym);
    return keys[keycode/8] & (1<<keycode%8);
}

static void ImGui_ImplX11_SendClipboard(XSelectionRequestEvent* sender)
{
    ImGui_ImplX11_Data* bd = ImGui_ImplX11_GetBackendData();

    XSelectionEvent event;

    XChangeProperty(bd->hDisplay, sender->requestor, sender->property, bd->FmtIdUtf8String, 8, PropModeReplace,
                    (const unsigned char*)bd->ClipboardBuffer, bd->ClipboardBufferLength);

    event.type = SelectionNotify;
    event.requestor = sender->requestor;
    event.selection = sender->selection;
    event.target = sender->target;
    event.property = sender->property;
    event.time = sender->time;

    XSendEvent(bd->hDisplay, sender->requestor, True, NoEventMask, (XEvent *)&event);
}

static void ImGui_ImplX11_SetClipboardText(void* user_data, const char* text)
{
    ImGui_ImplX11_Data* bd = ImGui_ImplX11_GetBackendData();

    bd->ClipboardBufferLength = strlen(text);
    if(bd->ClipboardBufferLength > 0)
    {
        if(bd->ClipboardBufferLength >= bd->ClipboardBufferSize)
        {
            free(bd->ClipboardBuffer);
            bd->ClipboardBuffer = (char*)malloc(sizeof(char) * bd->ClipboardBufferLength);
            bd->ClipboardBufferSize = bd->ClipboardBufferLength;
        }
        memcpy(bd->ClipboardBuffer, text, bd->ClipboardBufferLength);
    
        if(!bd->ClipboardOwned)
        {
            bd->ClipboardOwned = true;
            XSetSelectionOwner(bd->hDisplay, bd->BufId, bd->hWindow, CurrentTime);
        }
    }
}

static const char* ImGui_ImplX11_GetClipboardText(void *user_data)
{
    ImGui_ImplX11_Data* bd = ImGui_ImplX11_GetBackendData();

    XEvent event;
    char *result;
    unsigned long ressize, restail;
    int resbits;
    bool timed_out;
    time_t now;

    now = time(NULL);
    timed_out = false;
    XConvertSelection(bd->hDisplay, bd->BufId, bd->FmtIdUtf8String, bd->PropId, bd->hWindow, CurrentTime);
    do
    {
        XNextEvent(bd->hDisplay, &event);
        if(event.type == SelectionRequest)
        {// This happens when we are requesting our own buffer.
            ImGui_ImplX11_SendClipboard(&event.xselectionrequest);
            continue;
        }
        if(time(NULL) - now > 2)
        {
            timed_out = true;
            break;
        }
    } 
    while (event.type != SelectionNotify || event.xselection.selection != bd->BufId);

    bd->ClipboardBuffer[0] = '\0';
    if (!timed_out && event.xselection.property)
    {
        XGetWindowProperty(bd->hDisplay, bd->hWindow, bd->PropId, 0, LONG_MAX/4, False, AnyPropertyType, 
            &bd->FmtIdUtf8String, &resbits, &ressize, &restail, (unsigned char**)&result);

        if (bd->FmtIdUtf8String == bd->IncrId)
        {
            IM_ASSERT(0 && "Buffer is too large and INCR reading is not implemented yet.\n");
        }
        else
        {
            if(ressize > bd->ClipboardBufferSize)
            {
                free(bd->ClipboardBuffer);
                bd->ClipboardBufferSize = ressize + 1;
                bd->ClipboardBuffer = (char*)malloc(sizeof(char) * bd->ClipboardBufferSize);
            }
            memcpy(bd->ClipboardBuffer, result, ressize);
            bd->ClipboardBuffer[ressize] = '\0';
        }

        XFree(result);
    }

    return bd->ClipboardBuffer;
}

bool    ImGui_ImplX11_Init(void *display, void *window)
{
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.BackendPlatformUserData == NULL && "Already initialized a platform backend!");

    // Setup backend capabilities flags
    ImGui_ImplX11_Data* bd = IM_NEW(ImGui_ImplX11_Data)();
    io.BackendPlatformUserData = (void*)bd;
    io.BackendPlatformName = "imgui_impl_X11";
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;         // We can honor GetMouseCursor() values (optional)
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;          // We can honor io.WantSetMousePos requests (optional, rarely used)
    io.GetClipboardTextFn = ImGui_ImplX11_GetClipboardText;
    io.SetClipboardTextFn = ImGui_ImplX11_SetClipboardText;

    timespec ts, tsres;
    clock_getres(CLOCK_MONOTONIC_RAW, &tsres);
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);

    bd->hDisplay = reinterpret_cast<Display*>(display);
    bd->hWindow = reinterpret_cast<Window>(window);
    bd->WantUpdateHasGamepad = true;
    bd->TicksPerSecond = 1000000000.0f / (static_cast<uint64_t>(tsres.tv_nsec) + static_cast<uint64_t>(tsres.tv_sec)*1000000000);
    bd->Time = static_cast<uint64_t>(ts.tv_nsec) + static_cast<uint64_t>(ts.tv_sec)*1000000000;
    bd->LastMouseCursor = ImGuiMouseCursor_COUNT;
    bd->ClipboardBuffer = (char*)malloc(sizeof(char) * 256);
    bd->ClipboardBufferSize = 256;

    bd->BufId           = XInternAtom(bd->hDisplay, "CLIPBOARD", False);
    bd->PropId          = XInternAtom(bd->hDisplay, "XSEL_DATA", False);
    bd->FmtIdUtf8String = XInternAtom(bd->hDisplay, "UTF8_STRING", False);
    bd->IncrId          = XInternAtom(bd->hDisplay, "INCR", False);

    return true;
}

void    ImGui_ImplX11_Shutdown()
{
    ImGui_ImplX11_Data* bd = ImGui_ImplX11_GetBackendData();
    IM_ASSERT(bd != NULL && "No platform backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();

    io.GetClipboardTextFn = NULL;
    io.SetClipboardTextFn = NULL;
    io.BackendPlatformName = NULL;
    io.BackendPlatformUserData = NULL;

    free(bd->ClipboardBuffer);
    IM_DELETE(bd);
}

static bool ImGui_ImplX11_UpdateMouseCursor()
{
    return true;
}

static void ImGui_ImplX11_AddKeyEvent(ImGuiKey key, bool down, int native_keycode, int native_scancode = -1)
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(key, down);
    io.SetKeyEventNativeData(key, native_keycode, native_scancode); // To support legacy indexing (<1.87 user code)
    IM_UNUSED(native_scancode);
}

static void ImGui_ImplX11_UpdateKeyModifiers()
{
    ImGui_ImplX11_Data* bd = ImGui_ImplX11_GetBackendData();
    ImGuiIO& io = ImGui::GetIO();

    bool k;
    char szKey[32];
    XQueryKeymap(bd->hDisplay, szKey);

    io.AddKeyEvent(ImGuiKey_ModCtrl , GetKeyState(bd->hDisplay, XK_Control_L, szKey) || GetKeyState(bd->hDisplay, XK_Control_R, szKey));
    io.AddKeyEvent(ImGuiKey_ModShift, GetKeyState(bd->hDisplay, XK_Shift_L  , szKey) || GetKeyState(bd->hDisplay, XK_Shift_R  , szKey));
    io.AddKeyEvent(ImGuiKey_ModAlt  , GetKeyState(bd->hDisplay, XK_Alt_L    , szKey) || GetKeyState(bd->hDisplay, XK_Alt_R    , szKey));
    io.AddKeyEvent(ImGuiKey_ModSuper, GetKeyState(bd->hDisplay, XK_Super_L  , szKey) || GetKeyState(bd->hDisplay, XK_Super_R  , szKey));
}

static void ImGui_ImplX11_UpdateMouseData()
{
    ImGui_ImplX11_Data* bd = ImGui_ImplX11_GetBackendData();
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(bd->hWindow != 0);

    const bool is_app_focused = true;//(::GetForegroundWindow() == bd->hWnd);
    if (is_app_focused)
    {
        // (Optional) Set OS mouse position from Dear ImGui if requested (rarely used, only when ImGuiConfigFlags_NavEnableSetMousePos is enabled by user)
        if (io.WantSetMousePos)
        {
            //POINT pos = { (int)io.MousePos.x, (int)io.MousePos.y };
            //if (::ClientToScreen(bd->hWnd, &pos))
            //    ::SetCursorPos(pos.x, pos.y);
        }

        // (Optional) Fallback to provide mouse position when focused (WM_MOUSEMOVE already provides this when hovered or captured)
        if (!io.WantSetMousePos && !bd->MouseTracked)
        {
            Window unused_window;
            int rx, ry, x, y;
            unsigned int mask;

            XQueryPointer(bd->hDisplay, bd->hWindow, &unused_window, &unused_window, &rx, &ry, &x, &y, &mask);

            io.AddMousePosEvent((float)x, (float)y);
        }
    }
}

// Gamepad navigation mapping
static void ImGui_ImplX11_UpdateGamepads()
{
    // TODO: support linux gamepad ?
#ifndef IMGUI_IMPL_X11_DISABLE_GAMEPAD
#endif
}

bool    ImGui_ImplX11_NewFrame()
{
    ImGui_ImplX11_Data* bd = ImGui_ImplX11_GetBackendData();
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.Fonts->IsBuilt() && "Font atlas not built! It is generally built by the renderer back-end. Missing call to renderer _NewFrame() function? e.g. ImGui_ImplOpenGL3_NewFrame().");

    unsigned int width, height;
    Window unused_window;
    int unused_int;
    unsigned int unused_unsigned_int;

    XGetGeometry(bd->hDisplay, bd->hWindow, &unused_window, &unused_int, &unused_int, &width, &height, &unused_unsigned_int, &unused_unsigned_int);

    io.DisplaySize.x = width;
    io.DisplaySize.y = height;

    timespec ts, tsres;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);

    uint64_t current_time = static_cast<uint64_t>(ts.tv_nsec) + static_cast<uint64_t>(ts.tv_sec)*1000000000;

    io.DeltaTime = (float)(current_time - bd->Time) / bd->TicksPerSecond;
    bd->Time = current_time;

    // Update OS mouse position
    ImGui_ImplX11_UpdateMouseData();

    // Update game controllers (if enabled and available)
    ImGui_ImplX11_UpdateGamepads();
    return true;
}

// Map XK_xxx to ImGuiKey_xxx.
static ImGuiKey ImGui_ImplX11_VirtualKeyToImGuiKey(uint32_t param)
{
    switch (param)
    {
        case XK_Tab   :  return ImGuiKey_Tab;
        case XK_ISO_Left_Tab:  return ImGuiKey_Tab;
        case XK_Left  : return ImGuiKey_LeftArrow;
        case XK_Right : return ImGuiKey_RightArrow;
        case XK_Up    : return ImGuiKey_UpArrow;
        case XK_Down  : return ImGuiKey_DownArrow;
        case XK_Prior : return ImGuiKey_PageUp;
        case XK_Next  : return ImGuiKey_PageDown;
        case XK_Home  : return ImGuiKey_Home;
        case XK_End   : return ImGuiKey_End;
        case XK_Insert: return ImGuiKey_Insert;
        case XK_Delete: return ImGuiKey_Delete;
        case XK_BackSpace: return ImGuiKey_Backspace;
        case XK_space : return ImGuiKey_Space;
        case XK_Return: return ImGuiKey_Enter;
        case XK_Escape: return ImGuiKey_Escape;
        case XK_apostrophe: return ImGuiKey_Apostrophe;
        case XK_comma: return ImGuiKey_Comma;
        case XK_minus: return ImGuiKey_Minus;
        case XK_period: return ImGuiKey_Period;
        case XK_slash: return ImGuiKey_Slash;
        case XK_semicolon: return ImGuiKey_Semicolon;
        case XK_equal: return ImGuiKey_Equal;
        case XK_bracketleft: return ImGuiKey_LeftBracket;
        case XK_backslash: return ImGuiKey_Backslash;
        case XK_bracketright: return ImGuiKey_RightBracket;
        case XK_grave: return ImGuiKey_GraveAccent;
        case XK_Caps_Lock: return ImGuiKey_CapsLock;
        case XK_Scroll_Lock: return ImGuiKey_ScrollLock;
        case XK_Num_Lock: return ImGuiKey_NumLock;
        case XK_Print: return ImGuiKey_PrintScreen;
        case XK_Pause: return ImGuiKey_Pause;
        case XK_KP_Insert   : case XK_KP_0: return ImGuiKey_Keypad0;
        case XK_KP_End      : case XK_KP_1: return ImGuiKey_Keypad1;
        case XK_KP_Down     : case XK_KP_2: return ImGuiKey_Keypad2;
        case XK_KP_Page_Down: case XK_KP_3: return ImGuiKey_Keypad3;
        case XK_KP_Left     : case XK_KP_4: return ImGuiKey_Keypad4;
        case XK_KP_Begin    : case XK_KP_5: return ImGuiKey_Keypad5;
        case XK_KP_Right    : case XK_KP_6: return ImGuiKey_Keypad6;
        case XK_KP_Home     : case XK_KP_7: return ImGuiKey_Keypad7;
        case XK_KP_Up       : case XK_KP_8: return ImGuiKey_Keypad8;
        case XK_KP_Page_Up  : case XK_KP_9: return ImGuiKey_Keypad9;
        case XK_KP_Decimal: return ImGuiKey_KeypadDecimal;
        case XK_KP_Divide: return ImGuiKey_KeypadDivide;
        case XK_KP_Multiply: return ImGuiKey_KeypadMultiply;
        case XK_KP_Subtract: return ImGuiKey_KeypadSubtract;
        case XK_KP_Add: return ImGuiKey_KeypadAdd;
        case XK_KP_Enter: return ImGuiKey_KeypadEnter;
        case XK_Shift_L: return ImGuiKey_LeftShift;
        case XK_Control_L: return ImGuiKey_LeftCtrl;
        case XK_Alt_L: return ImGuiKey_LeftAlt;
        case XK_Super_L: return ImGuiKey_LeftSuper;
        case XK_Shift_R: return ImGuiKey_RightShift;
        case XK_Control_R: return ImGuiKey_RightCtrl;
        case XK_Alt_R: return ImGuiKey_RightAlt;
        case XK_Super_R: return ImGuiKey_RightSuper;
        //case XK_APPS: return ImGuiKey_Menu;
        case XK_0 : return ImGuiKey_0;
        case XK_1 : return ImGuiKey_1;
        case XK_2 : return ImGuiKey_2;
        case XK_3 : return ImGuiKey_3;
        case XK_4 : return ImGuiKey_4;
        case XK_5 : return ImGuiKey_5;
        case XK_6 : return ImGuiKey_6;
        case XK_7 : return ImGuiKey_7;
        case XK_8 : return ImGuiKey_8;
        case XK_9 : return ImGuiKey_9;
        case XK_a : case XK_A : return ImGuiKey_A;
        case XK_b : case XK_B : return ImGuiKey_B;
        case XK_c : case XK_C : return ImGuiKey_C;
        case XK_d : case XK_D : return ImGuiKey_D;
        case XK_e : case XK_E : return ImGuiKey_E;
        case XK_f : case XK_F : return ImGuiKey_F;
        case XK_g : case XK_G : return ImGuiKey_G;
        case XK_h : case XK_H : return ImGuiKey_H;
        case XK_i : case XK_I : return ImGuiKey_I;
        case XK_j : case XK_J : return ImGuiKey_J;
        case XK_k : case XK_K : return ImGuiKey_K;
        case XK_l : case XK_L : return ImGuiKey_L;
        case XK_m : case XK_M : return ImGuiKey_M;
        case XK_n : case XK_N : return ImGuiKey_N;
        case XK_o : case XK_O : return ImGuiKey_O;
        case XK_p : case XK_P : return ImGuiKey_P;
        case XK_q : case XK_Q : return ImGuiKey_Q;
        case XK_r : case XK_R : return ImGuiKey_R;
        case XK_s : case XK_S : return ImGuiKey_S;
        case XK_t : case XK_T : return ImGuiKey_T;
        case XK_u : case XK_U : return ImGuiKey_U;
        case XK_v : case XK_V : return ImGuiKey_V;
        case XK_w : case XK_W : return ImGuiKey_W;
        case XK_x : case XK_X : return ImGuiKey_X;
        case XK_y : case XK_Y : return ImGuiKey_Y;
        case XK_z : case XK_Z : return ImGuiKey_Z;
        case XK_F1: return ImGuiKey_F1;
        case XK_F2: return ImGuiKey_F2;
        case XK_F3: return ImGuiKey_F3;
        case XK_F4: return ImGuiKey_F4;
        case XK_F5: return ImGuiKey_F5;
        case XK_F6: return ImGuiKey_F6;
        case XK_F7: return ImGuiKey_F7;
        case XK_F8: return ImGuiKey_F8;
        case XK_F9: return ImGuiKey_F9;
        case XK_F10: return ImGuiKey_F10;
        case XK_F11: return ImGuiKey_F11;
        case XK_F12: return ImGuiKey_F12;
        default: return ImGuiKey_None;
    }
}

// Process X11 mouse/keyboard inputs.
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
IMGUI_IMPL_API int ImGui_ImplX11_EventHandler(XEvent &event)
{
    ImGui_ImplX11_Data* bd = ImGui_ImplX11_GetBackendData();
    if (ImGui::GetCurrentContext() == NULL)
        return 0;

    ImGuiIO& io = ImGui::GetIO();
    switch (event.type)
    {
        case ButtonPress:
        case ButtonRelease:
        {
            const bool is_key_down = event.type == ButtonPress;
            switch(event.xbutton.button)
            {
                case Button1:
                    io.AddMouseButtonEvent(ImGuiMouseButton_Left, is_key_down);
                    break;

                case Button2:
                    io.AddMouseButtonEvent(ImGuiMouseButton_Middle, is_key_down);
                    break;

                case Button3:
                    io.AddMouseButtonEvent(ImGuiMouseButton_Right, is_key_down);
                    break;

                case Button4: // Mouse wheel up
                    if( is_key_down )
                        io.AddMouseWheelEvent(0, 1);
                    break;

                case Button5: // Mouse wheel down
                    if( is_key_down )
                        io.AddMouseWheelEvent(0, -1);
                    break;

            }
        }
        return 0;

        case KeyPress:
        case KeyRelease:
        {
            const bool is_key_down = event.type == KeyPress;
            int vk = XkbKeycodeToKeysym(bd->hDisplay, event.xkey.keycode, 0, event.xkey.state & ShiftMask ? 1 : 0);

			//int keysyms_per_keycode_return;
    		//KeySym *keysym = XGetKeyboardMapping(bd->hDisplay,
        	//					event.xkey.keycode,
        	//					1,
        	//					&keysyms_per_keycode_return);

			//int vk = *keysym;

            //XFree(keysym);

            if( vk >= 0x1000100 && vk <= 0x110ffff )
            {
                if (is_key_down)
                    io.AddInputCharacterUTF16(vk);
            }
            else
            {
                // Submit modifiers
                ImGui_ImplX11_UpdateKeyModifiers();

                const ImGuiKey key = ImGui_ImplX11_VirtualKeyToImGuiKey(vk);
                if (key == XK_Shift_L)
                {
                // Some keys are wrapped with XK_Shift_L
                // XK_Shift_L Pressed
                // XK_KP_0 Pressed // real key press
                // XK_Shift_L Released
                // ... Wait some time ...
                // XK_KP_Insert Released
                // So ignore here XK_Shilt_L
                }
                else
                {                
                	if (key != ImGuiKey_None)
                    	ImGui_ImplX11_AddKeyEvent(key, is_key_down, vk, event.xkey.keycode);

                	if (is_key_down && vk < 256)
                    	io.AddInputCharacter(vk);
                }
            }

            //
            return 0;
        }

        case FocusOut:
            bd->MouseTracked = false;
            io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
            return 0;


        case SelectionClear:
            bd->ClipboardOwned = false;
            return 0;

        case SelectionRequest:
        {
            ImGui_ImplX11_SendClipboard(&event.xselectionrequest);
        }
    }
    return 0;
}

