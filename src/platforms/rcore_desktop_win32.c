/**********************************************************************************************
*
*   rcore_desktop_win32 - Functions to manage window, graphics device and inputs
*
*   PLATFORM: DESKTOP: WIN32
*       - Windows (Win32, Win64)
*
*   LIMITATIONS:
*       - Initial development stage, lot of functionality missing
*       - No support for MOUSE_BUTTON_FORWARD/MOUSE_BUTTON_BACK
*
*   POSSIBLE IMPROVEMENTS:
*       - Improvement 01
*       - Improvement 02
*
*   CONFIGURATION:
*       #define RCORE_PLATFORM_CUSTOM_FLAG
*           Custom flag for rcore on target platform -not used-
*
*   DEPENDENCIES:
*       - Win32 API (windows.h)
*
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2013-2026 Ramon Santamaria (@raysan5) and contributors
*
*   This software is provided "as-is", without any express or implied warranty. In no event
*   will the authors be held liable for any damages arising from the use of this software.
*
*   Permission is granted to anyone to use this software for any purpose, including commercial
*   applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*     1. The origin of this software must not be misrepresented; you must not claim that you
*     wrote the original software. If you use this software in a product, an acknowledgment
*     in the product documentation would be appreciated but is not required.
*
*     2. Altered source versions must be plainly marked as such, and must not be misrepresented
*     as being the original software.
*
*     3. This notice may not be removed or altered from any source distribution.
*
**********************************************************************************************/

// Move windows.h symbols to new names to avoid redefining the same names as raylib
#define CloseWindow CloseWindowWin32
#define Rectangle RectangleWin32
#define ShowCursor ShowCursorWin32
#define DrawTextA DrawTextAWin32
#define DrawTextW DrawTextWin32
#define DrawTextExA DrawTextExAWin32
#define DrawTextExW DrawTextExWin32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#undef CloseWindow      // raylib symbol collision
#undef Rectangle        // raylib symbol collision
#undef ShowCursor       // raylib symbol collision
#undef LoadImage        // raylib symbol collision
#undef DrawText         // raylib symbol collision
#undef DrawTextA
#undef DrawTextW
#undef DrawTextEx       // raylib symbol collision
#undef DrawTextExA
#undef DrawTextExW

#include <windowsx.h>

// Avoid including shellscalingapi.h by defining these two enums directly
#ifndef DPI_ENUMS_DECLARED
typedef enum PROCESS_DPI_AWARENESS {
    PROCESS_DPI_UNAWARE = 0,
    PROCESS_SYSTEM_DPI_AWARE = 1,
    PROCESS_PER_MONITOR_DPI_AWARE = 2
} PROCESS_DPI_AWARENESS;

typedef enum MONITOR_DPI_TYPE {
    MDT_EFFECTIVE_DPI = 0,
    MDT_ANGULAR_DPI = 1,
    MDT_RAW_DPI = 2,
    MDT_DEFAULT = MDT_EFFECTIVE_DPI
} MONITOR_DPI_TYPE;

#define DPI_ENUMS_DECLARED
#endif

#include <malloc.h>          // Required for alloca()

#if !defined(GRAPHICS_API_OPENGL_SOFTWARE)
    #include <GL/gl.h>
#endif

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

// NOTE: appScreenSize is the last screen size requested by the app,
// the backend must keep the client area this size (after DPI scaling is applied)
// when the window isn't fullscreen/maximized/minimized
typedef struct {
    HWND hwnd;              // Window handler
    HDC hdc;                // Graphic context handler
    HGLRC glContext;        // OpenGL context handler

    HICON largeIcon;        // Large icon typically used for the taskbar
    HICON smallIcon;        // Small icon typically used for the window's title bar

    // Software renderer variables
    HDC hdcmem;             // Memory graphic context handler
    HBITMAP hbitmap;        // GDI bitmap handler
    unsigned int *pixels;   // Pointer to pixel data buffer (BGRA format)

    int previousRenderWidth;
    int previousRenderHeight;
    int appScreenWidth;
    int appScreenHeight;
    unsigned int desiredFlags;

    bool perMonitorDPIAwareV2; // Flag to check DPI-awareness level. This is used mainly to distinguish between per-monitor DPI awareness v1 and v2

    LARGE_INTEGER timerFrequency;

    char *clipboardText;
} PlatformData;

// Define WGL function pointer types (no wglext.h needed)
typedef HGLRC (WINAPI *PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int *);
typedef BOOL (WINAPI *PFNWGLCHOOSEPIXELFORMATARBPROC)(HDC, const int *, const FLOAT *, UINT, int *, UINT *);
typedef BOOL (WINAPI *PFNWGLSWAPINTERVALEXTPROC)(int);
typedef const char *(WINAPI *PFNWGLGETEXTENSIONSSTRINGARBPROC)(HDC hdc);

// Define Win32 function pointer types
typedef UINT (WINAPI *GetDpiForWindowProc)(HWND hwnd);
typedef BOOL (WINAPI *AdjustWindowRectExForDpiProc)(LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle, UINT dpi);
typedef BOOL (WINAPI *SetProcessDpiAwarenessContextProc)(HANDLE value);
typedef HRESULT (WINAPI *SetProcessDpiAwarenessProc)(PROCESS_DPI_AWARENESS value);
typedef BOOL (WINAPI *SetProcessDPIAwareProc)(void);
typedef HRESULT (WINAPI *GetDpiForMonitorProc)(HMONITOR monitor, MONITOR_DPI_TYPE dpiType, UINT *dpiX, UINT *dpiY);
typedef HANDLE (WINAPI *GetThreadDpiAwarenessContextProc)(void);
typedef BOOL (WINAPI *AreDpiAwarenessContextsEqualProc)(HANDLE a, HANDLE b);

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
extern CoreData CORE;                   // Global CORE state context

static PlatformData platform = {};   // Platform specific data

// Required WGL functions
static PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = nullptr;
static PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = nullptr;
static PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = nullptr;
static PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB = nullptr;

// Win32 functions that need to be dynamically loaded to support older versions of Windows
// Before calling any of these functions, they should be null-checked
static GetDpiForWindowProc pGetDpiForWindow = nullptr;
static AdjustWindowRectExForDpiProc pAdjustWindowRectExForDpi = nullptr;
static SetProcessDpiAwarenessContextProc pSetProcessDpiAwarenessContext = nullptr;
static SetProcessDpiAwarenessProc pSetProcessDpiAwareness = nullptr;
static SetProcessDPIAwareProc pSetProcessDPIAware = nullptr;
static GetDpiForMonitorProc pGetDpiForMonitor = nullptr;
static GetThreadDpiAwarenessContextProc pGetThreadDpiAwarenessContext = nullptr;
static AreDpiAwarenessContextsEqualProc pAreDpiAwarenessContextsEqual = nullptr;

static HMODULE shcore = nullptr; // shcore.dll (Windows 8.1 and early versions of Windows 10): loaded once, NULL on other versions

// --------------------------------------------------------------------------------
// This part of the file contains pure functions that never access global state
// This distinction helps keep the backend maintainable as the inputs and outputs
// of every function called in this section can be fully derived from the call-site alone
// --------------------------------------------------------------------------------

// Prevent any code in this part of the file from accessing the global CORE state
#define CORE DONT_USE_CORE_HERE

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
#define A_TO_W_ALLOCA(outWstr, inAnsi)   do {                   \
    size_t outLen = AToWLen(inAnsi);                            \
    outWstr = static_cast<WCHAR *>(alloca(sizeof(WCHAR)*(outLen + 1)));      \
    AToWCopy(inAnsi, outWstr, outLen);                          \
    outWstr[outLen] = 0;                                        \
} while (0)

#define STYLE_MASK_ALL          0xffffffff
#define STYLE_MASK_READONLY     (WS_MINIMIZE | WS_MAXIMIZE)
#define STYLE_MASK_WRITABLE     (~STYLE_MASK_READONLY)

#define STYLE_FLAGS_RESIZABLE   (WS_THICKFRAME | WS_MAXIMIZEBOX)

#define STYLE_FLAGS_UNDECORATED_OFF     (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX)
#define STYLE_FLAGS_UNDECORATED_ON      WS_POPUP

#define WINDOW_STYLE_EX         0

#define CLASS_NAME              L"raylibWindow"

#define FLAG_MASK_OPTIONAL      (FLAG_VSYNC_HINT | FLAG_WINDOW_UNFOCUSED)
#define FLAG_MASK_REQUIRED      ~(FLAG_MASK_OPTIONAL)

// Flags that have no operations to perform during an update
#define FLAG_MASK_NO_UPDATE     (FLAG_WINDOW_HIGHDPI | MSAA_FLAGS)

// Lowest extended-key scancode, start of keyboardKeyFromExtendedScancode[]
#define EXTENDED_KEY_OFFSET     0x1C

#define WGL_DRAW_TO_WINDOW_ARB              0x2001
#define WGL_ACCELERATION_ARB                0x2003
#define WGL_SUPPORT_OPENGL_ARB              0x2010
#define WGL_DOUBLE_BUFFER_ARB               0x2011
#define WGL_PIXEL_TYPE_ARB                  0x2013
#define WGL_COLOR_BITS_ARB                  0x2014
#define WGL_RED_BITS_ARB                    0x2015
#define WGL_RED_SHIFT_ARB                   0x2016
#define WGL_GREEN_BITS_ARB                  0x2017
#define WGL_GREEN_SHIFT_ARB                 0x2018
#define WGL_BLUE_BITS_ARB                   0x2019
#define WGL_BLUE_SHIFT_ARB                  0x201a
#define WGL_ALPHA_BITS_ARB                  0x201b
#define WGL_ALPHA_SHIFT_ARB                 0x201c
#define WGL_DEPTH_BITS_ARB                  0x2022
#define WGL_STENCIL_BITS_ARB                0x2023
#define WGL_TYPE_RGBA_ARB                   0x202b

// Context acceleration types
#define WGL_NO_ACCELERATION_ARB             0x2025      // OpenGL 1.1 GDI software rasterizer
#define WGL_GENERIC_ACCELERATION_ARB        0x2026
#define WGL_FULL_ACCELERATION_ARB           0x2027      // OpenGL hardware-accelerated, using GPU-drivers provided by vendor

// WGL_ARB_multisample extension supported
#define WGL_SAMPLE_BUFFERS_ARB              0x2041      // Multisampling: 1 if multisample buffers are supported
#define WGL_SAMPLES_ARB                     0x2042      // Multisampling: Number of samples per pixel (4, 8, 16)

// WGL_ARB_framebuffer_sRGB extension supported
#define WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB    0x20a9      // GL_TRUE if the framebuffer can do sRGB conversion

#define WGL_NUMBER_PIXEL_FORMATS_ARB        0x2000
#define WGL_CONTEXT_MAJOR_VERSION_ARB       0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB       0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB        0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB    0x00000001
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#define WGL_CONTEXT_ES_PROFILE_BIT_EXT        0x00000004
#define WGL_CONTEXT_ES2_PROFILE_BIT_EXT        0x00000004

// Polyfill macro definitions and enums for older Windows SDKs
#ifndef WM_DPICHANGED
    #define WM_DPICHANGED                   0x02E0
#endif

#ifndef WM_GETDPISCALEDSIZE
    #define WM_GETDPISCALEDSIZE             0x02E4
#endif

#ifndef WM_MOUSEHWHEEL
    #define WM_MOUSEHWHEEL                  0x020E
#endif

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE
    #define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE (static_cast<HANDLE>(3))
#endif
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
    #define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 (static_cast<HANDLE>(4))
#endif

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// Maximize-minimize request types
typedef enum {
    MIZED_NONE,
    MIZED_MIN,
    MIZED_MAX
} Mized;

// Flag operations
// NOTE: Some ops need to be deferred
typedef struct {
    DWORD set;
    DWORD clear;
} FlagsOp;

// Monitor info type
typedef struct {
    HMONITOR needle;
    int index;
    int matchIndex;
    RECT rect;
} MonitorInfo;

//----------------------------------------------------------------------------------
// Global Const Variables Definition
//----------------------------------------------------------------------------------
// Maps scancodes to Raylib keys
static constexpr unsigned short keyboardKeyFromScancode[] = {
    [0x01] = KEY_ESCAPE,
    [0x02] = KEY_ONE,
    [0x03] = KEY_TWO,
    [0x04] = KEY_THREE,
    [0x05] = KEY_FOUR,
    [0x06] = KEY_FIVE,
    [0x07] = KEY_SIX,
    [0x08] = KEY_SEVEN,
    [0x09] = KEY_EIGHT,
    [0x0A] = KEY_NINE,
    [0x0B] = KEY_ZERO,
    [0x0C] = KEY_MINUS,
    [0x0D] = KEY_EQUAL,
    [0x0E] = KEY_BACKSPACE,
    [0x0F] = KEY_TAB,
    [0x10] = KEY_Q,
    [0x11] = KEY_W,
    [0x12] = KEY_E,
    [0x13] = KEY_R,
    [0x14] = KEY_T,
    [0x15] = KEY_Y,
    [0x16] = KEY_U,
    [0x17] = KEY_I,
    [0x18] = KEY_O,
    [0x19] = KEY_P,
    [0x1A] = KEY_LEFT_BRACKET,
    [0x1B] = KEY_RIGHT_BRACKET,
    [0x1C] = KEY_ENTER,
    [0x1D] = KEY_LEFT_CONTROL,
    [0x1E] = KEY_A,
    [0x1F] = KEY_S,
    [0x20] = KEY_D,
    [0x21] = KEY_F,
    [0x22] = KEY_G,
    [0x23] = KEY_H,
    [0x24] = KEY_J,
    [0x25] = KEY_K,
    [0x26] = KEY_L,
    [0x27] = KEY_SEMICOLON,
    [0x28] = KEY_APOSTROPHE,
    [0x29] = KEY_GRAVE,
    [0x2A] = KEY_LEFT_SHIFT,
    [0x2B] = KEY_BACKSLASH,
    [0x2C] = KEY_Z,
    [0x2D] = KEY_X,
    [0x2E] = KEY_C,
    [0x2F] = KEY_V,
    [0x30] = KEY_B,
    [0x31] = KEY_N,
    [0x32] = KEY_M,
    [0x33] = KEY_COMMA,
    [0x34] = KEY_PERIOD,
    [0x35] = KEY_SLASH,
    [0x36] = KEY_RIGHT_SHIFT,
    [0x37] = KEY_KP_MULTIPLY,
    [0x38] = KEY_LEFT_ALT,
    [0x39] = KEY_SPACE,
    [0x3A] = KEY_CAPS_LOCK,
    [0x3B] = KEY_F1,
    [0x3C] = KEY_F2,
    [0x3D] = KEY_F3,
    [0x3E] = KEY_F4,
    [0x3F] = KEY_F5,
    [0x40] = KEY_F6,
    [0x41] = KEY_F7,
    [0x42] = KEY_F8,
    [0x43] = KEY_F9,
    [0x44] = KEY_F10,
    [0x45] = KEY_PAUSE,
    [0x46] = KEY_SCROLL_LOCK,
    [0x47] = KEY_KP_7,
    [0x48] = KEY_KP_8,
    [0x49] = KEY_KP_9,
    [0x4A] = KEY_KP_SUBTRACT,
    [0x4B] = KEY_KP_4,
    [0x4C] = KEY_KP_5,
    [0x4D] = KEY_KP_6,
    [0x4E] = KEY_KP_ADD,
    [0x4F] = KEY_KP_1,
    [0x50] = KEY_KP_2,
    [0x51] = KEY_KP_3,
    [0x52] = KEY_KP_0,
    [0x53] = KEY_KP_DECIMAL,
    [0x57] = KEY_F11,
    [0x58] = KEY_F12,
    [0x59] = KEY_KP_EQUAL,

    // Duplicate scancodes
    [0x54] = KEY_PRINT_SCREEN,      // Alt+PrtSc sends the SysRq scancode
};

// Maps scancodes with extended-flag set to Raylib keys; lowest key is 0x1C
static constexpr unsigned short keyboardKeyFromExtendedScancode[] = {
    [0x1C - EXTENDED_KEY_OFFSET] = KEY_KP_ENTER,
    [0x1D - EXTENDED_KEY_OFFSET] = KEY_RIGHT_CONTROL,
    [0x35 - EXTENDED_KEY_OFFSET] = KEY_KP_DIVIDE,
    [0x37 - EXTENDED_KEY_OFFSET] = KEY_PRINT_SCREEN,
    [0x38 - EXTENDED_KEY_OFFSET] = KEY_RIGHT_ALT,
    [0x45 - EXTENDED_KEY_OFFSET] = KEY_NUM_LOCK,
    [0x47 - EXTENDED_KEY_OFFSET] = KEY_HOME,
    [0x48 - EXTENDED_KEY_OFFSET] = KEY_UP,
    [0x49 - EXTENDED_KEY_OFFSET] = KEY_PAGE_UP,
    [0x4B - EXTENDED_KEY_OFFSET] = KEY_LEFT,
    [0x4D - EXTENDED_KEY_OFFSET] = KEY_RIGHT,
    [0x4F - EXTENDED_KEY_OFFSET] = KEY_END,
    [0x50 - EXTENDED_KEY_OFFSET] = KEY_DOWN,
    [0x51 - EXTENDED_KEY_OFFSET] = KEY_PAGE_DOWN,
    [0x52 - EXTENDED_KEY_OFFSET] = KEY_INSERT,
    [0x53 - EXTENDED_KEY_OFFSET] = KEY_DELETE,
    [0x5B - EXTENDED_KEY_OFFSET] = KEY_LEFT_SUPER,
    [0x5C - EXTENDED_KEY_OFFSET] = KEY_RIGHT_SUPER,
    [0x5D - EXTENDED_KEY_OFFSET] = KEY_KB_MENU,

    // Duplicate scancodes
    [0x46 - EXTENDED_KEY_OFFSET] = KEY_PAUSE,           // Ctrl+Pause sends the Break scancode
    [0x36 - EXTENDED_KEY_OFFSET] = KEY_RIGHT_SHIFT,     // Some keyboards set the extended bit on Right Shift
};

//----------------------------------------------------------------------------------
// Module Internal Functions Declaration
//----------------------------------------------------------------------------------
// Get ASCII to WCHAR length
static size_t AToWLen(const char *ascii)
{
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, ascii, -1, nullptr, 0);

    if (sizeNeeded < 0) TRACELOG(LOG_ERROR, "WIN32: Failed to calculate wide length [ERROR: %u]", GetLastError());

    return sizeNeeded;
}

// Copy ASCII to WCHAR string
static void AToWCopy(const char *ascii, wchar_t *outPtr, size_t outLen)
{
    int size = MultiByteToWideChar(CP_UTF8, 0, ascii, -1, outPtr, static_cast<int>(outLen));
    if (size != outLen) TRACELOG(LOG_WARNING, "WIN32: Failed to convert %i UTF-8 chars to WCHAR, converted %i chars", outLen, size);
}

// Dynamically load Windows functions that may not be available on older versions
static void LoadWin32FunctionPointers(void)
{
    HMODULE user32 = GetModuleHandleW(L"user32.dll");

    pGetDpiForWindow = (GetDpiForWindowProc)GetProcAddress(user32, "GetDpiForWindow");
    pAdjustWindowRectExForDpi = (AdjustWindowRectExForDpiProc)GetProcAddress(user32, "AdjustWindowRectExForDpi");
    pSetProcessDpiAwarenessContext = (SetProcessDpiAwarenessContextProc)GetProcAddress(user32, "SetProcessDpiAwarenessContext");

    pSetProcessDPIAware = (SetProcessDPIAwareProc)GetProcAddress(user32, "SetProcessDPIAware");
    pGetThreadDpiAwarenessContext = (GetThreadDpiAwarenessContextProc)GetProcAddress(user32, "GetThreadDpiAwarenessContext");
    pAreDpiAwarenessContextsEqual = (AreDpiAwarenessContextsEqualProc)GetProcAddress(user32, "AreDpiAwarenessContextsEqual");

    // SetProcessDpiAwarenessContext (Windows 10 v1703) supersedes SetProcessDpiAwareness (Windows 8.1)
    // We can avoid loading the shcore.dll if the newer DPI functions are available
    if (pSetProcessDpiAwarenessContext == nullptr)
    {
        shcore = LoadLibraryW(L"shcore.dll");
        if (shcore != nullptr)
        {
            pSetProcessDpiAwareness = (SetProcessDpiAwarenessProc)GetProcAddress(shcore, "SetProcessDpiAwareness");
            pGetDpiForMonitor = (GetDpiForMonitorProc)GetProcAddress(shcore, "GetDpiForMonitor");
        }
    }
}

// GetDpiForWindow with a fallback for older versions of Windows
static UINT GetDpiForWindowCompat(HWND hwnd)
{
    UINT result = 0;

    if (pGetDpiForWindow != nullptr)
    {
        // This API requires Windows 10 v1607 or newer
        // Returns 0 on failure
        result = pGetDpiForWindow(hwnd);
    }
    else if (pGetDpiForMonitor != nullptr)
    {
        // Windows 8.1 and early versions of Windows 10
        // Get the DPI based on which monitor the window is on
        HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        UINT dpiX = 0;
        UINT dpiY = 0;
        HRESULT hr = pGetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
        if (hr == S_OK) result = dpiX;
    }

    if (result == 0)
    {
        // If running on older versions of Windows or if the modern API calls fail: fallback to system-wide DPI
        // We can call GetDC without a corresponding ReleaseDC because the window was created with CS_OWNDC
        result = GetDeviceCaps(GetDC(hwnd), LOGPIXELSX);
    }

    return result;
}

// AdjustWindowRectExForDpi with a fallback for older versions of Windows
static BOOL AdjustWindowRectExForDpiCompat(LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle, UINT dpi)
{
    BOOL result = FALSE;
    if ((pAdjustWindowRectExForDpi != nullptr) && platform.perMonitorDPIAwareV2)
    {
        result = pAdjustWindowRectExForDpi(lpRect, dwStyle, bMenu, dwExStyle, dpi);
    }
    else
    {
        result = AdjustWindowRectEx(lpRect, dwStyle, bMenu, dwExStyle);
    }
    return result;
}

// Set process to highest level of dpi awareness on this version of Windows
// DPI awareness can only be set once. This function will fail on subsequent calls (which is harmless)
// Windows 10 v1703+  - per-monitor awareness v2
// Windows 8.1/10     - per-monitor awareness v1
// Windows Vista/7/8  - system-aware
// Windows XP         - No DPI awareness
static void SetProcessDpiAwareCompat(void)
{
    if (pSetProcessDpiAwarenessContext != nullptr)
    {
        // Windows 10 v1703+
        BOOL succeeded = pSetProcessDpiAwarenessContext(static_cast<HANDLE>(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2));

        if (!succeeded)
        {
            // If monitor aware v2 is not available, try v1
            pSetProcessDpiAwarenessContext(static_cast<HANDLE>(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE));
        }
    }
    else if (pSetProcessDpiAwareness != nullptr)
    {
        // Windows 8.1+
        pSetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
    }
    else if (pSetProcessDPIAware != nullptr)
    {
        // Windows Vista+
        pSetProcessDPIAware();
    }

    platform.perMonitorDPIAwareV2 = false;
    if ((pGetThreadDpiAwarenessContext != nullptr) && (pAreDpiAwarenessContextsEqual != nullptr))
    {
        // There are subtle differences between per-monitor v1 and v2
        platform.perMonitorDPIAwareV2 = pAreDpiAwarenessContextsEqual(pGetThreadDpiAwarenessContext(), static_cast<HANDLE>(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2));
    }
}

static bool DecoratedFromStyle(DWORD style)
{
    if (style & STYLE_FLAGS_UNDECORATED_ON)
    {
        if (style & STYLE_FLAGS_UNDECORATED_OFF) TRACELOG(LOG_ERROR, "WIN32: FLAGS: Style 0x%x has both undecorated on/off flags", style);
        return false; // Not decorated
    }

    DWORD masked = (style & STYLE_FLAGS_UNDECORATED_OFF);
    if (STYLE_FLAGS_UNDECORATED_OFF != masked)
    {
        TRACELOG(LOG_ERROR, "WIN32: FLAGS: Style 0x%x is missing flags 0x%x", masked, masked ^ STYLE_FLAGS_UNDECORATED_OFF);
    }

    return true; // Decorated
}

// Get window style from required flags
static DWORD MakeWindowStyle(unsigned flags)
{
    // Flag is not needed because there are no child windows,
    // but supposedly it improves efficiency, plus, windows adds this
    // flag automatically anyway so it keeps flags in sync with the OS
    DWORD style = WS_CLIPSIBLINGS;

    style |= (flags & FLAG_WINDOW_HIDDEN)? 0 : WS_VISIBLE;
    style |= (flags & FLAG_WINDOW_RESIZABLE)? STYLE_FLAGS_RESIZABLE : 0;
    style |= (flags & FLAG_WINDOW_UNDECORATED)? STYLE_FLAGS_UNDECORATED_ON : STYLE_FLAGS_UNDECORATED_OFF;

    // Minimized takes precedence over maximized
    int mized = MIZED_NONE;
    if (flags & FLAG_WINDOW_MINIMIZED) mized = MIZED_MIN;
    else if (flags & FLAG_WINDOW_MAXIMIZED) mized = MIZED_MAX;

    switch (mized)
    {
        case MIZED_NONE: break;
        case MIZED_MIN: style |= WS_MINIMIZE; break;
        case MIZED_MAX: style |= WS_MAXIMIZE; break;
        default: break;
    }

    return style;
}

// Check flags state, enforces that the actual window/platform state is in sync with raylib's flags
static void CheckFlags(const char *context, HWND hwnd, DWORD flags, DWORD expectedStyle, DWORD styleCheckMask)
{
    DWORD styleFromFlags = MakeWindowStyle(flags);
    if ((styleFromFlags & styleCheckMask) != (expectedStyle & styleCheckMask))
    {
        TRACELOG(LOG_ERROR, "WIN32: FLAGS: %s: window flags (0x%x) produced style 0x%x which != expected 0x%x (diff=0x%x, mask=0x%x)",
            context, flags, styleFromFlags & styleCheckMask, expectedStyle & styleCheckMask,
            (styleFromFlags & styleCheckMask) ^ (expectedStyle & styleCheckMask), styleCheckMask);
    }

    SetLastError(0);
    LONG actualStyle = static_cast<LONG>(GetWindowLongPtrW(hwnd, GWL_STYLE));
    if ((actualStyle & styleCheckMask) != (expectedStyle & styleCheckMask))
    {
        TRACELOG(LOG_ERROR, "WIN32: FLAGS: %s: expected style 0x%x but got 0x%x (diff=0x%x, mask=0x%x, lasterror=%lu)",
            context, expectedStyle & styleCheckMask, actualStyle & styleCheckMask,
            (expectedStyle & styleCheckMask) ^ (actualStyle & styleCheckMask),
            styleCheckMask, GetLastError());
    }

    if (styleCheckMask & WS_MINIMIZE)
    {
        bool isIconic = IsIconic(hwnd);
        bool styleMinimized = !!(WS_MINIMIZE & actualStyle);
        if (isIconic != styleMinimized) TRACELOG(LOG_ERROR, "WIN32: FLAGS: IsIconic(%d) != WS_MINIMIZED(%d)", isIconic, styleMinimized);
    }

    if (styleCheckMask & WS_MAXIMIZE)
    {
        WINDOWPLACEMENT placement;
        placement.length = sizeof(placement);
        if (!GetWindowPlacement(hwnd, &placement))
        {
            TRACELOG(LOG_ERROR, "WIN32: FLAGS: %s failed, error=%lu", "GetWindowPlacement", GetLastError());
        }
        bool placementMaximized = (placement.showCmd == SW_SHOWMAXIMIZED);
        bool styleMaximized = WS_MAXIMIZE & actualStyle;
        if (placementMaximized != styleMaximized)
        {
            TRACELOG(LOG_ERROR, "WIN32: FLAGS: Maximized state desync, placement maximized=%d (showCmd=%lu) style maximized=%d",
                placementMaximized, placement.showCmd, styleMaximized);
        }
    }
}

// Calculate window size (with borders, title-bar...) from desired client size (framebuffer size)
static SIZE CalcWindowSize(UINT dpi, SIZE clientSize, DWORD style)
{
    RECT rect = { 0, 0, clientSize.cx, clientSize.cy };

    int result = AdjustWindowRectExForDpiCompat(&rect, style, 0, WINDOW_STYLE_EX, dpi);
    if (result == 0) TRACELOG(LOG_ERROR, "WIN32: Failed to adjust window rect [ERROR: %lu]", GetLastError());

    return SIZE{ rect.right - rect.left, rect.bottom - rect.top };
}

// Update window size if required
// NOTE: Returns true if the window size was updated, false otherwise
static bool UpdateWindowSize(int mode, HWND hwnd, int width, int height, unsigned flags)
{
    if (flags & FLAG_WINDOW_MINIMIZED) return false;

    if (flags & FLAG_WINDOW_MAXIMIZED)
    {
        CheckFlags("UpdateWindowSize(maximized)", hwnd, flags, MakeWindowStyle(flags), STYLE_MASK_ALL);
        return false;
    }

    if (flags & FLAG_BORDERLESS_WINDOWED_MODE)
    {
        MONITORINFO info = {};
        HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
        info.cbSize = sizeof(info);
        if (!GetMonitorInfoW(monitor, &info)) TRACELOG(LOG_ERROR, "WIN32: Failed to get monitor info [ERROR: %lu]", GetLastError());

        RECT windowRect = {};
        if (!GetWindowRect(hwnd, &windowRect)) TRACELOG(LOG_ERROR, "WIN32: Failed to get window rect [ERROR: %lu]", GetLastError());

        if ((windowRect.left == info.rcMonitor.left) &&
            (windowRect.top == info.rcMonitor.top) &&
            ((windowRect.right - windowRect.left) == (info.rcMonitor.right - info.rcMonitor.left)) &&
            ((windowRect.bottom - windowRect.top) == (info.rcMonitor.bottom - info.rcMonitor.top))) return false;

        if (!SetWindowPos(hwnd, HWND_TOP,
            info.rcMonitor.left, info.rcMonitor.top,
            info.rcMonitor.right - info.rcMonitor.left,
            info.rcMonitor.bottom - info.rcMonitor.top,
            SWP_NOOWNERZORDER))
        {
            TRACELOG(LOG_ERROR, "WIN32: Failed to set window position [ERROR: %lu]", GetLastError());
        }

        return true;
    }

    // Get size in pixels from points, considering high-dpi
    UINT dpi = GetDpiForWindowCompat(hwnd);
    float dpiScale = (static_cast<float>(dpi))/96.0f;
    bool dpiScaling = flags & FLAG_WINDOW_HIGHDPI;
    SIZE desiredSize = {
        .cx = dpiScaling? static_cast<int>(roundf(static_cast<float>(width)*dpiScale)) : width,
        .cy = dpiScaling? static_cast<int>(roundf(static_cast<float>(height)*dpiScale)) : height
    };

    // Get client size (framebuffer inside the window)
    RECT rect = {};
    GetClientRect(hwnd, &rect);
    SIZE clientSize = { rect.right, rect.bottom };

    // If client size is alread desired size, no need to update
    if ((clientSize.cx == desiredSize.cx) && (clientSize.cy == desiredSize.cy)) return false;

    TRACELOG(LOG_INFO, "WIN32: Restoring client size from [%dx%d] to [%dx%d] (dpi:%lu dpiScaling:%d app:%ix%i)",
        clientSize.cx, clientSize.cy, desiredSize.cx, desiredSize.cy, dpi, dpiScaling, width, height);

    // Calculate window size from desired framebuffer size and window flags
    SIZE windowSize = CalcWindowSize(dpi, desiredSize, MakeWindowStyle(flags));
    POINT windowPos = {};
    UINT swpFlags = SWP_NOZORDER | SWP_FRAMECHANGED;

    if (mode == 0) // UPDATE_WINDOW_FIRST
    {
        HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
        if (!monitor) TRACELOG(LOG_ERROR, "WIN32: Failed to get monitor from window [ERROR: %lu]", GetLastError());

        MONITORINFO info = {};
        info.cbSize = sizeof(info);
        if (!GetMonitorInfoW(monitor, &info)) TRACELOG(LOG_ERROR, "WIN32: Failed to get monitor info [ERROR: %lu]", GetLastError());

        #define MAX(a,b) (((a)>(b))? (a):(b))

        LONG monitorWidth = info.rcMonitor.right - info.rcMonitor.left;
        LONG monitorHeight = info.rcMonitor.bottom - info.rcMonitor.top;
        windowPos = POINT{
            MAX(0, (monitorWidth - windowSize.cx)/2),
            MAX(0, (monitorHeight - windowSize.cy)/2),
        };
    }
    else swpFlags |= SWP_NOMOVE;

    // WARNING: This code must be called after swInit() has been called, after InitPlatform() in [rcore]
    SetWindowPos(hwnd, nullptr, windowPos.x, windowPos.y, windowSize.cx, windowSize.cy, SWP_NOMOVE | SWP_NOZORDER);

    return true;
}

// Get OpenGL function pointers
static void *WglGetProcAddress(const char *procname)
{
    void *proc = reinterpret_cast<void *>(wglGetProcAddress(procname));

    if ((proc == nullptr) ||
        // NOTE: Some GPU drivers could return following
        // invalid sentinel values instead of NULL
        (proc == reinterpret_cast<void *>(0x1)) ||
        (proc == reinterpret_cast<void *>(0x2)) ||
        (proc == reinterpret_cast<void *>(0x3)) ||
        (proc == reinterpret_cast<void *>(-1)))
    {
        // TODO: Keep gl module pointer as global platform data?
        HMODULE glModule = LoadLibraryW(L"opengl32.dll");
        proc = reinterpret_cast<void *>(GetProcAddress(glModule, procname));

        //if (proc == NULL) TRACELOG(LOG_ERROR, "GL: GetProcAddress() failed to get %s [%p], error=%u", procname, proc, GetLastError());
        //else TRACELOG(LOG_INFO, "GL: Found entry point for %s [%p]", procname, proc);
    }

    return proc;
}

// Get cursor name
static LPCWSTR GetCursorName(int cursor)
{
    LPCWSTR name = static_cast<LPCWSTR>(IDC_ARROW);

    switch (cursor)
    {
        case MOUSE_CURSOR_DEFAULT: name = static_cast<LPCWSTR>(IDC_ARROW); break;
        case MOUSE_CURSOR_ARROW: name = static_cast<LPCWSTR>(IDC_ARROW); break;
        case MOUSE_CURSOR_IBEAM: name = static_cast<LPCWSTR>(IDC_IBEAM); break;
        case MOUSE_CURSOR_CROSSHAIR: name = static_cast<LPCWSTR>(IDC_CROSS); break;
        case MOUSE_CURSOR_POINTING_HAND: name = static_cast<LPCWSTR>(IDC_HAND); break;
        case MOUSE_CURSOR_RESIZE_EW: name = static_cast<LPCWSTR>(IDC_SIZEWE); break;
        case MOUSE_CURSOR_RESIZE_NS: name = static_cast<LPCWSTR>(IDC_SIZENS); break;
        case MOUSE_CURSOR_RESIZE_NWSE: name = static_cast<LPCWSTR>(IDC_SIZENWSE); break;
        case MOUSE_CURSOR_RESIZE_NESW: name = static_cast<LPCWSTR>(IDC_SIZENESW); break;
        case MOUSE_CURSOR_RESIZE_ALL: name = static_cast<LPCWSTR>(IDC_SIZEALL); break;
        case MOUSE_CURSOR_NOT_ALLOWED: name = static_cast<LPCWSTR>(IDC_NO); break;
        default: break;
    }

    return name;
}

// Delete platform.smallIcon and platform.largeIcon
static void DeletePlatformIconHandles(void)
{
    HICON smallIcon = platform.smallIcon;
    HICON largeIcon = platform.largeIcon;

    if (smallIcon != nullptr) DestroyIcon(smallIcon);

    // Only delete largeIcon if it's not pointing to the same underlying resource as smallIcon
    if ((largeIcon != nullptr) && (largeIcon != smallIcon)) DestroyIcon(largeIcon);

    platform.smallIcon = nullptr;
    platform.largeIcon = nullptr;
}

// Delete existing icons and replace them with the standard window/taskbar icons
static void ResetToDefaultWindowsIcons(void)
{
    HICON smallIcon = reinterpret_cast<HICON>(GetClassLongPtrW(platform.hwnd, GCLP_HICONSM));
    HICON largeIcon = reinterpret_cast<HICON>(GetClassLongPtrW(platform.hwnd, GCLP_HICON));

    SendMessage(platform.hwnd, WM_SETICON, ICON_SMALL, static_cast<LPARAM>(smallIcon));
    SendMessage(platform.hwnd, WM_SETICON, ICON_BIG, static_cast<LPARAM>(largeIcon));

    // The built-in Windows icons shouldn't be deleted, so there's no need to store a reference to them
    // Just delete the previous icons and exit
    DeletePlatformIconHandles();
}

// Convert Image to Win32 HICON (must be RGBA)
static HICON ConvertImageToWindowsIcon(Image image)
{
    if (image.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8)
    {
        TRACELOG(LOG_WARNING, "WIN32: Window icon image must be in R8G8B8A8 pixel format");
        return nullptr;
    }

    HDC hdc = GetDC(platform.hwnd);

    // Create 32-bit BGRA DIB for color
    BITMAPV5HEADER bi = {};
    ZeroMemory(&bi, sizeof(bi));
    bi.bV5Size = sizeof(bi);
    bi.bV5Width = image.width;
    bi.bV5Height = -image.height; // Negative = top-down bitmap
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;

    unsigned char *targetBits = nullptr;
    HBITMAP hColorBitmap = CreateDIBSection(hdc, static_cast<const BITMAPINFO *>(&bi), DIB_RGB_COLORS, reinterpret_cast<void * *>(&targetBits), nullptr, 0);
    if (!hColorBitmap)
    {
        ReleaseDC(platform.hwnd, hdc);
        return nullptr;
    }

    // Copy RGBA > BGRA (Win32 expects BGRA)
    for (int y = 0; y < image.height; y++)
    {
        for (int x = 0; x < image.width; x++)
        {
            int i = (y*image.width + x)*4;
            targetBits[i + 0] = (static_cast<unsigned char *>(image.data))[i + 2]; // B
            targetBits[i + 1] = (static_cast<unsigned char *>(image.data))[i + 1]; // G
            targetBits[i + 2] = (static_cast<unsigned char *>(image.data))[i + 0]; // R
            targetBits[i + 3] = (static_cast<unsigned char *>(image.data))[i + 3]; // A
        }
    }

    // Create mask bitmap (1-bit, all opaque)
    HBITMAP hMaskBitmap = CreateBitmap(image.width, image.height, 1, 1, nullptr);

    // Build icon info
    ICONINFO ii = {};
    ZeroMemory(&ii, sizeof(ii));
    ii.fIcon = TRUE;
    ii.hbmMask = hMaskBitmap;
    ii.hbmColor = hColorBitmap;

    HICON result = CreateIconIndirect(&ii);

    // Clean up GDI bitmaps (icon keeps copies internally)
    DeleteObject(hColorBitmap);
    DeleteObject(hMaskBitmap);
    ReleaseDC(platform.hwnd, hdc);

    return result;
}

// Count monitors process
// NOTE: Required by GetMonitorCount()
static BOOL CALLBACK CountMonitorsProc(HMONITOR handle, HDC hdc, LPRECT rect, LPARAM lparam)
{
    int *count = static_cast<int *>(lparam);
    *count += 1;

    // Always return TRUE to continue the loop, otherwise, the caller
    // can't distinguish between stopping the loop and an error
    return TRUE;
}

// Find monitor process
// NOTE: Required by GetCurrentMonitor()
static BOOL CALLBACK FindMonitorProc(HMONITOR handle, HDC hdc, LPRECT rect, LPARAM lparam)
{
    MonitorInfo *monitor = reinterpret_cast<MonitorInfo *>(lparam);

    if (handle == monitor->needle)
    {
        monitor->matchIndex = monitor->index;
        monitor->rect = *rect;
    }

    monitor->index += 1;

    // Always return TRUE to continue the loop, otherwise, the caller
    // can't distinguish between stopping the loop and an error
    return TRUE;
}

// Get style changed required operations flags
// NOTE: Required for deferred operations
static void GetStyleChangeFlagOps(DWORD coreWindowFlags, STYLESTRUCT *style, FlagsOp *deferredFlags)
{
    // Check window resizable flag change
    bool resizable = (coreWindowFlags & FLAG_WINDOW_RESIZABLE);
    bool resizableOld = ((style->styleOld & STYLE_FLAGS_RESIZABLE) != 0);
    bool resizableNew = ((style->styleNew & STYLE_FLAGS_RESIZABLE) != 0);
    if (resizable != resizableOld) TRACELOG(LOG_ERROR, "WIN32: Expected resizable %u but got %u", resizable, resizableOld);
    if (resizableOld != resizableNew)
    {
        if (resizableNew) deferredFlags->set |= FLAG_WINDOW_RESIZABLE;
        else deferredFlags->clear |= FLAG_WINDOW_RESIZABLE;
    }

    // Check window decorated flag change
    bool decorated = (0 == (coreWindowFlags & FLAG_WINDOW_UNDECORATED));
    bool decoratedOld = DecoratedFromStyle(style->styleOld);
    bool decoratedNew = DecoratedFromStyle(style->styleNew);
    if (decorated != decoratedOld) TRACELOG(LOG_ERROR, "WIN32: Expected decorated %u but got %u", decorated, decoratedOld);
    if (decoratedOld != decoratedNew)
    {
        if (decoratedNew) deferredFlags->clear |= FLAG_WINDOW_UNDECORATED;
        else deferredFlags->set |= FLAG_WINDOW_UNDECORATED;
    }

    // Check window hidden flag change
    bool hidden = (coreWindowFlags & FLAG_WINDOW_HIDDEN);
    bool hiddenOld = ((style->styleOld & WS_VISIBLE) == 0);
    bool hiddenNew = ((style->styleNew & WS_VISIBLE) == 0);
    if (hidden != hiddenOld) TRACELOG(LOG_ERROR, "WIN32: Expected hidden %u but got %u", hidden, hiddenOld);
    if (hiddenOld != hiddenNew)
    {
        if (hiddenNew) deferredFlags->set |= FLAG_WINDOW_HIDDEN;
        else deferredFlags->clear |= FLAG_WINDOW_HIDDEN;
    }
}

// Adopt window resize
// NOTE: Call when the window is rezised, returns true
// if the new window size should update the desired app size
static bool AdoptWindowResize(unsigned flags)
{
    if (flags & FLAG_WINDOW_MINIMIZED) return false;
    if (flags & FLAG_WINDOW_MAXIMIZED) return false;
    if (flags & FLAG_FULLSCREEN_MODE) return false;
    if (flags & FLAG_BORDERLESS_WINDOWED_MODE) return false;
    if (!(flags & FLAG_WINDOW_RESIZABLE)) return false;

    return true;
}

// ---------------------------------------------------------------------------------------------
// Here's the end of the "pure function section", the rest of the file can access global state
// ---------------------------------------------------------------------------------------------

// Unlock the ability to use CORE in the rest of the file
#undef CORE

//----------------------------------------------------------------------------------
// Module Internal Functions Declaration
//----------------------------------------------------------------------------------
int InitPlatform(void);         // Initialize platform (graphics, inputs and more)
void ClosePlatform(void);       // Close platform

// Win32 process messages management function
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

// Win32: Handle inputs functions
static void HandleKey(WPARAM wparam, LPARAM lparam, char state);
static void HandleMouseButton(int button, char state);
static void HandleRawInput(LPARAM lparam);
static void HandleWindowResize(HWND hwnd, int *width, int *height);

static void UpdateWindowStyle(HWND hwnd, unsigned desiredFlags);
static unsigned SanitizeFlags(unsigned flags);
static void UpdateFlags(HWND hwnd, unsigned desiredFlags, int width, int height); // Update window flags

// Check if OpenGL extension is available
static bool IsWglExtensionAvailable(HDC hdc, const char *extension);

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
// NOTE: Functions declaration is provided by raylib.h

//----------------------------------------------------------------------------------
// Module Functions Definition: Window and Graphics Device
//----------------------------------------------------------------------------------

// Check if application should close
bool WindowShouldClose(void)
{
    return CORE.Window.shouldClose;
}

// Toggle fullscreen mode
void ToggleFullscreen(void)
{
    TRACELOG(LOG_WARNING, "WIN32: Toggle full screen functionality not implemented");
}

// Toggle borderless windowed mode
void ToggleBorderlessWindowed(void)
{
    if (CORE.Window.flags & FLAG_BORDERLESS_WINDOWED_MODE) ClearWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
    else SetWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
}

// Set window state: maximized, if resizable
void MaximizeWindow(void)
{
    SetWindowState(FLAG_WINDOW_MAXIMIZED);
}

// Set window state: minimized
void MinimizeWindow(void)
{
    SetWindowState(FLAG_WINDOW_MINIMIZED);
}

// Restore window from being minimized/maximized
void RestoreWindow(void)
{
    if ((CORE.Window.flags & FLAG_WINDOW_MAXIMIZED) &&
        (CORE.Window.flags & FLAG_WINDOW_MINIMIZED)) ClearWindowState(FLAG_WINDOW_MINIMIZED);
    else ClearWindowState(FLAG_WINDOW_MINIMIZED | FLAG_WINDOW_MAXIMIZED);
}

// Set window configuration state using flags
void SetWindowState(unsigned int flags)
{
    if (flags & MSAA_FLAGS) TRACELOG(LOG_WARNING, "WINDOW: MSAA can only be configured before window initialization");
    if (flags & FLAG_WINDOW_HIGHDPI) TRACELOG(LOG_WARNING, "WINDOW: High DPI can only be configured before window initialization");

    platform.desiredFlags = SanitizeFlags(CORE.Window.flags | flags);
    UpdateFlags(platform.hwnd, platform.desiredFlags, platform.appScreenWidth, platform.appScreenHeight);
}

// Clear window configuration state flags
void ClearWindowState(unsigned int flags)
{
    if (flags & MSAA_FLAGS) TRACELOG(LOG_WARNING, "WINDOW: MSAA can only be configured before window initialization");
    if (flags & FLAG_WINDOW_HIGHDPI) TRACELOG(LOG_WARNING, "WINDOW: High DPI can only be configured before window initialization");

    platform.desiredFlags = SanitizeFlags(CORE.Window.flags & ~flags);
    UpdateFlags(platform.hwnd, platform.desiredFlags, platform.appScreenWidth, platform.appScreenHeight);
}

// Set icon for window
void SetWindowIcon(Image image)
{
    if (!platform.hwnd) return;

    if ((image.data == nullptr) || (image.width <= 0) || (image.height <= 0))
    {
        ResetToDefaultWindowsIcons();
        return;
    }

    HICON hIcon = ConvertImageToWindowsIcon(image);
    if (hIcon)
    {
        SendMessage(platform.hwnd, WM_SETICON, ICON_SMALL, static_cast<LPARAM>(hIcon));
        SendMessage(platform.hwnd, WM_SETICON, ICON_BIG, static_cast<LPARAM>(hIcon));
        DeletePlatformIconHandles();
        platform.smallIcon = hIcon;
        platform.largeIcon = hIcon;
    }
}

// Set icon for window (multiple sizes)
void SetWindowIcons(Image *images, int count)
{
    if ((images == nullptr) || (count <= 0))
    {
        ResetToDefaultWindowsIcons();
        return;
    }

    // Large icon has a resolution of (SM_CXICON*SM_CXICON) pixels
    int largeIconRenderSize = GetSystemMetrics(SM_CXICON);

    // Attempt to find a best match for the large icon based on each image's width
    int bestLargeIconIndex = -1;
    int bestLargeIconSizeDifference = INT_MAX;
    for (int i = 0; i < count; i++)
    {
        Image img = images[i];
        int difference = abs(img.width - largeIconRenderSize);
        if ((img.data != nullptr) && (img.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) && (difference < bestLargeIconSizeDifference))
        {
            bestLargeIconIndex = i;
            bestLargeIconSizeDifference = difference;
        }
    }

    // Small icon has a resolution of (SM_CYSMICON*SM_CYSMICON) pixels
    int smallIconRenderSize = GetSystemMetrics(SM_CYSMICON);

    // Attempt to find a best match for the small icon based on each image's width
    int bestSmallIconIndex = -1;
    int bestSmallIconSizeDifference = INT_MAX;
    for (int i = 0; i < count; i++)
    {
        Image img = images[i];
        int difference = abs(img.width - smallIconRenderSize);
        if ((img.data != nullptr) && (img.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) && (difference < bestSmallIconSizeDifference))
        {
            bestSmallIconIndex = i;
            bestSmallIconSizeDifference = difference;
        }
    }

    if ((bestSmallIconIndex != -1) && (bestLargeIconIndex != -1))
    {
        HICON smallIcon = ConvertImageToWindowsIcon(images[bestSmallIconIndex]);
        HICON largeIcon = ConvertImageToWindowsIcon(images[bestLargeIconIndex]);

        SendMessage(platform.hwnd, WM_SETICON, ICON_SMALL, static_cast<LPARAM>(smallIcon));
        SendMessage(platform.hwnd, WM_SETICON, ICON_BIG, static_cast<LPARAM>(largeIcon));

        DeletePlatformIconHandles();

        platform.smallIcon = smallIcon;
        platform.largeIcon = largeIcon;
    }
    else TRACELOG(LOG_WARNING, "WIN32: All icons passed to SetWindowIcons were invalid");
}

void SetWindowTitle(std::string_view titleView)
{
    const std::string titleStr(titleView);
    const char *title = titleStr.c_str();

    CORE.Window.title = title;

    WCHAR *titleWide = nullptr;
    A_TO_W_ALLOCA(titleWide, CORE.Window.title.c_str());

    int result = SetWindowTextW(platform.hwnd, titleWide);
    if (result == 0) TRACELOG(LOG_WARNING, "WIN32: Failed to set window title [ERROR: %lu]", GetLastError());
}

// Set window position on screen (windowed mode)
void SetWindowPosition(int x, int y)
{
    if (platform.hwnd != nullptr)
    {
        RECT rect = {};
        if (GetWindowRect(platform.hwnd, &rect))
        {
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;

            // Move the window to the new position (keeping size and z-order)
            SetWindowPos(platform.hwnd, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }
}

// Set monitor for the current window
void SetWindowMonitor(int monitor)
{
    TRACELOG(LOG_WARNING, "SetWindowMonitor not implemented");
}

// Set window minimum dimensions (FLAG_WINDOW_RESIZABLE)
void SetWindowMinSize(int width, int height)
{
    if ((width > CORE.Window.screenMax.width) || (height > CORE.Window.screenMax.height))
    {
        TRACELOG(LOG_WARNING, "WIN32: WINDOW: Cannot set minimum screen size higher than the maximum");
        return;
    }

    CORE.Window.screenMin.width = width;
    CORE.Window.screenMin.height = height;

    SetWindowSize(platform.appScreenWidth, platform.appScreenHeight);
}

// Set window maximum dimensions (FLAG_WINDOW_RESIZABLE)
void SetWindowMaxSize(int width, int height)
{
    if ((width < CORE.Window.screenMin.width) || (height < CORE.Window.screenMin.height))
    {
        TRACELOG(LOG_WARNING, "WIN32: WINDOW: Cannot set maximum screen size lower than the minimum");
        return;
    }

    CORE.Window.screenMax.width = width;
    CORE.Window.screenMax.height = height;

    SetWindowSize(platform.appScreenWidth, platform.appScreenHeight);
}

// Set window dimensions
void SetWindowSize(int width, int height)
{
    int screenWidth = fmaxf(CORE.Window.screenMin.width, fminf(CORE.Window.screenMax.width, width));
    int screenHeight = fmaxf(CORE.Window.screenMin.height, fminf(CORE.Window.screenMax.height, height));

    UpdateWindowSize(1, platform.hwnd, screenWidth, screenHeight, platform.desiredFlags);
}

// Set window opacity, value opacity is between 0.0 and 1.0
void SetWindowOpacity(float opacity)
{
    TRACELOG(LOG_WARNING, "SetWindowOpacity not implemented");
}

// Set window focused
void SetWindowFocused(void)
{
    BringWindowToTop(platform.hwnd);
    SetForegroundWindow(platform.hwnd);
    SetFocus(platform.hwnd);

    if (GetForegroundWindow() != platform.hwnd) TRACELOG(LOG_WARNING, "WIN32: WINDOW: Failed to set window focus");
}

// Get native window handle
void *GetWindowHandle(void)
{
    return static_cast<void *>(platform.hwnd);
}

int GetMonitorCount(void)
{
    int count = 0;

    int result = EnumDisplayMonitors(nullptr, nullptr, CountMonitorsProc, static_cast<LPARAM>(count));
    if (result == 0) TRACELOG(LOG_ERROR, "%s failed, error=%lu", "EnumDisplayMonitors", GetLastError());

    return count;
}

// Get current monitor where window is placed
int GetCurrentMonitor(void)
{
    HMONITOR monitor = MonitorFromWindow(platform.hwnd, MONITOR_DEFAULTTOPRIMARY);
    if (!monitor) TRACELOG(LOG_ERROR, "%s failed, error=%lu", "MonitorFromWindow", GetLastError());

    MonitorInfo info = {};
    info.needle = monitor;
    info.index = 0;
    info.matchIndex = -1;

    int result = EnumDisplayMonitors(nullptr, nullptr, FindMonitorProc, static_cast<LPARAM>(info));
    if (result == 0) TRACELOG(LOG_ERROR, "%s failed, error=%lu", "EnumDisplayMonitors", GetLastError());

    return info.matchIndex;
}

// Get selected monitor position
vec2 GetMonitorPosition(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorPosition not implemented");
    return vec2{ 0, 0 };
}

// Get selected monitor width (currently used by monitor)
int GetMonitorWidth(int monitor)
{
    //TRACELOG(LOG_WARNING, "GetMonitorWidth not implemented");
    return 0;
}

// Get selected monitor height (currently used by monitor)
int GetMonitorHeight(int monitor)
{
    //TRACELOG(LOG_WARNING, "GetMonitorHeight not implemented");
    return 0;
}

// Get selected monitor physical width in millimetres
int GetMonitorPhysicalWidth(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorPhysicalWidth not implemented");
    return 0;
}

// Get selected monitor physical height in millimetres
int GetMonitorPhysicalHeight(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorPhysicalHeight not implemented");
    return 0;
}

// Get selected monitor refresh rate
int GetMonitorRefreshRate(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorRefreshRate not implemented");
    return 0;
}

// Get the human-readable, UTF-8 encoded name of the selected monitor
std::string GetMonitorName(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorName not implemented");
    return {};
}

// Get window position XY on monitor
vec2 GetWindowPosition(void)
{
    //TRACELOG(LOG_WARNING, "GetWindowPosition not implemented");
    return vec2{ 0, 0 };
}

// Get window scale DPI factor for current monitor
vec2 GetWindowScaleDPI(void)
{
    float scale = (static_cast<float>(GetDpiForWindowCompat(platform.hwnd)))/96.0f;
    return vec2{ scale, scale };
}

// Set clipboard text content
void SetClipboardText(std::string_view textView)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();

    if (OpenClipboard(platform.hwnd))
    {
        if (text == nullptr) EmptyClipboard();
        else
        {
            size_t length = strlen(text);

            int clipboardTextLength = MultiByteToWideChar(CP_UTF8, 0, text, static_cast<int>((length + 1)), nullptr, 0);
            if (clipboardTextLength != 0)
            {
                HGLOBAL globalHandle = GlobalAlloc(GMEM_MOVEABLE, clipboardTextLength*sizeof(wchar_t));

                if (globalHandle)
                {
                    LPVOID clipboardText = GlobalLock(globalHandle);
                    int charactersCopied = 0;
                    if (clipboardText != nullptr)
                    {
                        // Directly copies text to clipboardText and converts from UTF-8 to UTF-16
                        charactersCopied = MultiByteToWideChar(CP_UTF8, 0, text, static_cast<int>((length + 1)), clipboardText, clipboardTextLength);
                        GlobalUnlock(globalHandle);
                    }

                    if (charactersCopied != 0)
                    {
                        EmptyClipboard();

                        // We don't need to call GlobalFree when SetClipboardData succeeds; Windows takes control of the memory
                        HANDLE clipboardData = SetClipboardData(CF_UNICODETEXT, globalHandle);
                        if (!clipboardData)
                        {
                            TRACELOG(LOG_WARNING, "Clipboard: Failed to set clipboard text.");
                            GlobalFree(globalHandle);
                        }
                    }
                    else GlobalFree(globalHandle);
                }
            }
        }

        CloseClipboard();
    }
    else TRACELOG(LOG_WARNING, "Clipboard: Failed to write to clipboard. It may be locked by another program.");
}

// Get clipboard text content
std::string GetClipboardText(void)
{
    if (platform.clipboardText != nullptr)
    {
        RL_FREE(platform.clipboardText);
        platform.clipboardText = nullptr;
    }

    if (OpenClipboard(platform.hwnd))
    {
        HANDLE clipboardDataHandle = GetClipboardData(CF_UNICODETEXT);
        if (clipboardDataHandle)
        {
            wchar_t *clipboardText = GlobalLock(clipboardDataHandle);
            if (clipboardText != nullptr)
            {
                // The clipboard data can't be trusted. It may or may not be null-terminated!
                size_t maxLength = GlobalSize(clipboardDataHandle)/sizeof(wchar_t);
                int length = static_cast<int>(wcsnlen(clipboardText, maxLength));

                int size = WideCharToMultiByte(CP_UTF8, 0, clipboardText, length, nullptr, 0, nullptr, nullptr);
                char *utf8Text = static_cast<char *>(RL_CALLOC(size + 1, 1));

                if ((size > 0) && (WideCharToMultiByte(CP_UTF8, 0, clipboardText, length, utf8Text, size, nullptr, nullptr) == 0)) size = 0;
                utf8Text[size] = '\0';

                platform.clipboardText = utf8Text;

                GlobalUnlock(clipboardDataHandle);
            }
        }

        CloseClipboard();
    }
    else
    {
        TRACELOG(LOG_WARNING, "Clipboard: Failed to read clipboard. It may be locked by another program.");
    }

    return platform.clipboardText;
}

#if SUPPORT_CLIPBOARD_IMAGE && SUPPORT_MODULE_RTEXTURES
    #define WIN32_CLIPBOARD_IMPLEMENTATION
    #define WINUSER_ALREADY_INCLUDED
    #define WINBASE_ALREADY_INCLUDED
    #define WINGDI_ALREADY_INCLUDED
    #include "../external/win32_clipboard.h"
#endif

// Get clipboard image
Image GetClipboardImage(void)
{
    Image image = {};

#if SUPPORT_CLIPBOARD_IMAGE && SUPPORT_MODULE_RTEXTURES
    unsigned int dataSize = 0;
    void *fileData = nullptr;
    int width = 0;
    int height = 0;

    fileData = static_cast<void *>(Win32GetClipboardImageData(&width, &height, &dataSize);

    if (fileData == nullptr) TRACELOG(LOG_WARNING, "Clipboard image: Couldn't get clipboard data.");
    else
    {
        image = LoadImageFromMemory(".bmp", static_cast<const unsigned char *>(fileData), static_cast<int>(dataSize));
        RL_FREE(fileData);
    }
#else
    TRACELOG(LOG_WARNING, "Clipboard image: SUPPORT_CLIPBOARD_IMAGE requires SUPPORT_MODULE_RTEXTURES to work properly");
#endif // SUPPORT_CLIPBOARD_IMAGE

    return image;
}

// Show mouse cursor
void ShowCursor(void)
{
    SetCursor(LoadCursorW(nullptr, static_cast<LPCWSTR>(IDC_ARROW)));
    CORE.Input.Mouse.cursorHidden = false;
}

// Hide mouse cursor
void HideCursor(void)
{
    // NOTE: Using SetCursor() instead of ShowCursor() because
    // it makes it easy to only hide the cursor while it's inside the client area
    SetCursor(nullptr);
    CORE.Input.Mouse.cursorHidden = true;
}

// Enable cursor (unlock cursor)
void EnableCursor(void)
{
    if (CORE.Input.Mouse.cursorLocked)
    {
        if (!ClipCursor(nullptr)) TRACELOG(LOG_WARNING, "WIN32: Failed to clip cursor [ERROR: %lu]", GetLastError());

        RAWINPUTDEVICE rid = {};
        rid.usUsagePage = 0x01;     // HID_USAGE_PAGE_GENERIC
        rid.usUsage = 0x02;         // HID_USAGE_GENERIC_MOUSE
        rid.dwFlags = RIDEV_REMOVE; // Add to this window even in background
        rid.hwndTarget = nullptr;
        int result = RegisterRawInputDevices(&rid, 1, sizeof(rid));
        if (result == 0) TRACELOG(LOG_WARNING, "WIN32: Failed to register raw input devices [ERROR: %lu]", GetLastError());

        ShowCursor();
        CORE.Input.Mouse.cursorLocked = false;
    }
}

// Disables cursor (lock cursor)
void DisableCursor(void)
{
    if (!CORE.Input.Mouse.cursorLocked)
    {
        RAWINPUTDEVICE rid = {};
        rid.usUsagePage = 0x01;        // HID_USAGE_PAGE_GENERIC
        rid.usUsage = 0x02;            // HID_USAGE_GENERIC_MOUSE
        rid.dwFlags = RIDEV_INPUTSINK; // Add to this window even in background
        rid.hwndTarget = platform.hwnd;
        int result = RegisterRawInputDevices(&rid, 1, sizeof(rid));
        if (result == 0) TRACELOG(LOG_WARNING, "WIN32: Failed to register raw input devices [ERROR: %lu]", GetLastError());

        RECT clientRect = {};
        if (!GetClientRect(platform.hwnd, &clientRect)) TRACELOG(LOG_WARNING, "WIN32: Failed to get client rectangle [ERROR: %lu]", GetLastError());

        POINT topleft = { clientRect.left, clientRect.top };
        if (!ClientToScreen(platform.hwnd, &topleft)) TRACELOG(LOG_WARNING, "WIN32: Failed to get client to screen size [ERROR: %lu]", GetLastError());

        LONG width = clientRect.right - clientRect.left;
        LONG height = clientRect.bottom - clientRect.top;

        TRACELOG(LOG_INFO, "WIN32: Clip cursor client rect: [%d,%d %d,%d], top-left: (%d,%d)",
            clientRect.left, clientRect.top, clientRect.right, clientRect.bottom, topleft.x, topleft.y);

        LONG centerX = topleft.x + width/2;
        LONG centerY = topleft.y + height/2;
        RECT clipRect = { centerX, centerY, centerX + 1, centerY + 1 };
        if (!ClipCursor(&clipRect)) TRACELOG(LOG_WARNING, "WIN32: Failed to clip cursor [ERROR: %lu]", GetLastError());

        CORE.Input.Mouse.previousPosition = vec2{ 0, 0 };
        CORE.Input.Mouse.currentPosition = vec2{ 0, 0 };
        HideCursor();

        CORE.Input.Mouse.cursorLocked = true;
    }
}

// Swap back buffer with front buffer (screen drawing)
void SwapScreenBuffer(void)
{
    if (!platform.hdc) abort();

#if defined(GRAPHICS_API_OPENGL_SOFTWARE)
    // Update framebuffer
    rlCopyFramebuffer(0, 0, CORE.Window.render.width, CORE.Window.render.height, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, platform.pixels);

    // Force redraw
    InvalidateRect(platform.hwnd, nullptr, FALSE);
    UpdateWindow(platform.hwnd);
#else
    if (!SwapBuffers(platform.hdc)) TRACELOG(LOG_ERROR, "WIN32: Failed to swap buffers [ERROR: %lu]", GetLastError());
    if (!ValidateRect(platform.hwnd, nullptr)) TRACELOG(LOG_ERROR, "WIN32: Failed to validate screen rect [ERROR: %lu]", GetLastError());
#endif
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Misc
//----------------------------------------------------------------------------------

// Get elapsed time measure in seconds
double GetTime(void)
{
    double time = 0.0;
    LARGE_INTEGER now = {};
    QueryPerformanceCounter(&now);
    time = static_cast<double>((now.QuadPart - CORE.Time.base))/static_cast<double>(platform.timerFrequency.QuadPart);

    return time;
}

// Open URL with default system browser (if available)
// WARNING: This function is only safe to use if you control the URL given,
// a user could craft a malicious string to perform and undesired action
// NOTE: Some safety checks have been added to mitigate security issues
void OpenURL(std::string_view urlView)
{
    const std::string urlStr(urlView);
    const char *url = urlStr.c_str();

    // Security check to (partially) avoid malicious code
    if ((strchr(url, '\'') != nullptr) || (strchr(url, '\"') != nullptr))
    {
        // Filter characters: ' and "
        TRACELOG(LOG_WARNING, "SYSTEM: Provided URL could be potentially malicious, avoid [\'\"] characters");
    }
    else if ((strncmp(url, "http://", 7) != 0) && (strncmp(url, "https://", 8) != 0))
    {
        // Only allow URL starting with "http://" or "https://" protocols
        TRACELOG(LOG_WARNING, "SYSTEM: Provided URL must start with 'http://' or 'https://' protocols");
    }
    else
    {
        int len = strlen(url) + 16;
        char *cmd = static_cast<char *>(RL_CALLOC(len, sizeof(char)));
        snprintf(cmd, len, "explorer \"%s\"", url);

        int result = system(cmd);
        if (result == -1) TRACELOG(LOG_WARNING, "OpenURL() child process could not be created");
        RL_FREE(cmd);
    }
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Inputs
//----------------------------------------------------------------------------------

// Set internal gamepad mappings
int SetGamepadMappings(std::string_view mappingsView)
{
    const std::string mappingsStr(mappingsView);
    const char *mappings = mappingsStr.c_str();

    TRACELOG(LOG_WARNING, "SetGamepadMappings not implemented");

    return -1;
}

// Set gamepad vibration
void SetGamepadVibration(int gamepad, float leftMotor, float rightMotor, float duration)
{
    TRACELOG(LOG_WARNING, "SetGamepadVibration not implemented");
}

// Set mouse position XY
void SetMousePosition(int x, int y)
{
    if (!CORE.Input.Mouse.cursorLocked)
    {
        // Set the cursor position relative to the top-left corner of the window's client area
        POINT topLeft = {};

        // Skip if window is minimized or if ClientToScreen calculation fails
        if (!IsIconic(platform.hwnd) && ClientToScreen(platform.hwnd, &topLeft))
        {
            vec2 scale = CORE.Input.Mouse.scale;
            int xScaled = static_cast<int>(roundf(x/scale.x));
            int yScaled = static_cast<int>(roundf(y/scale.y));
            if (SetCursorPos(topLeft.x + xScaled, topLeft.y + yScaled))
            {
                CORE.Input.Mouse.currentPosition = vec2{ static_cast<float>(xScaled), static_cast<float>(yScaled) };
            }
        }
    }
    else TRACELOG(LOG_WARNING, "INPUT: MOUSE: Unable to set mouse position. The cursor is locked");
}

// Set mouse cursor
void SetMouseCursor(int cursor)
{
    LPCWSTR cursorName = GetCursorName(cursor);
    HCURSOR hcursor = LoadCursorW(nullptr, cursorName);
    if (!hcursor) TRACELOG(LOG_ERROR, "WIN32: Failed to load requested cursor [ERROR: %lu]", GetLastError());

    SetCursor(hcursor);
    CORE.Input.Mouse.cursorHidden = false;
}

// Get physical key name
std::string GetKeyName(int key)
{
    TRACELOG(LOG_WARNING, "GetKeyName not implemented");
    return {};
}

// Register all input events
void PollInputEvents(void)
{
    // Reset keys/chars pressed registered
    CORE.Input.Keyboard.keyPressedQueueCount = 0;
    CORE.Input.Keyboard.charPressedQueueCount = 0;

    // Reset key repeats
    for (int i = 0; i < MAX_KEYBOARD_KEYS; i++) CORE.Input.Keyboard.keyRepeatInFrame[i] = 0;

    // Register previous mouse states
    for (int i = 0; i < MAX_MOUSE_BUTTONS; i++) CORE.Input.Mouse.previousButtonState[i] = CORE.Input.Mouse.currentButtonState[i];

    // Reset last gamepad button/axis registered state
    CORE.Input.Gamepad.lastButtonPressed = 0; // GAMEPAD_BUTTON_UNKNOWN
    //CORE.Input.Gamepad.axisCount = 0;

    // Register previous touch states
    for (int i = 0; i < MAX_TOUCH_POINTS; i++) CORE.Input.Touch.previousTouchState[i] = CORE.Input.Touch.currentTouchState[i];

    // Reset touch positions
    // TODO: It resets on target platform the mouse position and not filled again until a move-event,
    // so, if mouse is not moved it returns a (0, 0) position... this behaviour should be reviewed!
    //for (int i = 0; i < MAX_TOUCH_POINTS; i++) CORE.Input.Touch.position[i] = vec2{ 0, 0 };

    memcpy(CORE.Input.Keyboard.previousKeyState, CORE.Input.Keyboard.currentKeyState, sizeof(CORE.Input.Keyboard.previousKeyState));

    // Register previous mouse wheel state
    CORE.Input.Mouse.previousWheelMove = CORE.Input.Mouse.currentWheelMove;
    CORE.Input.Mouse.currentWheelMove = vec2{ 0.0f, 0.0f };

    // Register previous mouse position
    CORE.Input.Mouse.previousPosition = CORE.Input.Mouse.currentPosition;

    CORE.Window.resizedLastFrame = false;
    CORE.Window.shouldClose = false;

    // Process windows messages
    MSG msg = {};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if ((platform.previousRenderWidth != CORE.Window.render.width) || (platform.previousRenderHeight != CORE.Window.render.height))
    {
        SetupViewport(CORE.Window.render.width, CORE.Window.render.height);
#if defined(GRAPHICS_API_OPENGL_SOFTWARE)
        if (rlGetVersion() == RL_OPENGL_SOFTWARE)
        {
            BITMAPINFO bmi = {};
            ZeroMemory(&bmi, sizeof(bmi));
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = static_cast<LONG>(CORE.Window.render.width);
            bmi.bmiHeader.biHeight = -static_cast<LONG>((CORE.Window.render.height)); // Top-down bitmap
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount  = 32;         // 32-bit BGRA
            bmi.bmiHeader.biCompression = BI_RGB;

            platform.hbitmap = CreateDIBSection(
                platform.hdcmem, &bmi, DIB_RGB_COLORS,
                reinterpret_cast<void **>(&platform.pixels), nullptr, 0);

            HGDIOBJ oldBitmap = SelectObject(platform.hdcmem, platform.hbitmap);
            DeleteObject(oldBitmap);

            swResize(CORE.Window.render.width, CORE.Window.render.height);
        }
#endif
        platform.previousRenderWidth = CORE.Window.render.width;
        platform.previousRenderHeight = CORE.Window.render.height;
    }

    // Windows will miss key up events for specific keys; check them manually
    if ((GetKeyState(VK_LSHIFT) & 0x8000) == 0) CORE.Input.Keyboard.currentKeyState[KEY_LEFT_SHIFT] = 0;
    if ((GetKeyState(VK_RSHIFT) & 0x8000) == 0) CORE.Input.Keyboard.currentKeyState[KEY_RIGHT_SHIFT] = 0;
    if ((GetKeyState(VK_LWIN) & 0x8000) == 0) CORE.Input.Keyboard.currentKeyState[KEY_LEFT_SUPER] = 0;
    if ((GetKeyState(VK_RWIN) & 0x8000) == 0) CORE.Input.Keyboard.currentKeyState[KEY_RIGHT_SUPER] = 0;

    // Windows won't send a WM_KEYDOWN event for print screen; check it manually
    bool isPrintScreenDown = ((GetAsyncKeyState(VK_SNAPSHOT) & 0x8000) != 0) && (GetForegroundWindow() == platform.hwnd);
    CORE.Input.Keyboard.currentKeyState[KEY_PRINT_SCREEN] = isPrintScreenDown;
    if (IsKeyPressed(KEY_PRINT_SCREEN) && (CORE.Input.Keyboard.exitKey == KEY_PRINT_SCREEN)) CORE.Window.shouldClose = true;
}

//----------------------------------------------------------------------------------
// Module Internal Functions Definition
//----------------------------------------------------------------------------------
// Initialize modern OpenGL context
// NOTE: Creating a dummy context first to query required extensions
HGLRC InitOpenGL(HWND hwnd, HDC hdc)
{
    // Create an invisible window for initial dummy OpenGL context
    WNDCLASSEXW dummyWindowClass = {
        .cbSize = sizeof(WNDCLASSEXW),
        .style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
        .lpfnWndProc = DefWindowProcW,
        .hInstance = GetModuleHandleW(nullptr),
        .lpszClassName = L"wglDummy"
    };
    RegisterClassExW(&dummyWindowClass);

    HWND dummyWindow = CreateWindowExW(
        0, dummyWindowClass.lpszClassName, L"dummyWindow",
        0, 0, 0, 1, 1, nullptr, nullptr, nullptr, nullptr);

    HDC dummyHDC = GetDC(dummyWindow);

    // Create a dummy OpenGL context to get WGL extensions
    PIXELFORMATDESCRIPTOR pixelFormatDesc = {
        .nSize = sizeof(PIXELFORMATDESCRIPTOR),
        .nVersion = 1,
        .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        .iPixelType = PFD_TYPE_RGBA,
        .cColorBits = 32,
        .cAlphaBits = 8,
        .cDepthBits = 24,
        .cStencilBits = 8,
        .iLayerType = PFD_MAIN_PLANE
    };

    int dummyPixelFormat = ChoosePixelFormat(dummyHDC, &pixelFormatDesc);
    SetPixelFormat(dummyHDC, dummyPixelFormat, &pixelFormatDesc);
    //int pixelFormat = ChoosePixelFormat(platform.hdc, &pixelFormatDesc);
    //if (!pixelFormat) { TRACELOG(LOG_ERROR, "%s failed, error=%lu", "ChoosePixelFormat", GetLastError()); return -1; }
    //if (!SetPixelFormat(platform.hdc, pixelFormat, &pixelFormatDesc)) { TRACELOG(LOG_ERROR, "%s failed, error=%lu", "SetPixelFormat", GetLastError()); return -1; }

    HGLRC tempContext = wglCreateContext(dummyHDC);
    //if (!tempContext) { TRACELOG(LOG_ERROR, "%s failed, error=%lu", "wglCreateContext", GetLastError()); return -1; }
    BOOL result = wglMakeCurrent(dummyHDC, tempContext);
    //if (!result) { TRACELOG(LOG_ERROR, "%s failed, error=%lu", "wglMakeCurrent", GetLastError()); return -1; }

    // Load WGL extension entry points
    wglCreateContextAttribsARB = reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(wglGetProcAddress("wglCreateContextAttribsARB"));
    wglChoosePixelFormatARB = reinterpret_cast<PFNWGLCHOOSEPIXELFORMATARBPROC>(wglGetProcAddress("wglChoosePixelFormatARB"));
    wglSwapIntervalEXT = reinterpret_cast<PFNWGLSWAPINTERVALEXTPROC>(wglGetProcAddress("wglSwapIntervalEXT"));
    wglGetExtensionsStringARB = reinterpret_cast<PFNWGLGETEXTENSIONSSTRINGARBPROC>(wglGetProcAddress("wglGetExtensionsStringARB"));

    // Setup modern pixel format if extension is available
    BOOL setPixelFormatSucceeded = FALSE;
    if (wglChoosePixelFormatARB)
    {
        // Try the requested MSAA samples first, degrading to the closest samples count the graphics device provides
        int requestedSamples = GetWindowRequestedSamples();
        int samples = requestedSamples;

        while (setPixelFormatSucceeded == FALSE)
        {
            int pixelFormatAttribs[] = {
                WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
                WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
                WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
                WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
                WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
                WGL_COLOR_BITS_ARB, 32,
                WGL_RED_BITS_ARB, 8,
                WGL_GREEN_BITS_ARB, 8,
                WGL_BLUE_BITS_ARB, 8,
                WGL_ALPHA_BITS_ARB, 8,
                WGL_DEPTH_BITS_ARB, 24,
                WGL_STENCIL_BITS_ARB, 8,
                WGL_SAMPLES_ARB, samples,
                WGL_SAMPLE_BUFFERS_ARB, (samples > 0)? 1 : 0,
                0 // Terminator
            };

            int format = 0;
            UINT numFormats = 0;
            if (wglChoosePixelFormatARB(hdc, pixelFormatAttribs, nullptr, 1, &format, &numFormats) && (numFormats > 0))
            {
                PIXELFORMATDESCRIPTOR newPixelFormatDescriptor = {};
                DescribePixelFormat(hdc, format, sizeof(newPixelFormatDescriptor), &newPixelFormatDescriptor);

                if (SetPixelFormat(hdc, format, &newPixelFormatDescriptor))
                {
                    setPixelFormatSucceeded = TRUE;
                    CORE.Window.samples = samples;
                    break;
                }
            }

            // Nothing left to try, request the pixel format without multisampling
            if (samples == 0) break;
            if (samples <= 2) samples = 0;
            else samples /= 2;
        }
    }

    if (!setPixelFormatSucceeded)
    {
        // If modern pixel format selection failed, fall back to old SetPixelFormat
        int pixelFormat = ChoosePixelFormat(hdc, &pixelFormatDesc);
        setPixelFormatSucceeded = SetPixelFormat(hdc, pixelFormat, &pixelFormatDesc);

        if (setPixelFormatSucceeded) TRACELOG(LOG_WARNING, "WGL: Using legacy pixel format, MSAA not available");
        else TRACELOG(LOG_ERROR, "WGL: Unable to find a suitable pixel format with this graphics driver");
    }

    // Create real modern OpenGL context (3.3 core)
    HGLRC realContext = nullptr;
    if (wglCreateContextAttribsARB)
    {
        int glContextVersionMajor = 1;
        int glContextVersionMinor = 1;
        int glContextProfile = WGL_CONTEXT_CORE_PROFILE_BIT_ARB;

        if (rlGetVersion() == RL_OPENGL_21)         // Request OpenGL 2.1 context
        {
            glContextVersionMajor = 2;
            glContextVersionMinor = 1;
        }
        else if (rlGetVersion() == RL_OPENGL_33)    // Request OpenGL 3.3 context
        {
            glContextVersionMajor = 3;
            glContextVersionMinor = 3;
        }
        else if (rlGetVersion() == RL_OPENGL_43)    // Request OpenGL 4.3 context
        {
            glContextVersionMajor = 4;
            glContextVersionMinor = 3;
        }
        else if (rlGetVersion() == RL_OPENGL_46)    // Request OpenGL 4.6 context
        {
            glContextVersionMajor = 4;
            glContextVersionMinor = 6;
        }
        else if (rlGetVersion() == RL_OPENGL_ES_20) // Request OpenGL ES 2.0 context
        {
            if (IsWglExtensionAvailable(platform.hdc, "WGL_EXT_create_context_es_profile") ||
                IsWglExtensionAvailable(platform.hdc, "WGL_EXT_create_context_es2_profile"))
            {
                glContextVersionMajor = 2;
                glContextVersionMinor = 0;
                glContextProfile = WGL_CONTEXT_ES_PROFILE_BIT_EXT;
            }
            else TRACELOG(LOG_WARNING, "GL: OpenGL ES context not supported by GPU");
        }
        else if (rlGetVersion() == RL_OPENGL_ES_30) // Request OpenGL ES 3.0 context
        {
            if (IsWglExtensionAvailable(platform.hdc, "WGL_EXT_create_context_es_profile") ||
                IsWglExtensionAvailable(platform.hdc, "WGL_EXT_create_context_es2_profile"))
            {
                glContextVersionMajor = 3;
                glContextVersionMinor = 0;
                glContextProfile = WGL_CONTEXT_ES_PROFILE_BIT_EXT;
            }
            else TRACELOG(LOG_WARNING, "GL: OpenGL ES context not supported by GPU");
        }

        int contextAttribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, glContextVersionMajor,
            WGL_CONTEXT_MINOR_VERSION_ARB, glContextVersionMinor,
            WGL_CONTEXT_PROFILE_MASK_ARB, glContextProfile, // WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB, WGL_CONTEXT_ES_PROFILE_BIT_EXT (if supported)
            //WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB | WGL_CONTEXT_DEBUG_BIT_ARB [glDebugMessageCallback()]
            0 // Terminator
        };

        // NOTE: Not sharing context resources so, second parameters is NULL
        realContext = wglCreateContextAttribsARB(hdc, nullptr, contextAttribs);

        // Check for error context creation errors
        // ERROR_INVALID_VERSION_ARB (0x2095)
        // ERROR_INVALID_PROFILE_ARB (0x2096)
        if (realContext == nullptr) TRACELOG(LOG_ERROR, "GL: Error creating requested context: %lu", GetLastError());
    }
    else
    {
        // If modern context extension isn't available, fall back to classic context creation
        realContext = wglCreateContext(hdc);
    }

    // Cleanup dummy temp context and window
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(tempContext);
    ReleaseDC(dummyWindow, dummyHDC);
    DestroyWindow(dummyWindow);
    UnregisterClassW(dummyWindowClass.lpszClassName, dummyWindowClass.hInstance);

    // Activate real context
    if (realContext) wglMakeCurrent(hdc, realContext);

    // Once a real modern OpenGL context is created,
    // required extensions can be loaded (function pointers)
    rlLoadExtensions(WglGetProcAddress);

    return realContext;
}

// Initialize platform: graphics, inputs and more
int InitPlatform(void)
{
    int result = 0;

    platform.appScreenWidth = CORE.Window.screen.width;
    platform.appScreenHeight = CORE.Window.screen.height;
    platform.desiredFlags = SanitizeFlags(CORE.Window.flags);

    // NOTE: From this point CORE.Window.flags should always reflect the actual state of the window
    CORE.Window.flags = FLAG_WINDOW_HIDDEN | (platform.desiredFlags & FLAG_MASK_NO_UPDATE);
    CORE.Window.screenMax.width = 9999;
    CORE.Window.screenMax.height = 9999;

    LoadWin32FunctionPointers();

    SetProcessDpiAwareCompat();

    HINSTANCE hInstance = GetModuleHandleW(0);

    // Define window class
    WNDCLASSEXW windowClass = {
        .cbSize = sizeof(WNDCLASSEXW),
        .style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
        .lpfnWndProc = WndProc,                         // Custom procedure assigned
        .cbWndExtra = sizeof(LONG_PTR),                 // extra space for the Tuple object ptr
        .hInstance = hInstance,
        .hCursor = LoadCursorW(nullptr, static_cast<LPCWSTR>(IDC_ARROW)), // TODO: Check if this is really required, since WM_SETCURSOR event is processed
        .lpszClassName = CLASS_NAME                     // Class name: L"raylibWindow"
    };

    // Load user-provided icon if available
    // NOTE: raylib resource file defaults to GLFW_ICON id, so looking for same identifier
    windowClass.hIcon = LoadImageW(hInstance, L"GLFW_ICON", IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
    if (!windowClass.hIcon) windowClass.hIcon = LoadImageW(nullptr, static_cast<LPCWSTR>(IDI_APPLICATION), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);

    // Register window class
    result = static_cast<int>(RegisterClassExW(&windowClass));
    if (result == 0) TRACELOG(LOG_ERROR, "WIN32: Failed to register window class [ERROR: %lu]", GetLastError());

    // Get primary monitor info
    POINT primaryTopLeft = {};
    HMONITOR monitor = MonitorFromPoint(primaryTopLeft, MONITOR_DEFAULTTOPRIMARY);
    if (monitor != nullptr)
    {
        MONITORINFO info = {};
        info.cbSize = sizeof(info);
        result = static_cast<int>(GetMonitorInfoW(monitor, &info));

        if (result == 0) TRACELOG(LOG_WARNING, "WIN32: DISPLAY: Failed to get monitor info [ERROR: %u]", GetLastError());
        else
        {
            CORE.Window.display.width = info.rcMonitor.right - info.rcMonitor.left;
            CORE.Window.display.height = info.rcMonitor.bottom - info.rcMonitor.top;
        }
    }
    else TRACELOG(LOG_WARNING, "WIN32: DISPLAY: Failed to get primary monitor from point [ERROR: %u]", GetLastError());

    // Create window
    // NOTE: Title string needs to be converted to WCHAR
    WCHAR *titleWide = nullptr;
    A_TO_W_ALLOCA(titleWide, CORE.Window.title.c_str());

    // Create window and get handle
    platform.hwnd = CreateWindowExW(
        WINDOW_STYLE_EX,
        CLASS_NAME,
        titleWide,
        MakeWindowStyle(CORE.Window.flags | FLAG_WINDOW_HIDDEN),
        CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, // Window size/position will be overridden after creation
        nullptr, nullptr,
        GetModuleHandleW(nullptr), nullptr);

    if (!platform.hwnd)
    {
        TRACELOG(LOG_ERROR, "WIN32: WINDOW: Failed to create window [ERROR: %lu]", GetLastError());
        return -1;
    }

    // Get handle to device drawing context
    // NOTE: Windows GDI object that represents a drawing surface
    platform.hdc = GetDC(platform.hwnd);

    bool highDPIWindowFlag = platform.desiredFlags & FLAG_WINDOW_HIGHDPI;
    UINT dpi = GetDpiForWindowCompat(platform.hwnd);
    float dpiScale = dpi/96.0f;

    if (rlGetVersion() != RL_OPENGL_SOFTWARE)
    {
        // Init hardware-accelerated OpenGL modern context
        platform.glContext = InitOpenGL(platform.hwnd, platform.hdc);
    }

    CORE.Window.ready = true;

    RECT rect = { 0, 0, platform.appScreenWidth, platform.appScreenHeight };
    if (highDPIWindowFlag)
    {
        rect.right = static_cast<LONG>(roundf(platform.appScreenWidth*dpiScale));
        rect.bottom = static_cast<LONG>(roundf(platform.appScreenHeight*dpiScale));
    }
    AdjustWindowRectExForDpiCompat(
        &rect, MakeWindowStyle(platform.desiredFlags),
        FALSE, WINDOW_STYLE_EX, dpi
    );

    int windowWidth = rect.right - rect.left;
    int windowHeight = rect.bottom - rect.top;

    HMONITOR currentMonitor = MonitorFromWindow(platform.hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (GetMonitorInfoW(currentMonitor, &monitorInfo))
    {
        // rcWork is the monitor area, not including the taskbar
        RECT rcWork = monitorInfo.rcWork;
        int monitorWidth = rcWork.right - rcWork.left;
        int monitorHeight = rcWork.bottom - rcWork.top;
        int x = rcWork.left + (monitorWidth - windowWidth)/2;
        int y = rcWork.top + (monitorHeight - windowHeight)/2;

        // Ensure the window doesn't go off-screen; otherwise, the user might not be able to move it
        if (x < rcWork.left) x = rcWork.left;
        if (y < rcWork.top) y = rcWork.top;

        // Center the window and set it to the correct DPI-adjusted size
        SetWindowPos(platform.hwnd, nullptr, x, y, windowWidth, windowHeight, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    else TRACELOG(LOG_WARNING, "DISPLAY: Unable to retrieve information about monitor");

    if ((platform.desiredFlags & FLAG_WINDOW_HIDDEN) == 0)
    {
        // Show window and taskbar icon, activate unless FLAG_WINDOW_UNFOCUSED is set
        bool isUnfocused = ((platform.desiredFlags & FLAG_WINDOW_UNFOCUSED) != 0);
        if (isUnfocused) CORE.Window.flags |= FLAG_WINDOW_UNFOCUSED;
        int nCmdShow = (isUnfocused)? SW_SHOWNA : SW_SHOWDEFAULT;
        ShowWindow(platform.hwnd, nCmdShow);
    }

    // Update flags (in case of deferred state change required)
    UpdateFlags(platform.hwnd, platform.desiredFlags, platform.appScreenWidth, platform.appScreenHeight);

    CORE.Window.resizedLastFrame = false;
    TRACELOG(LOG_INFO, "DISPLAY: Device initialized successfully %s",
        FLAG_IS_SET(CORE.Window.flags, FLAG_WINDOW_HIGHDPI)? "(HighDPI)" : "");
    TRACELOG(LOG_INFO, "    > Display size: %i x %i", CORE.Window.display.width, CORE.Window.display.height);
    TRACELOG(LOG_INFO, "    > Screen size:  %i x %i", CORE.Window.screen.width, CORE.Window.screen.height);
    TRACELOG(LOG_INFO, "    > Render size:  %i x %i", CORE.Window.render.width, CORE.Window.render.height);
    TRACELOG(LOG_INFO, "    > Viewport offsets: %i, %i", CORE.Window.renderOffset.x, CORE.Window.renderOffset.y);

    if (rlGetVersion() == RL_OPENGL_SOFTWARE) // Using software renderer
    {
        TRACELOG(LOG_INFO, "GL: OpenGL device information:");
        TRACELOG(LOG_INFO, "    > Vendor:   %s", glGetString(GL_VENDOR));
        TRACELOG(LOG_INFO, "    > Renderer: %s", glGetString(GL_RENDERER));
        TRACELOG(LOG_INFO, "    > Version:  %s", glGetString(GL_VERSION));
        TRACELOG(LOG_INFO, "    > GLSL:     %s", "NOT SUPPORTED");

        // Initialize software framebuffer
        BITMAPINFO bmi = {};
        ZeroMemory(&bmi, sizeof(bmi));
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = static_cast<LONG>(CORE.Window.render.width);
        bmi.bmiHeader.biHeight = -static_cast<LONG>((CORE.Window.render.height)); // Top-down bitmap
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount  = 32;         // 32-bit BGRA
        bmi.bmiHeader.biCompression = BI_RGB;

        platform.hdcmem = CreateCompatibleDC(platform.hdc);

        platform.hbitmap = CreateDIBSection(
            platform.hdcmem, &bmi, DIB_RGB_COLORS,
            reinterpret_cast<void **>(&platform.pixels), nullptr, 0);

        SelectObject(platform.hdcmem, platform.hbitmap);
        //ReleaseDC(platform.hwnd, platform.hdc); // Not required because the window class uses CS_OWNDC
    }

    // Initialize timing system
    //----------------------------------------------------------------------------
    LARGE_INTEGER time = {};
    QueryPerformanceCounter(&time);
    QueryPerformanceFrequency(&platform.timerFrequency);
    CORE.Time.base = time.QuadPart;

    InitTimer();
    //----------------------------------------------------------------------------

    // Initialize storage system
    //----------------------------------------------------------------------------
    CORE.Storage.basePath = GetWorkingDirectory();
    //----------------------------------------------------------------------------

    TRACELOG(LOG_INFO, "PLATFORM: DESKTOP: WIN32: Initialized successfully");

    return {};
}

// Close platform
void ClosePlatform(void)
{
    // Clear required platform data
    if (platform.hwnd)
    {
        // Disable cursor lock (if enabled) and unregister raw input devices
        EnableCursor();

        BOOL result = DestroyWindow(platform.hwnd);
        if (!result) TRACELOG(LOG_WARNING, "WIN32: WINDOW: Failed on window destroy [ERROR: %u]", GetLastError());

        HINSTANCE hInstance = GetModuleHandleW(0);
        result = UnregisterClassW(CLASS_NAME, hInstance);
        if (!result) TRACELOG(LOG_WARNING, "WIN32: WINDOW: Failed to unregister window class [ERROR: %u]", GetLastError());

        DeletePlatformIconHandles();

        platform.hwnd = nullptr;
    }

    // Clear clipboard
    if (platform.clipboardText != nullptr)
    {
        RL_FREE(platform.clipboardText);
        platform.clipboardText = nullptr;
    }

    // Unload shcore.dll
    if (shcore != nullptr)
    {
        FreeLibrary(shcore);
        shcore = nullptr;
    }

#if SUPPORT_WINMM_HIGHRES_TIMER && !SUPPORT_BUSY_WAIT_LOOP
    timeEndPeriod(1); // Restore time period
#endif
}

// Window procedure, message processing callback
// NOTE: All window event messages are processed here
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    LRESULT result = 0;

    // Sanity check
    DWORD mask = STYLE_MASK_ALL;
    if (platform.hwnd == hwnd)
    {
        if (msg == WM_WINDOWPOSCHANGING) mask &= ~(WS_MINIMIZE | WS_MAXIMIZE);
        CheckFlags("WndProc", hwnd, CORE.Window.flags, MakeWindowStyle(CORE.Window.flags), mask);
    }

    FlagsOp flagsOp = {};
    FlagsOp *deferredFlags = &flagsOp;

    // Message processing
    //------------------------------------------------------------------------------------
    switch (msg)
    {
        case WM_CREATE:
        {
            // WARNING: Not recommended to do OpenGL intialization at this point

        } break;
        //case WM_ACTIVATE
        case WM_DESTROY:
        {
            // Clean up for window destruction
            if (rlGetVersion() == RL_OPENGL_SOFTWARE) // Using software renderer
            {
                if (platform.hdcmem)
                {
                    DeleteDC(platform.hdcmem);
                    platform.hdcmem = nullptr;
                }

                if (platform.hbitmap)
                {
                    DeleteObject(platform.hbitmap); // Clears platform.pixels data
                    platform.hbitmap = nullptr;
                    platform.pixels = nullptr; // NOTE: Pointer invalid after DeleteObject()
                }
            }
            else // OpenGL hardware renderer
            {
                wglMakeCurrent(platform.hdc, nullptr);
                if (platform.glContext)
                {
                    if (!wglDeleteContext(platform.glContext)) abort();
                    platform.glContext = nullptr;
                }
            }

            if (platform.hdc)
            {
                if (!ReleaseDC(hwnd, platform.hdc)) abort();
                platform.hdc = nullptr;
            }

            PostQuitMessage(0);

        } break;
        case WM_CLOSE: CORE.Window.shouldClose = true; break; // Window close button [x], ALT+F4
        //case WM_QUIT: // Application closing, not related to window
        case WM_KILLFOCUS:
        {
            CORE.Window.flags |= FLAG_WINDOW_UNFOCUSED;
            memset(CORE.Input.Keyboard.currentKeyState, 0, sizeof(CORE.Input.Keyboard.currentKeyState));
            memset(CORE.Input.Mouse.currentButtonState, 0, sizeof(CORE.Input.Mouse.currentButtonState));
            memset(CORE.Input.Touch.currentTouchState, 0, sizeof(CORE.Input.Touch.currentTouchState));
        } break;
        case WM_SETFOCUS: CORE.Window.flags &= ~FLAG_WINDOW_UNFOCUSED; break;
        case WM_SIZING: // Sent to a window that the user is resizing
        {
            if (CORE.Window.flags & FLAG_WINDOW_RESIZABLE)
            {
                //HandleWindowResize(hwnd, &platform.appScreenWidth, &platform.appScreenHeight);
            }

            result = TRUE;
        } break;
        case WM_SIZE:
        {
            // WARNING: Don't trust the docs, they say this message can not be obtained if not calling DefWindowProc()
            // in response to WM_WINDOWPOSCHANGED but looks like when a window is created,
            // this message can be obtained without getting WM_WINDOWPOSCHANGED
            HandleWindowResize(hwnd, &platform.appScreenWidth, &platform.appScreenHeight);
            result = 0; // If an application processes WM_SIZE message, it should return zero
        } break;
        case WM_GETMINMAXINFO:
        {
            UINT dpi = GetDpiForWindowCompat(hwnd);
            DWORD style = MakeWindowStyle(platform.desiredFlags);
            float clientScale = (CORE.Window.flags & FLAG_WINDOW_HIGHDPI)? (static_cast<float>(dpi))/96.0f : 1.0f;
            SIZE maxClientSize = { static_cast<LONG>(roundf(CORE.Window.screenMax.width*clientScale)), static_cast<LONG>(roundf(CORE.Window.screenMax.height*clientScale)) };
            SIZE maxWindowSize = CalcWindowSize(dpi, maxClientSize, style);
            SIZE minClientSize = { static_cast<LONG>(roundf(CORE.Window.screenMin.width*clientScale)), static_cast<LONG>(roundf(CORE.Window.screenMin.height*clientScale)) };
            SIZE minWindowSize = CalcWindowSize(dpi, minClientSize, style);

            LPMINMAXINFO lpmmi = reinterpret_cast<LPMINMAXINFO>(lparam);
            lpmmi->ptMaxSize.x = maxWindowSize.cx;
            lpmmi->ptMaxSize.y = maxWindowSize.cy;
            lpmmi->ptMaxTrackSize.x = maxWindowSize.cx;
            lpmmi->ptMaxTrackSize.y = maxWindowSize.cy;
            lpmmi->ptMinTrackSize.x = minWindowSize.cx;
            lpmmi->ptMinTrackSize.y = minWindowSize.cy;
        } break;
        case WM_STYLECHANGING:
        {
            if (wparam == GWL_STYLE)
            {
                STYLESTRUCT *ss = reinterpret_cast<STYLESTRUCT *>(lparam);
                GetStyleChangeFlagOps(CORE.Window.flags, ss, deferredFlags);

                UINT dpi = GetDpiForWindowCompat(hwnd);
                // Get client size (framebuffer inside the window)
                RECT rect = {};
                GetClientRect(hwnd, &rect);
                SIZE clientSize = { rect.right, rect.bottom };
                SIZE oldSize = CalcWindowSize(dpi, clientSize, ss->styleOld);
                SIZE newSize = CalcWindowSize(dpi, clientSize, ss->styleNew);

                if (oldSize.cx != newSize.cx || oldSize.cy != newSize.cy)
                {
                    TRACELOG(LOG_INFO, "WIN32: WINDOW: Resize from style change [%dx%d] to [%dx%d]", oldSize.cx, oldSize.cy, newSize.cx, newSize.cy);

                    if (CORE.Window.flags & FLAG_WINDOW_MAXIMIZED)
                    {
                        // looks like windows will automatically "unminimize" a window
                        // if a style changes modifies it's size
                        TRACELOG(LOG_INFO, "WIN32: WINDOW: Style change modified window size, removing maximized flag");
                        deferredFlags->clear |= FLAG_WINDOW_MAXIMIZED;
                    }
                }
            }
        } break;
        case WM_WINDOWPOSCHANGING:
        {
            WINDOWPOS *pos = reinterpret_cast<WINDOWPOS *>(lparam);
            if (pos->flags & SWP_SHOWWINDOW) deferredFlags->clear |= FLAG_WINDOW_HIDDEN;
            else if (pos->flags & SWP_HIDEWINDOW) deferredFlags->set |= FLAG_WINDOW_HIDDEN;

            Mized mized = MIZED_NONE;
            bool isIconic = IsIconic(hwnd);
            bool styleMinimized = !!(WS_MINIMIZE & GetWindowLongPtrW(hwnd, GWL_STYLE));
            if (isIconic != styleMinimized) TRACELOG(LOG_WARNING, "WIN32: IsIconic state different from WS_MINIMIZED state");

            if (isIconic) mized = MIZED_MIN;
            else
            {
                WINDOWPLACEMENT placement;
                placement.length = sizeof(placement);
                if (!GetWindowPlacement(hwnd, &placement)) TRACELOG(LOG_ERROR, "WIN32: WINDOW: FAiled to get monitor placement [ERROR: %lu]", GetLastError());

                if (placement.showCmd == SW_SHOWMAXIMIZED) mized = MIZED_MAX;
            }

            switch (mized)
            {
                case MIZED_NONE:
                {
                    deferredFlags->clear |= (FLAG_WINDOW_MINIMIZED | FLAG_WINDOW_MAXIMIZED);
                    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
                    MONITORINFO info;
                    info.cbSize = sizeof(info);
                    if (!GetMonitorInfoW(monitor, &info)) TRACELOG(LOG_ERROR, "WIN32: MONITOR: Failed to get monitor info [ERROR: %lu]", GetLastError());

                    if ((pos->x == info.rcMonitor.left) &&
                        (pos->y == info.rcMonitor.top) &&
                        (pos->cx == (info.rcMonitor.right - info.rcMonitor.left)) &&
                        (pos->cy == (info.rcMonitor.bottom - info.rcMonitor.top))) deferredFlags->set |= FLAG_BORDERLESS_WINDOWED_MODE;
                    else deferredFlags->clear |= FLAG_BORDERLESS_WINDOWED_MODE;

                } break;
                case MIZED_MIN:
                {
                    // !!! NOTE !!! Do not update the maximized/borderless
                    // flags because when hwnd is minimized it temporarily overrides
                    // the maximized state/flag which gets restored on SW_RESTORE
                    deferredFlags->set |= FLAG_WINDOW_MINIMIZED;
                } break;
                case MIZED_MAX:
                {
                    deferredFlags->clear |= FLAG_WINDOW_MINIMIZED;
                    deferredFlags->set |= FLAG_WINDOW_MAXIMIZED;
                } break;
                default: break;
            }
        } break;
        //case WM_MOVE: break;
        case WM_WINDOWPOSCHANGED:
        {
            WINDOWPOS *pos = reinterpret_cast<WINDOWPOS *>(lparam);
            if (!(pos->flags & SWP_NOSIZE)) HandleWindowResize(hwnd, &platform.appScreenWidth, &platform.appScreenHeight);

            DefWindowProc(hwnd, msg, wparam, lparam);
        } break;
        case WM_GETDPISCALEDSIZE:
        {
            SIZE *inoutSize = reinterpret_cast<SIZE *>(lparam);
            UINT newDpi = static_cast<UINT>(wparam); // TODO: WARNING: Converting from WPARAM = UINT_PTR

            // For the following flag changes, a window resize event should be posted,
            // Keep the pending size when minimized, maximized, or borderless
            if (CORE.Window.flags & FLAG_WINDOW_MINIMIZED) return TRUE;
            if (CORE.Window.flags & FLAG_WINDOW_MAXIMIZED) return TRUE;
            if (CORE.Window.flags & FLAG_BORDERLESS_WINDOWED_MODE) return TRUE;

            float dpiScale = (static_cast<float>(newDpi))/96.0f;
            bool dpiScaling = CORE.Window.flags & FLAG_WINDOW_HIGHDPI;
            // Calculate client area in pixels from points
            SIZE desired = {
                .cx = dpiScaling? static_cast<int>(roundf(static_cast<float>(platform.appScreenWidth)*dpiScale)) : platform.appScreenWidth,
                .cy = dpiScaling? static_cast<int>(roundf(static_cast<float>(platform.appScreenHeight)*dpiScale)) : platform.appScreenHeight
            };
            // Calculate and set window size, including title and window border
            *inoutSize = CalcWindowSize(newDpi, desired, MakeWindowStyle(CORE.Window.flags));

            result = TRUE;
        } break;
        case WM_DPICHANGED:
        {
            // Get current dpi scale factor
            //float scalex = LOWORD(wparam)/96.0f;
            //float scaley = HIWORD(wparam)/96.0f;

            RECT *suggestedRect = reinterpret_cast<RECT *>(lparam);

            // Never set the window size to anything other than the suggested rect here
            // Doing so can cause a window to stutter between monitors when transitioning between them
            if (platform.perMonitorDPIAwareV2)
            {
                BOOL succeeded = SetWindowPos(hwnd, nullptr,
                    suggestedRect->left, suggestedRect->top,
                    suggestedRect->right - suggestedRect->left,
                    suggestedRect->bottom - suggestedRect->top,
                    SWP_NOZORDER | SWP_NOACTIVATE);

                if (succeeded == FALSE) TRACELOG(LOG_ERROR, "Failed to set window position [ERROR: %lu]", GetLastError());
            }
            else if ((CORE.Window.flags & FLAG_WINDOW_HIGHDPI) && !(CORE.Window.flags & (FLAG_WINDOW_MINIMIZED | FLAG_WINDOW_MAXIMIZED | FLAG_BORDERLESS_WINDOWED_MODE)))
            {
                // This code only runs when per-monitor dpi awareness v1 is enabled. v1 doesn't send WM_GETDPISCALEDSIZE and the suggested RECT is subtly wrong (doesn't properly account for the title bar size)
                // The solution is to manually build a properly sized RECT (and ignore the advice of not using the suggested RECT). This makes a per-monitor v1 process match the behavior of per-monitor v2
                // To prevent the potential stutter when dragging the window between monitors, the manually sized rect keeps the same center as the suggested RECT
                RECT windowRect = {};
                RECT clientRect = {};
                GetWindowRect(hwnd, &windowRect);
                GetClientRect(hwnd, &clientRect);

                float dpiScale = (static_cast<float>(LOWORD(wparam)))/96.0f;
                int width = static_cast<int>(roundf(platform.appScreenWidth*dpiScale)) + (windowRect.right - windowRect.left) - clientRect.right;
                int height = static_cast<int>(roundf(platform.appScreenHeight*dpiScale)) + (windowRect.bottom - windowRect.top) - clientRect.bottom;
                int x = suggestedRect->left + ((suggestedRect->right - suggestedRect->left) - width)/2;
                int y = suggestedRect->top + ((suggestedRect->bottom - suggestedRect->top) - height)/2;

                BOOL succeeded = SetWindowPos(hwnd, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);

                if (succeeded == FALSE) TRACELOG(LOG_ERROR, "Failed to set window position [ERROR: %lu]", GetLastError());
            }

            HandleWindowResize(hwnd, &platform.appScreenWidth, &platform.appScreenHeight);

        } break;
        case WM_SETCURSOR:
        {
            // Called when mouse moves, enters/leaves window...
            if (LOWORD(lparam) == HTCLIENT)
            {
                SetCursor(CORE.Input.Mouse.cursorHidden? nullptr : LoadCursorW(nullptr, static_cast<LPCWSTR>(IDC_ARROW)));
                return 0;
            }

            result = DefWindowProc(hwnd, msg, wparam, lparam);
        } break;
        case WM_PAINT:
        {
            if (rlGetVersion() == RL_OPENGL_SOFTWARE) // Using software renderer
            {
                PAINTSTRUCT ps = {};
                HDC hdc = BeginPaint(hwnd, &ps);

                // Blit from memory DC to window DC
                BitBlt(hdc, 0, 0, CORE.Window.render.width, CORE.Window.render.height, platform.hdcmem, 0, 0, SRCCOPY);

                EndPaint(hwnd, &ps);
            }
            else DefWindowProc(hwnd, msg, wparam, lparam);
        } break;
        case WM_INPUT:
        {
            //HandleRawInput(lparam);
        } break;
        case WM_MOUSEMOVE:
        {
            if (!CORE.Input.Mouse.cursorLocked)
            {
                CORE.Input.Mouse.currentPosition.x = static_cast<float>(GET_X_LPARAM(lparam));
                CORE.Input.Mouse.currentPosition.y = static_cast<float>(GET_Y_LPARAM(lparam));
                CORE.Input.Touch.position[0] = CORE.Input.Mouse.currentPosition;
            }
        } break;
        case WM_SYSKEYDOWN:
        {
            HandleKey(wparam, lparam, 1);
            // WM_SYSKEYDOWN events should still call DefWindowProc to allow things like alt+F4 to work
            result = DefWindowProcW(hwnd, msg, wparam, lparam);
        } break;
        case WM_SYSKEYUP:
        {
            HandleKey(wparam, lparam, 0);
            result = DefWindowProcW(hwnd, msg, wparam, lparam);
        } break;
        case WM_KEYDOWN: HandleKey(wparam, lparam, 1); break;
        case WM_KEYUP: HandleKey(wparam, lparam, 0); break;
        case WM_SYSCOMMAND:
        {
            if ((wparam & 0xFFF0) == SC_KEYMENU)
            {
                // Prevent the window from freezing when pressing Alt or F10
                result = 0;
            }
            else
            {
                result = DefWindowProcW(hwnd, msg, wparam, lparam);
            }
        } break;
        case WM_LBUTTONDOWN: HandleMouseButton(MOUSE_BUTTON_LEFT, 1); break;
        case WM_LBUTTONUP  : HandleMouseButton(MOUSE_BUTTON_LEFT, 0); break;
        case WM_RBUTTONDOWN: HandleMouseButton(MOUSE_BUTTON_RIGHT, 1); break;
        case WM_RBUTTONUP  : HandleMouseButton(MOUSE_BUTTON_RIGHT, 0); break;
        case WM_MBUTTONDOWN: HandleMouseButton(MOUSE_BUTTON_MIDDLE, 1); break;
        case WM_MBUTTONUP  : HandleMouseButton(MOUSE_BUTTON_MIDDLE, 0); break;
        case WM_XBUTTONDOWN:
        {
            switch (HIWORD(wparam))
            {
                case XBUTTON1: HandleMouseButton(MOUSE_BUTTON_SIDE, 1); break;
                case XBUTTON2: HandleMouseButton(MOUSE_BUTTON_EXTRA, 1); break;
                default: TRACELOG(LOG_WARNING, "TODO: Handle ex mouse button DOWN [wparam=%u]", HIWORD(wparam)); break;
            }
        } break;
        case WM_XBUTTONUP:
        {
            switch (HIWORD(wparam))
            {
                case XBUTTON1: HandleMouseButton(MOUSE_BUTTON_SIDE, 0); break;
                case XBUTTON2: HandleMouseButton(MOUSE_BUTTON_EXTRA, 0); break;
                default: TRACELOG(LOG_WARNING, "TODO: Handle ex mouse button UP [wparam=%u]", HIWORD(wparam)); break;
            }
        } break;
        case WM_MOUSEWHEEL: CORE.Input.Mouse.currentWheelMove.y += (static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)))/WHEEL_DELTA; break;
        // Horizontal wheel movement is inverted to match GLFW
        case WM_MOUSEHWHEEL: CORE.Input.Mouse.currentWheelMove.x -= (static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)))/WHEEL_DELTA; break;

        default: result = DefWindowProcW(hwnd, msg, wparam, lparam); // Message passed directly for execution (default behaviour)
    }
    //------------------------------------------------------------------------------------

    // Sanity check for flags
    if (platform.hwnd == hwnd) CheckFlags("After WndProc", hwnd, CORE.Window.flags, MakeWindowStyle(CORE.Window.flags), mask);

    // Operations to execute after the above check
    if (flagsOp.set & flagsOp.clear) TRACELOG(LOG_WARNING, "WIN32: FLAGS: Flags 0x%x were both set and cleared", flagsOp.set & flagsOp.clear);

    DWORD save = CORE.Window.flags;
    CORE.Window.flags |= flagsOp.set;
    CORE.Window.flags &= ~flagsOp.clear;
    if (save != CORE.Window.flags) TRACELOG(LOG_DEBUG, "WIN32: FLAGS: Current deferred flags: 0x%x > 0x%x (diff 0x%x)", save, CORE.Window.flags, save ^ CORE.Window.flags);

    return result;
}

// Handle keyboard input event
static void HandleKey(WPARAM wparam, LPARAM lparam, char state)
{
    bool isExtendedKey = ((HIWORD(lparam) & KF_EXTENDED) != 0);

    WORD scancode = LOBYTE(HIWORD(lparam));
    if (scancode == 0)
    {
        // Certain keys don't arrive with a scancode; fall back to virtual key mapping
        scancode = MapVirtualKeyW(static_cast<UINT>(wparam), MAPVK_VK_TO_VSC);
    }

    KeyboardKey key = KEY_NULL;
    if (isExtendedKey)
    {
        unsigned int index = scancode - EXTENDED_KEY_OFFSET;
        unsigned int arrayLength = sizeof(keyboardKeyFromExtendedScancode)/sizeof(keyboardKeyFromExtendedScancode[0]);
        if (index < arrayLength) key = keyboardKeyFromExtendedScancode[index];
    }
    else
    {
        unsigned int arrayLength = sizeof(keyboardKeyFromScancode)/sizeof(keyboardKeyFromScancode[0]);
        if (scancode < arrayLength) key = keyboardKeyFromScancode[scancode];
    }

    bool ignoreKey = false;
    if ((wparam == VK_CONTROL) && !isExtendedKey)
    {
        // Pressing AltGr causes Windows to send a fake Left Ctrl message
        // GLFW does something similar in win32_window.c
        MSG next = {};
        LONG time = GetMessageTime();

        if (PeekMessageW(&next, nullptr, 0, 0, PM_NOREMOVE))
        {
            // If the current message is Left Ctrl (VK_CONTROL) and the next message is AltGr (VK_MENU) with the exact same timestamp,
            // silently drop the fake control key event
            bool isNextExtendedKey = ((HIWORD(next.lParam) & KF_EXTENDED) != 0);
            if ((next.wParam == VK_MENU) && (static_cast<LONG>(next.time) == time) && isNextExtendedKey)
            {
                if ((next.message == WM_KEYDOWN) || (next.message == WM_SYSKEYDOWN) ||
                    (next.message == WM_KEYUP) || (next.message == WM_SYSKEYUP)) ignoreKey = true;
            }
        }
    }

    if (!ignoreKey)
    {
        if (key != KEY_NULL)
        {
            CORE.Input.Keyboard.currentKeyState[key] = state;

            bool isKeyRepeatEvent = ((HIWORD(lparam) & KF_REPEAT) != 0);
            if (isKeyRepeatEvent && (state == 1)) CORE.Input.Keyboard.keyRepeatInFrame[key] = 1;
            if (!isKeyRepeatEvent && (key == CORE.Input.Keyboard.exitKey) && (state == 1)) CORE.Window.shouldClose = true;
        }
        else TRACELOG(LOG_WARNING, "INPUT: Unknown (or currently unhandled) virtual keycode %d (0x%x)", wparam, wparam);
    }

    // TODO: Add key to the queue as well?
}

// Handle mouse button input event
static void HandleMouseButton(int button, char state)
{
    if (state == 1)
    {
        // Capture the mouse on MOUSEDOWN events and no other buttons are pressed
        bool allMouseButtonsReleased = true;
        for (unsigned int i = 0; i < MAX_MOUSE_BUTTONS; i++)
        {
            if (CORE.Input.Mouse.currentButtonState[i] != 0)
            {
                allMouseButtonsReleased = false;
                break;
            }
        }

        if (allMouseButtonsReleased) SetCapture(platform.hwnd);
    }

    // Register current mouse button state
    CORE.Input.Mouse.currentButtonState[button] = state;
    CORE.Input.Touch.currentTouchState[button] = state;

    if (state == 0)
    {
        // Release mouse capture when all mouse buttons are released
        bool allMouseButtonsReleased = true;
        for (unsigned int i = 0; i < MAX_MOUSE_BUTTONS; i++)
        {
            if (CORE.Input.Mouse.currentButtonState[i] != 0)
            {
                allMouseButtonsReleased = false;
                break;
            }
        }

        if (allMouseButtonsReleased) ReleaseCapture();
    }
}

// Handle raw input event
static void HandleRawInput(LPARAM lparam)
{
    RAWINPUT input = {};
    UINT inputSize = 0;

    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, nullptr, &inputSize, sizeof(RAWINPUTHEADER)) != 0) return;
    if (inputSize > sizeof(input)) return;

    UINT size = GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, &input, &inputSize, sizeof(RAWINPUTHEADER));

    if (size == static_cast<UINT>(1)) TRACELOG(LOG_ERROR, "WIN32: Failed to get raw input data [ERROR: %lu]", GetLastError());

    if (input.header.dwType != RIM_TYPEMOUSE) TRACELOG(LOG_ERROR, "WIN32: Unexpected WM_INPUT type %lu", input.header.dwType);

    if (input.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) TRACELOG(LOG_ERROR, "TODO: Handle absolute mouse inputs");

    if (input.data.mouse.usFlags & MOUSE_VIRTUAL_DESKTOP) TRACELOG(LOG_ERROR, "TODO: Handle virtual desktop mouse inputs");

    // Trick to keep the mouse position at (0,0) and instead move
    // the previous position so a proper mouse delta can still be retrieved
    //CORE.Input.Mouse.previousPosition.x -= input.data.mouse.lLastX;
    //CORE.Input.Mouse.previousPosition.y -= input.data.mouse.lLastY;
    //if (CORE.Input.Mouse.currentPosition.x != 0) abort();
    //if (CORE.Input.Mouse.currentPosition.y != 0) abort();
}

// Handle window resizing event
static void HandleWindowResize(HWND hwnd, int *width, int *height)
{
    if (CORE.Window.flags & FLAG_WINDOW_MINIMIZED) return;

    // Get client size (framebuffer inside the window)
    RECT rect = {};
    GetClientRect(hwnd, &rect);
    SIZE clientSize = { rect.right, rect.bottom };

    if ((CORE.Window.render.width != clientSize.cx) || (CORE.Window.render.height != clientSize.cy)) CORE.Window.resizedLastFrame = true;
    CORE.Window.render.width = static_cast<int>(clientSize.cx);
    CORE.Window.render.height = static_cast<int>(clientSize.cy);
    CORE.Window.currentFbo.width = static_cast<int>(clientSize.cx);
    CORE.Window.currentFbo.height = static_cast<int>(clientSize.cy);

    float dpiScale = (static_cast<float>(GetDpiForWindowCompat(hwnd)))/96.0f;
    bool highdpi = !!(CORE.Window.flags & FLAG_WINDOW_HIGHDPI);
    unsigned int screenWidth = highdpi? static_cast<unsigned int>(roundf((static_cast<float>(clientSize.cx))/dpiScale)) : clientSize.cx;
    unsigned int screenHeight = highdpi? static_cast<unsigned int>(roundf((static_cast<float>(clientSize.cy))/dpiScale)) : clientSize.cy;
    CORE.Window.screen.width = screenWidth;
    CORE.Window.screen.height = screenHeight;

    if (AdoptWindowResize(CORE.Window.flags))
    {
        TRACELOG(LOG_DEBUG, "WIN32: WINDOW: Updating app size to [%ix%i] from window resize", screenWidth, screenHeight);
        *width = screenWidth;
        *height = screenHeight;
    }

    CORE.Window.screenScale = glm::scale(mat4(1.0f), vec3{static_cast<float>(CORE.Window.render.width)/CORE.Window.screen.width,
        static_cast<float>(CORE.Window.render.height)/CORE.Window.screen.height, 1.0f});

    if (CORE.Window.flags & FLAG_WINDOW_HIGHDPI)
    {
        float mouseScale = 1.0f/dpiScale;
        CORE.Input.Mouse.scale = vec2{ mouseScale, mouseScale };
    }
}

// Update window style
static void UpdateWindowStyle(HWND hwnd, unsigned desiredFlags)
{
    DWORD current = STYLE_MASK_WRITABLE & MakeWindowStyle(CORE.Window.flags);
    DWORD desired = STYLE_MASK_WRITABLE & MakeWindowStyle(desiredFlags);

    if (current != desired)
    {
        SetLastError(0);
        DWORD previous = STYLE_MASK_WRITABLE & SetWindowLongPtrW(hwnd, GWL_STYLE, desired);
        if (previous != current)
        {
            TRACELOG(LOG_ERROR, "WIN32: WINDOW: SetWindowLongPtr() returned writable flags 0x%x but expected 0x%x (diff=0x%x, error=%lu)",
                previous, current, previous ^ current, GetLastError());
        }

        CheckFlags("UpdateWindowStyle", hwnd, desiredFlags, desired, STYLE_MASK_WRITABLE);
    }

    // Minimized takes precedence over maximized
    Mized currentMized = MIZED_NONE;
    Mized desiredMized = MIZED_NONE;
    if (CORE.Window.flags & FLAG_WINDOW_MINIMIZED) currentMized = MIZED_MIN;
    else if (CORE.Window.flags & FLAG_WINDOW_MAXIMIZED) currentMized = MIZED_MAX;
    if (desiredFlags & FLAG_WINDOW_MINIMIZED) desiredMized = MIZED_MIN;
    else if (desiredFlags & FLAG_WINDOW_MAXIMIZED) desiredMized = MIZED_MAX;

    if (currentMized != desiredMized)
    {
        switch (desiredMized)
        {
            case MIZED_NONE: ShowWindow(hwnd, SW_RESTORE); break;
            case MIZED_MIN: ShowWindow(hwnd, SW_MINIMIZE); break;
            case MIZED_MAX: ShowWindow(hwnd, SW_MAXIMIZE); break;
        }
    }
}

// Sanitize flags
static unsigned SanitizeFlags(unsigned flags)
{
    if (flags & FLAG_WINDOW_MAXIMIZED)
    {
        if (flags & FLAG_BORDERLESS_WINDOWED_MODE)
        {
            TRACELOG(LOG_WARNING, "WIN32: WINDOW: Borderless windows mode overriding maximized window flag");
            flags &= ~FLAG_WINDOW_MAXIMIZED;
        }

        if (~flags & FLAG_WINDOW_RESIZABLE)
        {
            if (!(CORE.Window.flags & FLAG_WINDOW_MAXIMIZED))
            {
                TRACELOG(LOG_WARNING, "WIN32: WINDOW: Cannot maximize a non-resizable window");
                flags &= ~FLAG_WINDOW_MAXIMIZED;
            }
            else if (CORE.Window.flags & FLAG_WINDOW_RESIZABLE)
            {
                TRACELOG(LOG_WARNING, "WIN32: WINDOW: Cannot set window as non-resizable when maximized");
                flags |= FLAG_WINDOW_RESIZABLE;
            }
        }
        else if (!(CORE.Window.flags & FLAG_WINDOW_MAXIMIZED))
        {
            if (CORE.Window.flags & FLAG_WINDOW_MINIMIZED)
            {
                // Window needs to be unminimized before it can be maximized since minimizing takes precedence
                flags &= ~FLAG_WINDOW_MINIMIZED;
            }
            else if ((flags & FLAG_WINDOW_MINIMIZED) && !(CORE.Window.flags & FLAG_WINDOW_MINIMIZED))
            {
                TRACELOG(LOG_WARNING, "WIN32: WINDOW: Cannot minimize and maximize a window in the same frame");
                flags &= ~FLAG_WINDOW_MINIMIZED;
                flags &= ~FLAG_WINDOW_MAXIMIZED;
            }
        }
    }

    // Don't change flags that can't be updated after window initialization
    flags = (flags & ~FLAG_MASK_NO_UPDATE) | (CORE.Window.flags & FLAG_MASK_NO_UPDATE);

    return flags;
}

// All window state changes from raylib flags go through this function. It performs
// whatever operations are needed to update the window state to match the desired flags
// In most cases this function should not update CORE.Window.flags directly, instead,
// the window itself should update CORE.Window.flags in response to actual state changes
// This means that CORE.Window.flags should always represent the actual state of the
// window. This function will continue to perform these update operations so long as
// the state continues to change
//
// This design takes care of many odd corner cases. For example, in case of restoring
// a window that was previously maximized AND minimized and those two flags need to be removed,
// ShowWindow with SW_RESTORE twice need to be actually calleed. Another example is
// wheen having a maximized window, if the undecorated flag is modified then the window style
// needs to be updated, but updating the style would mean the window size would change
// causing the window to lose its Maximized state which would mean the window size
// needs to be updated, followed by the update of window style, a second time, to restore that maximized
// state. This implementation is able to handle any/all of these special situations with a
// retry loop that continues until either the desired state is reached or the state stops changing
static void UpdateFlags(HWND hwnd, unsigned desiredFlags, int width, int height)
{
    // Flags that apply immediately without needing any operations
    CORE.Window.flags |= (desiredFlags & FLAG_MASK_NO_UPDATE);

    int vsync = (desiredFlags & FLAG_VSYNC_HINT)? 1 : 0;
    if (wglSwapIntervalEXT)
    {
        wglSwapIntervalEXT(vsync);
        if (vsync) CORE.Window.flags |= FLAG_VSYNC_HINT;
        else CORE.Window.flags &= ~FLAG_VSYNC_HINT;
    }

    // TODO: Review flags update code
    DWORD previousStyle = 0;
    for (unsigned attempt = 1; ; attempt++)
    {
        CheckFlags("UpdateFlags", hwnd, CORE.Window.flags, MakeWindowStyle(CORE.Window.flags), STYLE_MASK_ALL);

        bool windowSizeUpdated = false;
        if (MakeWindowStyle(CORE.Window.flags) == MakeWindowStyle(desiredFlags))
        {
            windowSizeUpdated = UpdateWindowSize(1, hwnd, width, height, desiredFlags);
            if ((FLAG_MASK_REQUIRED & desiredFlags) == (FLAG_MASK_REQUIRED & CORE.Window.flags)) break;
        }

        if ((attempt > 1) && (previousStyle == MakeWindowStyle(CORE.Window.flags)) && !windowSizeUpdated)
        {
            TRACELOG(LOG_ERROR, "WIN32: WINDOW: UpdateFlags() failed after %u attempt(s) wanted 0x%x but is 0x%x (diff=0x%x)",
                attempt, desiredFlags, CORE.Window.flags, desiredFlags ^ CORE.Window.flags);
            break;
        }

        previousStyle = MakeWindowStyle(CORE.Window.flags);
        UpdateWindowStyle(hwnd, desiredFlags);
    }
}

// Check if OpenGL extension is available
static bool IsWglExtensionAvailable(HDC hdc, const char *extension)
{
    bool result = false;

    if (wglGetExtensionsStringARB != nullptr)
    {
        const char *extList = wglGetExtensionsStringARB(hdc);
        if (extList != nullptr)
        {
            // Simple substring search (could use strtok or strstr)
            if (strstr(extList, extension) != nullptr) result = true;
        }
    }

    return result;
}
