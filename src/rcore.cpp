/**********************************************************************************************
*
*   rcore - Window/display management, Graphic device/context management and input management
*
*   PLATFORMS SUPPORTED:
*       > PLATFORM_DESKTOP_GLFW (GLFW backend):
*           - Windows (Win32, Win64)
*           - Linux (X11/Wayland desktop mode)
*           - macOS/OSX (x64, arm64)
*           - FreeBSD, OpenBSD, NetBSD, DragonFly (X11 desktop)
*       > PLATFORM_DESKTOP_SDL (SDL backend):
*           - Windows (Win32, Win64)
*           - Linux (X11/Wayland desktop mode)
*           - Others (not tested)
*       > PLATFORM_DESKTOP_RGFW (RGFW backend):
*           - Windows (Win32, Win64)
*           - Linux (X11/Wayland desktop mode)
*           - macOS/OSX (x64, arm64)
*           - Others (not tested)
*       > PLATFORM_DESKTOP_WIN32 (native Win32):
*           - Windows (Win32, Win64)
*       > PLATFORM_WEB (GLFW + Emscripten):
*           - HTML5 (WebAssembly)
*       > PLATFORM_WEB_EMSCRIPTEN (Emscripten):
*           - HTML5 (WebAssembly)
*       > PLATFORM_WEB_RGFW (Emscripten):
*           - HTML5 (WebAssembly)
*       > PLATFORM_DRM (native DRM):
*           - Raspberry Pi 0-5 (DRM/KMS)
*           - Linux DRM subsystem (KMS mode)
*           - Embedded devices (with GPU)
*       > PLATFORM_ANDROID (native NDK):
*           - Android (ARM, ARM64)
*       > PLATFORM_MEMORY
*           - Memory framebuffer output, using software renderer, no OS required
*
*   CONFIGURATION:
*       #define SUPPORT_CAMERA_SYSTEM       1
*           Camera module is included (rcamera.h) and multiple predefined cameras are available:
*               free, 1st/3rd person, orbital, custom
*
*       #define SUPPORT_GESTURES_SYSTEM     1
*           Gestures module is included (rgestures.h) to support gestures detection: tap, hold, swipe, drag
*
*       #define SUPPORT_MOUSE_GESTURES      1
*           Mouse gestures are directly mapped like touches and processed by gestures system
*
*       #define SUPPORT_BUSY_WAIT_LOOP      1
*           Use busy wait loop for timing sync, if not defined, a high-resolution timer is setup and used
*
*       #define SUPPORT_PARTIALBUSY_WAIT_LOOP 0
*           Use a partial-busy wait loop, in this case frame sleeps for most of the time and runs a busy-wait-loop at the end
*
*       #define SUPPORT_SCREEN_CAPTURE      1
*           Allow automatic screen capture of current screen pressing F12, defined in KeyCallback()
*
*       #define SUPPORT_COMPRESSION_API     1
*           Support CompressData() and DecompressData() functions, those functions use zlib implementation
*           provided by stb_image and stb_image_write libraries, so, those libraries must be enabled on textures module
*           for linkage
*
*       #define SUPPORT_AUTOMATION_EVENTS   1
*           Support automatic events recording and playing, useful for automated testing systems or AI based game playing
*
*   DEPENDENCIES:
*       raymath  - 3D math functionality (vec2, vec3, mat4, quat)
*       camera   - Multiple 3D camera modes (free, orbital, 1st person, 3rd person)
*       gestures - Gestures system for touch-ready devices (or simulated from mouse inputs)
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

//----------------------------------------------------------------------------------
// Feature Test Macros required for this module
//----------------------------------------------------------------------------------
#if (defined(__linux__) || defined(PLATFORM_WEB) || defined(PLATFORM_WEB_RGFW)) && (_XOPEN_SOURCE < 500)
    #undef _XOPEN_SOURCE
    #define _XOPEN_SOURCE 500       // Required for: readlink if compiled with c99 without GNU extensions
#endif

#if (defined(__linux__) || defined(PLATFORM_WEB) || defined(PLATFORM_WEB_RGFW)) && (_POSIX_C_SOURCE < 199309L)
    #undef _POSIX_C_SOURCE
    #define _POSIX_C_SOURCE 199309L // Required for: CLOCK_MONOTONIC if compiled with c99 without GNU extensions
#endif

#include "raylib.h"                 // Declares module functions

#include "config.h"                 // Defines module configuration flags
#include "logging.h"                // Log levels and central logging pipeline

#include <stdlib.h>                 // Required for: srand(), rand(), exit()
#include <array>                    // Required for: std::array [Used in DecodeDataBase64()]
#include <format>                   // Required for: std::format [Used in trace-log call sites]
#include <stdio.h>                  // Required for: FILE, fopen(), fseek(), ftell(), fread(), fwrite(), fprintf(), vprintf(), fclose(), sprintf() [Used in OpenURL()]
#include <string.h>                 // Required for: strlen(), strcmp(), strrchr(), memset(), memcpy(), strcat()
#include <stdarg.h>                 // Required for: va_list, va_start(), va_end() [Used in TraceLog()]

#ifndef PICO_RP2350
#include <time.h>                   // Required for: time() [Used in InitTimer()]
#else
#include "pico/rand.h"              // Pico doesn't implement time, and will implement its own hardware timer elsewhere.  However, we need something to get a random seed from...
#endif
#include <math.h>                   // Required for: tan() [Used in BeginMode3D()], atan2f() [Used in LoadVrStereoConfig()]

#if defined(PLATFORM_MEMORY) || defined(PLATFORM_WEB)
    #define SW_GL_FRAMEBUFFER_COPY_BGRA false
#endif
#define RLGL_IMPLEMENTATION
#include "rlgl.h"                   // OpenGL abstraction layer to OpenGL 1.1, 3.3+ or ES2

#include "raymath.h"                // vec2, vec3, quat and mat4 helpers (glm based)

#if SUPPORT_GESTURES_SYSTEM
    #define RGESTURES_IMPLEMENTATION
    #include "rgestures.h"          // Gestures detection functionality
#endif

#if SUPPORT_CAMERA_SYSTEM
    #define RCAMERA_IMPLEMENTATION
    #include "rcamera.h"            // Camera system functionality
#endif

#if SUPPORT_COMPRESSION_API
    #define SINFL_IMPLEMENTATION
    #define SINFL_NO_SIMD
    #include "external/sinfl.h"     // Deflate (RFC 1951) decompressor

    #define SDEFL_IMPLEMENTATION
    #include "external/sdefl.h"     // Deflate (RFC 1951) compressor
#endif

#if SUPPORT_RPRAND_GENERATOR
    #define RPRAND_IMPLEMENTATION
    #include "external/rprand.h"
#endif

// Platform specific defines to handle GetApplicationDirectory()
#if defined(_WIN32)
    #if !defined(MAX_PATH)
        #define MAX_PATH 260
    #endif

    struct HINSTANCE__;
    #if defined(__cplusplus)
    extern "C" {
    #endif
    __declspec(dllimport) unsigned long __stdcall GetFileAttributesA(const char *lpFileName);
    __declspec(dllimport) unsigned long __stdcall GetModuleFileNameA(struct HINSTANCE__ *hModule, char *lpFilename, unsigned long nSize);
    __declspec(dllimport) unsigned long __stdcall GetModuleFileNameW(struct HINSTANCE__ *hModule, wchar_t *lpFilename, unsigned long nSize);
    __declspec(dllimport) i32 __stdcall WideCharToMultiByte(u32 cp, unsigned long flags, const wchar_t *widestr, i32 cchwide, char *str, i32 cbmb, const char *defchar, i32 *used_default);
    __declspec(dllimport) u32 __stdcall timeBeginPeriod(u32 uPeriod);
    __declspec(dllimport) u32 __stdcall timeEndPeriod(u32 uPeriod);
    #if defined(__cplusplus)
    }
    #endif
#elif defined(__linux__)
    #include <unistd.h>
#elif defined(__FreeBSD__)
    #include <sys/types.h>
    #include <sys/sysctl.h>
    #include <unistd.h>
#elif defined(__APPLE__)
    #include <sys/syslimits.h>
    #include <mach-o/dyld.h>
#endif

#define _CRT_INTERNAL_NONSTDC_NAMES  1
#include <sys/stat.h>               // Required for: stat(), S_ISREG, S_ISDIR [Used in GetFileModTime(), IsPathFile(), IsPathDirectory()]

#if !defined(S_ISREG) && defined(S_IFMT) && defined(S_IFREG)
    #define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif
#if !defined(S_ISDIR) && defined(S_IFMT) && defined(S_IFDIR)
    #define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

#if defined(_WIN32) && (defined(_MSC_VER) || defined(__TINYC__))
    #define DIRENT_MALLOC malloc
    #define DIRENT_FREE free

    #include "external/dirent.h"    // Required for: DIR, opendir(), closedir() [Used in LoadDirectoryFiles()]
#else
    #include <dirent.h>             // Required for: DIR, opendir(), closedir() [Used in LoadDirectoryFiles()]
#endif

#if defined(_WIN32)
    #include <io.h>                 // Required for: _access() [Used in FileExists()]
    #include <direct.h>             // Required for: _getch(), _chdir(), _mkdir()
    #define GETCWD _getcwd          // NOTE: MSDN recommends not to use getcwd(), chdir()
    #define CHDIR _chdir
    #define MKDIR(dir) _mkdir(dir)
    #define ACCESS(fn) _access(fn, 0)
#else
    #include <unistd.h>             // Required for: getch(), chdir(), mkdir(), access()
    #define GETCWD getcwd
    #define CHDIR chdir
    #define MKDIR(dir) mkdir(dir, 0777)
    #define ACCESS(fn) access(fn, F_OK)
#endif

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
#ifndef MAX_FILEPATH_CAPACITY
    #define MAX_FILEPATH_CAPACITY       8192        // Maximum capacity for filepath
#endif
#ifndef MAX_FILEPATH_LENGTH
    #if defined(_WIN32)
        #define MAX_FILEPATH_LENGTH      260        // On Win32, MAX_PATH = 260 (limits.h) but Windows 10, Version 1607 enables long paths...
    #else
        #define MAX_FILEPATH_LENGTH     4096        // On Linux, PATH_MAX = 4096 by default (limits.h)
    #endif
#endif

#ifndef MAX_KEYBOARD_KEYS
    #define MAX_KEYBOARD_KEYS            512        // Maximum number of keyboard keys supported
#endif
#ifndef MAX_MOUSE_BUTTONS
    #define MAX_MOUSE_BUTTONS              8        // Maximum number of mouse buttons supported
#endif
#ifndef MAX_GAMEPADS
    #define MAX_GAMEPADS                   4        // Maximum number of gamepads supported
#endif
#ifndef MAX_GAMEPAD_NAME_LENGTH
    #define MAX_GAMEPAD_NAME_LENGTH      128        // Maximum number of characters in a gamepad name (byte size)
#endif
#ifndef MAX_GAMEPAD_AXES
    #define MAX_GAMEPAD_AXES               8        // Maximum number of axes supported (per gamepad)
#endif
#ifndef MAX_GAMEPAD_BUTTONS
    #define MAX_GAMEPAD_BUTTONS           32        // Maximum number of buttons supported (per gamepad)
#endif
#ifndef MAX_GAMEPAD_VIBRATION_TIME
    #define MAX_GAMEPAD_VIBRATION_TIME     2.0f     // Maximum vibration time in seconds
#endif
#ifndef MAX_TOUCH_POINTS
    #define MAX_TOUCH_POINTS               8        // Maximum number of touch points supported
#endif
#ifndef MAX_KEY_PRESSED_QUEUE
    #define MAX_KEY_PRESSED_QUEUE         16        // Maximum number of keys in the key input queue
#endif
#ifndef MAX_CHAR_PRESSED_QUEUE
    #define MAX_CHAR_PRESSED_QUEUE        16        // Maximum number of characters in the char input queue
#endif

#ifndef MAX_DECOMPRESSION_SIZE
    #define MAX_DECOMPRESSION_SIZE        64        // Maximum size allocated for decompression in MB
#endif

#ifndef MAX_AUTOMATION_EVENTS
    #define MAX_AUTOMATION_EVENTS      16384        // Maximum number of automation events to record
#endif

// File and directory scan filters
// NOTE: Used in ScanDirectoryFiles(), LoadDirectoryFilesEx() and GetDirectoryFileCountEx()
// WARNING: Custom file filters can be specified but following raylib IsFileExtension() convention: ".png;.wav;.glb"
#ifndef FILE_FILTER_TAG_ALL
    #define FILE_FILTER_TAG_ALL        "*.*"        // Filter to include all file types and directories on scan
#endif
#ifndef FILE_FILTER_TAG_FILE_ONLY
    #define FILE_FILTER_TAG_FILE_ONLY  "FILES*"     // Filter to include all file types on scan (no directories)
#endif
#ifndef FILE_FILTER_TAG_DIR_ONLY
    #define FILE_FILTER_TAG_DIR_ONLY   "DIRS*"      // Filter to include only directories on scan
#endif

// Flags bitwise operation macros
#define FLAG_SET(n, f) ((n) |= (f))
#define FLAG_CLEAR(n, f) ((n) &= ~(f))
#define FLAG_TOGGLE(n, f) ((n) ^= (f))
#define FLAG_IS_SET(n, f) (((n) & (f)) == (f))

// All the window flags requesting MSAA, the highest one set wins
#define MSAA_FLAGS        (FLAG_MSAA_4X_HINT | FLAG_MSAA_8X_HINT | FLAG_MSAA_16X_HINT)

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct { i32 x; i32 y; } Point;
typedef struct { u32 width; u32 height; } Size;

// Core global state context data
typedef struct CoreData {
    struct {
        std::string title;                  // Window text title
        u32 flags;                 // Configuration flags (bit based), keeps window state
        i32 samples;                        // MSAA samples in use, zero means MSAA disabled
        bool ready;                         // Check if window has been initialized successfully
        bool shouldClose;                   // Check if window set for closing
        bool resizedLastFrame;              // Check if window has been resized last frame
        bool eventWaiting;                  // Wait for events before ending frame
        bool usingFbo;                      // Using FBO (RenderTexture) for rendering instead of default framebuffer

        Size display;                       // Display width and height (monitor, device-screen, LCD, ...)
        Size screen;                        // Screen current width and height
        Point position;                     // Window current position
        Size previousScreen;                // Screen previous width and height (required on fullscreen/borderless-windowed toggle)
        Point previousPosition;             // Window previous position (required on fullscreen/borderless-windowed toggle)
        Size render;                        // Screen framebuffer width and height
        Point renderOffset;                 // Screen framebuffer render offset (Not required anymore?)
        Size currentFbo;                    // Current framebuffer render width and height (depends on active render texture)
        Size screenMin;                     // Screen minimum width and height (for resizable window)
        Size screenMax;                     // Screen maximum width and height (for resizable window)
        mat4 screenScale;                   // mat4 to scale screen (framebuffer rendering)

        char **dropFilepaths;               // Store dropped files paths pointers (provided by GLFW)
        u32 dropFileCount;         // Count dropped files strings

    } Window;
    struct {
        std::string basePath;                // Base path for data storage

    } Storage;
    struct {
        struct {
            i32 exitKey;                              // Default exit key
            char currentKeyState[MAX_KEYBOARD_KEYS];  // Registers current frame key state
            char previousKeyState[MAX_KEYBOARD_KEYS]; // Registers previous frame key state

            // NOTE: Since key press logic involves comparing previous vs current key state,
            // key repeats needs to be handled specially
            char keyRepeatInFrame[MAX_KEYBOARD_KEYS]; // Registers key repeats for current frame

            i32 keyPressedQueue[MAX_KEY_PRESSED_QUEUE]; // Input keys queue
            i32 keyPressedQueueCount;                   // Input keys queue count

            i32 charPressedQueue[MAX_CHAR_PRESSED_QUEUE]; // Input characters queue (unicode)
            i32 charPressedQueueCount;                    // Input characters queue count

        } Keyboard;
        struct {
            vec2 offset;               // Mouse offset
            vec2 scale;                // Mouse scaling
            vec2 currentPosition;      // Mouse position on screen
            vec2 previousPosition;     // Previous mouse position
            vec2 lockedPosition;       // Mouse position when locked

            i32 cursor;                     // Tracks current mouse cursor
            bool cursorHidden;              // Track if cursor is hidden
            bool cursorLocked;              // Track if cursor is locked (disabled)
            bool cursorOnScreen;            // Tracks if cursor is inside client area

            char currentButtonState[MAX_MOUSE_BUTTONS];  // Registers current mouse button state
            char previousButtonState[MAX_MOUSE_BUTTONS]; // Registers previous mouse button state
            vec2 currentWheelMove;                       // Registers current mouse wheel variation
            vec2 previousWheelMove;                      // Registers previous mouse wheel variation

        } Mouse;
        struct {
            i32 pointCount;                                 // Number of touch points active
            i32 pointId[MAX_TOUCH_POINTS];                  // Point identifiers
            vec2 position[MAX_TOUCH_POINTS];                // Touch position on screen
            vec2 previousPosition[MAX_TOUCH_POINTS];        // Previous touch position on screen
            char currentTouchState[MAX_TOUCH_POINTS];       // Registers current touch state
            char previousTouchState[MAX_TOUCH_POINTS];      // Registers previous touch state

        } Touch;
        struct {
            i32 lastButtonPressed;                                          // Register last gamepad button pressed
            i32 axisCount[MAX_GAMEPADS];                                    // Register number of available gamepad axes
            bool ready[MAX_GAMEPADS];                                       // Flag to know if gamepad is ready
            char name[MAX_GAMEPADS][MAX_GAMEPAD_NAME_LENGTH];               // Gamepad name holder
            char currentButtonState[MAX_GAMEPADS][MAX_GAMEPAD_BUTTONS];     // Current gamepad buttons state
            char previousButtonState[MAX_GAMEPADS][MAX_GAMEPAD_BUTTONS];    // Previous gamepad buttons state
            f32 axisState[MAX_GAMEPADS][MAX_GAMEPAD_AXES];                // Gamepad axes state

        } Gamepad;

    } Input;
    struct {
        f64 current;                     // Current time measure (seconds)
        f64 previous;                    // Previous time measure (seconds)
        f64 update;                      // Time measure for frame update (seconds)
        f64 draw;                        // Time measure for frame draw (seconds)
        f64 frame;                       // Time measure for one frame (seconds)
        f64 target;                      // Desired time for one frame, if 0 not applied (seconds)
        unsigned long long base;            // Base time measure for hi-res timer (ticks or nanoseconds)
        u32 frameCounter;          // Frame counter (frames)

    } Time;
} CoreData;

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
RLAPI const char *raylib_version = RAYLIB_VERSION;  // raylib version exported symbol, required for some bindings

auto CORE = CoreData{};                              // Global CORE state context

// Convert a C string that may be NULL into a std::string
static std::string GetStringOrEmpty(const char *str) { return (str != nullptr)? std::string(str) : std::string(); }

static TraceLogCallback traceLog = nullptr;            // TraceLog callback function pointer
static LoadFileDataCallback loadFileData = nullptr;    // LoadFileData callback function pointer
static SaveFileDataCallback saveFileData = nullptr;    // SaveFileText callback function pointer
static LoadFileTextCallback loadFileText = nullptr;    // LoadFileText callback function pointer
static SaveFileTextCallback saveFileText = nullptr;    // SaveFileText callback function pointer

#if SUPPORT_SCREEN_CAPTURE
static auto screenshotCounter = 0;                   // Screenshots counter
#endif

#if SUPPORT_AUTOMATION_EVENTS
// Automation events type
typedef enum AutomationEventType {
    EVENT_NONE = 0,
    // Input events
    INPUT_KEY_UP,                   // param[0]: key
    INPUT_KEY_DOWN,                 // param[0]: key
    INPUT_KEY_PRESSED,              // param[0]: key
    INPUT_KEY_RELEASED,             // param[0]: key
    INPUT_MOUSE_BUTTON_UP,          // param[0]: button
    INPUT_MOUSE_BUTTON_DOWN,        // param[0]: button
    INPUT_MOUSE_POSITION,           // param[0]: x, param[1]: y
    INPUT_MOUSE_WHEEL_MOTION,       // param[0]: x delta, param[1]: y delta
    INPUT_GAMEPAD_CONNECT,          // param[0]: gamepad
    INPUT_GAMEPAD_DISCONNECT,       // param[0]: gamepad
    INPUT_GAMEPAD_BUTTON_UP,        // param[0]: button
    INPUT_GAMEPAD_BUTTON_DOWN,      // param[0]: button
    INPUT_GAMEPAD_AXIS_MOTION,      // param[0]: axis, param[1]: delta
    INPUT_TOUCH_UP,                 // param[0]: id
    INPUT_TOUCH_DOWN,               // param[0]: id
    INPUT_TOUCH_POSITION,           // param[0]: x, param[1]: y
    INPUT_GESTURE,                  // param[0]: gesture
    // Window events
    WINDOW_CLOSE,                   // no params
    WINDOW_MAXIMIZE,                // no params
    WINDOW_MINIMIZE,                // no params
    WINDOW_RESIZE,                  // param[0]: width, param[1]: height
    // Custom events
    ACTION_TAKE_SCREENSHOT,         // no params
    ACTION_SETTARGETFPS             // param[0]: fps
} AutomationEventType;

#define MAX_AUTOMATION_EVENT_TYPES  24

// Event type name strings, required for export
static constexpr const char *autoEventTypeName[MAX_AUTOMATION_EVENT_TYPES] = {
    "EVENT_NONE",
    "INPUT_KEY_UP",
    "INPUT_KEY_DOWN",
    "INPUT_KEY_PRESSED",
    "INPUT_KEY_RELEASED",
    "INPUT_MOUSE_BUTTON_UP",
    "INPUT_MOUSE_BUTTON_DOWN",
    "INPUT_MOUSE_POSITION",
    "INPUT_MOUSE_WHEEL_MOTION",
    "INPUT_GAMEPAD_CONNECT",
    "INPUT_GAMEPAD_DISCONNECT",
    "INPUT_GAMEPAD_BUTTON_UP",
    "INPUT_GAMEPAD_BUTTON_DOWN",
    "INPUT_GAMEPAD_AXIS_MOTION",
    "INPUT_TOUCH_UP",
    "INPUT_TOUCH_DOWN",
    "INPUT_TOUCH_POSITION",
    "INPUT_GESTURE",
    "WINDOW_CLOSE",
    "WINDOW_MAXIMIZE",
    "WINDOW_MINIMIZE",
    "WINDOW_RESIZE",
    "ACTION_TAKE_SCREENSHOT",
    "ACTION_SETTARGETFPS"
};

static AutomationEventList *currentEventList = nullptr;        // Current automation events list, set by user, keep internal pointer
static auto automationEventRecording = false;                  // Recording automation events flag
//static short automationEventEnabled = 0b0000001111111111; // Automation events enabled for recording/playing
#endif
//-----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
// NOTE: Those functions are common for all platforms!
//----------------------------------------------------------------------------------
#if SUPPORT_MODULE_RTEXT
extern void LoadFontDefault();      // [Module: text] Loads default font on InitWindow()
extern void UnloadFontDefault();    // [Module: text] Unloads default font from GPU memory
#endif

extern i32 InitPlatform();          // Initialize platform (graphics, inputs and more)
extern void ClosePlatform();        // Close platform

static void InitTimer();                                // Initialize timer, hi-resolution if available (required by InitPlatform())
static void SetupViewport(i32 width, i32 height);           // Set viewport for a provided width and height
static i32 GetWindowRequestedSamples();                 // Get the MSAA samples requested by the window flags (required by InitPlatform())
static void UpdateWindowProvidedSamples();              // Get the MSAA samples really provided by the graphics device (required by InitWindow())

static void ScanDirectoryFiles(const char *basePath, FilePathList *list, const char *filter, const bool scanSubdirs); // Scan all files and directories in a base path

#if SUPPORT_AUTOMATION_EVENTS
static void RecordAutomationEvent(); // Record frame events (to internal events array)
#endif

#if defined(_WIN32) && !defined(PLATFORM_DESKTOP_RGFW)
// NOTE: Declaring Sleep() function symbol to avoid including windows.h (kernel32.lib linkage required)
#ifdef __cplusplus
extern "C"
#endif
__declspec(dllimport) void __stdcall Sleep(unsigned long msTimeout); // Required for: WaitTime()
#endif

#if !SUPPORT_MODULE_RTEXT
std::string TextFormat(std::string_view text, ...); // Formatting of text with variables to 'embed'
#endif

// NOTE: PLATFORM_DESKTOP defaults to GLFW backend
#if defined(PLATFORM_DESKTOP)
    #define PLATFORM_DESKTOP_GLFW
#endif

// Include platform-specific submodules
#if defined(PLATFORM_DESKTOP_GLFW)
    #include "platforms/rcore_desktop_glfw.c"
#elif defined(PLATFORM_DESKTOP_SDL)
    #include "platforms/rcore_desktop_sdl.c"
#elif (defined(PLATFORM_DESKTOP_RGFW) || defined(PLATFORM_WEB_RGFW))
    #include "platforms/rcore_desktop_rgfw.c"
#elif defined(PLATFORM_DESKTOP_WIN32)
    #include "platforms/rcore_desktop_win32.c"
#elif defined(PLATFORM_WEB)
    #include "platforms/rcore_web.c"
#elif defined(PLATFORM_DRM)
    #include "platforms/rcore_drm.c"
#elif defined(PLATFORM_ANDROID)
    #include "platforms/rcore_android.c"
#elif defined(PLATFORM_MEMORY)
    #include "platforms/rcore_memory.c"
#else
    #pragma message ("WARNING: No [rcore] platform defined")
#endif

//----------------------------------------------------------------------------------
// Module Functions Definition: Window and Graphics Device
//----------------------------------------------------------------------------------

// NOTE: Functions with a platform-specific implementation on rcore_<platform>.c
//bool WindowShouldClose(void)
//void ToggleFullscreen(void)
//void ToggleBorderlessWindowed(void)
//void MaximizeWindow(void)
//void MinimizeWindow(void)
//void RestoreWindow(void)

//void SetWindowState(unsigned int flags)
//void ClearWindowState(unsigned int flags)

//void SetWindowIcon(Image image)
//void SetWindowIcons(Image *images, int count)
//void SetWindowTitle(const char *title)
//void SetWindowPosition(int x, int y)
//void SetWindowMonitor(int monitor)
//void SetWindowMinSize(int width, int height)
//void SetWindowMaxSize(int width, int height)
//void SetWindowSize(int width, int height)
//void SetWindowOpacity(float opacity)
//void SetWindowFocused(void)
//void *GetWindowHandle(void)
//vec2 GetWindowPosition(void)
//vec2 GetWindowScaleDPI(void)

//int GetMonitorCount(void)
//int GetCurrentMonitor(void)
//int GetMonitorWidth(int monitor)
//int GetMonitorHeight(int monitor)
//int GetMonitorPhysicalWidth(int monitor)
//int GetMonitorPhysicalHeight(int monitor)
//int GetMonitorRefreshRate(int monitor)
//vec2 GetMonitorPosition(int monitor)
//const char *GetMonitorName(int monitor)

//void SetClipboardText(const char *text)
//const char *GetClipboardText(void)

//void ShowCursor(void)
//void HideCursor(void)
//void EnableCursor(void)
//void DisableCursor(void)

// Get the MSAA samples requested for the window, zero means MSAA disabled
// NOTE: FLAG_MSAA_4X_HINT, FLAG_MSAA_8X_HINT and FLAG_MSAA_16X_HINT configure it, the highest one set wins
static i32 GetWindowRequestedSamples()
{
    auto samples = 0;

    if (FLAG_IS_SET(CORE.Window.flags, FLAG_MSAA_16X_HINT)) samples = 16;
    else if (FLAG_IS_SET(CORE.Window.flags, FLAG_MSAA_8X_HINT)) samples = 8;
    else if (FLAG_IS_SET(CORE.Window.flags, FLAG_MSAA_4X_HINT)) samples = 4;

    return samples;
}

// Set the MSAA samples to request for the window, only powers of two are actually requested,
// the graphics device may provide less samples than the requested ones
// NOTE: It must be called before InitWindow(), samples can not be changed once the window context exists
void SetWindowSamples(i32 samples)
{
    if (CORE.Window.ready)
    {
        logMessage(LogLevel::warn, "WINDOW: MSAA samples can only be configured before window initialization");
        return;
    }

    // Clear the previous MSAA request, only one MSAA level can be requested at a time
    FLAG_CLEAR(CORE.Window.flags, MSAA_FLAGS);

    // Select the closest supported samples count, any request of 2 or 3 asks for a 4x MSAA context
    if (samples >= 16) FLAG_SET(CORE.Window.flags, FLAG_MSAA_16X_HINT);
    else if (samples >= 8) FLAG_SET(CORE.Window.flags, FLAG_MSAA_8X_HINT);
    else if (samples >= 2) FLAG_SET(CORE.Window.flags, FLAG_MSAA_4X_HINT);

    CORE.Window.samples = GetWindowRequestedSamples();
}

// Get the MSAA samples in use, zero means MSAA disabled
// NOTE: Before InitWindow() it returns the requested samples,
// once the window is created it returns the samples really provided by the graphics device
i32 GetWindowSamples()
{
    return CORE.Window.samples;
}

// Update the MSAA samples really provided by the graphics device, zero means MSAA disabled
// NOTE: It queries the current OpenGL default framebuffer, it must be called once the context is active,
// some platform backends also set CORE.Window.samples while creating the window context
static void UpdateWindowProvidedSamples()
{
#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
    const auto requestedSamples = GetWindowRequestedSamples();
    if (requestedSamples == 0)
    {
        CORE.Window.samples = 0;
        return;
    }

    auto providedSamples = 0;
    glGetIntegerv(GL_SAMPLES, &providedSamples);
    CORE.Window.samples = providedSamples;

    if (providedSamples < requestedSamples) logMessage(LogLevel::warn, std::format("DISPLAY: Graphics device only provided MSAA x{}", providedSamples));
#else
    // MSAA is not supported by this graphics API
    CORE.Window.samples = 0;
#endif
}

// Initialize window and OpenGL context
void InitWindow(i32 width, i32 height, std::string_view titleView)
{
    const std::string titleStr(titleView);
    const char *title = titleStr.c_str();

    logMessage(LogLevel::info, std::format("Initializing raylib {}", RAYLIB_VERSION));

#if defined(PLATFORM_DESKTOP_GLFW)
    logMessage(LogLevel::info, "Platform backend: DESKTOP (GLFW)");
#elif defined(PLATFORM_DESKTOP_SDL)
    logMessage(LogLevel::info, "Platform backend: DESKTOP (SDL)");
#elif defined(PLATFORM_DESKTOP_RGFW)
    logMessage(LogLevel::info, "Platform backend: DESKTOP (RGFW)");
#elif defined(PLATFORM_DESKTOP_WIN32)
    logMessage(LogLevel::info, "Platform backend: DESKTOP (WIN32)");
#elif defined(PLATFORM_WEB_RGFW)
    logMessage(LogLevel::info, "Platform backend: WEB (RGFW) (HTML5)");
#elif defined(PLATFORM_WEB)
    logMessage(LogLevel::info, "Platform backend: WEB (HTML5)");
#elif defined(PLATFORM_DRM)
    logMessage(LogLevel::info, "Platform backend: NATIVE DRM");
#elif defined(PLATFORM_ANDROID)
    logMessage(LogLevel::info, "Platform backend: ANDROID");
#elif defined(PLATFORM_MEMORY)
    logMessage(LogLevel::info, "Platform backend: MEMORY (No OS)");
#else
    logMessage(LogLevel::info, "Platform backend: CUSTOM");
#endif

    logMessage(LogLevel::info, "Supported raylib modules:");
    logMessage(LogLevel::info, "    > rcore:..... loaded (mandatory)");
    logMessage(LogLevel::info, "    > rlgl:...... loaded (mandatory)");
#if SUPPORT_MODULE_RSHAPES
    logMessage(LogLevel::info, "    > rshapes:... loaded (optional)");
#else
    logMessage(LogLevel::info, "    > rshapes:... not loaded (optional)");
#endif
#if SUPPORT_MODULE_RTEXTURES
    logMessage(LogLevel::info, "    > rtextures:. loaded (optional)");
#else
    logMessage(LogLevel::info, "    > rtextures:. not loaded (optional)");
#endif
#if SUPPORT_MODULE_RTEXT
    logMessage(LogLevel::info, "    > rtext:..... loaded (optional)");
#else
    logMessage(LogLevel::info, "    > rtext:..... not loaded (optional)");
#endif
#if SUPPORT_MODULE_RMODELS
    logMessage(LogLevel::info, "    > rmodels:... loaded (optional)");
#else
    logMessage(LogLevel::info, "    > rmodels:... not loaded (optional)");
#endif
#if SUPPORT_MODULE_RAUDIO
    logMessage(LogLevel::info, "    > raudio:.... loaded (optional)");
#else
    logMessage(LogLevel::info, "    > raudio:.... not loaded (optional)");
#endif

    // Initialize window data
    CORE.Window.screen.width = width;
    CORE.Window.screen.height = height;

    CORE.Window.eventWaiting = false;
    CORE.Window.screenScale = mat4(1.0f); // No draw scaling required by default
    if ((title != nullptr) && (title[0] != 0)) CORE.Window.title = title;

    // Initialize global input state
    CORE.Input.Keyboard.exitKey = KEY_ESCAPE;
    CORE.Input.Mouse.scale = vec2{ 1.0f, 1.0f };
    CORE.Input.Mouse.cursor = MOUSE_CURSOR_ARROW;
    CORE.Input.Gamepad.lastButtonPressed = GAMEPAD_BUTTON_UNKNOWN;

    // Store the MSAA samples requested by the window flags,
    // NOTE: Some platform backends replace it with the samples really provided by the graphics device
    CORE.Window.samples = GetWindowRequestedSamples();

    // Initialize platform
    //--------------------------------------------------------------
    const auto result = InitPlatform();

    if (result != 0)
    {
        logMessage(LogLevel::warn, "SYSTEM: Failed to initialize platform");
        return;
    }

    // Initialize render dimensions for embedded platforms
    // NOTE: On desktop platforms (GLFW, SDL, etc.), CORE.Window.render.width/height are set during window creation
    // On embedded platforms with no window manager, InitPlatform() doesn't set these values, so they should be initialized
    // here from screen dimensions (which are set from the InitWindow parameters)
    if ((CORE.Window.render.width == 0) || (CORE.Window.render.height == 0))
    {
        CORE.Window.render.width = CORE.Window.screen.width;
        CORE.Window.render.height = CORE.Window.screen.height;
    }
    //--------------------------------------------------------------

    // Initialize rlgl default data (buffers and shaders)
    // NOTE: Current fbo size stored as globals in rlgl for convenience
    rlglInit(CORE.Window.render.width, CORE.Window.render.height);

    // Query the graphics device for the MSAA samples really provided, it can be less than the requested ones
    UpdateWindowProvidedSamples();

    // Setup default viewport
    SetupViewport(CORE.Window.render.width, CORE.Window.render.height);

#if SUPPORT_MODULE_RTEXT
    // Load default font
    // WARNING: External function: Module required: rtext
    LoadFontDefault();
    #if SUPPORT_MODULE_RSHAPES
    // Set font white rectangle for shapes drawing, so shapes and text can be batched together
    // WARNING: rshapes module is required, if not available, default internal white rectangle is used
    const Rectangle rec = GetFontDefault().recs[95];
    if (CORE.Window.samples > 0)
    {
        // NOTE: Try to maximize rec padding to avoid pixel bleeding on MSAA filtering
        SetShapesTexture(GetFontDefault().texture, Rectangle{ rec.x + 2, rec.y + 2, 1, 1 });
    }
    else
    {
        // NOTE: Set up a 1px padding on char rectangle to avoid pixel bleeding
        SetShapesTexture(GetFontDefault().texture, Rectangle{ rec.x + 1, rec.y + 1, rec.width - 2, rec.height - 2 });
    }
    #endif
#else
    #if SUPPORT_MODULE_RSHAPES
    // Set default texture and rectangle to be used for shapes drawing
    // NOTE: rlgl default texture is a 1x1 pixel UNCOMPRESSED_R8G8B8A8
    auto texture = Texture2D{ rlGetTextureIdDefault(), 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    SetShapesTexture(texture, Rectangle{ 0.0f, 0.0f, 1.0f, 1.0f });    // WARNING: Module required: rshapes
    #endif
#endif

    // NOTE: Should be already initialized to 0 but just in case
    CORE.Time.frameCounter = 0;
    CORE.Window.shouldClose = false;

    // Initialize random seed using available timer source instead of standard time() on embedded platforms
#ifndef PICO_RP2350
    SetRandomSeed(static_cast<u32>(time(nullptr)));
#else
    SetRandomSeed(get_rand_32());
#endif

    logMessage(LogLevel::info, std::format("SYSTEM: Working Directory: {}", GetWorkingDirectory().c_str()));
}

// Close window and unload OpenGL context
void CloseWindow()
{
#if SUPPORT_MODULE_RTEXT
    UnloadFontDefault();        // WARNING: Module required: rtext
#endif

    rlglClose();                // De-init rlgl

    // De-initialize platform
    //--------------------------------------------------------------
    ClosePlatform();
    //--------------------------------------------------------------

    // Clear CORE data, in case new window is initialized
    CORE = CoreData{};
    
    logMessage(LogLevel::info, "Window closed successfully");
}

// Check if window has been initialized successfully
bool IsWindowReady()
{
    return CORE.Window.ready;
}

// Check if window is currently fullscreen
bool IsWindowFullscreen()
{
    return FLAG_IS_SET(CORE.Window.flags, FLAG_FULLSCREEN_MODE);
}

// Check if window is currently hidden
bool IsWindowHidden()
{
    return FLAG_IS_SET(CORE.Window.flags, FLAG_WINDOW_HIDDEN);
}

// Check if window has been minimized
bool IsWindowMinimized()
{
    return FLAG_IS_SET(CORE.Window.flags, FLAG_WINDOW_MINIMIZED);
}

// Check if window has been maximized
bool IsWindowMaximized()
{
    return FLAG_IS_SET(CORE.Window.flags, FLAG_WINDOW_MAXIMIZED);
}

// Check if window has the focus
bool IsWindowFocused()
{
    return !FLAG_IS_SET(CORE.Window.flags, FLAG_WINDOW_UNFOCUSED);
}

// Check if window has been resizedLastFrame
bool IsWindowResized()
{
    return CORE.Window.resizedLastFrame;
}

// Check if one specific window flag is enabled
bool IsWindowState(const u32 flag)
{
    return FLAG_IS_SET(CORE.Window.flags, flag);
}

// Get current screen width
i32 GetScreenWidth()
{
    return CORE.Window.screen.width;
}

// Get current screen height
i32 GetScreenHeight()
{
    return CORE.Window.screen.height;
}

// Get current render width which is equal to screen width*dpi scale
i32 GetRenderWidth()
{
    auto width = 0;

    if (CORE.Window.usingFbo) width = CORE.Window.currentFbo.width;
    else width = CORE.Window.render.width;

    return width;
}

// Get current screen height which is equal to screen height*dpi scale
i32 GetRenderHeight()
{
    auto height = 0;

    if (CORE.Window.usingFbo) height = CORE.Window.currentFbo.height;
    else height = CORE.Window.render.height;

    return height;
}

// Enable waiting for events on EndDrawing(), no automatic event polling
void EnableEventWaiting()
{
    CORE.Window.eventWaiting = true;
}

// Disable waiting for events on EndDrawing(), automatic events polling
void DisableEventWaiting()
{
    CORE.Window.eventWaiting = false;
}

// Check if cursor is not visible
bool IsCursorHidden()
{
    return CORE.Input.Mouse.cursorHidden;
}

// Check if cursor is on the current screen
bool IsCursorOnScreen()
{
    return CORE.Input.Mouse.cursorOnScreen;
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Screen Drawing
//----------------------------------------------------------------------------------

// Clear background (framebuffer) to color
void ClearBackground(const Color color)
{
    rlClearColor(color.r, color.g, color.b, color.a);   // Set clear color
    rlClearScreenBuffers();                             // Clear current framebuffers
}

// Begin canvas (framebuffer) drawing
void BeginDrawing()
{
    // WARNING: Previously to BeginDrawing() other render textures drawing could happen,
    // consequently the measure for update vs draw is not accurate (only the total frame time is accurate)

    CORE.Time.current = GetTime();      // Number of elapsed seconds since InitTimer()
    CORE.Time.update = CORE.Time.current - CORE.Time.previous;
    CORE.Time.previous = CORE.Time.current;

    rlLoadIdentity();                                      // Reset current matrix (modelview)
    rlMultMatrixf(MatrixToFloat(CORE.Window.screenScale)); // Apply screen scaling

    //rlTranslatef(0.375, 0.375, 0);    // HACK to have 2D pixel-perfect drawing on OpenGL 1.1
                                        // NOTE: Not required with OpenGL 3.3+
}

// End canvas (framebuffer) drawing and swap buffers (double buffering)
void EndDrawing()
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch

#if SUPPORT_AUTOMATION_EVENTS
    if (automationEventRecording) RecordAutomationEvent();    // Event recording
#endif

#if !SUPPORT_CUSTOM_FRAME_CONTROL
    SwapScreenBuffer();                  // Copy back buffer to front buffer (screen)

    // Frame time control system
    CORE.Time.current = GetTime();
    CORE.Time.draw = CORE.Time.current - CORE.Time.previous;
    CORE.Time.previous = CORE.Time.current;

    CORE.Time.frame = CORE.Time.update + CORE.Time.draw;

    // Wait for some milliseconds...
    if (CORE.Time.frame < CORE.Time.target)
    {
        WaitTime(CORE.Time.target - CORE.Time.frame);

        CORE.Time.current = GetTime();
        const f64 waitTime = CORE.Time.current - CORE.Time.previous;
        CORE.Time.previous = CORE.Time.current;

        CORE.Time.frame += waitTime;    // Total frame time: update + draw + wait
    }

    PollInputEvents();      // Poll user events (before next frame update)
#endif

#if SUPPORT_SCREEN_CAPTURE
    if (IsKeyPressed(KEY_F12))
    {
        TakeScreenshot(TextFormat("screenshot%03i.png", screenshotCounter));
        screenshotCounter++;
    }
#endif

    CORE.Time.frameCounter++;
}

// Initialize 2D mode with custom camera (2D)
void BeginMode2D(const Camera2D camera)
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch

    rlLoadIdentity();               // Reset current matrix (modelview)

    // Apply 2d camera transformation to modelview
    rlMultMatrixf(MatrixToFloat(GetCameraMatrix2D(camera)));
}

// End 2D mode with custom camera
void EndMode2D()
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch

    rlLoadIdentity();               // Reset current matrix (modelview)

    if (rlGetActiveFramebuffer() == 0) rlMultMatrixf(MatrixToFloat(CORE.Window.screenScale)); // Apply screen scaling if required
}

// Initializes 3D mode with custom camera (3D)
void BeginMode3D(const Camera camera)
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch

    rlMatrixMode(RL_PROJECTION);    // Switch to projection matrix
    rlPushMatrix();                 // Save previous matrix, which contains the settings for the 2d ortho projection
    rlLoadIdentity();               // Reset current matrix (projection)

    const f32 aspect = static_cast<f32>(CORE.Window.currentFbo.width)/static_cast<f32>(CORE.Window.currentFbo.height);

    // NOTE: zNear and zFar values are important when computing depth buffer values
    if (camera.projection == CAMERA_PERSPECTIVE)
    {
        // Setup perspective projection
        const f32 top = rlGetCullDistanceNear()*tanf(camera.fovy*0.5f*DEG2RAD);
        const f32 right = top*aspect;

        rlFrustum(-right, right, -top, top, rlGetCullDistanceNear(), rlGetCullDistanceFar());
    }
    else if (camera.projection == CAMERA_ORTHOGRAPHIC)
    {
        // Setup orthographic projection
        const f32 top = camera.fovy/2.0f;
        const f32 right = top*aspect;

        rlOrtho(-right, right, -top,top, rlGetCullDistanceNear(), rlGetCullDistanceFar());
    }

    rlMatrixMode(RL_MODELVIEW);     // Switch back to modelview matrix
    rlLoadIdentity();               // Reset current matrix (modelview)

    // Setup Camera view
    const mat4 matView = glm::lookAt(camera.position, camera.target, camera.up);
    rlMultMatrixf(MatrixToFloat(matView));      // Multiply modelview matrix by view matrix (camera)

    rlEnableDepthTest();            // Enable DEPTH_TEST for 3D
}

// End 3D mode and returns to default 2D orthographic mode
void EndMode3D()
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch

    rlMatrixMode(RL_PROJECTION);    // Switch to projection matrix
    rlPopMatrix();                  // Restore previous matrix (projection) from matrix stack

    rlMatrixMode(RL_MODELVIEW);     // Switch back to modelview matrix
    rlLoadIdentity();               // Reset current matrix (modelview)

    if (rlGetActiveFramebuffer() == 0) rlMultMatrixf(MatrixToFloat(CORE.Window.screenScale)); // Apply screen scaling if required

    rlDisableDepthTest();           // Disable DEPTH_TEST for 2D
}

// Initializes render texture for drawing
void BeginTextureMode(const RenderTexture2D target)
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch

    rlEnableFramebuffer(target.id); // Enable render target

    // Set viewport and RLGL internal framebuffer size
    rlViewport(0, 0, target.texture.width, target.texture.height);
    rlSetFramebufferWidth(target.texture.width);
    rlSetFramebufferHeight(target.texture.height);

    rlMatrixMode(RL_PROJECTION);    // Switch to projection matrix
    rlLoadIdentity();               // Reset current matrix (projection)

    // Set orthographic projection to current framebuffer size
    // NOTE: Configured top-left corner as (0, 0)
    rlOrtho(0.0f, static_cast<f32>(target.texture.width), static_cast<f32>(target.texture.height), 0.0f, 0.0f, 1.0f);

    rlMatrixMode(RL_MODELVIEW);     // Switch back to modelview matrix
    rlLoadIdentity();               // Reset current matrix (modelview)

    //rlScalef(0.0f, -1.0f, 0.0f);  // Flip Y-drawing (?)

    // Setup current width/height for proper aspect ratio
    // calculation when using BeginTextureMode()
    CORE.Window.currentFbo.width = target.texture.width;
    CORE.Window.currentFbo.height = target.texture.height;
    CORE.Window.usingFbo = true;
}

// End drawing to render texture
void EndTextureMode()
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch

    rlDisableFramebuffer();         // Disable render target (fbo)

    // Set viewport to default framebuffer size
    SetupViewport(CORE.Window.render.width, CORE.Window.render.height);

    // Go back to the modelview state from BeginDrawing, back to the main framebuffer
    rlMatrixMode(RL_MODELVIEW);                            // Switch back to modelview matrix
    rlLoadIdentity();                                      // Reset current matrix (modelview)
    rlMultMatrixf(MatrixToFloat(CORE.Window.screenScale)); // Apply screen scaling if required

    // Reset current fbo to screen size
    CORE.Window.currentFbo.width = CORE.Window.render.width;
    CORE.Window.currentFbo.height = CORE.Window.render.height;
    CORE.Window.usingFbo = false;
}

// Begin custom shader mode
void BeginShaderMode(const Shader shader)
{
    rlSetShader(shader.id, shader.locs);
}

// End custom shader mode (returns to default shader)
void EndShaderMode()
{
    rlSetShader(rlGetShaderIdDefault(), rlGetShaderLocsDefault());
}

// Begin blending mode (alpha, additive, multiplied, subtract, custom)
// NOTE: Blend modes supported are enumerated in BlendMode enum
void BeginBlendMode(const i32 mode)
{
    rlSetBlendMode(mode);
}

// End blending mode (reset to default: alpha blending)
void EndBlendMode()
{
    rlSetBlendMode(BLEND_ALPHA);
}

// Begin scissor mode (define screen area for following drawing)
// NOTE: Scissor rec refers to bottom-left corner, changing it to upper-left
void BeginScissorMode(const i32 x, const i32 y, const i32 width, const i32 height)
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch

    rlEnableScissorTest();

#if defined(__APPLE__)
    if (!CORE.Window.usingFbo)
    {
        auto scale = GetWindowScaleDPI();
        rlScissor(static_cast<i32>((x*scale.x)), static_cast<i32>((GetScreenHeight()*scale.y - (((y + height)*scale.y)))), static_cast<i32>((width*scale.x)), static_cast<i32>((height*scale.y)));
    }
#else
    if (!CORE.Window.usingFbo && FLAG_IS_SET(CORE.Window.flags, FLAG_WINDOW_HIGHDPI))
    {
        const auto scale = GetWindowScaleDPI();
        rlScissor(static_cast<i32>(x*scale.x), static_cast<i32>(CORE.Window.currentFbo.height - (y + height)*scale.y), static_cast<i32>(width*scale.x), static_cast<i32>(height*scale.y));
    }
#endif
    else
    {
        rlScissor(x, CORE.Window.currentFbo.height - (y + height), width, height);
    }
}

// End scissor mode
void EndScissorMode()
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch
    rlDisableScissorTest();
}

//----------------------------------------------------------------------------------
// Module Functions Definition: VR Stereo Rendering
//----------------------------------------------------------------------------------

// Begin VR drawing configuration
void BeginVrStereoMode(const VrStereoConfig config)
{
    rlEnableStereoRender();

    // Set stereo render matrices
    rlSetMatrixProjectionStereo(config.projection[0], config.projection[1]);
    rlSetMatrixViewOffsetStereo(config.viewOffset[0], config.viewOffset[1]);
}

// End VR drawing process (and desktop mirror)
void EndVrStereoMode()
{
    rlDisableStereoRender();
}

// Load VR stereo config for VR simulator device parameters
VrStereoConfig LoadVrStereoConfig(const VrDeviceInfo device)
{
    auto config = VrStereoConfig{ 0 };

    if (rlGetVersion() != RL_OPENGL_11)
    {
        // Compute aspect ratio
        const f32 aspect = (static_cast<f32>(device.hResolution)*0.5f)/static_cast<f32>(device.vResolution);

        // Compute lens parameters
        const f32 lensShift = (device.hScreenSize*0.25f - device.lensSeparationDistance*0.5f)/device.hScreenSize;
        config.leftLensCenter[0] = 0.25f + lensShift;
        config.leftLensCenter[1] = 0.5f;
        config.rightLensCenter[0] = 0.75f - lensShift;
        config.rightLensCenter[1] = 0.5f;
        config.leftScreenCenter[0] = 0.25f;
        config.leftScreenCenter[1] = 0.5f;
        config.rightScreenCenter[0] = 0.75f;
        config.rightScreenCenter[1] = 0.5f;

        // Compute distortion scale parameters
        // NOTE: To get lens max radius, lensShift must be normalized to [-1..1]
        const f32 lensRadius = fabsf(-1.0f - 4.0f*lensShift);
        const f32 lensRadiusSq = lensRadius*lensRadius;
        const f32 distortionScale = device.lensDistortionValues[0] +
                                device.lensDistortionValues[1]*lensRadiusSq +
                                device.lensDistortionValues[2]*lensRadiusSq*lensRadiusSq +
                                device.lensDistortionValues[3]*lensRadiusSq*lensRadiusSq*lensRadiusSq;

        constexpr auto normScreenWidth = 0.5f;
        constexpr auto normScreenHeight = 1.0f;
        config.scaleIn[0] = 2.0f/normScreenWidth;
        config.scaleIn[1] = 2.0f/normScreenHeight/aspect;
        config.scale[0] = normScreenWidth*0.5f/distortionScale;
        config.scale[1] = normScreenHeight*0.5f*aspect/distortionScale;

        // Fovy is normally computed with: 2*atan2f(device.vScreenSize, 2*device.eyeToScreenDistance)
        // ...but with lens distortion it is increased (see Oculus SDK Documentation)
        const f32 fovy = 2.0f*atan2f(device.vScreenSize*0.5f*distortionScale, device.eyeToScreenDistance);     // Really need distortionScale?
       // float fovy = 2.0f*(float)atan2f(device.vScreenSize*0.5f, device.eyeToScreenDistance);

        // Compute camera projection matrices
        const f32 projOffset = 4.0f*lensShift;      // Scaled to projection space coordinates [-1..1]
        const mat4 proj = glm::perspective(fovy, aspect, rlGetCullDistanceNear(), rlGetCullDistanceFar());

        config.projection[0] = glm::translate(mat4(1.0f), vec3{projOffset, 0.0f, 0.0f})*proj;
        config.projection[1] = glm::translate(mat4(1.0f), vec3{-projOffset, 0.0f, 0.0f})*proj;

        // Compute camera transformation matrices
        // NOTE: Camera movement might seem more natural if modelling the head
        // Axis of rotation is the base of the head, so adding some y (base of head to eye level
        // and -z (center of head to eye protrusion) to the camera positions
        config.viewOffset[0] = glm::translate(mat4(1.0f), vec3{device.interpupillaryDistance*0.5f, 0.075f, 0.045f});
        config.viewOffset[1] = glm::translate(mat4(1.0f), vec3{-device.interpupillaryDistance*0.5f, 0.075f, 0.045f});

        // Compute eyes Viewports
        /*
        config.eyeViewportRight[0] = 0;
        config.eyeViewportRight[1] = 0;
        config.eyeViewportRight[2] = device.hResolution/2;
        config.eyeViewportRight[3] = device.vResolution;

        config.eyeViewportLeft[0] = device.hResolution/2;
        config.eyeViewportLeft[1] = 0;
        config.eyeViewportLeft[2] = device.hResolution/2;
        config.eyeViewportLeft[3] = device.vResolution;
        */
    }
    else logMessage(LogLevel::warn, "RLGL: VR Simulator not supported on OpenGL 1.1");

    return config;
}

// Unload VR stereo config properties
void UnloadVrStereoConfig(const VrStereoConfig config)
{
    logMessage(LogLevel::info, "UnloadVrStereoConfig not implemented in rcore.c");
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Shaders Management
//----------------------------------------------------------------------------------

// Load shader from files and bind default locations
// NOTE: If shader filename is NULL, using default vertex/fragment shaders
Shader LoadShader(std::string_view vsFileNameView, std::string_view fsFileNameView)
{
    auto shader = Shader{ 0 };

    std::string vShaderStr;
    std::string fShaderStr;

    if (!vsFileNameView.empty()) vShaderStr = LoadFileText(vsFileNameView);
    if (!fsFileNameView.empty()) fShaderStr = LoadFileText(fsFileNameView);

    if (vShaderStr.empty() && fShaderStr.empty()) logMessage(LogLevel::warn, "SHADER: Shader files provided are not valid, using default shader");

    shader = LoadShaderFromMemory(vShaderStr, fShaderStr);

    return shader;
}

// Load shader from code strings and bind default locations
Shader LoadShaderFromMemory(std::string_view vsCodeView, std::string_view fsCodeView)
{
    const std::string vsCodeStr(vsCodeView);
    const char *vsCode = vsCodeStr.c_str();
    const std::string fsCodeStr(fsCodeView);
    const char *fsCode = fsCodeStr.c_str();

    auto shader = Shader{ 0 };

    shader.id = rlLoadShaderProgram(vsCode, fsCode);

    if (shader.id == 0)
    {
        // Shader could not be loaded but still loading the location points to avoid potential crashes
        // NOTE: All locations set to -1 (no location found)
        shader.locs = static_cast<i32 *>(calloc(RL_MAX_SHADER_LOCATIONS, sizeof(i32)));
        for (i32 i = 0; i < RL_MAX_SHADER_LOCATIONS; i++) shader.locs[i] = -1;
    }
    else if (shader.id == rlGetShaderIdDefault()) shader.locs = rlGetShaderLocsDefault();
    else if (shader.id > 0)
    {
        // After custom shader loading, trying to set default location names
        // Default shader attribute locations have been binded before linking:
        //  - vertex position location    = 0
        //  - vertex texcoord location    = 1
        //  - vertex normal location      = 2
        //  - vertex color location       = 3
        //  - vertex tangent location     = 4
        //  - vertex texcoord2 location   = 5
        //  - vertex boneIndices location = 6
        //  - vertex boneWeights location = 7

        // NOTE: If any location is not found, loc point becomes -1

        // Load shader locations array
        // NOTE: All locations set to -1 (no location)
        shader.locs = static_cast<i32 *>(calloc(RL_MAX_SHADER_LOCATIONS, sizeof(i32)));
        for (i32 i = 0; i < RL_MAX_SHADER_LOCATIONS; i++) shader.locs[i] = -1;

        // Get handles to GLSL input attribute locations
        shader.locs[SHADER_LOC_VERTEX_POSITION] = rlGetLocationAttrib(shader.id, RL_DEFAULT_SHADER_ATTRIB_NAME_POSITION);
        shader.locs[SHADER_LOC_VERTEX_TEXCOORD01] = rlGetLocationAttrib(shader.id, RL_DEFAULT_SHADER_ATTRIB_NAME_TEXCOORD);
        shader.locs[SHADER_LOC_VERTEX_TEXCOORD02] = rlGetLocationAttrib(shader.id, RL_DEFAULT_SHADER_ATTRIB_NAME_TEXCOORD2);
        shader.locs[SHADER_LOC_VERTEX_NORMAL] = rlGetLocationAttrib(shader.id, RL_DEFAULT_SHADER_ATTRIB_NAME_NORMAL);
        shader.locs[SHADER_LOC_VERTEX_TANGENT] = rlGetLocationAttrib(shader.id, RL_DEFAULT_SHADER_ATTRIB_NAME_TANGENT);
        shader.locs[SHADER_LOC_VERTEX_COLOR] = rlGetLocationAttrib(shader.id, RL_DEFAULT_SHADER_ATTRIB_NAME_COLOR);
        shader.locs[SHADER_LOC_VERTEX_BONEIDS] = rlGetLocationAttrib(shader.id, RL_DEFAULT_SHADER_ATTRIB_NAME_BONEINDICES);
        shader.locs[SHADER_LOC_VERTEX_BONEWEIGHTS] = rlGetLocationAttrib(shader.id, RL_DEFAULT_SHADER_ATTRIB_NAME_BONEWEIGHTS);
        shader.locs[SHADER_LOC_VERTEX_INSTANCETRANSFORM] = rlGetLocationAttrib(shader.id, RL_DEFAULT_SHADER_ATTRIB_NAME_INSTANCETRANSFORM);

        // Get handles to GLSL uniform locations (vertex shader)
        shader.locs[SHADER_LOC_MATRIX_MVP] = rlGetLocationUniform(shader.id, RL_DEFAULT_SHADER_UNIFORM_NAME_MVP);
        shader.locs[SHADER_LOC_MATRIX_VIEW] = rlGetLocationUniform(shader.id, RL_DEFAULT_SHADER_UNIFORM_NAME_VIEW);
        shader.locs[SHADER_LOC_MATRIX_PROJECTION] = rlGetLocationUniform(shader.id, RL_DEFAULT_SHADER_UNIFORM_NAME_PROJECTION);
        shader.locs[SHADER_LOC_MATRIX_MODEL] = rlGetLocationUniform(shader.id, RL_DEFAULT_SHADER_UNIFORM_NAME_MODEL);
        shader.locs[SHADER_LOC_MATRIX_NORMAL] = rlGetLocationUniform(shader.id, RL_DEFAULT_SHADER_UNIFORM_NAME_NORMAL);
        shader.locs[SHADER_LOC_MATRIX_BONETRANSFORMS] = rlGetLocationUniform(shader.id, RL_DEFAULT_SHADER_UNIFORM_NAME_BONEMATRICES);

        // Get handles to GLSL uniform locations (fragment shader)
        shader.locs[SHADER_LOC_COLOR_DIFFUSE] = rlGetLocationUniform(shader.id, RL_DEFAULT_SHADER_UNIFORM_NAME_COLOR);
        shader.locs[SHADER_LOC_MAP_DIFFUSE] = rlGetLocationUniform(shader.id, RL_DEFAULT_SHADER_SAMPLER2D_NAME_TEXTURE0);  // SHADER_LOC_MAP_ALBEDO
        shader.locs[SHADER_LOC_MAP_SPECULAR] = rlGetLocationUniform(shader.id, RL_DEFAULT_SHADER_SAMPLER2D_NAME_TEXTURE1); // SHADER_LOC_MAP_METALNESS
        shader.locs[SHADER_LOC_MAP_NORMAL] = rlGetLocationUniform(shader.id, RL_DEFAULT_SHADER_SAMPLER2D_NAME_TEXTURE2);
    }

    return shader;
}

// Check if shader is valid (loaded on GPU)
bool IsShaderValid(const Shader shader)
{
    return ((shader.id > 0) &&          // Validate shader id (GPU loaded successfully)
            (shader.locs != nullptr));     // Validate memory has been allocated for default shader locations

    // The following locations are tried to be set automatically (locs[i] >= 0),
    // any of them can be checked for validation but the only mandatory one is, afaik, SHADER_LOC_VERTEX_POSITION
    // NOTE: Users can also setup manually their own attributes/uniforms and do not used the default raylib ones

    // Vertex shader attribute locations (default)
    // shader.locs[SHADER_LOC_VERTEX_POSITION]      // Set by default internal shader
    // shader.locs[SHADER_LOC_VERTEX_TEXCOORD01]    // Set by default internal shader
    // shader.locs[SHADER_LOC_VERTEX_TEXCOORD02]
    // shader.locs[SHADER_LOC_VERTEX_NORMAL]
    // shader.locs[SHADER_LOC_VERTEX_TANGENT]
    // shader.locs[SHADER_LOC_VERTEX_COLOR]         // Set by default internal shader

    // Vertex shader uniform locations (default)
    // shader.locs[SHADER_LOC_MATRIX_MVP]           // Set by default internal shader
    // shader.locs[SHADER_LOC_MATRIX_VIEW]
    // shader.locs[SHADER_LOC_MATRIX_PROJECTION]
    // shader.locs[SHADER_LOC_MATRIX_MODEL]
    // shader.locs[SHADER_LOC_MATRIX_NORMAL]

    // Fragment shader uniform locations (default)
    // shader.locs[SHADER_LOC_COLOR_DIFFUSE]        // Set by default internal shader
    // shader.locs[SHADER_LOC_MAP_DIFFUSE]          // Set by default internal shader
    // shader.locs[SHADER_LOC_MAP_SPECULAR]
    // shader.locs[SHADER_LOC_MAP_NORMAL]
}

// Unload shader from GPU memory (VRAM)
void UnloadShader(Shader shader)
{
    if (shader.id != rlGetShaderIdDefault())
    {
        rlUnloadShaderProgram(shader.id);

        // NOTE: If shader loading failed, it should be 0
        free(shader.locs);
    }
}

// Get shader uniform location
i32 GetShaderLocation(const Shader shader, std::string_view uniformNameView)
{
    const std::string uniformNameStr(uniformNameView);
    const char *uniformName = uniformNameStr.c_str();

    return rlGetLocationUniform(shader.id, uniformName);
}

// Get shader attribute location
i32 GetShaderLocationAttrib(const Shader shader, std::string_view attribNameView)
{
    const std::string attribNameStr(attribNameView);
    const char *attribName = attribNameStr.c_str();

    return rlGetLocationAttrib(shader.id, attribName);
}

// Set shader uniform value
void SetShaderValue(const Shader shader, const i32 locIndex, const void *value, const i32 uniformType)
{
    SetShaderValueV(shader, locIndex, value, uniformType, 1);
}

// Set shader uniform value vector
void SetShaderValueV(const Shader shader, const i32 locIndex, const void *value, const i32 uniformType, const i32 count)
{
    if (locIndex > -1)
    {
        rlEnableShader(shader.id);
        rlSetUniform(locIndex, value, uniformType, count);
        //rlDisableShader();      // Avoid resetting current shader program, in case other uniforms are set
    }
}

// Set shader uniform value (matrix 4x4)
void SetShaderValueMatrix(const Shader shader, const i32 locIndex, const mat4 mat)
{
    if (locIndex > -1)
    {
        rlEnableShader(shader.id);
        rlSetUniformMatrix(locIndex, mat);
        //rlDisableShader();
    }
}

// Set shader uniform value for texture
void SetShaderValueTexture(const Shader shader, const i32 locIndex, const Texture2D texture)
{
    if (locIndex > -1)
    {
        rlEnableShader(shader.id);
        rlSetUniformSampler(locIndex, texture.id);
        //rlDisableShader();
    }
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Screen-space Queries
//----------------------------------------------------------------------------------

// Get a ray trace from screen position (i.e mouse)
Ray GetScreenToWorldRay(const vec2 position, const Camera camera)
{
    auto ray = GetScreenToWorldRayEx(position, camera, GetScreenWidth(), GetScreenHeight());

    return ray;
}

// Get a ray trace from the screen position (i.e mouse) within a specific section of the screen
Ray GetScreenToWorldRayEx(vec2 position, const Camera camera, const i32 width, const i32 height)
{
    Ray ray{};

    // Calculate normalized device coordinates
    // NOTE: y value is negative
    const f32 x = (2.0f*position.x)/static_cast<f32>(width) - 1.0f;
    const f32 y = 1.0f - (2.0f*position.y)/static_cast<f32>(height);
    constexpr auto z = 1.0f;

    // Store values in a vector
    const auto deviceCoords = vec3{ x, y, z };

    // Calculate view matrix from camera look at
    const mat4 matView = glm::lookAt(camera.position, camera.target, camera.up);

    auto matProj = mat4(1.0f);

    if (camera.projection == CAMERA_PERSPECTIVE)
    {
        // Calculate projection matrix from perspective
        matProj = glm::perspective(camera.fovy*DEG2RAD, static_cast<f32>(width)/static_cast<f32>(height), rlGetCullDistanceNear(), rlGetCullDistanceFar());
    }
    else if (camera.projection == CAMERA_ORTHOGRAPHIC)
    {
        const f32 aspect = static_cast<f32>(width)/static_cast<f32>(height);
        const f32 top = camera.fovy/2.0f;
        const f32 right = top*aspect;

        // Calculate projection matrix from orthographic
        matProj = glm::ortho(-right, right, -top, top, rlGetCullDistanceNear(), rlGetCullDistanceFar());
    }

    // Unproject far/near points
    const auto nearPoint = Vector3Unproject(vec3{ deviceCoords.x, deviceCoords.y, 0.0f }, matProj, matView);
    const auto farPoint = Vector3Unproject(vec3{ deviceCoords.x, deviceCoords.y, 1.0f }, matProj, matView);

    // Unproject the mouse cursor in the near plane
    // It is needed as the source position because orthographic projects,
    // compared to perspective doesn't have a convergence point,
    // meaning that the "eye" of the camera is more like a plane than a point
    const auto cameraPlanePointerPos = Vector3Unproject(vec3{ deviceCoords.x, deviceCoords.y, -1.0f }, matProj, matView);

    // Calculate normalized direction vector
    const vec3 direction = glm::normalize(farPoint - nearPoint);

    if (camera.projection == CAMERA_PERSPECTIVE) ray.position = camera.position;
    else if (camera.projection == CAMERA_ORTHOGRAPHIC) ray.position = cameraPlanePointerPos;

    // Apply calculated vectors to ray
    ray.direction = direction;

    return ray;
}

// Get transform matrix for camera
mat4 GetCameraMatrix(const Camera camera)
{
    mat4 mat = glm::lookAt(camera.position, camera.target, camera.up);

    return mat;
}

// Get camera 2d transform matrix
mat4 GetCameraMatrix2D(const Camera2D camera)
{
    mat4 matTransform{};
    // The camera in world-space is set by
    //   1. Move it to target
    //   2. Rotate by -rotation and scale by (1/zoom)
    //      When setting higher scale, it's more intuitive for the world to become bigger (= camera become smaller),
    //      not for the camera getting bigger, hence the invert. Same deal with rotation
    //   3. Move it by (-offset);
    //      Offset defines target transform relative to screen, but since effectively "moving" screen (camera)
    //      it needs to be moved into opposite direction (inverse transform)

    // Having camera transform in world-space, inverse of it gives the modelview transform
    // Since (A*B*C)' = C'*B'*A', the modelview is
    //   1. Move to offset
    //   2. Rotate and Scale
    //   3. Move by -target
    const mat4 matOrigin = glm::translate(mat4(1.0f), vec3{-camera.target.x, -camera.target.y, 0.0f});
    const mat4 matRotation = glm::rotate(mat4(1.0f), camera.rotation*DEG2RAD, vec3{0.0f, 0.0f, 1.0f});
    const mat4 matScale = glm::scale(mat4(1.0f), vec3{camera.zoom, camera.zoom, 1.0f});
    const mat4 matTranslation = glm::translate(mat4(1.0f), vec3{camera.offset.x, camera.offset.y, 0.0f});

    // NOTE: raylib matrices use row vectors (MatrixMultiply(A, B) = A * B), glm uses column
    //       vectors, so the equivalent glm product is the reverse order
    matTransform = matTranslation*(matRotation*matScale)*matOrigin;

    return matTransform;
}

// Get screen space position from a 3d world space position
vec2 GetWorldToScreen(const vec3 position, const Camera camera)
{
    const auto screenPosition = GetWorldToScreenEx(position, camera, GetScreenWidth(), GetScreenHeight());

    return screenPosition;
}

// Get sized screen space position for a 3d world space position (useful for texture drawing)
vec2 GetWorldToScreenEx(const vec3 position, const Camera camera, const i32 width, const i32 height)
{
    // Calculate projection matrix (from perspective instead of frustum
    auto matProj = mat4(1.0f);

    if (camera.projection == CAMERA_PERSPECTIVE)
    {
        // Calculate projection matrix from perspective
        matProj = glm::perspective(camera.fovy*DEG2RAD, static_cast<f32>(width)/static_cast<f32>(height), rlGetCullDistanceNear(), rlGetCullDistanceFar());
    }
    else if (camera.projection == CAMERA_ORTHOGRAPHIC)
    {
        const f32 aspect = static_cast<f32>(width)/static_cast<f32>(height);
        const f32 top = camera.fovy/2.0f;
        const f32 right = top*aspect;

        // Calculate projection matrix from orthographic
        matProj = glm::ortho(-right, right, -top, top, rlGetCullDistanceNear(), rlGetCullDistanceFar());
    }

    // Calculate view matrix from camera look at (and transpose it)
    const mat4 matView = glm::lookAt(camera.position, camera.target, camera.up);

    // Convert world position vector to homogeneous clip-space position
    auto worldPos = vec4{ position.x, position.y, position.z, 1.0f };

    // Transform world position to view
    worldPos = matView*worldPos;

    // Transform result to projection (clip space position)
    worldPos = matProj*worldPos;

    // Calculate normalized device coordinates (inverted y)
    const auto ndcPos = vec3{ worldPos.x/worldPos.w, -worldPos.y/worldPos.w, worldPos.z/worldPos.w };

    // Calculate 2d screen position vector
    const auto screenPosition = vec2{ (ndcPos.x + 1.0f)/2.0f*static_cast<f32>(width), (ndcPos.y + 1.0f)/2.0f*static_cast<f32>(height) };

    return screenPosition;
}

// Get screen space position for a 2d camera world space position
vec2 GetWorldToScreen2D(const vec2 position, const Camera2D camera)
{
    const auto matCamera = GetCameraMatrix2D(camera);
    const auto transform = vec3(matCamera*vec4(position.x, position.y, 0.0f, 1.0f));

    return vec2{ transform.x, transform.y };
}

// Get world space position for a 2d camera screen space position
vec2 GetScreenToWorld2D(const vec2 position, const Camera2D camera)
{
    const mat4 invMatCamera = glm::inverse(GetCameraMatrix2D(camera));
    const auto transform = vec3(invMatCamera*vec4(position.x, position.y, 0.0f, 1.0f));

    return vec2{ transform.x, transform.y };
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Timing
//----------------------------------------------------------------------------------

// NOTE: Functions with a platform-specific implementation on rcore_<platform>.c
//double GetTime(void)

// Set target FPS (maximum)
void SetTargetFPS(const i32 fps)
{
    if (fps < 1) CORE.Time.target = 0.0;
    else CORE.Time.target = 1.0/static_cast<f64>(fps);

    logMessage(LogLevel::info, std::format("TIMER: Target time per frame: {:02.03f} milliseconds", static_cast<f32>(CORE.Time.target)*1000.0f));
}

// Get current FPS
// NOTE: Calculating an average framerate
i32 GetFPS()
{
    auto fps = 0;

#if !SUPPORT_CUSTOM_FRAME_CONTROL
    #define FPS_CAPTURE_FRAMES_COUNT    30      // 30 captures
    #define FPS_AVERAGE_TIME_SECONDS   0.5f     // 500 milliseconds
    #define FPS_STEP (FPS_AVERAGE_TIME_SECONDS/FPS_CAPTURE_FRAMES_COUNT)

    static auto index = 0;
    static u32 captureFramesCounted = 0;
    static f32 history[FPS_CAPTURE_FRAMES_COUNT] = { 0 };
    static f32 average = 0, last = 0;
    const auto fpsFrame = GetFrameTime();

    // If reseting the window, reset the FPS info
    if (CORE.Time.frameCounter == 0)
    {
        average = 0;
        last = 0;
        index = 0;
        captureFramesCounted = 0;

        for (i32 i = 0; i < FPS_CAPTURE_FRAMES_COUNT; i++) history[i] = 0;
    }

    if (fpsFrame != 0)
    {
        if ((GetTime() - last) > FPS_STEP)
        {
            last = static_cast<f32>(GetTime());
            index = (index + 1)%FPS_CAPTURE_FRAMES_COUNT;
            average -= history[index];
            history[index] = fpsFrame/FPS_CAPTURE_FRAMES_COUNT;
            average += history[index];

            if (captureFramesCounted < FPS_CAPTURE_FRAMES_COUNT) captureFramesCounted += 1;
        }

        if (captureFramesCounted > 0) fps = static_cast<i32>(roundf(captureFramesCounted/(average*FPS_CAPTURE_FRAMES_COUNT)));
    }
    else fps = 0;
#endif

    return fps;
}

// Get time in seconds for last frame drawn (delta time)
f32 GetFrameTime()
{
    return static_cast<f32>(CORE.Time.frame);
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Custom frame control
//----------------------------------------------------------------------------------

// NOTE: Functions with a platform-specific implementation on rcore_<platform>.c
//void SwapScreenBuffer(void);
//void PollInputEvents(void);

// Wait for some time (stop program execution)
// NOTE: Sleep() granularity could be around 10 ms, it means, Sleep() could
// take longer than expected... for that reason a busy wait loop is used
// REF: http://stackoverflow.com/questions/43057578/c-programming-win32-games-sleep-taking-longer-than-expected
// REF: http://www.geisswerks.com/ryan/FAQS/timing.html --> All about timing on Win32!
void WaitTime(const f64 seconds)
{
    if (seconds < 0) return;    // Security check

#if SUPPORT_BUSY_WAIT_LOOP || SUPPORT_PARTIALBUSY_WAIT_LOOP
    const f64 destinationTime = GetTime() + seconds;
#endif

#if SUPPORT_BUSY_WAIT_LOOP
    while (GetTime() < destinationTime) { }
#else
    #if SUPPORT_PARTIALBUSY_WAIT_LOOP
        const f64 sleepSeconds = seconds - seconds*0.05;  // NOTE: Reserve a percentage of the time for busy waiting
    #else
        f64 sleepSeconds = seconds;
    #endif

    // System halt functions
    #if defined(_WIN32)
        Sleep(static_cast<unsigned long>((sleepSeconds*1000.0)));
    #endif
    #if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__EMSCRIPTEN__)
        timespec req{};
        const time_t sec = sleepSeconds;
        const long nsec = (sleepSeconds - sec)*1000000000L;
        req.tv_sec = sec;
        req.tv_nsec = nsec;

        // NOTE: Use nanosleep() on Unix platforms... usleep() it's deprecated
        while (nanosleep(&req, &req) == -1) continue;
    #endif
    #if defined(__APPLE__)
        usleep(sleepSeconds*1000000.0);
    #endif

    #if SUPPORT_PARTIALBUSY_WAIT_LOOP
        while (GetTime() < destinationTime) { }
    #endif
#endif
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Misc
//----------------------------------------------------------------------------------

// NOTE: Functions with a platform-specific implementation on rcore_<platform>.c
//void OpenURL(const char *url)

// Set the seed for the random number generator
void SetRandomSeed(const u32 seed)
{
#if SUPPORT_RPRAND_GENERATOR
    rprand_set_seed(seed);
#else
    srand(seed);
#endif
}

// Get a random value between min and max included
i32 GetRandomValue(i32 min, i32 max)
{
    auto value = 0;

    if (min > max)
    {
        const i32 tmp = max;
        max = min;
        min = tmp;
    }

#if SUPPORT_RPRAND_GENERATOR
    value = rprand_get_value(min, max);
#else
    // WARNING: Ranges higher than RAND_MAX will return invalid results
    // More specifically, if (max - min) > INT_MAX there will be an overflow,
    // and otherwise if (max - min) > RAND_MAX the random value will incorrectly never exceed a certain threshold
    // NOTE: Depending on the library it can be as low as 32767
    if (static_cast<u32>((max - min)) > static_cast<u32>(RAND_MAX))
    {
        logMessage(LogLevel::warn, std::format("Invalid GetRandomValue() arguments, range should not be higher than {}", RAND_MAX));
    }

    // NOTE: This one-line approach produces a non-uniform distribution,
    // as stated by Donald Knuth in the book The Art of Programming, so
    // using below approach for more uniform results
    //value = (rand()%(abs(max - min) + 1) + min);

    // More uniform range solution
    i32 range = (max - min) + 1;

    // Degenerate/overflow case: fall back to min (same behavior as "always min" instead of UB)
    if (range <= 0) value = min;
    else
    {
        // Rejection sampling to get a uniform integer in [min, max]
        unsigned long c = static_cast<unsigned long>(RAND_MAX) + 1UL; // Number of possible results
        auto m = static_cast<unsigned long>(range);          // Size of the target interval
        unsigned long t = c - (c%m);                                  // Largest multiple of m <= c
        unsigned long r = 0;

        for (;;)
        {
            r = static_cast<unsigned long>(rand());
            if (r < t) break;   // Only accept values within the fair region
        }

        value = min + static_cast<i32>((r%m));
    }
#endif

    return value;
}

// Load random values sequence, no values repeated, min and max included
i32 *LoadRandomSequence(const u32 count, const i32 min, const i32 max)
{
    i32 *values = nullptr;

#if SUPPORT_RPRAND_GENERATOR
    values = rprand_load_sequence(count, min, max);
#else
    if (count > (static_cast<u32>(abs(max - min)) + 1)) return values;  // Security check

    values = static_cast<i32 *>(calloc(count, sizeof(i32)));

    auto value = 0;
    auto dupValue = false;

    for (i32 i = 0; i < static_cast<i32>(count);)
    {
        value = GetRandomValue(min, max);
        dupValue = false;

        for (i32 j = 0; j < i; j++)
        {
            if (values[j] == value)
            {
                dupValue = true;
                break;
            }
        }

        if (!dupValue)
        {
            values[i] = value;
            i++;
        }
    }
#endif

    return values;
}

// Unload random values sequence
void UnloadRandomSequence(i32 *sequence)
{
#if SUPPORT_RPRAND_GENERATOR
    rprand_unload_sequence(sequence);
#else
    free(sequence);
#endif
}

// Takes a screenshot of current screen
void TakeScreenshot(std::string_view fileNameView)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();

#if SUPPORT_MODULE_RTEXTURES
    // Security check to (partially) avoid malicious code
    if (strchr(fileName, '\'') != nullptr) { logMessage(LogLevel::warn, "SYSTEM: Provided fileName could be potentially malicious, avoid [\'] character"); return; }

    // Apply content scaling if required
    auto scale = vec2{ 1.0f, 1.0f };
    if (FLAG_IS_SET(CORE.Window.flags, FLAG_WINDOW_HIGHDPI)) scale = GetWindowScaleDPI();

    auto imgData = rlReadScreenPixels(static_cast<i32>(static_cast<f32>(CORE.Window.render.width)*scale.x), static_cast<i32>(static_cast<f32>(CORE.Window.render.height)*scale.y));
    const auto image = Image{ imgData, static_cast<i32>(static_cast<f32>(CORE.Window.render.width)*scale.x), static_cast<i32>(static_cast<f32>(CORE.Window.render.height)*scale.y), 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };

    char path[MAX_FILEPATH_LENGTH] = { 0 };
    if (!IsPathAbsolute(fileName)) snprintf(path, MAX_FILEPATH_LENGTH, "%s", TextFormat("%s/%s", CORE.Storage.basePath.c_str(), fileName).c_str());
    else snprintf(path, MAX_FILEPATH_LENGTH, "%s", fileName);

    ExportImage(image, path); // WARNING: Module required: rtextures
    free(imgData);

    if (FileExists(path)) logMessage(LogLevel::info, std::format("SYSTEM: [{}] Screenshot taken successfully", path));
    else logMessage(LogLevel::warn, std::format("SYSTEM: [{}] Screenshot could not be saved", path));
#else
    logMessage(LogLevel::warn, "IMAGE: ExportImage() requires module: rtextures");
#endif
}

// Set up window configuration flags (view FLAGS)
// NOTE: This function is expected to be called before window creation,
// because it sets up some flags for the window creation process
// To configure window states after creation, use SetWindowState()
void SetConfigFlags(const u32 flags)
{
    if (CORE.Window.ready) logMessage(LogLevel::warn, "WINDOW: SetConfigFlags called after window initialization, Use \"SetWindowState\" to set flags instead");

    // Selected flags are set but not evaluated at this point,
    // flag evaluation happens at InitWindow() or SetWindowState()
    FLAG_SET(CORE.Window.flags, flags);
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Logging system
//----------------------------------------------------------------------------------
// Map raylib log levels to the engine log levels
static LogLevel ToLogLevel(const i32 logType)
{
    switch (logType)
    {
        case LOG_ALL:     return LogLevel::trace;
        case LOG_TRACE:   return LogLevel::trace;
        case LOG_DEBUG:   return LogLevel::debug;
        case LOG_INFO:    return LogLevel::info;
        case LOG_WARNING: return LogLevel::warn;
        case LOG_ERROR:   return LogLevel::error;
        case LOG_FATAL:   return LogLevel::fatal;
        case LOG_NONE:    return LogLevel::off;
        default:          return LogLevel::info;
    }
}

// Map engine log levels back to raylib log levels
static i32 ToRaylibLogLevel(const LogLevel level)
{
    switch (level)
    {
        case LogLevel::trace:   return LOG_TRACE;
        case LogLevel::debug:   return LOG_DEBUG;
        case LogLevel::info:    return LOG_INFO;
        case LogLevel::warn: return LOG_WARNING;
        case LogLevel::error:   return LOG_ERROR;
        case LogLevel::fatal:   return LOG_FATAL;
        default:                return LOG_NONE;
    }
}

// Bridge from the engine logging pipeline to the public TraceLogCallback
// NOTE: The callback receives the fully formatted line (timestamp and level included)
static void TraceLogBridge(const LogLevel level, const std::string_view line, void *userData)
{
    (void)userData;
    if (traceLog != nullptr) traceLog(ToRaylibLogLevel(level), line);
}

// Set the current threshold (minimum) log level
void SetTraceLogLevel(const i32 logType) { setLogLevel(ToLogLevel(logType)); }

// Show trace log messages (LOG_INFO, LOG_WARNING, LOG_ERROR, LOG_DEBUG)
// NOTE: printf-style varargs adapter, the formatted message is forwarded to the logging system
void TraceLog(const i32 logType, const std::string_view text, ...)
{
    const std::string formatStr(text);  // vsnprintf() requires a null-terminated string

    va_list args;
    va_start(args, text);

    va_list argsCopy;
    va_copy(argsCopy, args);
    const i32 textLength = std::vsnprintf(nullptr, 0, formatStr.c_str(), argsCopy);
    va_end(argsCopy);

    std::string message;
    if (textLength >= 0)
    {
        // Resize to textLength + 1 so vsnprintf() can write the terminating null byte
        // inside the string storage, then drop it again
        message.resize(static_cast<std::size_t>(textLength) + 1);
        std::vsnprintf(message.data(), message.size(), formatStr.c_str(), args);
        message.resize(static_cast<std::size_t>(textLength));
    }

    va_end(args);

    logMessage(ToLogLevel(logType), message);

    if (logType == LOG_FATAL) exit(EXIT_FAILURE);  // If fatal logging, exit program
}

// Set custom trace log
void SetTraceLogCallback(const TraceLogCallback callback)
{
    traceLog = callback;
    setLogCallback((callback != nullptr)? TraceLogBridge : nullptr, nullptr);
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Memory management
//----------------------------------------------------------------------------------
// Internal memory allocator
// NOTE: Initializes to zero by default
void *MemAlloc(const u32 size)
{
    void *ptr = calloc(size, 1);
    return ptr;
}

// Internal memory reallocator
void *MemRealloc(void *ptr, const u32 size)
{
    void *ret = realloc(ptr, size);
    return ret;
}

// Internal memory free
void MemFree(void *ptr)
{
    free(ptr);
}

//----------------------------------------------------------------------------------
// Module Functions Definition: File System management
//----------------------------------------------------------------------------------
// Load data from file into a buffer
u8 * LoadFileData(std::string_view fileNameView, i32 *dataSize)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();

    u8 *data = nullptr;
    *dataSize = 0;

    if (fileName != nullptr)
    {
        if (loadFileData != nullptr) return loadFileData(fileName, dataSize);

        FILE *file = fopen(fileName, "rb");

        if (file != nullptr)
        {
            // WARNING: On binary streams SEEK_END could not be found,
            // using fseek() and ftell() could not work in some (rare) cases
            fseek(file, 0, SEEK_END);
            const i32 size = ftell(file);     // WARNING: ftell() returns 'long int', maximum size returned is INT_MAX (2147483647 bytes)
            fseek(file, 0, SEEK_SET);

            if (size > 0)
            {
                data = static_cast<u8 *>(calloc(size, sizeof(u8)));

                if (data != nullptr)
                {
                    // NOTE: fread() returns number of read elements instead of bytes, so reading [1 byte, size elements]
                    const size_t count = fread(data, sizeof(u8), size, file);

                    // WARNING: fread() returns a size_t value, usually 'unsigned int' (32bit compilation) and 'unsigned long long' (64bit compilation)
                    // dataSize is unified along raylib as a 'int' type, so, for file-sizes >INT_MAX (2147483647 bytes) there is a limitation
                    if (count > 2147483647)
                    {
                        logMessage(LogLevel::warn, std::format("FILEIO: [{}] File is bigger than 2147483647 bytes, avoid using LoadFileData()", fileName));

                        free(data);
                        data = nullptr;
                    }
                    else
                    {
                        *dataSize = static_cast<i32>(count);

                        if ((*dataSize) != size) logMessage(LogLevel::warn, std::format("FILEIO: [{}] File partially loaded ({} bytes out of {})", fileName, *dataSize, size));
                        else logMessage(LogLevel::info, std::format("FILEIO: [{}] File loaded successfully", fileName));
                    }
                }
                else logMessage(LogLevel::warn, std::format("FILEIO: [{}] Failed to allocated memory for file reading", fileName));
            }
            else logMessage(LogLevel::warn, std::format("FILEIO: [{}] Failed to read file", fileName));

            fclose(file);
        }
        else logMessage(LogLevel::warn, std::format("FILEIO: [{}] Failed to open file", fileName));
    }
    else logMessage(LogLevel::warn, "FILEIO: File name provided is not valid");

    return data;
}

// Unload file data allocated by LoadFileData()
void UnloadFileData(u8 *data)
{
    free(data);
}

// Save data to file from buffer
bool SaveFileData(std::string_view fileNameView, const void *data, const i32 dataSize)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();

    auto result = false;

    if (fileName != nullptr)
    {
        if (saveFileData != nullptr) return saveFileData(fileName, data, dataSize);

        FILE *file = fopen(fileName, "wb");

        if (file != nullptr)
        {
            // WARNING: fwrite() returns a size_t value, usually 'unsigned int' (32bit compilation) and 'unsigned long long' (64bit compilation)
            // and expects a size_t input value but as dataSize is limited to INT_MAX (2147483647 bytes), there shouldn't be a problem
            const auto count = static_cast<i32>(fwrite(data, sizeof(u8), dataSize, file));

            if (count == 0) logMessage(LogLevel::warn, std::format("FILEIO: [{}] Failed to write file", fileName));
            else if (count != dataSize) logMessage(LogLevel::warn, std::format("FILEIO: [{}] File partially written", fileName));
            else logMessage(LogLevel::info, std::format("FILEIO: [{}] File saved successfully", fileName));

            const i32 closed = fclose(file);
            if (closed == 0) result = true;
        }
        else logMessage(LogLevel::warn, std::format("FILEIO: [{}] Failed to open file", fileName));
    }
    else logMessage(LogLevel::warn, "FILEIO: File name provided is not valid");

    return result;
}

// Export data to code (.h), returns true on success
bool ExportDataAsCode(const u8 *data, const i32 dataSize, std::string_view fileNameView)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();

    auto result = false;

    if ((data == nullptr) || (dataSize <= 0))
    {
        logMessage(LogLevel::warn, "FILEIO: Invalid data provided to export as code");
        return false;
    }

#ifndef TEXT_BYTES_PER_LINE
    #define TEXT_BYTES_PER_LINE     20
#endif

    // NOTE: Text data buffer size is estimated considering raw data size in bytes
    // and requiring 6 char bytes for every byte: "0x00, "
    auto txtData = static_cast<char *>(calloc(dataSize*6 + 2000, sizeof(char)));

    auto byteCount = 0;
    byteCount += sprintf(txtData + byteCount, "////////////////////////////////////////////////////////////////////////////////////////\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// DataAsCode exporter v1.0 - Raw data exported as an array of bytes                  //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// more info and bugs-report:  github.com/raysan5/raylib                              //\n");
    byteCount += sprintf(txtData + byteCount, "// feedback and support:       ray[at]raylib.com                                      //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// Copyright (c) 2022-2026 Ramon Santamaria (@raysan5)                                //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "////////////////////////////////////////////////////////////////////////////////////////\n\n");

    // Get file name from path
    char varFileName[256] = { 0 };
    snprintf(varFileName, 256, "%s", GetFileNameWithoutExt(fileName).c_str());
    for (i32 i = 0; varFileName[i] != '\0'; i++)
    {
        // Convert variable name to uppercase
        if ((varFileName[i] >= 'a') && (varFileName[i] <= 'z')) { varFileName[i] = varFileName[i] - 32; }
        // Replace non valid character for C identifier with '_'
        else if (varFileName[i] == '.' || varFileName[i] == '-' || varFileName[i] == '?' || varFileName[i] == '!' || varFileName[i] == '+') { varFileName[i] = '_'; }
    }

    byteCount += sprintf(txtData + byteCount, "#define %s_DATA_SIZE     %i\n\n", varFileName, dataSize);

    byteCount += sprintf(txtData + byteCount, "static unsigned char %s_DATA[%s_DATA_SIZE] = { ", varFileName, varFileName);
    for (i32 i = 0; i < (dataSize - 1); i++) byteCount += sprintf(txtData + byteCount, ((i%TEXT_BYTES_PER_LINE == 0)? "0x%x,\n" : "0x%x, "), data[i]);
    byteCount += sprintf(txtData + byteCount, "0x%x };\n", data[dataSize - 1]);

    // NOTE: Text data size exported is determined by '\0' (NULL) character
    result = SaveFileText(fileName, txtData);

    free(txtData);

    if (result != 0) logMessage(LogLevel::info, std::format("FILEIO: [{}] Data as code exported successfully", fileName));
    else logMessage(LogLevel::warn, std::format("FILEIO: [{}] Failed to export data as code", fileName));

    return result;
}

// Load text data from file
std::string LoadFileText(std::string_view fileNameView)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();

    char *text = nullptr;
    size_t textLength = 0;

    if (fileName != nullptr)
    {
        if (loadFileText != nullptr) return loadFileText(fileName);

        FILE *file = fopen(fileName, "rt");

        if (file != nullptr)
        {
            // WARNING: When reading a file as 'text' file,
            // text mode causes carriage return-linefeed translation...
            // ...but using fseek() should return correct byte-offset
            fseek(file, 0, SEEK_END);
            const long fileSize = ftell(file);
            fseek(file, 0, SEEK_SET);

            if (fileSize > 0)
            {
                const auto size = static_cast<size_t>(fileSize);
                text = static_cast<char *>(calloc(size + 1, sizeof(char)));

                if (text != nullptr)
                {
                    const auto count = fread(text, sizeof(char), size, file);

                    // WARNING: \r\n is converted to \n on reading, so,
                    // read bytes count gets reduced by the number of lines
                    // NOTE: No realloc() is required: the buffer was allocated with size + 1 bytes,
                    // so it is always large enough to hold the terminating null byte
                    text[count] = '\0';
                    textLength = count;

                    logMessage(LogLevel::info, std::format("FILEIO: [{}] Text file loaded successfully", fileName));
                }
                else logMessage(LogLevel::warn, std::format("FILEIO: [{}] Failed to allocated memory for file reading", fileName));
            }
            else logMessage(LogLevel::warn, std::format("FILEIO: [{}] Failed to read text file", fileName));

            fclose(file);
        }
        else logMessage(LogLevel::warn, std::format("FILEIO: [{}] Failed to open text file", fileName));
    }
    else logMessage(LogLevel::warn, "FILEIO: File name provided is not valid");

    std::string fileText;
    if (text != nullptr) fileText.assign(text, textLength);
    free(text);

    return fileText;
}

// Unload file text data allocated by LoadFileText()
void UnloadFileText(char *text)
{
    free(text);
}

// Save text data to file (write), string must be '\0' terminated
bool SaveFileText(std::string_view fileNameView, std::string_view textView)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();
    const std::string textStr(textView);
    const char *text = textStr.c_str();

    auto result = false;

    if (fileName != nullptr)
    {
        if (saveFileText != nullptr) return saveFileText(fileName, text);

        FILE *file = fopen(fileName, "wt");

        if (file != nullptr)
        {
            const i32 count = fprintf(file, "%s", text);

            if (count < 0) logMessage(LogLevel::warn, std::format("FILEIO: [{}] Failed to write text file", fileName));
            else logMessage(LogLevel::info, std::format("FILEIO: [{}] Text file saved successfully", fileName));

            const i32 closed = fclose(file);
            if (closed == 0) result = true;
        }
        else logMessage(LogLevel::warn, std::format("FILEIO: [{}] Failed to open text file", fileName));
    }
    else logMessage(LogLevel::warn, "FILEIO: File name provided is not valid");

    return result;
}

// File access custom callbacks
// WARNING: Callbacks setup is intended for advanced users

// Set custom file binary data loader
void SetLoadFileDataCallback(const LoadFileDataCallback callback)
{
    loadFileData = callback;
}

// Set custom file binary data saver
void SetSaveFileDataCallback(const SaveFileDataCallback callback)
{
    saveFileData = callback;
}

// Set custom file text data loader
void SetLoadFileTextCallback(const LoadFileTextCallback callback)
{
    loadFileText = callback;
}

// Set custom file text data saver
void SetSaveFileTextCallback(const SaveFileTextCallback callback)
{
    saveFileText = callback;
}

// Rename file (if exists)
// NOTE: Only rename file name required, not full path
i32 FileRename(std::string_view fileNameView, std::string_view fileRenameView)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();
    const std::string fileRenameStr(fileRenameView);
    const char *fileRename = fileRenameStr.c_str();

    auto result = -1;

    if (FileExists(fileName))
    {
        result = rename(fileName, fileRename);
    }

    return result;
}

// Remove file (if exists)
i32 FileRemove(std::string_view fileNameView)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();

    auto result = -1;

    if (FileExists(fileName))
    {
        result = remove(fileName);
    }

    return result;
}

// Copy file from one path to another
// NOTE: If destination path does not exist, it is created!
i32 FileCopy(std::string_view srcPathView, std::string_view dstPathView)
{
    const std::string srcPathStr(srcPathView);
    const char *srcPath = srcPathStr.c_str();
    const std::string dstPathStr(dstPathView);
    const char *dstPath = dstPathStr.c_str();

    auto result = -1;
    auto srcDataSize = 0;
    auto srcFileData = LoadFileData(srcPath, &srcDataSize);

    if ((srcFileData != nullptr) && (srcDataSize > 0))
    {
        // Get full destination directory path
        const auto dstDirPath = GetDirectoryPath(dstPath);
        
        // Check if directory exist or create it otherwise
        if (DirectoryExists(dstDirPath) || (MakeDirectory(dstDirPath) == 0))
        {
            const auto saved = SaveFileData(dstPath, srcFileData, srcDataSize);
            if (saved) result = 0; // WARNING: SaveFileData() returns true on success
        }
    }

    UnloadFileData(srcFileData);

    return result;
}

// Move file from one directory to another
// NOTE: If dst directories do not exists they are created
i32 FileMove(std::string_view srcPathView, std::string_view dstPathView)
{
    const std::string srcPathStr(srcPathView);
    const char *srcPath = srcPathStr.c_str();
    const std::string dstPathStr(dstPathView);
    const char *dstPath = dstPathStr.c_str();

    auto result = -1;

    if (FileExists(srcPath))
    {
        result = FileCopy(srcPath, dstPath);

        if (result == 0)
        {
            // Make sure file has been correctly copied before removing
            if (FileExists(dstPath) && (GetFileLength(srcPath) == GetFileLength(dstPath)))
            {
                result = FileRemove(srcPath);
                if (result != 0) logMessage(LogLevel::warn, std::format("FILEIO: [{}] Failed to remove source file after copy", srcPath));
            }
            else logMessage(LogLevel::warn, std::format("FILEIO: [{}] Failed to copy file to [{}]", srcPath, dstPath));
        }
    }
	else logMessage(LogLevel::warn, std::format("FILEIO: [{}] Source file does not exist", srcPath));

    return result;
}

// Replace text in an existing file
// WARNING: DEPENDENCY: [rtext] module
i32 FileTextReplace(std::string_view fileNameView, std::string_view searchView, std::string_view replacementView)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();

    auto result = -1;

#if SUPPORT_MODULE_RTEXT
    if (FileExists(fileName) && !searchView.empty())
    {
        const auto fileText = LoadFileText(fileName);

        if (!fileText.empty())
        {
            // Replace every occurrence directly in a std::string: TextReplace() uses a
            // fixed-size static buffer, which silently fails for larger files
            std::string fileTextUpdated = fileText;
            std::size_t position = 0;

            while ((position = fileTextUpdated.find(searchView, position)) != std::string::npos)
            {
                fileTextUpdated.replace(position, searchView.size(), replacementView);
                position += replacementView.size();
            }

            const auto saved = SaveFileText(fileName, fileTextUpdated);
            if (saved) result = 0;
        }
    }
#else
    logMessage(LogLevel::warn, "FILE: File text replace requires [rtext] module");
#endif

    return result;
}

// Find text index position in existing file
// WARNING: DEPENDENCY: [rtext] module
i32 FileTextFindIndex(std::string_view fileNameView, std::string_view searchView)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();
    const std::string searchStr(searchView);
    const char *search = searchStr.c_str();

    auto result = -1;

    if (FileExists(fileName))
    {
        const auto fileText = LoadFileText(fileName);
        if (!fileText.empty())
        {
            const size_t pos = fileText.find(search);
            if (pos != std::string::npos) result = static_cast<i32>(pos);
        }
    }

    return result;
}

// Check if the file exists
bool FileExists(std::string_view fileNameView)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();

    auto result = false;

    if ((fileName != nullptr) && (ACCESS(fileName) != -1)) result = true;

    // NOTE: Alternatively, stat() can be used instead of access()
    //#include <sys/stat.h>
    //struct stat statbuf;
    //if (stat(filename, &statbuf) == 0) result = true;

    return result;
}

// Check file extension
bool IsFileExtension(std::string_view fileNameView, std::string_view extView)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();
    const std::string extStr(extView);
    const char *ext = extStr.c_str();

    #define MAX_FILE_EXTENSIONS  32

    auto result = false;
    auto fileExt = GetFileExtension(fileName);

    // WARNING: fileExt points to last '.' on fileName string but it could happen
    // that fileName is not correct: "myfile.png more text following\n"

    if (!fileExt.empty())
    {
        const auto fileExtLength = static_cast<i32>(fileExt.size());
        char fileExtLower[16] = { 0 };
        for (i32 i = 0; (i < fileExtLength) && (i < 15); i++)
        {
            // Copy and convert to lower-case
            if ((fileExt[i] >= 'A') && (fileExt[i] <= 'Z')) fileExtLower[i] =  fileExt[i] + 32;
            else fileExtLower[i] =  fileExt[i];
        }

        auto extCount = 1;
        const auto extLength = static_cast<i32>(strlen(ext));
        auto extList = static_cast<char *>(calloc(extLength + 1, 1));
        char *extListPtrs[MAX_FILE_EXTENSIONS] = { nullptr };
        memcpy(extList, ext, extLength);
        extListPtrs[0] = extList;

        for (i32 i = 0; i < extLength; i++)
        {
            // Convert to lower-case if extension is upper-case
            if ((extList[i] >= 'A') && (extList[i] <= 'Z')) extList[i] += 32;

            // Get pointer to next extension and add null-terminator
            if (extList[i] == ';')
            {
                extList[i] = '\0';

                if (extCount < MAX_FILE_EXTENSIONS)
                {
                    extListPtrs[extCount] = extList + i + 1;
                    extCount++;
                }
            }
        }

        for (i32 i = 0; i < extCount; i++)
        {
            // Consider the case where extension provided
            // does not start with the '.'
            char *fileExtLowerPtr = fileExtLower;
            if (extListPtrs[i][0] != '.') fileExtLowerPtr++;

            if (strcmp(fileExtLowerPtr, extListPtrs[i]) == 0)
            {
                result = true;
                break;
            }
        }

        free(extList);
    }

    return result;
}

// Check if file path (file or directory) is hidden by OS
bool IsFileHidden(std::string_view filePathView)
{
    const std::string filePathStr(filePathView);
    const char *filePath = filePathStr.c_str();

    auto result = false;
#if defined(_WIN32)
    unsigned long attribs = GetFileAttributesA(filePath);

    // Check !INVALID_FILE_ATTRIBUTES and FILE_ATTRIBUTE_HIDDEN
    if ((attribs != -1) && ((attribs & 0x2UL) != 0)) result = true;
#else
    const char *basePath = strrchr(filePath, '/');
    basePath = ((basePath != nullptr)? basePath + 1 : filePath);

    if ((basePath[0] == '.') && (strcmp(basePath, ".") != 0) && (strcmp(basePath, "..") != 0)) result = true;
#endif
    return result;
}

// Check if directory path exists
bool DirectoryExists(std::string_view dirPathView)
{
    const std::string dirPathStr(dirPathView);
    const char *dirPath = dirPathStr.c_str();

    auto result = false;
    DIR *dir = opendir(dirPath);

    if (dir != nullptr)
    {
        result = true;
        closedir(dir);
    }

    return result;
}

// Get file length in bytes
// NOTE: GetFileSize() conflicts with windows.h
i32 GetFileLength(std::string_view fileNameView)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();

    auto size = 0;

    // NOTE: On Unix-like systems, it can by used the POSIX system call: stat(),
    // but depending on the platform that call could not be available
    //struct stat result = { 0 };
    //stat(fileName, &result);
    //return result.st_size;

    FILE *file = fopen(fileName, "rb");

    if (file != nullptr)
    {
        fseek(file, 0L, SEEK_END);
        const i32 fileSize = ftell(file);

        // Check for size overflow (INT_MAX)
        if (fileSize > 2147483647) logMessage(LogLevel::warn, std::format("[{}] File size overflows expected limit, do not use GetFileLength()", fileName));
        else size = static_cast<i32>(fileSize);

        fclose(file);
    }

    return size;
}

// Get file modification time (last write time)
long GetFileModTime(std::string_view fileNameView)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();

    struct stat result = { 0 };
    long modTime = 0;

    if (stat(fileName, &result) == 0)
    {
        const time_t mod = result.st_mtime;
        modTime = static_cast<long>(mod);
    }

    return modTime;
}

// Get pointer to extension for a filename string (includes the dot: .png)
// WARNING: Getting the pointer to the input string extension position (not a string copy)
std::string GetFileExtension(std::string_view fileNameView)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();

    // Only the file name part can contain the extension, not a directory name
    const auto name = GetFileName(fileName);
    const size_t dot = name.rfind('.');

    if ((dot == std::string::npos) || (dot == 0)) return {};   // No extension for hidden files: ".gitignore"

    return name.substr(dot);
}

// String pointer reverse break: returns right-most occurrence of charset in s
static const char *strprbrk(const char *text, const char *charset)
{
    const char *latestMatch = nullptr;

    for (; (text != nullptr) && ((text = strpbrk(text, charset)) != nullptr); latestMatch = text++) { }

    return latestMatch;
}

// Get pointer to filename for a path string
std::string GetFileName(std::string_view filePathView)
{
    const std::string filePathStr(filePathView);
    const char *filePath = filePathStr.c_str();

    const char *fileName = nullptr;

    if (filePath != nullptr) fileName = strprbrk(filePath, "\\/");

    if (fileName == nullptr) return filePath;

    return fileName + 1;
}

// Get filename string without extension (uses static string)
std::string GetFileNameWithoutExt(std::string_view filePathView)
{
    const std::string filePathStr(filePathView);
    const char *filePath = filePathStr.c_str();

    #define MAX_FILENAME_LENGTH     256

    static char fileName[MAX_FILENAME_LENGTH] = { 0 };
    memset(fileName, 0, MAX_FILENAME_LENGTH);

    if (filePath != nullptr)
    {
        const auto baseName = GetFileName(filePath);
        snprintf(fileName, MAX_FILENAME_LENGTH, "%s", baseName.c_str()); // Get filename.ext without path
        const auto fileNameLength = static_cast<i32>(strlen(fileName));   // Get size in bytes

        for (i32 i = fileNameLength; i > 0; i--) // Reverse search '.'
        {
            if (fileName[i] == '.')
            {
                // NOTE: Break on first '.' found
                fileName[i] = '\0';
                break;
            }
        }
    }

    return fileName;
}

// Get directory for a provided filePath
std::string GetDirectoryPath(std::string_view filePathView)
{
    const std::string filePathStr(filePathView);
    const char *filePath = filePathStr.c_str();

    /*
    // NOTE: Directory separator is different in Windows and other platforms,
    // fortunately, Windows also support the '/' separator, that's the one should be used
    #if defined(_WIN32)
        char separator = '\\';
    #else
        char separator = '/';
    #endif
    */
    const char *lastSlash = nullptr;
    static char dirPath[MAX_FILEPATH_LENGTH] = { 0 };
    memset(dirPath, 0, MAX_FILEPATH_LENGTH);

    // In case provided path does not contain a root drive letter (C:\, D:\)
    // nor leading path separator (\, /), add the current directory path to dirPath
    const bool isRelative = (filePath[0] != '\\') && (filePath[0] != '/') && ((filePath[0] == '\0') || (filePath[1] != ':'));
    if (isRelative)
    {
        // For security, set starting path to current directory,
        // obtained path will be concatenated to this
        dirPath[0] = '.';
        dirPath[1] = '/';
    }

    lastSlash = strprbrk(filePath, "\\/");
    if (lastSlash != nullptr)
    {
        if (lastSlash == filePath)
        {
            // The last and only slash is the leading one: path is in a root directory
            dirPath[0] = filePath[0];
            dirPath[1] = '\0';
        }
        else
        {
            const i32 dirPathOffset = isRelative? 2 : 0;          // Skip "./" added for relative paths
            const auto dirPathLength = static_cast<i32>(lastSlash - filePath);

            if ((dirPathOffset + dirPathLength) < MAX_FILEPATH_LENGTH)
            {
                memcpy(dirPath + dirPathOffset, filePath, dirPathLength);
                dirPath[dirPathOffset + dirPathLength] = '\0';
            }
            else
            {
                logMessage(LogLevel::warn, "FILEIO: Directory path too long");
                dirPath[0] = '\0';
            }
        }
    }

    return dirPath;
}

// Get previous directory path for a provided path
std::string GetPrevDirectoryPath(std::string_view dirPathView)
{
    const std::string dirPathStr(dirPathView);
    const char *dirPath = dirPathStr.c_str();

    static char prevDirPath[MAX_FILEPATH_LENGTH] = { 0 };
    memset(prevDirPath, 0, MAX_FILEPATH_LENGTH);

    const i32 lastIndex = static_cast<i32>(strlen(dirPath)) - 1;
    auto isFile = IsPathFile(dirPath);
    for (i32 i = lastIndex; i >= 0; i--)
    {
        // If the character is a path separator.
        if ((dirPath[i] == '\\') || (dirPath[i] == '/'))
        {
            // If this character is a leading '/' (e.g. the '/' in "/usr") or
            // part of a drive root (e.g. "C:\"), include it with the result.
            if ((i == 0) || ((i == 2) && (dirPath[1] == ':'))) i += 1;
            // If this character is a trailing path separator (e.g. the last
            // '/' in "/usr/bin/" or the last '\' in "C:\raylib\"), continue.
            else if (i == lastIndex) continue;

            if (!isFile)
            {
                if (i < MAX_FILEPATH_LENGTH) memcpy(prevDirPath, dirPath, i);
                else logMessage(LogLevel::warn, "FILEIO: Directory path too long");
                break;
            }
            else isFile = false;
        }
    }

    return prevDirPath;
}

// Get current working directory
std::string GetWorkingDirectory()
{
    static char currentDir[MAX_FILEPATH_LENGTH] = { 0 };
    memset(currentDir, 0, MAX_FILEPATH_LENGTH);

    char *path = GETCWD(currentDir, MAX_FILEPATH_LENGTH);

    return GetStringOrEmpty(path);
}

std::string GetApplicationDirectory()
{
    static char appDir[MAX_FILEPATH_LENGTH] = { 0 };
    memset(appDir, 0, MAX_FILEPATH_LENGTH);

#if defined(_WIN32)
    auto len = 0;

    #if defined(UNICODE)
    wchar_t widePath[MAX_PATH];
    len = GetModuleFileNameW(nullptr, widePath, MAX_PATH);
    len = WideCharToMultiByte(0, 0, widePath, len, appDir, MAX_PATH, nullptr, nullptr);
    #else
    len = GetModuleFileNameA(nullptr, appDir, MAX_PATH);
    #endif

    if (len > 0)
    {
        for (i32 i = len; i >= 0; i--)
        {
            if (appDir[i] == '\\')
            {
                appDir[i + 1] = '\0';
                break;
            }
        }
    }
    else
    {
        appDir[0] = '.';
        appDir[1] = '\\';
    }

#elif defined(__linux__)

    const u32 size = sizeof(appDir);
    const ssize_t len = readlink("/proc/self/exe", appDir, size);

    if (len > 0)
    {
        for (i32 i = len; i >= 0; i--)
        {
            if (appDir[i] == '/')
            {
                appDir[i + 1] = '\0';
                break;
            }
        }
    }
    else
    {
        appDir[0] = '.';
        appDir[1] = '/';
    }

#elif defined(__APPLE__)

    u32 size = sizeof(appDir);

    if (_NSGetExecutablePath(appDir, &size) == 0)
    {
        auto appDirLength = static_cast<i32>(strlen(appDir));
        for (i32 i = appDirLength; i >= 0; i--)
        {
            if (appDir[i] == '/')
            {
                appDir[i + 1] = '\0';
                break;
            }
        }
    }
    else
    {
        appDir[0] = '.';
        appDir[1] = '/';
    }

#elif defined(__FreeBSD__)

    size_t size = sizeof(appDir);
    i32 mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};

    if (sysctl(mib, 4, appDir, &size, nullptr, 0) == 0)
    {
        auto appDirLength = static_cast<i32>(strlen(appDir));
        for (i32 i = appDirLength; i >= 0; i--)
        {
            if (appDir[i] == '/')
            {
                appDir[i + 1] = '\0';
                break;
            }
        }
    }
    else
    {
        appDir[0] = '.';
        appDir[1] = '/';
    }

#elif defined(__wasm__)

    appDir[0] = '/';
#endif

    return appDir;
}

// Load directory filepaths
// NOTE: Base path is prepended to the scanned filepaths
// WARNING: Directory is scanned twice, first time to get paths count
// Scanneed files and directories, no recursive/subdirs scanning
FilePathList LoadDirectoryFiles(std::string_view dirPathView)
{
    const std::string dirPathStr(dirPathView);
    const char *dirPath = dirPathStr.c_str();

    return LoadDirectoryFilesEx(dirPath, FILE_FILTER_TAG_ALL, false);
}

// Load directory filepaths with extension filtering and recursive directory scan
// Use "*.*" to include all files and directories on scan
// Use "FILES*" to include only files on scan
// Use "DIRS*" to include only directories on scan
// WARNING: Directory is scanned twice, first time to get paths count
FilePathList LoadDirectoryFilesEx(std::string_view basePathView, std::string_view filterView, const bool scanSubdirs)
{
    const std::string basePathStr(basePathView);
    const char *basePath = basePathStr.c_str();
    const std::string filterStr(filterView);
    const char *filter = filterStr.c_str();

    FilePathList files;

    if (DirectoryExists(basePath)) // It's a directory
    {

        // Reserve space for the scanned filepaths
        files.paths.reserve(GetDirectoryFileCountEx(basePath, filter, scanSubdirs));

        // Read filepaths
        // WARNING: basePath is always prepended to scanned paths
        ScanDirectoryFiles(basePath, &files, filter, scanSubdirs);
    }
    else logMessage(LogLevel::warn, std::format("FILEIO: Directory cannot be opened ({})", basePath));  // Maybe it's a file...

    return files;
}

// Create directories (including full path requested), returns 0 on success
i32 MakeDirectory(std::string_view dirPathView)
{
    const std::string dirPathStr(dirPathView);
    const char *dirPath = dirPathStr.c_str();

    if ((dirPath == nullptr) || (dirPath[0] == '\0')) return -1; // Path is not valid
    if (DirectoryExists(dirPath)) return 0;                      // Path already exists (is valid)

    // Copy path string to avoid modifying original
    const i32 dirPathLength = static_cast<i32>(strlen(dirPath)) + 1;
    auto pathcpy = static_cast<char *>(calloc(dirPathLength, 1));
    memcpy(pathcpy, dirPath, dirPathLength);

    // Iterate over pathcpy, create each subdirectory as needed
    for (i32 i = 0; (i < dirPathLength) && (pathcpy[i] != '\0'); i++)
    {
        if (pathcpy[i] == ':') i++;
        else
        {
            if ((pathcpy[i] == '\\') || (pathcpy[i] == '/'))
            {
                pathcpy[i] = '\0';
                if (!DirectoryExists(pathcpy)) MKDIR(pathcpy);
                pathcpy[i] = '/';
            }
        }
    }

    // Create final directory
    if (!DirectoryExists(pathcpy)) MKDIR(pathcpy);
    free(pathcpy);

    // In case something failed and requested directory
    // was not successfully created, return -1
    if (!DirectoryExists(dirPath)) return -1;

    return 0;
}

// Change working directory, returns true on success
i32 ChangeDirectory(std::string_view dirPathView)
{
    const std::string dirPathStr(dirPathView);
    const char *dirPath = dirPathStr.c_str();

    // NOTE: On success, CHDIR() return 0; on error, returns -1 and errno is set to indicate the error,
    // depending on the filesystem, other errors can be returned
    const i32 result = CHDIR(dirPath);

    if (result != 0) logMessage(LogLevel::warn, std::format("SYSTEM: Failed to change to directory: {}", dirPath));
    else logMessage(LogLevel::info, std::format("SYSTEM: Working Directory: {}", dirPath));

    return result;
}

// Check if provided path point to a file
bool IsPathFile(std::string_view pathView)
{
    const std::string pathStr(pathView);
    const char *path = pathStr.c_str();

    auto result = false;

    struct stat info = { 0 };
    stat(path, &info);

    if (S_ISREG(info.st_mode)) result = true;

    return result;
}

// Check if provided path point to a directory
bool IsPathDirectory(std::string_view pathView)
{
    const std::string pathStr(pathView);
    const char *path = pathStr.c_str();

    auto result = false;

    struct stat info = { 0 };
    if ((stat(path, &info) == 0) && S_ISDIR(info.st_mode)) result = true;

    return result;
}

// Check if provided path is an absolute path
bool IsPathAbsolute(std::string_view pathView)
{
    const std::string pathStr(pathView);
    const char *path = pathStr.c_str();

    i32 result = false;

    if ((path != nullptr) && (path[0] != '\0'))
    {
#if defined(_WIN32)
        // Check UNC path (\\server\share)
        if ((path[0] == '\\') && (path[1] == '\\')) result = true;
        // Check path starts with a drive letter (e.g. C:\ or D:/)
        else if ((((path[0] >= 'A') && (path[0] <= 'Z')) || ((path[0] >= 'a') && (path[0] <= 'z'))) &&
                 (path[1] != '\0') && (path[1] == ':') && (path[2] != '\0') && ((path[2] == '\\') || (path[2] == '/'))) result = true;
#else
        // Check POSIX path, must start with /
        if (path[0] == '/') result = true;
#endif
    }

    return result;
}

// Check if fileName is valid for the platform/OS
bool IsFileNameValid(std::string_view fileNameView)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();

    auto valid = true;

    if ((fileName != nullptr) && (fileName[0] != '\0'))
    {
        const auto fileNameLength = static_cast<i32>(strlen(fileName));
        auto allPeriods = true;

        for (i32 i = 0; i < fileNameLength; i++)
        {
            // Check invalid characters
            if ((fileName[i] == '<') ||
                (fileName[i] == '>') ||
                (fileName[i] == ':') ||
                (fileName[i] == '\"') ||
                (fileName[i] == '/') ||
                (fileName[i] == '\\') ||
                (fileName[i] == '|') ||
                (fileName[i] == '?') ||
                (fileName[i] == '*')) { valid = false; break; }

            // Check non-glyph characters
            if (static_cast<u8>(fileName[i]) < 32) { valid = false; break; }

            // Check if filename is not all periods
            if (fileName[i] != '.') allPeriods = false;
        }

        if (allPeriods) valid = false;

/*
        if (valid)
        {
            // Check invalid DOS names
            if (fileNameLength >= 3)
            {
                if (((fileName[0] == 'C') && (fileName[1] == 'O') && (fileName[2] == 'N')) ||   // CON
                    ((fileName[0] == 'P') && (fileName[1] == 'R') && (fileName[2] == 'N')) ||   // PRN
                    ((fileName[0] == 'A') && (fileName[1] == 'U') && (fileName[2] == 'X')) ||   // AUX
                    ((fileName[0] == 'N') && (fileName[1] == 'U') && (fileName[2] == 'L'))) valid = false; // NUL
            }

            if (fileNameLength >= 4)
            {
                if (((fileName[0] == 'C') && (fileName[1] == 'O') && (fileName[2] == 'M') && ((fileName[3] >= '0') && (fileName[3] <= '9'))) ||  // COM0-9
                    ((fileName[0] == 'L') && (fileName[1] == 'P') && (fileName[2] == 'T') && ((fileName[3] >= '0') && (fileName[3] <= '9')))) valid = false; // LPT0-9
            }
        }
*/
    }

    return valid;
}

// Check if file has been dropped into window
bool IsFileDropped()
{
    auto result = false;

    if (CORE.Window.dropFileCount > 0) result = true;

    return result;
}

// Load dropped filepaths
FilePathList LoadDroppedFiles()
{
    FilePathList files;

    files.paths.reserve(CORE.Window.dropFileCount);
    for (u32 i = 0; i < CORE.Window.dropFileCount; i++) files.paths.emplace_back(CORE.Window.dropFilepaths[i]);

    return files;
}

// Get the file count in a directory
u32 GetDirectoryFileCount(std::string_view dirPathView)
{
    const std::string dirPathStr(dirPathView);
    const char *dirPath = dirPathStr.c_str();

    return GetDirectoryFileCountEx(dirPath, FILE_FILTER_TAG_ALL, false);
}

// Get the file count in a directory with extension filtering and recursive directory scan. Use 'FILE_FILTER_TAG_DIR_ONLY' in the filter string to include directories in the result
u32 GetDirectoryFileCountEx(std::string_view basePathView, std::string_view filterView, const bool scanSubdirs)
{
    const std::string basePathStr(basePathView);
    const char *basePath = basePathStr.c_str();
    const std::string filterStr(filterView);
    const char *filter = filterStr.c_str();

    u32 fileCounter = 0;

    // WARNING: Path can not be static or it will be reused between recursive function calls!
    char path[MAX_FILEPATH_LENGTH] = { 0 };
    memset(path, 0, MAX_FILEPATH_LENGTH);

    struct dirent *entity;
    DIR *dir = opendir(basePath);

    if (dir != nullptr) // It's a directory
    {
        while ((entity = readdir(dir)) != nullptr)
        {
            // NOTE: Skipping '.' (current dir) and '..' (parent dir) filepaths
            if ((strcmp(entity->d_name, ".") != 0) && (strcmp(entity->d_name, "..") != 0))
            {
                // Construct new path from our base path
                #if defined(_WIN32)
                    i32 pathLength = snprintf(path, MAX_FILEPATH_LENGTH, "%s\\%s", basePath, entity->d_name);
                #else
                    const i32 pathLength = snprintf(path, MAX_FILEPATH_LENGTH, "%s/%s", basePath, entity->d_name);
                #endif
                // Don't add to count if path too long
                if ((pathLength < 0) || (pathLength >= MAX_FILEPATH_LENGTH))
                {
                    logMessage(LogLevel::warn, std::format("FILEIO: Path longer than {} characters ({}...)", MAX_FILEPATH_LENGTH, basePath));
                }
                else if (IsPathFile(path))
                {
                    if ((filter == nullptr) || (filter[0] == '\0') || (strstr(filter, FILE_FILTER_TAG_ALL) != nullptr) ||
                        (strstr(filter, FILE_FILTER_TAG_FILE_ONLY) != nullptr) || IsFileExtension(path, filter)) fileCounter++;
                }
                else
                {
                    if (!filterStr.empty() && ((strstr(filter, FILE_FILTER_TAG_ALL) != nullptr) || (strstr(filter, FILE_FILTER_TAG_DIR_ONLY) != nullptr))) fileCounter++;
                    if (scanSubdirs) fileCounter += GetDirectoryFileCountEx(path, filter, scanSubdirs);
                }
            }
        }
        closedir(dir);
    }
    else logMessage(LogLevel::warn, std::format("FILEIO: Directory cannot be opened ({})", basePath));  // Maybe it's a file...
    return fileCounter;
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Compression and Encoding
//----------------------------------------------------------------------------------

// Compress data (DEFLATE algorithm)
u8 *CompressData(const u8 *data, const i32 dataSize, i32 *compDataSize)
{
    #define COMPRESSION_QUALITY_DEFLATE  8

    u8 *compData = nullptr;

#if SUPPORT_COMPRESSION_API
    // Compress data and generate a valid DEFLATE stream
    auto sdefl = static_cast<struct sdefl *>(calloc(1, sizeof(struct sdefl)));   // WARNING: Possible stack overflow, struct sdefl is almost 1MB
    const i32 bounds = sdefl_bound(dataSize);
    compData = static_cast<u8 *>(calloc(bounds, 1));

    *compDataSize = sdeflate(sdefl, compData, data, dataSize, COMPRESSION_QUALITY_DEFLATE);   // Compression level 8, same as stbiw
    free(sdefl);

    logMessage(LogLevel::info, std::format("SYSTEM: Compress data: Original size: {} -> Comp. size: {}", dataSize, *compDataSize));
#endif

    return compData;
}

// Decompress data (DEFLATE algorithm)
u8 *DecompressData(const u8 *compData, const i32 compDataSize, i32 *dataSize)
{
    u8 *data = nullptr;

#if SUPPORT_COMPRESSION_API
    // Decompress data from a valid DEFLATE stream
    auto data0 = static_cast<u8 *>(calloc(MAX_DECOMPRESSION_SIZE*1024*1024, 1));
    const i32 size = sinflate(data0, MAX_DECOMPRESSION_SIZE*1024*1024, compData, compDataSize);

    // WARNING: realloc can make (and leave) data copies in memory,
    // that can be a security concern in case of compression of sensitive data
    // So, using a second buffer to copy data manually, wiping original buffer memory
    data = static_cast<u8 *>(calloc(size, 1));
    memcpy(data, data0, size);
    memset(data0, 0, MAX_DECOMPRESSION_SIZE*1024*1024); // Wipe memory, is memset() safe?
    free(data0);

    logMessage(LogLevel::info, std::format("SYSTEM: Decompress data: Comp. size: {} -> Original size: {}", compDataSize, size));

    *dataSize = size;
#endif

    return data;
}

// Encode data to Base64 string
// NOTE: Returned string includes NULL terminator, considered on outputSize
std::string EncodeDataBase64(const u8 *data, const i32 dataSize, i32 *outputSize)
{
    // Base64 conversion table from RFC 4648 [0..63]
    // NOTE: They represent 64 values (6 bits), to encode 3 bytes of data into 4 "sextets" (6bit characters)
    static constexpr char base64EncodeTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    // Compute expected size and padding
    i32 paddedSize = dataSize;
    while (paddedSize%3 != 0) paddedSize++; // Padding bytes to round 4*(dataSize/3) to 4 bytes
    i32 estimatedOutputSize = 4*(paddedSize/3);
    const i32 padding = paddedSize - dataSize;

    // Adding null terminator to string
    estimatedOutputSize += 1;

    // Load some memory to store encoded string
    auto encodedData = static_cast<char *>(calloc(estimatedOutputSize, 1));

    auto outputCount = 0;
    for (i32 i = 0; i < dataSize;)
    {
        u32 octetA = 0;
        u32 octetB = 0;
        u32 octetC = 0;
        u32 octetPack = 0;

        octetA = data[i];                               // Generates 2 sextets
        octetB = ((i + 1) < dataSize)? data[i + 1] : 0; // Generates 3 sextets
        octetC = ((i + 2) < dataSize)? data[i + 2] : 0; // Generates 4 sextets

        octetPack = (octetA << 16) | (octetB << 8) | octetC;

        encodedData[outputCount + 0] = static_cast<u8>(base64EncodeTable[(octetPack >> 18) & 0x3f]);
        encodedData[outputCount + 1] = static_cast<u8>(base64EncodeTable[(octetPack >> 12) & 0x3f]);
        encodedData[outputCount + 2] = static_cast<u8>(base64EncodeTable[(octetPack >> 6) & 0x3f]);
        encodedData[outputCount + 3] = static_cast<u8>(base64EncodeTable[octetPack & 0x3f]);
        outputCount += 4;
        i += 3;
    }

    // Add required padding bytes
    for (i32 p = 0; p < padding; p++) encodedData[outputCount - p - 1] = '=';

    // Add null terminator to string
    encodedData[outputCount] = '\0';
    outputCount++;

    if (outputCount != estimatedOutputSize) logMessage(LogLevel::warn, "BASE64: Output size differs from estimation");

    *outputSize = estimatedOutputSize;

    auto encoded = GetStringOrEmpty(encodedData);
    free(encodedData);

    return encoded;
}

// Decode Base64 string (expected NULL terminated)
u8 * DecodeDataBase64(std::string_view textView, i32 *outputSize)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();

    // Base64 decode table
    // NOTE: Following ASCII order [0..255] assigning the expected sixtet value to
    // every character in the corresponding ASCII position
    // NOTE: Built with a lambda initializer, designated initializers (['A'] = 0) are not valid C++
    static constexpr std::array<u8, 256> base64DecodeTable = []() {
        std::array<u8, 256> table{};
        for (i32 c = 'A'; c <= 'Z'; c++) table[c] = static_cast<u8>(c - 'A');
        for (i32 c = 'a'; c <= 'z'; c++) table[c] = static_cast<u8>(c - 'a' + 26);
        for (i32 c = '0'; c <= '9'; c++) table[c] = static_cast<u8>(c - '0' + 52);
        table['+'] = 62;
        table['/'] = 63;
        return table;
    }();

    *outputSize = 0;
    if (text == nullptr) return nullptr;

    // Compute expected size and padding
    const auto dataSize = static_cast<i32>(strlen(text)); // WARNING: Expecting NULL terminated strings!
    i32 ending = dataSize - 1;
    auto padding = 0;
    while ((ending >= 0) && (text[ending] == '=')) { padding++; ending--; }
    i32 estimatedOutputSize = 3*(dataSize/4) - padding;
    if (estimatedOutputSize < 0) estimatedOutputSize = 0;   // Invalid input, i.e. "QQ=" or "===="
    const i32 maxOutputSize = 3*(dataSize/4);

    // Load some memory to store decoded data
    // NOTE: Allocated enough size to include padding
    // NOTE: calloc(0) is implementation-defined (may return NULL), so allocate at least one byte
    auto decodedData = static_cast<u8 *>(calloc((maxOutputSize > 0) ? maxOutputSize : 1, 1));

    auto outputCount = 0;
    for (i32 i = 0; i < dataSize;)
    {
        // Every 4 sextets must generate 3 octets
        if ((i + 2) >= dataSize)
        {
            logMessage(LogLevel::warn, "BASE64: Decoding error: Input data size is not valid");
            break;
        }

        const u32 sixtetA = base64DecodeTable[static_cast<u8>(text[i])];
        const u32 sixtetB = base64DecodeTable[static_cast<u8>(text[i + 1])];
        const u32 sixtetC = (((i + 2) < dataSize) && static_cast<u8>(text[i + 2]) != '=')? base64DecodeTable[static_cast<u8>(text[i + 2])] : 0;
        const u32 sixtetD = (((i + 3) < dataSize) && static_cast<u8>(text[i + 3]) != '=')? base64DecodeTable[static_cast<u8>(text[i + 3])] : 0;

        const u32 octetPack = (sixtetA << 18) | (sixtetB << 12)  | (sixtetC << 6) | sixtetD;

        if ((outputCount + 3) > maxOutputSize)
        {
            logMessage(LogLevel::warn, "BASE64: Decoding error: Output data size is too small");
            break;
        }

        decodedData[outputCount + 0] = (octetPack >> 16) & 0xff;
        decodedData[outputCount + 1] = (octetPack >> 8) & 0xff;
        decodedData[outputCount + 2] = octetPack & 0xff;
        outputCount += 3;
        i += 4;
    }

    if (estimatedOutputSize != (outputCount - padding)) logMessage(LogLevel::warn, "BASE64: Decoded size differs from estimation");

    *outputSize = estimatedOutputSize;
    return decodedData;
}

// Compute CRC32 hash code
u32 ComputeCRC32(const u8 *data, const i32 dataSize)
{
    static const u32 crcTable[256] = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
        0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
        0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
        0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
        0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
        0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
        0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
        0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
        0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
        0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
        0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
        0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
        0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
        0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
        0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
        0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
        0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
        0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
        0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
        0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
        0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
        0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
        0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
        0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
        0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
        0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
        0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
        0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
        0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
    };

    u32 crc = ~0u;

    for (i32 i = 0; i < dataSize; i++) crc = (crc >> 8) ^ crcTable[data[i] ^ (crc & 0xff)];

    return ~crc;
}

// Compute MD5 hash code
// NOTE: Returns a static int[4] array (16 bytes)
u32 *ComputeMD5(const u8 *data, const i32 dataSize)
{
    #define ROTATE_LEFT(x, c) (((x) << (c)) | ((x) >> (32 - (c))))

    static u32 hash[4] = { 0 };  // Hash to be returned

    // WARNING: All variables are unsigned 32 bit and wrap modulo 2^32 when calculating

    // NOTE: r specifies the per-round shift amounts
    const u32 r[] = {
        7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
        5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
        4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
        6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
    };

    // Using binary integer part of the sines of integers (in radians) as constants
    const u32 k[] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
        0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
        0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
        0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
        0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
        0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
        0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
        0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
        0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
    };

    hash[0] = 0x67452301;
    hash[1] = 0xefcdab89;
    hash[2] = 0x98badcfe;
    hash[3] = 0x10325476;

    // Pre-processing: adding a single 1 bit
    // Append '1' bit to message
    // NOTE: The input bytes are considered as bits strings,
    // where the first bit is the most significant bit of the byte

    // Pre-processing: padding with zeros
    // Append '0' bit until message length in bit 448 (mod 512)
    // Append length mod (2 pow 64) to message

    const i32 newDataSize = ((((dataSize + 8)/64) + 1)*64) - 8;

    auto msg = static_cast<u8 *>(calloc(newDataSize + 64, 1)); // Initialize with '0' bits, allocating 64 extra bytes
    memcpy(msg, data, dataSize);
    msg[dataSize] = 128; // Write the '1' bit

    u32 bitsLen = 8*dataSize;
    memcpy(msg + newDataSize, &bitsLen, 4); // Append the len in bits at the end of the buffer

    // Process the message in successive 512-bit chunks for each 512-bit chunk of message
    for (i32 offset = 0; offset < newDataSize; offset += 64)  // 512/8
    {
        // Break chunk into sixteen 32-bit words w[j], 0 <= j <= 15
        auto w = reinterpret_cast<u32 *>(msg + offset);

        // Initialize hash value for this chunk
        u32 a = hash[0];
        u32 b = hash[1];
        u32 c = hash[2];
        u32 d = hash[3];

        for (i32 i = 0; i < 64; i++)
        {
            u32 f = 0;
            u32 g = 0;

            if (i < 16)
            {
                f = (b & c) | ((~b) & d);
                g = i;
            }
            else if (i < 32)
            {
                f = (d & b) | ((~d) & c);
                g = (5*i + 1)%16;
            }
            else if (i < 48)
            {
                f = b ^ c ^ d;
                g = (3*i + 5)%16;
            }
            else
            {
                f = c ^ (b | (~d));
                g = (7*i)%16;
            }

            const u32 temp = d;
            d = c;
            c = b;
            b = b + ROTATE_LEFT((a + f + k[i] + w[g]), r[i]);
            a = temp;
        }

        // Add chunk's hash to result so far
        hash[0] += a;
        hash[1] += b;
        hash[2] += c;
        hash[3] += d;
    }

    free(msg);

    return hash;
}

// Compute SHA-1 hash code
// NOTE: Returns a static int[5] array (20 bytes)
u32 *ComputeSHA1(const u8 *data, const i32 dataSize)
{
    #define SHA1_ROTATE_LEFT(x, c) (((x) << (c)) | ((x) >> (32 - (c))))

    static u32 hash[5] = { 0 };  // Hash to be returned

    // Initialize hash values
    hash[0] = 0x67452301;
    hash[1] = 0xEFCDAB89;
    hash[2] = 0x98BADCFE;
    hash[3] = 0x10325476;
    hash[4] = 0xC3D2E1F0;

    // Pre-processing: adding a single 1 bit
    // Append '1' bit to message
    // NOTE: The input bytes are considered as bits strings,
    // where the first bit is the most significant bit of the byte

    // Pre-processing: padding with zeros
    // Append '0' bit until message length in bit 448 (mod 512)
    // Append length mod (2 pow 64) to message

    const i32 newDataSize = ((((dataSize + 8)/64) + 1)*64);

    auto msg = static_cast<u8 *>(calloc(newDataSize, 1)); // Initialize with '0' bits
    memcpy(msg, data, dataSize);
    msg[dataSize] = 128; // Write the '1' bit

    const unsigned long long bitsLen = 8ULL*dataSize;
    msg[newDataSize - 1] = static_cast<u8>(bitsLen);
    msg[newDataSize - 2] = static_cast<u8>(bitsLen >> 8);
    msg[newDataSize - 3] = static_cast<u8>(bitsLen >> 16);
    msg[newDataSize - 4] = static_cast<u8>(bitsLen >> 24);
    msg[newDataSize - 5] = static_cast<u8>(bitsLen >> 32);
    msg[newDataSize - 6] = static_cast<u8>(bitsLen >> 40);
    msg[newDataSize - 7] = static_cast<u8>(bitsLen >> 48);
    msg[newDataSize - 8] = static_cast<u8>(bitsLen >> 56);

    // Process the message in successive 512-bit chunks
    for (i32 offset = 0; offset < newDataSize; offset += 64)  // 512/8
    {
        // Break chunk into sixteen 32-bit words w[j], 0 <= j <= 15
        u32 w[80] = { 0 };
        for (i32 i = 0; i < 16; i++)
        {
            w[i] = (static_cast<u32>(msg[offset + (i*4) + 0]) << 24) |
                   (static_cast<u32>(msg[offset + (i*4) + 1]) << 16) |
                   (static_cast<u32>(msg[offset + (i*4) + 2]) << 8) |
                   (static_cast<u32>(msg[offset + (i*4) + 3]));
        }

        // Message schedule: extend the sixteen 32-bit words into eighty 32-bit words:
        for (i32 i = 16; i < 80; i++) w[i] = SHA1_ROTATE_LEFT(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);

        // Initialize hash value for this chunk
        u32 a = hash[0];
        u32 b = hash[1];
        u32 c = hash[2];
        u32 d = hash[3];
        u32 e = hash[4];

        for (i32 i = 0; i < 80; i++)
        {
            u32 f = 0;
            u32 k = 0;

            if (i < 20)
            {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            }
            else if (i < 40)
            {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            }
            else if (i < 60)
            {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            }
            else
            {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }

            const u32 temp = SHA1_ROTATE_LEFT(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = SHA1_ROTATE_LEFT(b, 30);
            b = a;
            a = temp;
        }

        // Add this chunk's hash to result so far
        hash[0] += a;
        hash[1] += b;
        hash[2] += c;
        hash[3] += d;
        hash[4] += e;
    }

    free(msg);

    return hash;
}

// Compute SHA-256 hash code
// NOTE: Returns a static int[8] array (32 bytes)
u32 *ComputeSHA256(const u8 *data, const i32 dataSize)
{
    #define SHA256_ROTATE_RIGHT(x, c) ((x >> c) | (x << ((sizeof(u32)*8) - c)))
    #define SHA256_A0(x) (SHA256_ROTATE_RIGHT(x, 7) ^ SHA256_ROTATE_RIGHT(x, 18) ^ (x >> 3))
    #define SHA256_A1(x) (SHA256_ROTATE_RIGHT(x, 17) ^ SHA256_ROTATE_RIGHT(x, 19) ^ (x >> 10))

    static constexpr u32 k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
        0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
        0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
        0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
        0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    static u32 hash[8] = { 0 };
    hash[0] = 0x6A09e667;
    hash[1] = 0xbb67ae85;
    hash[2] = 0x3c6ef372;
    hash[3] = 0xa54ff53a;
    hash[4] = 0x510e527f;
    hash[5] = 0x9b05688c;
    hash[6] = 0x1f83d9ab;
    hash[7] = 0x5be0cd19;

    const unsigned long long bitLen = 8ULL*dataSize;
    unsigned long long paddedSize = dataSize + sizeof(bitLen); // Reserve room for the 64 bit message length appended at the end
    paddedSize += (64 - (paddedSize%64));
    auto buffer = static_cast<u8 *>(calloc(paddedSize, sizeof(u8)));

    memcpy(buffer, data, dataSize);
    buffer[dataSize] = 0x80;
    for (i32 i = 1; i <= sizeof(bitLen); i++)
    {
        buffer[(paddedSize - sizeof(bitLen)) + (i - 1)] = (bitLen >> (8*(sizeof(bitLen) - i))) & 0xFF;
    }

    for (unsigned long long blockN = 0; blockN < paddedSize/64; blockN++)
    {
        u32 a = hash[0];
        u32 b = hash[1];
        u32 c = hash[2];
        u32 d = hash[3];
        u32 e = hash[4];
        u32 f = hash[5];
        u32 g = hash[6];
        u32 h = hash[7];

        u8 *block = buffer + (blockN*64);
        u32 w[64] = { 0 };
        for (i32 i = 0; i < 16; i++)
        {
            w[i] = (static_cast<u32>(block[i*4 + 0]) << 24) |
                   (static_cast<u32>(block[i*4 + 1]) << 16) |
                   (static_cast<u32>(block[i*4 + 2]) << 8)  |
                   (static_cast<u32>(block[i*4 + 3]));
        }
        for (i32 t = 16; t < 64; t++) w[t] = SHA256_A1(w[t - 2]) + w[t - 7] + SHA256_A0(w[t - 15]) + w[t - 16];

        for (i32 t = 0; t < 64; t++)
        {
            const u32 e1 = (SHA256_ROTATE_RIGHT(e, 6) ^ SHA256_ROTATE_RIGHT(e, 11) ^ SHA256_ROTATE_RIGHT(e, 25));
            const u32 ch = ((e & f) ^ (~e & g));
            const u32 t1 = (h + e1 + ch + k[t] + w[t]);
            const u32 e0 = (SHA256_ROTATE_RIGHT(a, 2) ^ SHA256_ROTATE_RIGHT(a, 13) ^ SHA256_ROTATE_RIGHT(a, 22));
            const u32 maj = ((a & b) ^ (a & c) ^ (b & c));
            const u32 t2 = e0 + maj;

            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        hash[0] += a;
        hash[1] += b;
        hash[2] += c;
        hash[3] += d;
        hash[4] += e;
        hash[5] += f;
        hash[6] += g;
        hash[7] += h;
    }

    free(buffer);

    return hash;
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Automation Events Recording and Playing
//----------------------------------------------------------------------------------

// Load automation events list from file, NULL for empty list, capacity = MAX_AUTOMATION_EVENTS
AutomationEventList LoadAutomationEventList(std::string_view fileNameView)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();

    auto list = AutomationEventList{ 0 };

#if SUPPORT_AUTOMATION_EVENTS
    // Allocate and empty automation event list, ready to record new events
    list.events = static_cast<AutomationEvent *>(calloc(MAX_AUTOMATION_EVENTS, sizeof(AutomationEvent)));
    list.capacity = MAX_AUTOMATION_EVENTS;

    if (fileNameView.empty()) logMessage(LogLevel::info, "AUTOMATION: New empty events list loaded successfully");
    else
    {
        // Load automation events file (binary)
        /*
        //int dataSize = 0;
        //unsigned char *data = LoadFileData(fileName, &dataSize);

        FILE *raeFile = fopen(fileName, "rb");
        unsigned char fileId[4] = { 0 };

        fread(fileId, 1, 4, raeFile);

        if ((fileId[0] == 'r') && (fileId[1] == 'A') && (fileId[2] == 'E') && (fileId[1] == ' '))
        {
            fread(&eventCount, sizeof(int), 1, raeFile);
            logMessage(LogLevel::warn, std::format("Events loaded: {}\n", eventCount));
            fread(events, sizeof(AutomationEvent), eventCount, raeFile);
        }

        fclose(raeFile);
        */

        // Load events file (text)
        //unsigned char *buffer = LoadFileText(fileName);
        FILE *raeFile = fopen(fileName, "rt");

        if (raeFile != nullptr)
        {
            u32 counter = 0;
            char buffer[256] = { 0 };
            char eventDesc[64] = { 0 };

            char *result = fgets(buffer, 256, raeFile);
            if (result != buffer) logMessage(LogLevel::warn, std::format("AUTOMATION: [{}] Issue reading line to buffer", fileName));

            while (!feof(raeFile))
            {
                switch (buffer[0])
                {
                    case 'c': sscanf(buffer, "c %i", &list.count); break;
                    case 'e':
                    {
                        if (counter < list.capacity)
                        {
                            sscanf(buffer, "e %d %d %d %d %d %d %63[^\n]s", &list.events[counter].frame, &list.events[counter].type,
                                   &list.events[counter].params[0], &list.events[counter].params[1], &list.events[counter].params[2], &list.events[counter].params[3], eventDesc);

                            counter++;
                        }
                        else logMessage(LogLevel::warn, std::format("AUTOMATION: Event goes beyond automated list capacity (MAX: {}): {}", list.capacity, buffer));
                    } break;
                    default: break;
                }

                result = fgets(buffer, 256, raeFile);
                if (result != buffer) logMessage(LogLevel::warn, std::format("AUTOMATION: [{}] Issue reading line to buffer", fileName));
            }

            if (counter != list.count)
            {
                logMessage(LogLevel::warn, std::format("AUTOMATION: Events read from file [{}] do not mach event count specified [{}]", counter, list.count));
                list.count = counter;
            }

            fclose(raeFile);

            logMessage(LogLevel::info, "AUTOMATION: Events file loaded successfully");
        }

        logMessage(LogLevel::info, std::format("AUTOMATION: Events loaded from file: {}", list.count));
    }
#endif
    return list;
}

// Unload automation events list from file
void UnloadAutomationEventList(AutomationEventList list)
{
#if SUPPORT_AUTOMATION_EVENTS
    free(list.events);
#endif
}

// Export automation events list as text file
bool ExportAutomationEventList(const AutomationEventList list, std::string_view fileNameView)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();

    auto result = false;

#if SUPPORT_AUTOMATION_EVENTS
    // Export events as binary file
    // NOTE: Code not used, only for reference if required in the future
    /*
    if (list.count > 0)
    {
        int binarySize = 4 + sizeof(int) + sizeof(AutomationEvent)*list.count;
        unsigned char *binBuffer = (unsigned char *)calloc(binarySize, 1);
        int offset = 0;
        memcpy(binBuffer + offset, "rAE ", 4);
        offset += 4;
        memcpy(binBuffer + offset, &list.count, sizeof(int));
        offset += sizeof(int);
        memcpy(binBuffer + offset, list.events, sizeof(AutomationEvent)*list.count);
        offset += sizeof(AutomationEvent)*list.count;

        result = SaveFileData(TextFormat("%s.rae",fileName), binBuffer, binarySize);
        free(binBuffer);
    }
    */

    // Export events as text
    // NOTE: Save to memory buffer and SaveFileText()
    auto txtData = static_cast<char *>(calloc(256*list.count + 2048, sizeof(char))); // 256 characters per line plus some header

    auto byteCount = 0;
    byteCount += sprintf(txtData + byteCount, "#\n");
    byteCount += sprintf(txtData + byteCount, "# Automation events exporter v1.0 - raylib automation events list\n");
    byteCount += sprintf(txtData + byteCount, "#\n");
    byteCount += sprintf(txtData + byteCount, "#    c <events_count>\n");
    byteCount += sprintf(txtData + byteCount, "#    e <frame> <event_type> <param0> <param1> <param2> <param3> // <event_type_name>\n");
    byteCount += sprintf(txtData + byteCount, "#\n");
    byteCount += sprintf(txtData + byteCount, "# more info and bugs-report:  github.com/raysan5/raylib\n");
    byteCount += sprintf(txtData + byteCount, "# feedback and support:       ray[at]raylib.com\n");
    byteCount += sprintf(txtData + byteCount, "#\n");
    byteCount += sprintf(txtData + byteCount, "# Copyright (c) 2023-2026 Ramon Santamaria (@raysan5)\n");
    byteCount += sprintf(txtData + byteCount, "#\n\n");

    // Add events data
    byteCount += sprintf(txtData + byteCount, "c %i\n", list.count);
    for (u32 i = 0; i < list.count; i++)
    {
        if ((list.events[i].type < 0) || (list.events[i].type >= MAX_AUTOMATION_EVENT_TYPES)) list.events[i].type = 0;
        
        byteCount += snprintf(txtData + byteCount, 256, "e %i %i %i %i %i %i // Event: %s\n", list.events[i].frame, list.events[i].type,
            list.events[i].params[0], list.events[i].params[1], list.events[i].params[2], list.events[i].params[3], autoEventTypeName[list.events[i].type]);
    }

    // NOTE: Text data size exported is determined by '\0' (NULL) character
    result = SaveFileText(fileName, txtData);

    free(txtData);
#endif

    return result;
}

// Setup automation event list to record to
void SetAutomationEventList(AutomationEventList *list)
{
#if SUPPORT_AUTOMATION_EVENTS
    currentEventList = list;
#endif
}

// Set automation event internal base frame to start recording
void SetAutomationEventBaseFrame(const i32 frame)
{
    CORE.Time.frameCounter = frame;
}

// Start recording automation events (AutomationEventList must be set)
void StartAutomationEventRecording()
{
#if SUPPORT_AUTOMATION_EVENTS
    automationEventRecording = true;
#endif
}

// Stop recording automation events
void StopAutomationEventRecording()
{
#if SUPPORT_AUTOMATION_EVENTS
    automationEventRecording = false;
#endif
}

// Play a recorded automation event
void PlayAutomationEvent(const AutomationEvent event)
{
#if SUPPORT_AUTOMATION_EVENTS
    // WARNING: When should event be played? After/before/replace PollInputEvents()? -> Up to the user!

    if (!automationEventRecording)
    {
        switch (event.type)
        {
            // Input event
            case INPUT_KEY_UP: CORE.Input.Keyboard.currentKeyState[event.params[0]] = false; break;             // param[0]: key
            case INPUT_KEY_DOWN: {                                                                              // param[0]: key
                CORE.Input.Keyboard.currentKeyState[event.params[0]] = true;

                if (CORE.Input.Keyboard.previousKeyState[event.params[0]] == false)
                {
                    if (CORE.Input.Keyboard.keyPressedQueueCount < MAX_KEY_PRESSED_QUEUE)
                    {
                        // Add character to the queue
                        CORE.Input.Keyboard.keyPressedQueue[CORE.Input.Keyboard.keyPressedQueueCount] = event.params[0];
                        CORE.Input.Keyboard.keyPressedQueueCount++;
                    }
                }
            } break;
            case INPUT_MOUSE_BUTTON_UP: CORE.Input.Mouse.currentButtonState[event.params[0]] = false; break;    // param[0]: key
            case INPUT_MOUSE_BUTTON_DOWN: CORE.Input.Mouse.currentButtonState[event.params[0]] = true; break;   // param[0]: key
            case INPUT_MOUSE_POSITION:      // param[0]: x, param[1]: y
            {
                CORE.Input.Mouse.currentPosition.x = static_cast<f32>(event.params[0]);
                CORE.Input.Mouse.currentPosition.y = static_cast<f32>(event.params[1]);
            } break;
            case INPUT_MOUSE_WHEEL_MOTION:  // param[0]: x delta, param[1]: y delta
            {
                CORE.Input.Mouse.currentWheelMove.x = static_cast<f32>(event.params[0]);
                CORE.Input.Mouse.currentWheelMove.y = static_cast<f32>(event.params[1]);
            } break;
            case INPUT_TOUCH_UP: CORE.Input.Touch.currentTouchState[event.params[0]] = false; break;            // param[0]: id
            case INPUT_TOUCH_DOWN: CORE.Input.Touch.currentTouchState[event.params[0]] = true; break;           // param[0]: id
            case INPUT_TOUCH_POSITION:      // param[0]: id, param[1]: x, param[2]: y
            {
                CORE.Input.Touch.position[event.params[0]].x = static_cast<f32>(event.params[1]);
                CORE.Input.Touch.position[event.params[0]].y = static_cast<f32>(event.params[2]);
            } break;
            case INPUT_GAMEPAD_CONNECT: CORE.Input.Gamepad.ready[event.params[0]] = true; break;                                     // param[0]: gamepad
            case INPUT_GAMEPAD_DISCONNECT: CORE.Input.Gamepad.ready[event.params[0]] = false; break;                                 // param[0]: gamepad
            case INPUT_GAMEPAD_BUTTON_UP: CORE.Input.Gamepad.currentButtonState[event.params[0]][event.params[1]] = false; break;    // param[0]: gamepad, param[1]: button
            case INPUT_GAMEPAD_BUTTON_DOWN: CORE.Input.Gamepad.currentButtonState[event.params[0]][event.params[1]] = true; break;   // param[0]: gamepad, param[1]: button
            case INPUT_GAMEPAD_AXIS_MOTION: // param[0]: gamepad, param[1]: axis, param[2]: delta
            {
                CORE.Input.Gamepad.axisState[event.params[0]][event.params[1]] = (static_cast<f32>(event.params[2])/32768.0f);
            } break;
    #if SUPPORT_GESTURES_SYSTEM
            case INPUT_GESTURE: GESTURES.current = event.params[0]; break;     // param[0]: gesture (enum Gesture) -> rgestures.h: GESTURES.current
    #endif
            // Window event
            case WINDOW_CLOSE: CORE.Window.shouldClose = true; break;
            case WINDOW_MAXIMIZE: MaximizeWindow(); break;
            case WINDOW_MINIMIZE: MinimizeWindow(); break;
            case WINDOW_RESIZE: SetWindowSize(event.params[0], event.params[1]); break;
            // Custom event
    #if SUPPORT_SCREEN_CAPTURE
            case ACTION_TAKE_SCREENSHOT:
            {
                TakeScreenshot(TextFormat("screenshot%03i.png", screenshotCounter));
                screenshotCounter++;
            } break;
    #endif
            case ACTION_SETTARGETFPS: SetTargetFPS(event.params[0]); break;
            default: break;
        }

        logMessage(LogLevel::info, std::format("AUTOMATION PLAY: Frame: {} | Event type: {} | Event parameters: {}, {}, {}", event.frame, event.type, event.params[0], event.params[1], event.params[2]));
    }
#endif
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Input Handling: Keyboard
//----------------------------------------------------------------------------------

// Check if key has been pressed once
bool IsKeyPressed(const i32 key)
{
    auto pressed = false;

    if ((key > 0) && (key < MAX_KEYBOARD_KEYS))
    {
        if ((CORE.Input.Keyboard.previousKeyState[key] == 0) && (CORE.Input.Keyboard.currentKeyState[key] == 1)) pressed = true;
    }

    return pressed;
}

// Check if key has been pressed again
bool IsKeyPressedRepeat(const i32 key)
{
    auto repeat = false;

    if ((key > 0) && (key < MAX_KEYBOARD_KEYS))
    {
        if (CORE.Input.Keyboard.keyRepeatInFrame[key] == 1) repeat = true;
    }

    return repeat;
}

// Check if key is being pressed (key held down)
bool IsKeyDown(const i32 key)
{
    auto down = false;

    if ((key > 0) && (key < MAX_KEYBOARD_KEYS))
    {
        if (CORE.Input.Keyboard.currentKeyState[key] == 1) down = true;
    }

    return down;
}

// Check if key has been released once
bool IsKeyReleased(const i32 key)
{
    auto released = false;

    if ((key > 0) && (key < MAX_KEYBOARD_KEYS))
    {
        if ((CORE.Input.Keyboard.previousKeyState[key] == 1) && (CORE.Input.Keyboard.currentKeyState[key] == 0)) released = true;
    }

    return released;
}

// Check if key is NOT being pressed (key not held down)
bool IsKeyUp(const i32 key)
{
    auto up = false;

    if ((key > 0) && (key < MAX_KEYBOARD_KEYS))
    {
        if (CORE.Input.Keyboard.currentKeyState[key] == 0) up = true;
    }

    return up;
}

// Get the last key pressed
i32 GetKeyPressed()
{
    auto value = 0;

    if (CORE.Input.Keyboard.keyPressedQueueCount > 0)
    {
        // Get character from the queue head
        value = CORE.Input.Keyboard.keyPressedQueue[0];

        // Shift elements 1 step toward the head
        for (i32 i = 0; i < (CORE.Input.Keyboard.keyPressedQueueCount - 1); i++)
            CORE.Input.Keyboard.keyPressedQueue[i] = CORE.Input.Keyboard.keyPressedQueue[i + 1];

        // Reset last character in the queue
        CORE.Input.Keyboard.keyPressedQueue[CORE.Input.Keyboard.keyPressedQueueCount - 1] = 0;
        CORE.Input.Keyboard.keyPressedQueueCount--;
    }

    return value;
}

// Get the last char pressed
i32 GetCharPressed()
{
    auto value = 0;

    if (CORE.Input.Keyboard.charPressedQueueCount > 0)
    {
        // Get character from the queue head
        value = CORE.Input.Keyboard.charPressedQueue[0];

        // Shift elements 1 step toward the head
        for (i32 i = 0; i < (CORE.Input.Keyboard.charPressedQueueCount - 1); i++)
            CORE.Input.Keyboard.charPressedQueue[i] = CORE.Input.Keyboard.charPressedQueue[i + 1];

        // Reset last character in the queue
        CORE.Input.Keyboard.charPressedQueue[CORE.Input.Keyboard.charPressedQueueCount - 1] = 0;
        CORE.Input.Keyboard.charPressedQueueCount--;
    }

    return value;
}

// Set a custom key to exit program
// NOTE: default exitKey is set to ESCAPE
void SetExitKey(const i32 key)
{
    CORE.Input.Keyboard.exitKey = key;
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Input Handling: Gamepad
//----------------------------------------------------------------------------------

// NOTE: Functions with a platform-specific implementation on rcore_<platform>.c
//int SetGamepadMappings(const char *mappings)

// Check if gamepad is available
bool IsGamepadAvailable(const i32 gamepad)
{
    auto result = false;

    if ((gamepad >= 0) && (gamepad < MAX_GAMEPADS) && CORE.Input.Gamepad.ready[gamepad]) result = true;

    return result;
}

// Get gamepad internal name id
std::string GetGamepadName(const i32 gamepad)
{
    const char *name = nullptr;

    if ((gamepad >= 0) && (gamepad < MAX_GAMEPADS)) name = CORE.Input.Gamepad.name[gamepad];

    return GetStringOrEmpty(name);
}

// Check if gamepad button has been pressed once
bool IsGamepadButtonPressed(const i32 gamepad, const i32 button)
{
    auto pressed = false;

    if ((gamepad >= 0) && (gamepad < MAX_GAMEPADS) && CORE.Input.Gamepad.ready[gamepad] && (button >= 0) && (button < MAX_GAMEPAD_BUTTONS))
    {
        if ((CORE.Input.Gamepad.previousButtonState[gamepad][button] == 0) && (CORE.Input.Gamepad.currentButtonState[gamepad][button] == 1)) pressed = true;
    }

    return pressed;
}

// Check if gamepad button is being pressed
bool IsGamepadButtonDown(const i32 gamepad, const i32 button)
{
    auto down = false;

    if ((gamepad >= 0) && (gamepad < MAX_GAMEPADS) && CORE.Input.Gamepad.ready[gamepad] && (button >= 0) && (button < MAX_GAMEPAD_BUTTONS))
    {
        if (CORE.Input.Gamepad.currentButtonState[gamepad][button] == 1) down = true;
    }

    return down;
}

// Check if gamepad button has NOT been pressed once
bool IsGamepadButtonReleased(const i32 gamepad, const i32 button)
{
    auto released = false;

    if ((gamepad >= 0) && (gamepad < MAX_GAMEPADS) && CORE.Input.Gamepad.ready[gamepad] && (button >= 0) && (button < MAX_GAMEPAD_BUTTONS))
    {
        if ((CORE.Input.Gamepad.previousButtonState[gamepad][button] == 1) && (CORE.Input.Gamepad.currentButtonState[gamepad][button] == 0)) released = true;
    }

    return released;
}

// Check if gamepad button is NOT being pressed
bool IsGamepadButtonUp(const i32 gamepad, const i32 button)
{
    auto up = false;

    if ((gamepad >= 0) && (gamepad < MAX_GAMEPADS) && CORE.Input.Gamepad.ready[gamepad] && (button >= 0) && (button < MAX_GAMEPAD_BUTTONS))
    {
        if (CORE.Input.Gamepad.currentButtonState[gamepad][button] == 0) up = true;
    }

    return up;
}

// Get the last gamepad button pressed
// NOTE: Returns last gamepad button down, down->up change not considered
i32 GetGamepadButtonPressed()
{
    return CORE.Input.Gamepad.lastButtonPressed;
}

// Get gamepad axis count
i32 GetGamepadAxisCount(const i32 gamepad)
{
    auto result = 0;

    if ((gamepad >= 0) && (gamepad < MAX_GAMEPADS)) result = CORE.Input.Gamepad.axisCount[gamepad];

    return result;
}

// Get axis movement vector for a gamepad
f32 GetGamepadAxisMovement(const i32 gamepad, const i32 axis)
{
    f32 value = ((axis == GAMEPAD_AXIS_LEFT_TRIGGER) || (axis == GAMEPAD_AXIS_RIGHT_TRIGGER))? -1.0f : 0.0f;

    if ((gamepad >= 0) && (gamepad < MAX_GAMEPADS) && CORE.Input.Gamepad.ready[gamepad] && (axis >= 0) && (axis < MAX_GAMEPAD_AXES))
    {
        const f32 movement = (value < 0.0f)? CORE.Input.Gamepad.axisState[gamepad][axis] : fabsf(CORE.Input.Gamepad.axisState[gamepad][axis]);

        if (movement > value) value = CORE.Input.Gamepad.axisState[gamepad][axis];
    }

    return value;
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Input Handling: Mouse
//----------------------------------------------------------------------------------

// NOTE: Functions with a platform-specific implementation on rcore_<platform>.c
//void SetMousePosition(int x, int y)
//void SetMouseCursor(int cursor)

// Check if mouse button has been pressed once
bool IsMouseButtonPressed(const i32 button)
{
    auto pressed = false;

    if ((button >= 0) && (button <= MOUSE_BUTTON_BACK))
    {
        if ((CORE.Input.Mouse.currentButtonState[button] == 1) && (CORE.Input.Mouse.previousButtonState[button] == 0)) pressed = true;

        // Map touches to mouse buttons checking
        if ((CORE.Input.Touch.currentTouchState[button] == 1) && (CORE.Input.Touch.previousTouchState[button] == 0)) pressed = true;
    }

    return pressed;
}

// Check if mouse button is being pressed
bool IsMouseButtonDown(const i32 button)
{
    auto down = false;

    if ((button >= 0) && (button <= MOUSE_BUTTON_BACK))
    {
        if (CORE.Input.Mouse.currentButtonState[button] == 1) down = true;

        // NOTE: Touches are considered like mouse buttons
        if (CORE.Input.Touch.currentTouchState[button] == 1) down = true;
    }

    return down;
}

// Check if mouse button has been released once
bool IsMouseButtonReleased(const i32 button)
{
    auto released = false;

    if ((button >= 0) && (button <= MOUSE_BUTTON_BACK))
    {
        if ((CORE.Input.Mouse.currentButtonState[button] == 0) && (CORE.Input.Mouse.previousButtonState[button] == 1)) released = true;

        // Map touches to mouse buttons checking
        if ((CORE.Input.Touch.currentTouchState[button] == 0) && (CORE.Input.Touch.previousTouchState[button] == 1)) released = true;
    }

    return released;
}

// Check if mouse button is NOT being pressed
bool IsMouseButtonUp(const i32 button)
{
    auto up = false;

    if ((button >= 0) && (button <= MOUSE_BUTTON_BACK))
    {
        if (CORE.Input.Mouse.currentButtonState[button] == 0) up = true;

        // NOTE: Touches are considered like mouse buttons
        if (CORE.Input.Touch.currentTouchState[button] == 0) up = true;
    }

    return up;
}

// Get mouse position X
i32 GetMouseX()
{
    const auto mouseX = static_cast<i32>((CORE.Input.Mouse.currentPosition.x + CORE.Input.Mouse.offset.x)*CORE.Input.Mouse.scale.x);

    return mouseX;
}

// Get mouse position Y
i32 GetMouseY()
{
    const auto mouseY = static_cast<i32>((CORE.Input.Mouse.currentPosition.y + CORE.Input.Mouse.offset.y)*CORE.Input.Mouse.scale.y);

    return mouseY;
}

// Get mouse position XY
vec2 GetMousePosition()
{
    vec2 position{};

    position.x = (CORE.Input.Mouse.currentPosition.x + CORE.Input.Mouse.offset.x)*CORE.Input.Mouse.scale.x;
    position.y = (CORE.Input.Mouse.currentPosition.y + CORE.Input.Mouse.offset.y)*CORE.Input.Mouse.scale.y;

    return position;
}

// Get mouse delta between frames
vec2 GetMouseDelta()
{
    vec2 delta{};

    delta.x = (CORE.Input.Mouse.currentPosition.x - CORE.Input.Mouse.previousPosition.x)*CORE.Input.Mouse.scale.x;
    delta.y = (CORE.Input.Mouse.currentPosition.y - CORE.Input.Mouse.previousPosition.y)*CORE.Input.Mouse.scale.y;

    return delta;
}

// Set mouse offset
// NOTE: Useful when rendering to different size targets
void SetMouseOffset(const i32 offsetX, const i32 offsetY)
{
    CORE.Input.Mouse.offset = vec2{ static_cast<f32>(offsetX), static_cast<f32>(offsetY) };
}

// Set mouse scaling
// NOTE: Useful when rendering to different size targets
void SetMouseScale(const f32 scaleX, const f32 scaleY)
{
    CORE.Input.Mouse.scale = vec2{ scaleX, scaleY };
}

// Get mouse wheel movement Y
f32 GetMouseWheelMove()
{
    auto result = 0.0f;

    if (fabsf(CORE.Input.Mouse.currentWheelMove.x) > fabsf(CORE.Input.Mouse.currentWheelMove.y)) result = CORE.Input.Mouse.currentWheelMove.x;
    else result = CORE.Input.Mouse.currentWheelMove.y;

    return result;
}

// Get mouse wheel movement X/Y as a vector
vec2 GetMouseWheelMoveV()
{
    vec2 result{};

    result = CORE.Input.Mouse.currentWheelMove;

    return result;
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Input Handling: Touch
//----------------------------------------------------------------------------------

// Get touch position X for touch point 0 (relative to screen size)
i32 GetTouchX()
{
    const auto touchX = static_cast<i32>(CORE.Input.Touch.position[0].x);
    return touchX;
}

// Get touch position Y for touch point 0 (relative to screen size)
i32 GetTouchY()
{
    const auto touchY = static_cast<i32>(CORE.Input.Touch.position[0].y);
    return touchY;
}

// Get touch position XY for a touch point index (relative to screen size)
vec2 GetTouchPosition(const i32 index)
{
    auto position = vec2{ -1.0f, -1.0f };

    if ((index >= 0) && (index < MAX_TOUCH_POINTS)) position = CORE.Input.Touch.position[index];
    else logMessage(LogLevel::warn, std::format("INPUT: Required touch point out of range (Max touch points: {})", MAX_TOUCH_POINTS));

    return position;
}

// Get touch point identifier for provided index
i32 GetTouchPointId(const i32 index)
{
    auto id = -1;

    if ((index >= 0) && (index < MAX_TOUCH_POINTS)) id = CORE.Input.Touch.pointId[index];

    return id;
}

// Get number of touch points
i32 GetTouchPointCount()
{
    return CORE.Input.Touch.pointCount;
}

//----------------------------------------------------------------------------------
// Module Internal Functions Definition
//----------------------------------------------------------------------------------

// NOTE: Functions with a platform-specific implementation on rcore_<platform>.c
//int InitPlatform(void)
//void ClosePlatform(void)

// Initialize hi-resolution timer
void InitTimer()
{
    // Setting a higher resolution can improve the accuracy of time-out intervals in wait functions
    // However, it can also reduce overall system performance, because the thread scheduler switches tasks more often
    // High resolutions can also prevent the CPU power management system from entering power-saving modes
    // Setting a higher resolution does not improve the accuracy of the high-resolution performance counter
#if defined(_WIN32) && SUPPORT_WINMM_HIGHRES_TIMER && !SUPPORT_BUSY_WAIT_LOOP && !defined(PLATFORM_DESKTOP_SDL)
    timeBeginPeriod(1); // Setup high-resolution timer to 1ms (granularity of 1-2 ms)
#endif

#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__EMSCRIPTEN__)
    timespec now{};

    if (clock_gettime(CLOCK_MONOTONIC, &now) == 0) // Success
    {
        CORE.Time.base = static_cast<unsigned long long>(now.tv_sec)*1000000000LLU + static_cast<unsigned long long>(now.tv_nsec);
    }
    else logMessage(LogLevel::warn, "TIMER: Hi-resolution timer not available");
#endif

    CORE.Time.previous = GetTime(); // Get time as double
}

// Set viewport for a provided width and height
void SetupViewport(i32 width, i32 height)
{
    CORE.Window.render.width = width;
    CORE.Window.render.height = height;

    // Set viewport width and height
    rlViewport(CORE.Window.renderOffset.x/2, CORE.Window.renderOffset.y/2, CORE.Window.render.width, CORE.Window.render.height);

    rlMatrixMode(RL_PROJECTION);        // Switch to projection matrix
    rlLoadIdentity();                   // Reset current matrix (projection)

    // Set orthographic projection to current framebuffer size
    // NOTE: Configured top-left corner as (0, 0)
    rlOrtho(0.0f, static_cast<f32>(CORE.Window.render.width), static_cast<f32>(CORE.Window.render.height), 0.0f, 0.0f, 1.0f);

    rlMatrixMode(RL_MODELVIEW);         // Switch back to modelview matrix
    rlLoadIdentity();                   // Reset current matrix (modelview)
}

// Scan all files and directories in a base path
// WARNING: files.paths[] must be previously allocated and
// contain enough space to store all required paths
static void ScanDirectoryFiles(const char *basePath, FilePathList *files, const char *filter, const bool scanSubdirs)
{
    // WARNING: Path can not be static or it will be reused between recursive function calls!
    char path[MAX_FILEPATH_LENGTH] = { 0 };
    memset(path, 0, MAX_FILEPATH_LENGTH);

    struct dirent *dp = nullptr;
    DIR *dir = opendir(basePath);

    if (dir != nullptr)
    {
        while ((dp = readdir(dir)) != nullptr)
        {
            if ((strcmp(dp->d_name, ".") != 0) && (strcmp(dp->d_name, "..") != 0))
            {
                // Construct new path from our base path
            #if defined(_WIN32)
                i32 pathLength = snprintf(path, MAX_FILEPATH_LENGTH, "%s\\%s", basePath, dp->d_name);
            #else
                const i32 pathLength = snprintf(path, MAX_FILEPATH_LENGTH, "%s/%s", basePath, dp->d_name);
            #endif

                if ((pathLength < 0) || (pathLength >= MAX_FILEPATH_LENGTH))
                {
                    logMessage(LogLevel::warn, std::format("FILEIO: Path longer than {} characters ({}...)", MAX_FILEPATH_LENGTH, basePath));
                }
                else if (IsPathFile(path))
                {
                    if ((filter == nullptr) || (filter[0] == '\0') || (strstr(filter, FILE_FILTER_TAG_ALL) != nullptr) ||
                        (strstr(filter, FILE_FILTER_TAG_FILE_ONLY) != nullptr) || IsFileExtension(path, filter))
                    {
                        files->paths.emplace_back(path, pathLength);
                    }
                }
                else
                {
                    if ((filter != nullptr) && (filter[0] != '\0') && ((strstr(filter, FILE_FILTER_TAG_DIR_ONLY) != nullptr) || (strstr(filter, FILE_FILTER_TAG_ALL) != nullptr)))
                    {
                        files->paths.emplace_back(path, pathLength);
                    }

                    if (scanSubdirs) ScanDirectoryFiles(path, files, filter, scanSubdirs);
                }
            }
        }

        closedir(dir);
    }
    else logMessage(LogLevel::warn, std::format("FILEIO: Directory cannot be opened ({})", basePath));  // Maybe it's a file...
}

#if SUPPORT_AUTOMATION_EVENTS
// Automation event recording
// Checking events in current frame and save them into currentEventList
// NOTE: Recording is by default done at EndDrawing(), before PollInputEvents()
static void RecordAutomationEvent()
{
    if (currentEventList->count == currentEventList->capacity) return;

    // Keyboard input events recording
    //-------------------------------------------------------------------------------------
    for (i32 key = 0; key < MAX_KEYBOARD_KEYS; key++)
    {
        // Event type: INPUT_KEY_UP (only saved once)
        if (CORE.Input.Keyboard.previousKeyState[key] && !CORE.Input.Keyboard.currentKeyState[key])
        {
            currentEventList->events[currentEventList->count].frame = CORE.Time.frameCounter;
            currentEventList->events[currentEventList->count].type = INPUT_KEY_UP;
            currentEventList->events[currentEventList->count].params[0] = key;
            currentEventList->events[currentEventList->count].params[1] = 0;
            currentEventList->events[currentEventList->count].params[2] = 0;

            logMessage(LogLevel::info, std::format("AUTOMATION: Frame: {} | Event type: INPUT_KEY_UP | Event parameters: {}, {}, {}", currentEventList->events[currentEventList->count].frame, currentEventList->events[currentEventList->count].params[0], currentEventList->events[currentEventList->count].params[1], currentEventList->events[currentEventList->count].params[2]));
            currentEventList->count++;
        }

        if (currentEventList->count == currentEventList->capacity) return;    // Security check

        // Event type: INPUT_KEY_DOWN
        if (CORE.Input.Keyboard.currentKeyState[key])
        {
            currentEventList->events[currentEventList->count].frame = CORE.Time.frameCounter;
            currentEventList->events[currentEventList->count].type = INPUT_KEY_DOWN;
            currentEventList->events[currentEventList->count].params[0] = key;
            currentEventList->events[currentEventList->count].params[1] = 0;
            currentEventList->events[currentEventList->count].params[2] = 0;

            logMessage(LogLevel::info, std::format("AUTOMATION: Frame: {} | Event type: INPUT_KEY_DOWN | Event parameters: {}, {}, {}", currentEventList->events[currentEventList->count].frame, currentEventList->events[currentEventList->count].params[0], currentEventList->events[currentEventList->count].params[1], currentEventList->events[currentEventList->count].params[2]));
            currentEventList->count++;
        }

        if (currentEventList->count == currentEventList->capacity) return;    // Security check
    }
    //-------------------------------------------------------------------------------------

    // Mouse input currentEventList->events recording
    //-------------------------------------------------------------------------------------
    for (i32 button = 0; button < MAX_MOUSE_BUTTONS; button++)
    {
        // Event type: INPUT_MOUSE_BUTTON_UP
        if (CORE.Input.Mouse.previousButtonState[button] && !CORE.Input.Mouse.currentButtonState[button])
        {
            currentEventList->events[currentEventList->count].frame = CORE.Time.frameCounter;
            currentEventList->events[currentEventList->count].type = INPUT_MOUSE_BUTTON_UP;
            currentEventList->events[currentEventList->count].params[0] = button;
            currentEventList->events[currentEventList->count].params[1] = 0;
            currentEventList->events[currentEventList->count].params[2] = 0;

            logMessage(LogLevel::info, std::format("AUTOMATION: Frame: {} | Event type: INPUT_MOUSE_BUTTON_UP | Event parameters: {}, {}, {}", currentEventList->events[currentEventList->count].frame, currentEventList->events[currentEventList->count].params[0], currentEventList->events[currentEventList->count].params[1], currentEventList->events[currentEventList->count].params[2]));
            currentEventList->count++;
        }

        if (currentEventList->count == currentEventList->capacity) return;    // Security check

        // Event type: INPUT_MOUSE_BUTTON_DOWN
        if (CORE.Input.Mouse.currentButtonState[button])
        {
            currentEventList->events[currentEventList->count].frame = CORE.Time.frameCounter;
            currentEventList->events[currentEventList->count].type = INPUT_MOUSE_BUTTON_DOWN;
            currentEventList->events[currentEventList->count].params[0] = button;
            currentEventList->events[currentEventList->count].params[1] = 0;
            currentEventList->events[currentEventList->count].params[2] = 0;

            logMessage(LogLevel::info, std::format("AUTOMATION: Frame: {} | Event type: INPUT_MOUSE_BUTTON_DOWN | Event parameters: {}, {}, {}", currentEventList->events[currentEventList->count].frame, currentEventList->events[currentEventList->count].params[0], currentEventList->events[currentEventList->count].params[1], currentEventList->events[currentEventList->count].params[2]));
            currentEventList->count++;
        }

        if (currentEventList->count == currentEventList->capacity) return;    // Security check
    }

    // Event type: INPUT_MOUSE_POSITION (only saved if changed)
    if ((static_cast<i32>(CORE.Input.Mouse.currentPosition.x) != static_cast<i32>(CORE.Input.Mouse.previousPosition.x)) ||
        (static_cast<i32>(CORE.Input.Mouse.currentPosition.y) != static_cast<i32>(CORE.Input.Mouse.previousPosition.y)))
    {
        currentEventList->events[currentEventList->count].frame = CORE.Time.frameCounter;
        currentEventList->events[currentEventList->count].type = INPUT_MOUSE_POSITION;
        currentEventList->events[currentEventList->count].params[0] = static_cast<i32>(CORE.Input.Mouse.currentPosition.x);
        currentEventList->events[currentEventList->count].params[1] = static_cast<i32>(CORE.Input.Mouse.currentPosition.y);
        currentEventList->events[currentEventList->count].params[2] = 0;

        logMessage(LogLevel::info, std::format("AUTOMATION: Frame: {} | Event type: INPUT_MOUSE_POSITION | Event parameters: {}, {}, {}", currentEventList->events[currentEventList->count].frame, currentEventList->events[currentEventList->count].params[0], currentEventList->events[currentEventList->count].params[1], currentEventList->events[currentEventList->count].params[2]));
        currentEventList->count++;

        if (currentEventList->count == currentEventList->capacity) return;    // Security check
    }

    // Event type: INPUT_MOUSE_WHEEL_MOTION
    if ((static_cast<i32>(CORE.Input.Mouse.currentWheelMove.x) != static_cast<i32>(CORE.Input.Mouse.previousWheelMove.x)) ||
        (static_cast<i32>(CORE.Input.Mouse.currentWheelMove.y) != static_cast<i32>(CORE.Input.Mouse.previousWheelMove.y)))
    {
        currentEventList->events[currentEventList->count].frame = CORE.Time.frameCounter;
        currentEventList->events[currentEventList->count].type = INPUT_MOUSE_WHEEL_MOTION;
        currentEventList->events[currentEventList->count].params[0] = static_cast<i32>(CORE.Input.Mouse.currentWheelMove.x);
        currentEventList->events[currentEventList->count].params[1] = static_cast<i32>(CORE.Input.Mouse.currentWheelMove.y);
        currentEventList->events[currentEventList->count].params[2] = 0;

        logMessage(LogLevel::info, std::format("AUTOMATION: Frame: {} | Event type: INPUT_MOUSE_WHEEL_MOTION | Event parameters: {}, {}, {}", currentEventList->events[currentEventList->count].frame, currentEventList->events[currentEventList->count].params[0], currentEventList->events[currentEventList->count].params[1], currentEventList->events[currentEventList->count].params[2]));
        currentEventList->count++;

        if (currentEventList->count == currentEventList->capacity) return;    // Security check
    }
    //-------------------------------------------------------------------------------------

    // Touch input currentEventList->events recording
    //-------------------------------------------------------------------------------------
    for (i32 id = 0; id < MAX_TOUCH_POINTS; id++)
    {
        // Event type: INPUT_TOUCH_UP
        if (CORE.Input.Touch.previousTouchState[id] && !CORE.Input.Touch.currentTouchState[id])
        {
            currentEventList->events[currentEventList->count].frame = CORE.Time.frameCounter;
            currentEventList->events[currentEventList->count].type = INPUT_TOUCH_UP;
            currentEventList->events[currentEventList->count].params[0] = id;
            currentEventList->events[currentEventList->count].params[1] = 0;
            currentEventList->events[currentEventList->count].params[2] = 0;

            logMessage(LogLevel::info, std::format("AUTOMATION: Frame: {} | Event type: INPUT_TOUCH_UP | Event parameters: {}, {}, {}", currentEventList->events[currentEventList->count].frame, currentEventList->events[currentEventList->count].params[0], currentEventList->events[currentEventList->count].params[1], currentEventList->events[currentEventList->count].params[2]));
            currentEventList->count++;
        }

        if (currentEventList->count == currentEventList->capacity) return;    // Security check

        // Event type: INPUT_TOUCH_DOWN
        if (CORE.Input.Touch.currentTouchState[id])
        {
            currentEventList->events[currentEventList->count].frame = CORE.Time.frameCounter;
            currentEventList->events[currentEventList->count].type = INPUT_TOUCH_DOWN;
            currentEventList->events[currentEventList->count].params[0] = id;
            currentEventList->events[currentEventList->count].params[1] = 0;
            currentEventList->events[currentEventList->count].params[2] = 0;

            logMessage(LogLevel::info, std::format("AUTOMATION: Frame: {} | Event type: INPUT_TOUCH_DOWN | Event parameters: {}, {}, {}", currentEventList->events[currentEventList->count].frame, currentEventList->events[currentEventList->count].params[0], currentEventList->events[currentEventList->count].params[1], currentEventList->events[currentEventList->count].params[2]));
            currentEventList->count++;
        }

        if (currentEventList->count == currentEventList->capacity) return;    // Security check

        // Event type: INPUT_TOUCH_POSITION
        if ((static_cast<i32>(CORE.Input.Touch.position[id].x) != static_cast<i32>(CORE.Input.Touch.previousPosition[id].x)) ||
            (static_cast<i32>(CORE.Input.Touch.position[id].y) != static_cast<i32>(CORE.Input.Touch.previousPosition[id].y)))
        {
            currentEventList->events[currentEventList->count].frame = CORE.Time.frameCounter;
            currentEventList->events[currentEventList->count].type = INPUT_TOUCH_POSITION;
            currentEventList->events[currentEventList->count].params[0] = id;
            currentEventList->events[currentEventList->count].params[1] = static_cast<i32>(CORE.Input.Touch.position[id].x);
            currentEventList->events[currentEventList->count].params[2] = static_cast<i32>(CORE.Input.Touch.position[id].y);

            logMessage(LogLevel::info, std::format("AUTOMATION: Frame: {} | Event type: INPUT_TOUCH_POSITION | Event parameters: {}, {}, {}", currentEventList->events[currentEventList->count].frame, currentEventList->events[currentEventList->count].params[0], currentEventList->events[currentEventList->count].params[1], currentEventList->events[currentEventList->count].params[2]));
            currentEventList->count++;
        }


        if (currentEventList->count == currentEventList->capacity) return;    // Security check
    }
    //-------------------------------------------------------------------------------------

    // Gamepad input currentEventList->events recording
    //-------------------------------------------------------------------------------------
    for (i32 gamepad = 0; gamepad < MAX_GAMEPADS; gamepad++)
    {
        // Event type: INPUT_GAMEPAD_CONNECT
        /*
        if ((CORE.Input.Gamepad.currentState[gamepad] != CORE.Input.Gamepad.previousState[gamepad]) &&
            (CORE.Input.Gamepad.currentState[gamepad])) // Check if changed to ready
        {
            // TODO: Automation event: Save gamepad connect event
        }
        */

        // Event type: INPUT_GAMEPAD_DISCONNECT
        /*
        if ((CORE.Input.Gamepad.currentState[gamepad] != CORE.Input.Gamepad.previousState[gamepad]) &&
            (!CORE.Input.Gamepad.currentState[gamepad])) // Check if changed to not-ready
        {
            // TODO: Automation event: Save gamepad disconnect event
        }
        */

        for (i32 button = 0; button < MAX_GAMEPAD_BUTTONS; button++)
        {
            // Event type: INPUT_GAMEPAD_BUTTON_UP
            if (CORE.Input.Gamepad.previousButtonState[gamepad][button] && !CORE.Input.Gamepad.currentButtonState[gamepad][button])
            {
                currentEventList->events[currentEventList->count].frame = CORE.Time.frameCounter;
                currentEventList->events[currentEventList->count].type = INPUT_GAMEPAD_BUTTON_UP;
                currentEventList->events[currentEventList->count].params[0] = gamepad;
                currentEventList->events[currentEventList->count].params[1] = button;
                currentEventList->events[currentEventList->count].params[2] = 0;

                logMessage(LogLevel::info, std::format("AUTOMATION: Frame: {} | Event type: INPUT_GAMEPAD_BUTTON_UP | Event parameters: {}, {}, {}", currentEventList->events[currentEventList->count].frame, currentEventList->events[currentEventList->count].params[0], currentEventList->events[currentEventList->count].params[1], currentEventList->events[currentEventList->count].params[2]));
                currentEventList->count++;
            }

            if (currentEventList->count == currentEventList->capacity) return;    // Security check

            // Event type: INPUT_GAMEPAD_BUTTON_DOWN
            if (CORE.Input.Gamepad.currentButtonState[gamepad][button])
            {
                currentEventList->events[currentEventList->count].frame = CORE.Time.frameCounter;
                currentEventList->events[currentEventList->count].type = INPUT_GAMEPAD_BUTTON_DOWN;
                currentEventList->events[currentEventList->count].params[0] = gamepad;
                currentEventList->events[currentEventList->count].params[1] = button;
                currentEventList->events[currentEventList->count].params[2] = 0;

                logMessage(LogLevel::info, std::format("AUTOMATION: Frame: {} | Event type: INPUT_GAMEPAD_BUTTON_DOWN | Event parameters: {}, {}, {}", currentEventList->events[currentEventList->count].frame, currentEventList->events[currentEventList->count].params[0], currentEventList->events[currentEventList->count].params[1], currentEventList->events[currentEventList->count].params[2]));
                currentEventList->count++;
            }

            if (currentEventList->count == currentEventList->capacity) return;    // Security check
        }

        for (i32 axis = 0; axis < MAX_GAMEPAD_AXES; axis++)
        {
            // Event type: INPUT_GAMEPAD_AXIS_MOTION
            const f32 defaultMovement = ((axis == GAMEPAD_AXIS_LEFT_TRIGGER) || (axis == GAMEPAD_AXIS_RIGHT_TRIGGER))? -1.0f : 0.0f;
            if (GetGamepadAxisMovement(gamepad, axis) != defaultMovement)
            {
                currentEventList->events[currentEventList->count].frame = CORE.Time.frameCounter;
                currentEventList->events[currentEventList->count].type = INPUT_GAMEPAD_AXIS_MOTION;
                currentEventList->events[currentEventList->count].params[0] = gamepad;
                currentEventList->events[currentEventList->count].params[1] = axis;
                currentEventList->events[currentEventList->count].params[2] = static_cast<i32>(CORE.Input.Gamepad.axisState[gamepad][axis]*32768.0f);

                logMessage(LogLevel::info, std::format("AUTOMATION: Frame: {} | Event type: INPUT_GAMEPAD_AXIS_MOTION | Event parameters: {}, {}, {}", currentEventList->events[currentEventList->count].frame, currentEventList->events[currentEventList->count].params[0], currentEventList->events[currentEventList->count].params[1], currentEventList->events[currentEventList->count].params[2]));
                currentEventList->count++;
            }

            if (currentEventList->count == currentEventList->capacity) return;    // Security check
        }
    }
    //-------------------------------------------------------------------------------------

#if SUPPORT_GESTURES_SYSTEM
    // Gestures input currentEventList->events recording
    //-------------------------------------------------------------------------------------
    if (GESTURES.current != GESTURE_NONE)
    {
        // Event type: INPUT_GESTURE
        currentEventList->events[currentEventList->count].frame = CORE.Time.frameCounter;
        currentEventList->events[currentEventList->count].type = INPUT_GESTURE;
        currentEventList->events[currentEventList->count].params[0] = GESTURES.current;
        currentEventList->events[currentEventList->count].params[1] = 0;
        currentEventList->events[currentEventList->count].params[2] = 0;

        logMessage(LogLevel::info, std::format("AUTOMATION: Frame: {} | Event type: INPUT_GESTURE | Event parameters: {}, {}, {}", currentEventList->events[currentEventList->count].frame, currentEventList->events[currentEventList->count].params[0], currentEventList->events[currentEventList->count].params[1], currentEventList->events[currentEventList->count].params[2]));
        currentEventList->count++;

        if (currentEventList->count == currentEventList->capacity) return;    // Security check
    }
    //-------------------------------------------------------------------------------------
#endif
}
#endif

#if !SUPPORT_MODULE_RTEXT
// Formatting of text with variables to 'embed'
// WARNING: String returned will expire after this function is called MAX_TEXTFORMAT_BUFFERS times
std::string TextFormat(std::string_view textView, ...)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();

#ifndef MAX_TEXTFORMAT_BUFFERS
    #define MAX_TEXTFORMAT_BUFFERS      4        // Maximum number of static buffers for text formatting
#endif
#ifndef MAX_TEXT_BUFFER_LENGTH
    #define MAX_TEXT_BUFFER_LENGTH   1024        // Maximum size of static text buffer
#endif

    // Define an array of buffers, so strings don't expire until MAX_TEXTFORMAT_BUFFERS invocations
    static char buffers[MAX_TEXTFORMAT_BUFFERS][MAX_TEXT_BUFFER_LENGTH] = { 0 };
    static auto index = 0;

    char *currentBuffer = buffers[index];
    memset(currentBuffer, 0, MAX_TEXT_BUFFER_LENGTH);   // Clear buffer before using

    if (text != nullptr)
    {
        va_list args;
        va_start(args, textView);
        i32 requiredByteCount = vsnprintf(currentBuffer, MAX_TEXT_BUFFER_LENGTH, text, args);
        va_end(args);

        // If requiredByteCount is larger than the MAX_TEXT_BUFFER_LENGTH, then overflow occurred
        if (requiredByteCount >= MAX_TEXT_BUFFER_LENGTH)
        {
            // Inserting "..." at the end of the string to mark as truncated
            char *truncBuffer = buffers[index] + MAX_TEXT_BUFFER_LENGTH - 4; // Adding 4 bytes = "...\0"
            snprintf(truncBuffer, 4, "...");
        }

        index += 1; // Move to next buffer for next function call
        if (index >= MAX_TEXTFORMAT_BUFFERS) index = 0;
    }

    return currentBuffer;
}
#endif
