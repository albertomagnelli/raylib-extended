/**********************************************************************************************
*
*   raylib v6.5-dev - A simple and easy-to-use library to enjoy videogames programming (www.raylib.com)
*
*   FEATURES:
*       - NO external dependencies, all required libraries included with raylib
*       - Multiplatform: Windows, Linux, macOS, FreeBSD, Web, Android, Raspberry Pi, DRM native...
*       - Written in plain C code (C99) in PascalCase/camelCase notation
*       - Hardware accelerated with OpenGL (1.1, 2.1, 3.3, 4.3, 4.6, ES2, ES3 - choose at compile)
*       - Software renderer optional, for systems with no GPU: [rlsw]
*       - Custom OpenGL abstraction layer (usable as standalone module): [rlgl]
*       - Multiple Fonts formats supported (TTF, OTF, FNT, BDF, Sprite fonts)
*       - Many texture formats supported, including compressed formats (DXT, ETC, ASTC)
*       - Full 3d support for 3d Shapes, Models, Billboards, Heightmaps and more!
*       - Flexible Materials system, supporting classic maps and PBR maps
*       - Animated 3D models supported (skeletal bones animation) (IQM, M3D, GLTF)
*       - Shaders support, including Model shaders and Postprocessing shaders
*       - Powerful math module for vector, matrix and quaternion operations: [raymath]
*       - Audio loading and playing with streaming support (WAV, OGG, MP3, FLAC, QOA, XM, MOD)
*       - VR stereo rendering with configurable HMD device parameters
*       - Bindings to multiple programming languages available!
*
*   NOTES:
*       - One default Font is loaded on InitWindow()->LoadFontDefault() [core, text]
*       - One default Texture2D is loaded on rlglInit(), 1x1 white pixel R8G8B8A8 [rlgl] (OpenGL 3.3 or ES2)
*       - One default Shader is loaded on rlglInit()->rlLoadShaderDefault() [rlgl] (OpenGL 3.3 or ES2)
*       - One default RenderBatch is loaded on rlglInit()->rlLoadRenderBatch() [rlgl] (OpenGL 3.3 or ES2)
*
*   DEPENDENCIES:
*       [rcore] Depends on the selected platform backend, check rcore.c header for details
*       [rlgl] glad/glad_gles2 (David Herberth - github.com/Dav1dde/glad) for OpenGL extensions loading
*       [raudio] miniaudio (David Reid - github.com/mackron/miniaudio) for audio device/context management
*
*   OPTIONAL DEPENDENCIES (included):
*       [rcore] sinfl (Micha Mettke) for DEFLATE decompression algorithm
*       [rcore] sdefl (Micha Mettke) for DEFLATE compression algorithm
*       [rcore] rprand (Ramon Santamaria) for pseudo-random numbers generation
*       [rtextures] qoi (Dominic Szablewski - https://phoboslab.org) for QOI image management
*       [rtextures] stb_image (Sean Barrett) for images loading (BMP, TGA, PNG, JPEG, HDR...)
*       [rtextures] stb_image_write (Sean Barrett) for image writing (BMP, TGA, PNG, JPG)
*       [rtextures] stb_image_resize2 (Sean Barrett) for image resizing algorithms
*       [rtextures] stb_perlin (Sean Barrett) for Perlin Noise image generation
*       [rtextures] rltexgpu (Ramon Santamaria) for GPU-compressed texture formats
*       [rtext] stb_truetype (Sean Barrett) for ttf fonts loading
*       [rtext] stb_rect_pack (Sean Barrett) for rectangles packing
*       [rmodels] par_shapes (Philip Rideout) for parametric 3d shapes generation
*       [rmodels] tinyobj_loader_c (Syoyo Fujita) for models loading (OBJ, MTL)
*       [rmodels] cgltf (Johannes Kuhlmann) for models loading (glTF)
*       [rmodels] m3d (bzt) for models loading (M3D, https://bztsrc.gitlab.io/model3d)
*       [rmodels] vox_loader (Johann Nadalutti) for models loading (VOX)
*       [raudio] dr_wav (David Reid) for WAV audio file loading
*       [raudio] dr_flac (David Reid) for FLAC audio file loading
*       [raudio] dr_mp3 (David Reid) for MP3 audio file loading
*       [raudio] stb_vorbis (Sean Barrett) for OGG audio loading
*       [raudio] jar_xm (Joshua Reisenauer) for XM audio module loading
*       [raudio] jar_mod (Joshua Reisenauer) for MOD audio module loading
*       [raudio] qoa (Dominic Szablewski - https://phoboslab.org) for QOA audio management
*
*
*   LICENSE: zlib/libpng
*
*   raylib is licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software:
*
*   Copyright (c) 2013-2026 Ramon Santamaria (@raysan5)
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

#ifndef RAYLIB_H
#define RAYLIB_H

#include <string_view>    // Required for: std::string_view text API parameters

#define RAYLIB_VERSION_MAJOR 6
#define RAYLIB_VERSION_MINOR 5
#define RAYLIB_VERSION_PATCH 0
#define RAYLIB_VERSION  "6.5-dev"

// Function specifiers in case library is build/used as a shared library
// NOTE: Microsoft specifiers to tell compiler that symbols are imported/exported from a .dll
// NOTE: visibility("default") attribute makes symbols "visible" when compiled with -fvisibility=hidden
#if defined(_WIN32)
    #if defined(__TINYC__)
        #define __declspec(x) __attribute__((x))
    #endif
    #if defined(BUILD_LIBTYPE_SHARED)
        #define RLAPI __declspec(dllexport)     // Building the library as a Win32 shared library (.dll)
    #elif defined(USE_LIBTYPE_SHARED)
        #define RLAPI __declspec(dllimport)     // Using the library as a Win32 shared library (.dll)
    #endif
#else
    #if defined(BUILD_LIBTYPE_SHARED)
        #define RLAPI __attribute__((visibility("default"))) // Building as a Unix shared library (.so/.dylib)
    #endif
#endif

#ifndef RLAPI
    #define RLAPI       // Functions defined as 'extern' by default (implicit specifiers)
#endif

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
#ifndef PI
    #define PI 3.14159265358979323846f
#endif
#ifndef DEG2RAD
    #define DEG2RAD (PI/180.0f)
#endif
#ifndef RAD2DEG
    #define RAD2DEG (180.0f/PI)
#endif

// NOTE: raylib is a C++ library. Plain structures are initialized with { }
// (aggregate initialization). CLITERAL() is kept so the color macros below read
// the same in every caller.
#define CLITERAL(type)      type

#if defined(_MSVC_LANG) && (_MSVC_LANG < 201703L)
    #error "C++17 or later is required"
#elif !defined(_MSVC_LANG) && (__cplusplus < 201703L)
    #error "C++17 or later is required"
#endif

// NOTE: Set some defines with some data types declared by raylib
// Other modules (raymath, rlgl) also require some of those types, so,
// to be able to use those other modules as standalone (not depending on raylib)
// this defines are useful for internal check and avoid type (re)definitions
#define RL_COLOR_TYPE
#define RL_RECTANGLE_TYPE
#define RL_VECTOR2_TYPE
#define RL_VECTOR3_TYPE
#define RL_VECTOR4_TYPE
#define RL_QUATERNION_TYPE
#define RL_MATRIX_TYPE

// Some Basic Colors
// NOTE: Custom raylib color palette for amazing visuals on WHITE background
#define LIGHTGRAY  CLITERAL(Color){ 200, 200, 200, 255 }   // Light Gray
#define GRAY       CLITERAL(Color){ 130, 130, 130, 255 }   // Gray
#define DARKGRAY   CLITERAL(Color){ 80, 80, 80, 255 }      // Dark Gray
#define YELLOW     CLITERAL(Color){ 253, 249, 0, 255 }     // Yellow
#define GOLD       CLITERAL(Color){ 255, 203, 0, 255 }     // Gold
#define ORANGE     CLITERAL(Color){ 255, 161, 0, 255 }     // Orange
#define PINK       CLITERAL(Color){ 255, 109, 194, 255 }   // Pink
#define RED        CLITERAL(Color){ 230, 41, 55, 255 }     // Red
#define MAROON     CLITERAL(Color){ 190, 33, 55, 255 }     // Maroon
#define GREEN      CLITERAL(Color){ 0, 228, 48, 255 }      // Green
#define LIME       CLITERAL(Color){ 0, 158, 47, 255 }      // Lime
#define DARKGREEN  CLITERAL(Color){ 0, 117, 44, 255 }      // Dark Green
#define SKYBLUE    CLITERAL(Color){ 102, 191, 255, 255 }   // Sky Blue
#define BLUE       CLITERAL(Color){ 0, 121, 241, 255 }     // Blue
#define DARKBLUE   CLITERAL(Color){ 0, 82, 172, 255 }      // Dark Blue
#define PURPLE     CLITERAL(Color){ 200, 122, 255, 255 }   // Purple
#define VIOLET     CLITERAL(Color){ 135, 60, 190, 255 }    // Violet
#define DARKPURPLE CLITERAL(Color){ 112, 31, 126, 255 }    // Dark Purple
#define BEIGE      CLITERAL(Color){ 211, 176, 131, 255 }   // Beige
#define BROWN      CLITERAL(Color){ 127, 106, 79, 255 }    // Brown
#define DARKBROWN  CLITERAL(Color){ 76, 63, 47, 255 }      // Dark Brown

#define WHITE      CLITERAL(Color){ 255, 255, 255, 255 }   // White
#define BLACK      CLITERAL(Color){ 0, 0, 0, 255 }         // Black
#define BLANK      CLITERAL(Color){ 0, 0, 0, 0 }           // Blank (Transparent)
#define MAGENTA    CLITERAL(Color){ 255, 0, 255, 255 }     // Magenta
#define RAYWHITE   CLITERAL(Color){ 245, 245, 245, 255 }   // My own White (raylib logo)

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

// raylib's math vocabulary is glm through the short names in types.h:
// vec2/vec3/vec4, mat4 and quat. The C ABI layout is unchanged (those aliases
// are bit-identical to the structs raylib used to declare), so exported
// symbols keep working.
#include "types.h"

#include <string>
#include <string_view>
#include <vector>

static_assert(sizeof(vec2) == 8,  "vec2 must be 8 bytes (x, y)");
static_assert(sizeof(vec3) == 12, "vec3 must be 12 bytes (x, y, z)");
static_assert(sizeof(vec4) == 16, "vec4 must be 16 bytes (x, y, z, w)");
static_assert(sizeof(mat4) == 64, "mat4 must be 64 bytes (4x4 floats)");
static_assert(sizeof(quat) == 16, "quat must be 16 bytes (w, x, y, z)");

#if defined(GLM_FORCE_DEFAULT_ALIGNED_GENTYPES)
    #error "GLM_FORCE_DEFAULT_ALIGNED_GENTYPES changes the raylib type layout and is not supported"
#endif

// Color, 4 components, R8G8B8A8 (32bit)
typedef struct Color {
    u8 r;        // Color red value
    u8 g;        // Color green value
    u8 b;        // Color blue value
    u8 a;        // Color alpha value

    Color() : r(0), g(0), b(0), a(0) {}
    // NOTE: Defaulted components preserve partial-init semantics: Color c = { 255 } -> { 255, 0, 0, 0 }
    explicit constexpr Color(const u8 r, const u8 g = 0, const u8 b = 0, const u8 a = 0) : r(r), g(g), b(b), a(a) {}

    constexpr bool operator==(const Color &c) const { return (r == c.r) && (g == c.g) && (b == c.b) && (a == c.a); }
    constexpr bool operator!=(const Color &c) const { return !(*this == c); }
} Color;

// Rectangle, 4 components
typedef struct Rectangle {
    f32 x;                // Rectangle top-left corner position x
    f32 y;                // Rectangle top-left corner position y
    f32 width;            // Rectangle width
    f32 height;           // Rectangle height

    Rectangle() : x(0.0f), y(0.0f), width(0.0f), height(0.0f) {}
    explicit constexpr Rectangle(const f32 x, const f32 y = 0.0f, const f32 width = 0.0f, const f32 height = 0.0f) : x(x), y(y), width(width), height(height) {}

    constexpr bool operator==(const Rectangle &r) const { return (x == r.x) && (y == r.y) && (width == r.width) && (height == r.height); }
    constexpr bool operator!=(const Rectangle &r) const { return !(*this == r); }
} Rectangle;

// Image, pixel data stored in CPU memory (RAM)
typedef struct Image {
    void *data;             // Image raw data
    i32 width;              // Image base width
    i32 height;             // Image base height
    i32 mipmaps;            // Mipmap levels, 1 by default
    i32 format;             // Data format (PixelFormat type)
} Image;

// Texture, tex data stored in GPU memory (VRAM)
typedef struct Texture {
    u32 id;        // OpenGL texture id
    i32 width;              // Texture base width
    i32 height;             // Texture base height
    i32 mipmaps;            // Mipmap levels, 1 by default
    i32 format;             // Data format (PixelFormat type)
} Texture;

// Texture2D, same as Texture
typedef Texture Texture2D;

// TextureCubemap, same as Texture
typedef Texture TextureCubemap;

// RenderTexture, fbo for texture rendering
typedef struct RenderTexture {
    u32 id;        // OpenGL framebuffer object id
    Texture texture;        // Color buffer attachment texture
    Texture depth;          // Depth buffer attachment texture
} RenderTexture;

// RenderTexture2D, same as RenderTexture
typedef RenderTexture RenderTexture2D;

// NPatchInfo, n-patch layout info
typedef struct NPatchInfo {
    Rectangle source;         // Texture source rectangle
    i32 left{};               // Left border offset
    i32 top{};                // Top border offset
    i32 right{};              // Right border offset
    i32 bottom{};             // Bottom border offset
    i32 layout{};             // Layout of the n-patch: 3x3, 1x3 or 3x1
} NPatchInfo;

// GlyphInfo, font characters glyphs info
typedef struct GlyphInfo {
    i32 value;              // Character value (Unicode)
    i32 offsetX;            // Character offset X when drawing
    i32 offsetY;            // Character offset Y when drawing
    i32 advanceX;           // Character advance position X
    Image image;            // Character image data
} GlyphInfo;

// Font, font texture and GlyphInfo array data
typedef struct Font {
    i32 baseSize;           // Base size (default chars height)
    i32 glyphCount;         // Number of glyph characters
    i32 glyphPadding;       // Padding around the glyph characters
    Texture2D texture;      // Texture atlas containing the glyphs
    Rectangle *recs;        // Rectangles in texture for the glyphs
    GlyphInfo *glyphs;      // Glyphs info data
} Font;

// Camera, defines position/orientation in 3d space
typedef struct Camera3D {
    vec3 position;          // Camera position
    vec3 target;            // Camera target it looks-at
    vec3 up;                // Camera up vector (rotation over its axis)
    f32 fovy;             // Camera field-of-view aperture in Y (degrees) in perspective, used as near plane height in world units in orthographic
    i32 projection;         // Camera projection: CAMERA_PERSPECTIVE or CAMERA_ORTHOGRAPHIC
} Camera3D;

typedef Camera3D Camera;    // Camera type fallback, defaults to Camera3D

// Camera2D, defines position/orientation in 2d space
typedef struct Camera2D {
    vec2 offset;            // Camera offset (screen space offset from window origin, unscaled by DPI, real pixel size)
    vec2 target;            // Camera target (world space target point that is mapped to screen space offset)
    f32 rotation;         // Camera rotation in degrees (pivots around target)
    f32 zoom;             // Camera zoom (scaling around target), must not be set to 0, set to 1.0f for no scale
} Camera2D;

// Mesh, vertex data and vao/vbo
typedef struct Mesh {
    i32 vertexCount;        // Number of vertices stored in arrays
    i32 triangleCount;      // Number of triangles stored (indexed or not)

    // Vertex attributes data
    f32 *vertices;         // Vertex position (XYZ - 3 components per vertex) (shader-location = 0)
    f32 *texcoords;        // Vertex texture coordinates (UV - 2 components per vertex) (shader-location = 1)
    f32 *texcoords2;       // Vertex texture second coordinates (UV - 2 components per vertex) (shader-location = 5)
    f32 *normals;          // Vertex normals (XYZ - 3 components per vertex) (shader-location = 2)
    f32 *tangents;         // Vertex tangents (XYZW - 4 components per vertex) (shader-location = 4)
    u8 *colors;   // Vertex colors (RGBA - 4 components per vertex) (shader-location = 3)
    u16 *indices; // Vertex indices (in case vertex data comes indexed)

    // Skin data for animation
    i32 boneCount;              // Number of bones (MAX: 256 bones)
    u8 *boneIndices; // Vertex bone indices, up to 4 bones influence by vertex (skinning) (shader-location = 6)
    f32 *boneWeights;         // Vertex bone weight, up to 4 bones influence by vertex (skinning) (shader-location = 7)

    // Runtime animation vertex data (CPU skinning)
    // NOTE: In case of GPU skinning, not used, pointers are NULL
    f32 *animVertices;    // Animated vertex positions (after bones transformations)
    f32 *animNormals;     // Animated normals (after bones transformations)

    // OpenGL identifiers
    u32 vaoId;     // OpenGL Vertex Array Object id
    u32 *vboId;    // OpenGL Vertex Buffer Objects id (default vertex data)
} Mesh;

// Shader
typedef struct Shader {
    u32 id;        // Shader program id
    i32 *locs;              // Shader locations array (RL_MAX_SHADER_LOCATIONS)
} Shader;

// MaterialMap
typedef struct MaterialMap {
    Texture2D texture{};      // Material map texture
    Color color;              // Material map color
    f32 value{};            // Material map value
} MaterialMap;

// Material, includes shader and maps
typedef struct Material {
    Shader shader;          // Material shader
    MaterialMap *maps;      // Material maps array (MAX_MATERIAL_MAPS)
    f32 params[4];        // Material generic parameters (if required)
} Material;

// Transform, vertex transformation data
typedef struct Transform {
    vec3 translation;  // Translation
    quat rotation;     // Rotation
    vec3 scale;        // Scale
} Transform;

// Anim pose, an array of Transform[]
typedef Transform *ModelAnimPose;

// Bone, skeletal animation bone
typedef struct BoneInfo {
    char name[32];          // Bone name
    i32 parent;             // Bone parent
} BoneInfo;

// Skeleton, animation bones hierarchy
typedef struct ModelSkeleton {
    u32 boneCount; // Number of bones
    BoneInfo *bones;        // Bones information (skeleton)
    ModelAnimPose bindPose; // Bones base transformation (Transform[])
} ModelSkeleton;

// Model, meshes, materials and animation data
typedef struct Model {
    mat4 transform;       // Local transform matrix

    i32 meshCount;          // Number of meshes
    i32 materialCount;      // Number of materials
    Mesh *meshes;           // Meshes array
    Material *materials;    // Materials array
    i32 *meshMaterial;      // Mesh material number

    // Animation data
    ModelSkeleton skeleton; // Skeleton for animation

    // Runtime animation data (CPU/GPU skinning)
    ModelAnimPose currentPose; // Current animation pose (Transform[])
    mat4 *boneMatrices;        // Bones animated transformation matrices
} Model;

// ModelAnimation, contains a full animation sequence
typedef struct ModelAnimation {
    char name[32];          // Animation name

    u32 boneCount;       // Number of bones (per pose)
    i32 keyframeCount;            // Number of animation key frames
    ModelAnimPose *keyframePoses; // Animation sequence keyframe poses [keyframe][pose]
} ModelAnimation;

// Ray, ray for raycasting
typedef struct Ray {
    vec3 position;     // Ray position (origin)
    vec3 direction;    // Ray direction (normalized)
} Ray;

// RayCollision, ray hit information
typedef struct RayCollision {
    bool hit;               // Did the ray hit something?
    f32 distance;         // Distance to the nearest hit
    vec3 point;             // Point of the nearest hit
    vec3 normal;            // Surface normal of hit
} RayCollision;

// BoundingBox
typedef struct BoundingBox {
    vec3 min;          // Minimum vertex box-corner
    vec3 max;          // Maximum vertex box-corner
} BoundingBox;

// Wave, audio wave data
typedef struct Wave {
    u32 frameCount;    // Total number of frames (considering channels)
    u32 sampleRate;    // Frequency (samples per second)
    u32 sampleSize;    // Bit depth (bits per sample): 8, 16, 32 (24 not supported)
    u32 channels;      // Number of channels (1-mono, 2-stereo, ...)
    void *data;                 // Buffer data pointer
} Wave;

// Opaque structs declaration
// NOTE: Actual structs are defined internally in raudio module
typedef struct rAudioBuffer rAudioBuffer;
typedef struct rAudioProcessor rAudioProcessor;

// AudioStream, custom audio stream
typedef struct AudioStream {
    rAudioBuffer *buffer;       // Pointer to internal data used by the audio system
    rAudioProcessor *processor; // Pointer to internal data processor, useful for audio effects

    u32 sampleRate;    // Frequency (samples per second)
    u32 sampleSize;    // Bit depth (bits per sample): 8, 16, 32 (24 not supported)
    u32 channels;      // Number of channels (1-mono, 2-stereo, ...)
} AudioStream;

// Sound
typedef struct Sound {
    AudioStream stream;         // Audio stream
    u32 frameCount;    // Total number of frames (considering channels)
} Sound;

// Music, audio stream, anything longer than ~10 seconds should be streamed
typedef struct Music {
    AudioStream stream;         // Audio stream
    u32 frameCount;    // Total number of frames (considering channels)
    bool looping;               // Music looping enable

    i32 ctxType;                // Type of music context (audio filetype)
    void *ctxData;              // Audio context data, depends on type
} Music;

// VrDeviceInfo, Head-Mounted-Display device parameters
typedef struct VrDeviceInfo {
    i32 hResolution;                // Horizontal resolution in pixels
    i32 vResolution;                // Vertical resolution in pixels
    f32 hScreenSize;              // Horizontal size in meters
    f32 vScreenSize;              // Vertical size in meters
    f32 eyeToScreenDistance;      // Distance between eye and display in meters
    f32 lensSeparationDistance;   // Lens separation distance in meters
    f32 interpupillaryDistance;   // IPD (distance between pupils) in meters
    f32 lensDistortionValues[4];  // Lens distortion constant parameters
    f32 chromaAbCorrection[4];    // Chromatic aberration correction parameters
} VrDeviceInfo;

// VrStereoConfig, VR stereo rendering configuration for simulator
typedef struct VrStereoConfig {
    mat4 projection[2];             // VR projection matrices (per eye)
    mat4 viewOffset[2];             // VR view offset matrices (per eye)
    f32 leftLensCenter[2];        // VR left lens center
    f32 rightLensCenter[2];       // VR right lens center
    f32 leftScreenCenter[2];      // VR left screen center
    f32 rightScreenCenter[2];     // VR right screen center
    f32 scale[2];                 // VR distortion scale
    f32 scaleIn[2];               // VR distortion scale in
} VrStereoConfig;

// File path list
typedef struct FilePathList {
    std::vector<std::string> paths; // Filepaths entries
} FilePathList;

// Automation event
typedef struct AutomationEvent {
    u32 frame;             // Event frame
    u32 type;              // Event type (AutomationEventType)
    i32 params[4];                  // Event parameters (if required)
} AutomationEvent;

// Automation event list
typedef struct AutomationEventList {
    u32 capacity;          // Events max entries (MAX_AUTOMATION_EVENTS)
    u32 count;             // Events entries count
    AutomationEvent *events;        // Events entries
} AutomationEventList;

//----------------------------------------------------------------------------------
// Enumerators Definition
//----------------------------------------------------------------------------------
// System/Window config flags
// NOTE: Every bit registers one state (use it with bit masks)
// By default all flags are set to 0
typedef enum {
    FLAG_VSYNC_HINT         = 0x00000040,       // Set to try enabling V-Sync on GPU
    FLAG_FULLSCREEN_MODE    = 0x00000002,       // Set to run program in fullscreen
    FLAG_WINDOW_RESIZABLE   = 0x00000004,       // Set to allow resizable window
    FLAG_WINDOW_UNDECORATED = 0x00000008,       // Set to disable window decoration (frame and buttons)
    FLAG_WINDOW_HIDDEN      = 0x00000080,       // Set to hide window
    FLAG_WINDOW_MINIMIZED   = 0x00000200,       // Set to minimize window (iconify)
    FLAG_WINDOW_MAXIMIZED   = 0x00000400,       // Set to maximize window (expanded to monitor)
    FLAG_WINDOW_UNFOCUSED   = 0x00000800,       // Set to window non focused
    FLAG_WINDOW_TOPMOST     = 0x00001000,       // Set to window always on top
    FLAG_WINDOW_ALWAYS_RUN  = 0x00000100,       // Set to allow windows running while minimized
    FLAG_WINDOW_TRANSPARENT = 0x00000010,       // Set to allow transparent framebuffer
    FLAG_WINDOW_HIGHDPI     = 0x00002000,       // Set to support HighDPI
    FLAG_WINDOW_MOUSE_PASSTHROUGH = 0x00004000, // Set to support mouse passthrough, only supported when FLAG_WINDOW_UNDECORATED
    FLAG_BORDERLESS_WINDOWED_MODE = 0x00008000, // Set to run program in borderless windowed mode
    FLAG_MSAA_4X_HINT       = 0x00000020,       // Set to try enabling MSAA 4X
    FLAG_MSAA_8X_HINT       = 0x00020000,       // Set to try enabling MSAA 8X
    FLAG_MSAA_16X_HINT      = 0x00040000,       // Set to try enabling MSAA 16X
    FLAG_INTERLACED_HINT    = 0x00010000    // Set to try enabling interlaced video format (for V3D)
} ConfigFlags;

// Trace log level
// NOTE: Organized by priority level
typedef enum {
    LOG_ALL = 0,        // Display all logs
    LOG_TRACE,          // Trace logging, intended for internal use only
    LOG_DEBUG,          // Debug logging, used for internal debugging, it should be disabled on release builds
    LOG_INFO,           // Info logging, used for program execution info
    LOG_WARNING,        // Warning logging, used on recoverable failures
    LOG_ERROR,          // Error logging, used on unrecoverable failures
    LOG_FATAL,          // Fatal logging, used to abort program: exit(EXIT_FAILURE)
    LOG_NONE            // Disable logging
} TraceLogLevel;

// Keyboard keys (US keyboard layout)
// NOTE: Use GetKeyPressed() to allow redefining required keys for alternative layouts
typedef enum {
    KEY_NULL            = 0,        // Key: NULL, used for no key pressed
    // Alphanumeric keys
    KEY_APOSTROPHE      = 39,       // Key: '
    KEY_COMMA           = 44,       // Key: ,
    KEY_MINUS           = 45,       // Key: -
    KEY_PERIOD          = 46,       // Key: .
    KEY_SLASH           = 47,       // Key: /
    KEY_ZERO            = 48,       // Key: 0
    KEY_ONE             = 49,       // Key: 1
    KEY_TWO             = 50,       // Key: 2
    KEY_THREE           = 51,       // Key: 3
    KEY_FOUR            = 52,       // Key: 4
    KEY_FIVE            = 53,       // Key: 5
    KEY_SIX             = 54,       // Key: 6
    KEY_SEVEN           = 55,       // Key: 7
    KEY_EIGHT           = 56,       // Key: 8
    KEY_NINE            = 57,       // Key: 9
    KEY_SEMICOLON       = 59,       // Key: ;
    KEY_EQUAL           = 61,       // Key: =
    KEY_A               = 65,       // Key: A | a
    KEY_B               = 66,       // Key: B | b
    KEY_C               = 67,       // Key: C | c
    KEY_D               = 68,       // Key: D | d
    KEY_E               = 69,       // Key: E | e
    KEY_F               = 70,       // Key: F | f
    KEY_G               = 71,       // Key: G | g
    KEY_H               = 72,       // Key: H | h
    KEY_I               = 73,       // Key: I | i
    KEY_J               = 74,       // Key: J | j
    KEY_K               = 75,       // Key: K | k
    KEY_L               = 76,       // Key: L | l
    KEY_M               = 77,       // Key: M | m
    KEY_N               = 78,       // Key: N | n
    KEY_O               = 79,       // Key: O | o
    KEY_P               = 80,       // Key: P | p
    KEY_Q               = 81,       // Key: Q | q
    KEY_R               = 82,       // Key: R | r
    KEY_S               = 83,       // Key: S | s
    KEY_T               = 84,       // Key: T | t
    KEY_U               = 85,       // Key: U | u
    KEY_V               = 86,       // Key: V | v
    KEY_W               = 87,       // Key: W | w
    KEY_X               = 88,       // Key: X | x
    KEY_Y               = 89,       // Key: Y | y
    KEY_Z               = 90,       // Key: Z | z
    KEY_LEFT_BRACKET    = 91,       // Key: [
    KEY_BACKSLASH       = 92,       // Key: '\'
    KEY_RIGHT_BRACKET   = 93,       // Key: ]
    KEY_GRAVE           = 96,       // Key: `
    // Function keys
    KEY_SPACE           = 32,       // Key: Space
    KEY_ESCAPE          = 256,      // Key: Esc
    KEY_ENTER           = 257,      // Key: Enter
    KEY_TAB             = 258,      // Key: Tab
    KEY_BACKSPACE       = 259,      // Key: Backspace
    KEY_INSERT          = 260,      // Key: Ins
    KEY_DELETE          = 261,      // Key: Del
    KEY_RIGHT           = 262,      // Key: Cursor right
    KEY_LEFT            = 263,      // Key: Cursor left
    KEY_DOWN            = 264,      // Key: Cursor down
    KEY_UP              = 265,      // Key: Cursor up
    KEY_PAGE_UP         = 266,      // Key: Page up
    KEY_PAGE_DOWN       = 267,      // Key: Page down
    KEY_HOME            = 268,      // Key: Home
    KEY_END             = 269,      // Key: End
    KEY_CAPS_LOCK       = 280,      // Key: Caps lock
    KEY_SCROLL_LOCK     = 281,      // Key: Scroll down
    KEY_NUM_LOCK        = 282,      // Key: Num lock
    KEY_PRINT_SCREEN    = 283,      // Key: Print screen
    KEY_PAUSE           = 284,      // Key: Pause
    KEY_F1              = 290,      // Key: F1
    KEY_F2              = 291,      // Key: F2
    KEY_F3              = 292,      // Key: F3
    KEY_F4              = 293,      // Key: F4
    KEY_F5              = 294,      // Key: F5
    KEY_F6              = 295,      // Key: F6
    KEY_F7              = 296,      // Key: F7
    KEY_F8              = 297,      // Key: F8
    KEY_F9              = 298,      // Key: F9
    KEY_F10             = 299,      // Key: F10
    KEY_F11             = 300,      // Key: F11
    KEY_F12             = 301,      // Key: F12
    KEY_LEFT_SHIFT      = 340,      // Key: Shift left
    KEY_LEFT_CONTROL    = 341,      // Key: Control left
    KEY_LEFT_ALT        = 342,      // Key: Alt left
    KEY_LEFT_SUPER      = 343,      // Key: Super left
    KEY_RIGHT_SHIFT     = 344,      // Key: Shift right
    KEY_RIGHT_CONTROL   = 345,      // Key: Control right
    KEY_RIGHT_ALT       = 346,      // Key: Alt right
    KEY_RIGHT_SUPER     = 347,      // Key: Super right
    KEY_KB_MENU         = 348,      // Key: KB menu
    // Keypad keys
    KEY_KP_0            = 320,      // Key: Keypad 0
    KEY_KP_1            = 321,      // Key: Keypad 1
    KEY_KP_2            = 322,      // Key: Keypad 2
    KEY_KP_3            = 323,      // Key: Keypad 3
    KEY_KP_4            = 324,      // Key: Keypad 4
    KEY_KP_5            = 325,      // Key: Keypad 5
    KEY_KP_6            = 326,      // Key: Keypad 6
    KEY_KP_7            = 327,      // Key: Keypad 7
    KEY_KP_8            = 328,      // Key: Keypad 8
    KEY_KP_9            = 329,      // Key: Keypad 9
    KEY_KP_DECIMAL      = 330,      // Key: Keypad .
    KEY_KP_DIVIDE       = 331,      // Key: Keypad /
    KEY_KP_MULTIPLY     = 332,      // Key: Keypad *
    KEY_KP_SUBTRACT     = 333,      // Key: Keypad -
    KEY_KP_ADD          = 334,      // Key: Keypad +
    KEY_KP_ENTER        = 335,      // Key: Keypad Enter
    KEY_KP_EQUAL        = 336,      // Key: Keypad =
    // Android key buttons
    KEY_BACK            = 4,        // Key: Android back button
    KEY_MENU            = 5,        // Key: Android menu button
    KEY_VOLUME_UP       = 24,       // Key: Android volume up button
    KEY_VOLUME_DOWN     = 25        // Key: Android volume down button
} KeyboardKey;

// Add backwards compatibility support for deprecated names
#define MOUSE_LEFT_BUTTON   MOUSE_BUTTON_LEFT
#define MOUSE_RIGHT_BUTTON  MOUSE_BUTTON_RIGHT
#define MOUSE_MIDDLE_BUTTON MOUSE_BUTTON_MIDDLE

// Mouse buttons
typedef enum {
    MOUSE_BUTTON_LEFT    = 0,       // Mouse button left
    MOUSE_BUTTON_RIGHT   = 1,       // Mouse button right
    MOUSE_BUTTON_MIDDLE  = 2,       // Mouse button middle (pressed wheel)
    MOUSE_BUTTON_SIDE    = 3,       // Mouse button side (advanced mouse device)
    MOUSE_BUTTON_EXTRA   = 4,       // Mouse button extra (advanced mouse device)
    MOUSE_BUTTON_FORWARD = 5,       // Mouse button forward (advanced mouse device)
    MOUSE_BUTTON_BACK    = 6,       // Mouse button back (advanced mouse device)
} MouseButton;

// Mouse cursor
typedef enum {
    MOUSE_CURSOR_DEFAULT       = 0,     // Default pointer shape
    MOUSE_CURSOR_ARROW         = 1,     // Arrow shape
    MOUSE_CURSOR_IBEAM         = 2,     // Text writing cursor shape
    MOUSE_CURSOR_CROSSHAIR     = 3,     // Cross shape
    MOUSE_CURSOR_POINTING_HAND = 4,     // Pointing hand cursor
    MOUSE_CURSOR_RESIZE_EW     = 5,     // Horizontal resize/move arrow shape
    MOUSE_CURSOR_RESIZE_NS     = 6,     // Vertical resize/move arrow shape
    MOUSE_CURSOR_RESIZE_NWSE   = 7,     // Top-left to bottom-right diagonal resize/move arrow shape
    MOUSE_CURSOR_RESIZE_NESW   = 8,     // The top-right to bottom-left diagonal resize/move arrow shape
    MOUSE_CURSOR_RESIZE_ALL    = 9,     // The omnidirectional resize/move cursor shape
    MOUSE_CURSOR_NOT_ALLOWED   = 10     // The operation-not-allowed shape
} MouseCursor;

// Gamepad buttons
typedef enum {
    GAMEPAD_BUTTON_UNKNOWN = 0,         // Unknown button, for error checking
    GAMEPAD_BUTTON_LEFT_FACE_UP,        // Gamepad left DPAD up button
    GAMEPAD_BUTTON_LEFT_FACE_RIGHT,     // Gamepad left DPAD right button
    GAMEPAD_BUTTON_LEFT_FACE_DOWN,      // Gamepad left DPAD down button
    GAMEPAD_BUTTON_LEFT_FACE_LEFT,      // Gamepad left DPAD left button
    GAMEPAD_BUTTON_RIGHT_FACE_UP,       // Gamepad right button up (i.e. PS3: Triangle, Xbox: Y)
    GAMEPAD_BUTTON_RIGHT_FACE_RIGHT,    // Gamepad right button right (i.e. PS3: Circle, Xbox: B)
    GAMEPAD_BUTTON_RIGHT_FACE_DOWN,     // Gamepad right button down (i.e. PS3: Cross, Xbox: A)
    GAMEPAD_BUTTON_RIGHT_FACE_LEFT,     // Gamepad right button left (i.e. PS3: Square, Xbox: X)
    GAMEPAD_BUTTON_LEFT_TRIGGER_1,      // Gamepad top/back trigger left (first), it could be a trailing button
    GAMEPAD_BUTTON_LEFT_TRIGGER_2,      // Gamepad top/back trigger left (second), it could be a trailing button
    GAMEPAD_BUTTON_RIGHT_TRIGGER_1,     // Gamepad top/back trigger right (first), it could be a trailing button
    GAMEPAD_BUTTON_RIGHT_TRIGGER_2,     // Gamepad top/back trigger right (second), it could be a trailing button
    GAMEPAD_BUTTON_MIDDLE_LEFT,         // Gamepad center buttons, left one (i.e. PS3: Select)
    GAMEPAD_BUTTON_MIDDLE,              // Gamepad center buttons, middle one (i.e. PS3: PS, Xbox: XBOX)
    GAMEPAD_BUTTON_MIDDLE_RIGHT,        // Gamepad center buttons, right one (i.e. PS3: Start)
    GAMEPAD_BUTTON_LEFT_THUMB,          // Gamepad joystick pressed button left
    GAMEPAD_BUTTON_RIGHT_THUMB          // Gamepad joystick pressed button right
} GamepadButton;

// Gamepad axes
typedef enum {
    GAMEPAD_AXIS_LEFT_X        = 0,     // Gamepad left stick X axis
    GAMEPAD_AXIS_LEFT_Y        = 1,     // Gamepad left stick Y axis
    GAMEPAD_AXIS_RIGHT_X       = 2,     // Gamepad right stick X axis
    GAMEPAD_AXIS_RIGHT_Y       = 3,     // Gamepad right stick Y axis
    GAMEPAD_AXIS_LEFT_TRIGGER  = 4,     // Gamepad back trigger left, pressure level: [1..-1]
    GAMEPAD_AXIS_RIGHT_TRIGGER = 5      // Gamepad back trigger right, pressure level: [1..-1]
} GamepadAxis;

// Material map index
typedef enum {
    MATERIAL_MAP_ALBEDO = 0,        // Albedo material (same as: MATERIAL_MAP_DIFFUSE)
    MATERIAL_MAP_METALNESS,         // Metalness material (same as: MATERIAL_MAP_SPECULAR)
    MATERIAL_MAP_NORMAL,            // Normal material
    MATERIAL_MAP_ROUGHNESS,         // Roughness material
    MATERIAL_MAP_OCCLUSION,         // Ambient occlusion material
    MATERIAL_MAP_EMISSION,          // Emission material
    MATERIAL_MAP_HEIGHT,            // Heightmap material
    MATERIAL_MAP_CUBEMAP,           // Cubemap material (NOTE: Uses GL_TEXTURE_CUBE_MAP)
    MATERIAL_MAP_IRRADIANCE,        // Irradiance material (NOTE: Uses GL_TEXTURE_CUBE_MAP)
    MATERIAL_MAP_PREFILTER,         // Prefilter material (NOTE: Uses GL_TEXTURE_CUBE_MAP)
    MATERIAL_MAP_BRDF               // Brdf material
} MaterialMapIndex;

#define MATERIAL_MAP_DIFFUSE      MATERIAL_MAP_ALBEDO
#define MATERIAL_MAP_SPECULAR     MATERIAL_MAP_METALNESS

// Shader location index
// NOTE: Some locations are tried to be set automatically on shader loading,
// but only if default attributes/uniforms names are found, check config.h for names
typedef enum {
    SHADER_LOC_VERTEX_POSITION = 0,   // Shader location: vertex attribute: position
    SHADER_LOC_VERTEX_TEXCOORD01,     // Shader location: vertex attribute: texcoord01
    SHADER_LOC_VERTEX_TEXCOORD02,     // Shader location: vertex attribute: texcoord02
    SHADER_LOC_VERTEX_NORMAL,         // Shader location: vertex attribute: normal
    SHADER_LOC_VERTEX_TANGENT,        // Shader location: vertex attribute: tangent
    SHADER_LOC_VERTEX_COLOR,          // Shader location: vertex attribute: color
    SHADER_LOC_MATRIX_MVP,            // Shader location: matrix uniform: model-view-projection
    SHADER_LOC_MATRIX_VIEW,           // Shader location: matrix uniform: view (camera transform)
    SHADER_LOC_MATRIX_PROJECTION,     // Shader location: matrix uniform: projection
    SHADER_LOC_MATRIX_MODEL,          // Shader location: matrix uniform: model (transform)
    SHADER_LOC_MATRIX_NORMAL,         // Shader location: matrix uniform: normal
    SHADER_LOC_VECTOR_VIEW,           // Shader location: vector uniform: view
    SHADER_LOC_COLOR_DIFFUSE,         // Shader location: vector uniform: diffuse color
    SHADER_LOC_COLOR_SPECULAR,        // Shader location: vector uniform: specular color
    SHADER_LOC_COLOR_AMBIENT,         // Shader location: vector uniform: ambient color
    SHADER_LOC_MAP_ALBEDO,            // Shader location: sampler2d texture: albedo (same as: SHADER_LOC_MAP_DIFFUSE)
    SHADER_LOC_MAP_METALNESS,         // Shader location: sampler2d texture: metalness (same as: SHADER_LOC_MAP_SPECULAR)
    SHADER_LOC_MAP_NORMAL,            // Shader location: sampler2d texture: normal
    SHADER_LOC_MAP_ROUGHNESS,         // Shader location: sampler2d texture: roughness
    SHADER_LOC_MAP_OCCLUSION,         // Shader location: sampler2d texture: occlusion
    SHADER_LOC_MAP_EMISSION,          // Shader location: sampler2d texture: emission
    SHADER_LOC_MAP_HEIGHT,            // Shader location: sampler2d texture: heightmap
    SHADER_LOC_MAP_CUBEMAP,           // Shader location: samplerCube texture: cubemap
    SHADER_LOC_MAP_IRRADIANCE,        // Shader location: samplerCube texture: irradiance
    SHADER_LOC_MAP_PREFILTER,         // Shader location: samplerCube texture: prefilter
    SHADER_LOC_MAP_BRDF,              // Shader location: sampler2d texture: brdf
    SHADER_LOC_VERTEX_BONEIDS,        // Shader location: vertex attribute: bone indices
    SHADER_LOC_VERTEX_BONEWEIGHTS,    // Shader location: vertex attribute: bone weights
    SHADER_LOC_MATRIX_BONETRANSFORMS, // Shader location: matrix attribute: bone transforms (animation)
    SHADER_LOC_VERTEX_INSTANCETRANSFORM // Shader location: vertex attribute: instance transforms
} ShaderLocationIndex;

#define SHADER_LOC_MAP_DIFFUSE      SHADER_LOC_MAP_ALBEDO
#define SHADER_LOC_MAP_SPECULAR     SHADER_LOC_MAP_METALNESS

// Shader uniform data type
typedef enum {
    SHADER_UNIFORM_FLOAT = 0,       // Shader uniform type: float
    SHADER_UNIFORM_VEC2,            // Shader uniform type: vec2 (2 float)
    SHADER_UNIFORM_VEC3,            // Shader uniform type: vec3 (3 float)
    SHADER_UNIFORM_VEC4,            // Shader uniform type: vec4 (4 float)
    SHADER_UNIFORM_INT,             // Shader uniform type: int
    SHADER_UNIFORM_IVEC2,           // Shader uniform type: ivec2 (2 int)
    SHADER_UNIFORM_IVEC3,           // Shader uniform type: ivec3 (3 int)
    SHADER_UNIFORM_IVEC4,           // Shader uniform type: ivec4 (4 int)
    SHADER_UNIFORM_UINT,            // Shader uniform type: unsigned int
    SHADER_UNIFORM_UIVEC2,          // Shader uniform type: uivec2 (2 unsigned int)
    SHADER_UNIFORM_UIVEC3,          // Shader uniform type: uivec3 (3 unsigned int)
    SHADER_UNIFORM_UIVEC4,          // Shader uniform type: uivec4 (4 unsigned int)
    SHADER_UNIFORM_SAMPLER2D        // Shader uniform type: sampler2d
} ShaderUniformDataType;

// Shader attribute data types
typedef enum {
    SHADER_ATTRIB_FLOAT = 0,        // Shader attribute type: float
    SHADER_ATTRIB_VEC2,             // Shader attribute type: vec2 (2 float)
    SHADER_ATTRIB_VEC3,             // Shader attribute type: vec3 (3 float)
    SHADER_ATTRIB_VEC4              // Shader attribute type: vec4 (4 float)
} ShaderAttributeDataType;

// Pixel formats
// NOTE: Support depends on OpenGL version and platform
typedef enum {
    PIXELFORMAT_UNCOMPRESSED_GRAYSCALE = 1, // 8 bit per pixel (no alpha)
    PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA,    // 8*2 bpp (2 channels)
    PIXELFORMAT_UNCOMPRESSED_R5G6B5,        // 16 bpp
    PIXELFORMAT_UNCOMPRESSED_R8G8B8,        // 24 bpp
    PIXELFORMAT_UNCOMPRESSED_R5G5B5A1,      // 16 bpp (1 bit alpha)
    PIXELFORMAT_UNCOMPRESSED_R4G4B4A4,      // 16 bpp (4 bit alpha)
    PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,      // 32 bpp
    PIXELFORMAT_UNCOMPRESSED_R32,           // 32 bpp (1 channel - float)
    PIXELFORMAT_UNCOMPRESSED_R32G32B32,     // 32*3 bpp (3 channels - float)
    PIXELFORMAT_UNCOMPRESSED_R32G32B32A32,  // 32*4 bpp (4 channels - float)
    PIXELFORMAT_UNCOMPRESSED_R16,           // 16 bpp (1 channel - half float)
    PIXELFORMAT_UNCOMPRESSED_R16G16B16,     // 16*3 bpp (3 channels - half float)
    PIXELFORMAT_UNCOMPRESSED_R16G16B16A16,  // 16*4 bpp (4 channels - half float)
    PIXELFORMAT_COMPRESSED_DXT1_RGB,        // 4 bpp (no alpha)
    PIXELFORMAT_COMPRESSED_DXT1_RGBA,       // 4 bpp (1 bit alpha)
    PIXELFORMAT_COMPRESSED_DXT3_RGBA,       // 8 bpp
    PIXELFORMAT_COMPRESSED_DXT5_RGBA,       // 8 bpp
    PIXELFORMAT_COMPRESSED_ETC1_RGB,        // 4 bpp
    PIXELFORMAT_COMPRESSED_ETC2_RGB,        // 4 bpp
    PIXELFORMAT_COMPRESSED_ETC2_EAC_RGBA,   // 8 bpp
    PIXELFORMAT_COMPRESSED_PVRT_RGB,        // 4 bpp
    PIXELFORMAT_COMPRESSED_PVRT_RGBA,       // 4 bpp
    PIXELFORMAT_COMPRESSED_ASTC_4x4_RGBA,   // 8 bpp
    PIXELFORMAT_COMPRESSED_ASTC_8x8_RGBA    // 2 bpp
} PixelFormat;

// Texture parameters: filter mode
// NOTE 1: Filtering considers mipmaps if available in the texture
// NOTE 2: Filter is accordingly set for minification and magnification
typedef enum {
    TEXTURE_FILTER_POINT = 0,               // No filter, pixel approximation
    TEXTURE_FILTER_BILINEAR,                // Linear filtering
    TEXTURE_FILTER_TRILINEAR,               // Trilinear filtering (linear with mipmaps)
    TEXTURE_FILTER_ANISOTROPIC_4X,          // Anisotropic filtering 4x
    TEXTURE_FILTER_ANISOTROPIC_8X,          // Anisotropic filtering 8x
    TEXTURE_FILTER_ANISOTROPIC_16X,         // Anisotropic filtering 16x
} TextureFilter;

// Texture parameters: wrap mode
typedef enum {
    TEXTURE_WRAP_REPEAT = 0,                // Repeats texture in tiled mode
    TEXTURE_WRAP_CLAMP,                     // Clamps texture to edge pixel in tiled mode
    TEXTURE_WRAP_MIRROR_REPEAT,             // Mirrors and repeats the texture in tiled mode
    TEXTURE_WRAP_MIRROR_CLAMP               // Mirrors and clamps to border the texture in tiled mode
} TextureWrap;

// Cubemap layouts
typedef enum {
    CUBEMAP_LAYOUT_AUTO_DETECT = 0,         // Automatically detect layout type
    CUBEMAP_LAYOUT_LINE_VERTICAL,           // Layout is defined by a vertical line with faces
    CUBEMAP_LAYOUT_LINE_HORIZONTAL,         // Layout is defined by a horizontal line with faces
    CUBEMAP_LAYOUT_CROSS_THREE_BY_FOUR,     // Layout is defined by a 3x4 cross with cubemap faces
    CUBEMAP_LAYOUT_CROSS_FOUR_BY_THREE     // Layout is defined by a 4x3 cross with cubemap faces
} CubemapLayout;

// Font type, defines generation method
typedef enum {
    FONT_DEFAULT = 0,               // Default font generation, anti-aliased
    FONT_BITMAP,                    // Bitmap font generation, no anti-aliasing
    FONT_SDF                        // SDF font generation, requires external shader
} FontType;

// Color blending modes (pre-defined)
typedef enum {
    BLEND_ALPHA = 0,                // Blend textures considering alpha (default)
    BLEND_ADDITIVE,                 // Blend textures adding colors
    BLEND_MULTIPLIED,               // Blend textures multiplying colors
    BLEND_ADD_COLORS,               // Blend textures adding colors (alternative)
    BLEND_SUBTRACT_COLORS,          // Blend textures subtracting colors (alternative)
    BLEND_ALPHA_PREMULTIPLY,        // Blend premultiplied textures considering alpha
    BLEND_CUSTOM,                   // Blend textures using custom src/dst factors (use rlSetBlendFactors())
    BLEND_CUSTOM_SEPARATE           // Blend textures using custom rgb/alpha separate src/dst factors (use rlSetBlendFactorsSeparate())
} BlendMode;

// Gesture
// NOTE: Provided as bit-wise flags to enable only desired gestures
typedef enum {
    GESTURE_NONE        = 0,        // No gesture
    GESTURE_TAP         = 1,        // Tap gesture
    GESTURE_DOUBLETAP   = 2,        // Double tap gesture
    GESTURE_HOLD        = 4,        // Hold gesture
    GESTURE_DRAG        = 8,        // Drag gesture
    GESTURE_SWIPE_RIGHT = 16,       // Swipe right gesture
    GESTURE_SWIPE_LEFT  = 32,       // Swipe left gesture
    GESTURE_SWIPE_UP    = 64,       // Swipe up gesture
    GESTURE_SWIPE_DOWN  = 128,      // Swipe down gesture
    GESTURE_PINCH_IN    = 256,      // Pinch in gesture
    GESTURE_PINCH_OUT   = 512       // Pinch out gesture
} Gesture;

// Camera system modes
typedef enum {
    CAMERA_CUSTOM = 0,              // Camera custom, controlled by user (UpdateCamera() does nothing)
    CAMERA_FREE,                    // Camera free mode
    CAMERA_ORBITAL,                 // Camera orbital, around target, zoom supported
    CAMERA_FIRST_PERSON,            // Camera first person
    CAMERA_THIRD_PERSON             // Camera third person
} CameraMode;

// Camera projection
typedef enum {
    CAMERA_PERSPECTIVE = 0,         // Perspective projection
    CAMERA_ORTHOGRAPHIC             // Orthographic projection
} CameraProjection;

// N-patch layout
typedef enum {
    NPATCH_NINE_PATCH = 0,          // Npatch layout: 3x3 tiles
    NPATCH_THREE_PATCH_VERTICAL,    // Npatch layout: 1x3 tiles
    NPATCH_THREE_PATCH_HORIZONTAL   // Npatch layout: 3x1 tiles
} NPatchLayout;

// Callbacks to hook some internal functions
// WARNING: These callbacks are intended for advanced users
typedef void (*TraceLogCallback)(i32 logLevel, std::string_view text);                            // Logging: Redirect trace log messages (text is already formatted)
typedef u8 *(*LoadFileDataCallback)(std::string_view fileName, i32 *dataSize);        // FileIO: Load binary data
typedef bool (*SaveFileDataCallback)(std::string_view fileName, const void *data, i32 dataSize); // FileIO: Save binary data
typedef std::string (*LoadFileTextCallback)(std::string_view fileName);                          // FileIO: Load text data
typedef bool (*SaveFileTextCallback)(std::string_view fileName, std::string_view text);          // FileIO: Save text data

//------------------------------------------------------------------------------------
// Global Variables Definition
//------------------------------------------------------------------------------------
// It's lonely here...

//------------------------------------------------------------------------------------
// Window and Graphics Device Functions (Module: core)
//------------------------------------------------------------------------------------


// Window-related functions
RLAPI void InitWindow(i32 width, i32 height, std::string_view title);  // Initialize window and OpenGL context
RLAPI void CloseWindow();                                              // Close window and unload OpenGL context
RLAPI bool WindowShouldClose();                                        // Check if application should close (KEY_ESCAPE pressed or windows close icon clicked)
RLAPI bool IsWindowReady();                                            // Check if window has been initialized successfully
RLAPI bool IsWindowFullscreen();                                       // Check if window is currently fullscreen
RLAPI bool IsWindowHidden();                                           // Check if window is currently hidden
RLAPI bool IsWindowMinimized();                                        // Check if window is currently minimized
RLAPI bool IsWindowMaximized();                                        // Check if window is currently maximized
RLAPI bool IsWindowFocused();                                          // Check if window is currently focused
RLAPI bool IsWindowResized();                                          // Check if window has been resized last frame
RLAPI bool IsWindowState(u32 flag);                           // Check if one specific window flag is enabled
RLAPI void SetWindowState(u32 flags);                         // Set window configuration state using flags
RLAPI void ClearWindowState(u32 flags);                       // Clear window configuration state flags
RLAPI void SetWindowSamples(i32 samples);                              // Set the MSAA samples to request for the window, must be called before InitWindow()
RLAPI i32 GetWindowSamples();                                          // Get the MSAA samples in use, zero means MSAA disabled
RLAPI void ToggleFullscreen();                                         // Toggle window state: fullscreen/windowed, resizes monitor to match window resolution
RLAPI void ToggleBorderlessWindowed();                                 // Toggle window state: borderless windowed, resizes window to match monitor resolution
RLAPI void MaximizeWindow();                                           // Set window state: maximized, if resizable
RLAPI void MinimizeWindow();                                           // Set window state: minimized, if resizable
RLAPI void RestoreWindow();                                            // Restore window from being minimized/maximized
RLAPI void SetWindowIcon(Image image);                                 // Set icon for window (single image, RGBA 32bit)
RLAPI void SetWindowIcons(Image *images, i32 count);                   // Set icon for window (multiple images, RGBA 32bit)
RLAPI void SetWindowTitle(std::string_view title);                     // Set title for window
RLAPI void SetWindowPosition(i32 x, i32 y);                            // Set window position on screen
RLAPI void SetWindowMonitor(i32 monitor);                              // Set monitor for the current window
RLAPI void SetWindowMinSize(i32 width, i32 height);                    // Set window minimum dimensions (for FLAG_WINDOW_RESIZABLE)
RLAPI void SetWindowMaxSize(i32 width, i32 height);                    // Set window maximum dimensions (for FLAG_WINDOW_RESIZABLE)
RLAPI void SetWindowSize(i32 width, i32 height);                       // Set window dimensions
RLAPI void SetWindowOpacity(f32 opacity);                            // Set window opacity [0.0f..1.0f]
RLAPI void SetWindowFocused();                                         // Set window focused
RLAPI void *GetWindowHandle();                                         // Get native window handle
RLAPI i32 GetScreenWidth();                                            // Get current screen width
RLAPI i32 GetScreenHeight();                                           // Get current screen height
RLAPI i32 GetRenderWidth();                                            // Get current render width (it considers HiDPI)
RLAPI i32 GetRenderHeight();                                           // Get current render height (it considers HiDPI)
RLAPI i32 GetMonitorCount();                                           // Get number of connected monitors
RLAPI i32 GetCurrentMonitor();                                         // Get current monitor where window is placed
RLAPI vec2 GetMonitorPosition(i32 monitor);                            // Get specified monitor position
RLAPI i32 GetMonitorWidth(i32 monitor);                                // Get specified monitor width (current video mode used by monitor)
RLAPI i32 GetMonitorHeight(i32 monitor);                               // Get specified monitor height (current video mode used by monitor)
RLAPI i32 GetMonitorPhysicalWidth(i32 monitor);                        // Get specified monitor physical width in millimetres
RLAPI i32 GetMonitorPhysicalHeight(i32 monitor);                       // Get specified monitor physical height in millimetres
RLAPI i32 GetMonitorRefreshRate(i32 monitor);                          // Get specified monitor refresh rate
RLAPI vec2 GetWindowPosition();                                        // Get window position XY on monitor
RLAPI vec2 GetWindowScaleDPI();                                        // Get window scale DPI factor
RLAPI std::string GetMonitorName(i32 monitor);                         // Get the human-readable, UTF-8 encoded name of the specified monitor
RLAPI void SetClipboardText(std::string_view text);                    // Set clipboard text content
RLAPI std::string GetClipboardText();                                  // Get clipboard text content
RLAPI Image GetClipboardImage();                                       // Get clipboard image content
RLAPI void EnableEventWaiting();                                       // Enable waiting for events on EndDrawing(), no automatic event polling
RLAPI void DisableEventWaiting();                                      // Disable waiting for events on EndDrawing(), automatic events polling

// Cursor-related functions
RLAPI void ShowCursor();                                      // Show cursor
RLAPI void HideCursor();                                      // Hide cursor
RLAPI bool IsCursorHidden();                                  // Check if cursor is not visible
RLAPI void EnableCursor();                                    // Enable cursor (unlock cursor)
RLAPI void DisableCursor();                                   // Disable cursor (lock cursor)
RLAPI bool IsCursorOnScreen();                                // Check if cursor is on the screen

// Drawing-related functions
RLAPI void ClearBackground(Color color);                          // Clear background (framebuffer) to color
RLAPI void BeginDrawing();                                        // Begin canvas (framebuffer) drawing
RLAPI void EndDrawing();                                          // End canvas (framebuffer) drawing and swap buffers (double buffering)
RLAPI void BeginMode2D(Camera2D camera);                          // Begin 2D mode with custom camera (2D)
RLAPI void EndMode2D();                                           // End 2D mode with custom camera
RLAPI void BeginMode3D(Camera3D camera);                          // Begin 3D mode with custom camera (3D)
RLAPI void EndMode3D();                                           // End 3D mode and returns to default 2D orthographic mode
RLAPI void BeginTextureMode(RenderTexture2D target);              // Begin drawing to render texture
RLAPI void EndTextureMode();                                      // End drawing to render texture
RLAPI void BeginShaderMode(Shader shader);                        // Begin custom shader drawing
RLAPI void EndShaderMode();                                       // End custom shader drawing (use default shader)
RLAPI void BeginBlendMode(i32 mode);                              // Begin blending mode (alpha, additive, multiplied, subtract, custom)
RLAPI void EndBlendMode();                                        // End blending mode (reset to default: alpha blending)
RLAPI void BeginScissorMode(i32 x, i32 y, i32 width, i32 height); // Begin scissor mode (define screen area for following drawing)
RLAPI void EndScissorMode();                                      // End scissor mode
RLAPI void BeginVrStereoMode(VrStereoConfig config);              // Begin stereo rendering (requires VR simulator)
RLAPI void EndVrStereoMode();                                     // End stereo rendering (requires VR simulator)

// VR stereo config functions for VR simulator
RLAPI VrStereoConfig LoadVrStereoConfig(VrDeviceInfo device);     // Load VR stereo config for VR simulator device parameters
RLAPI void UnloadVrStereoConfig(VrStereoConfig config);           // Unload VR stereo config

// Shader management functions
// NOTE: Shader functionality is not available on OpenGL 1.1
RLAPI Shader LoadShader(std::string_view vsFileName, std::string_view fsFileName);                      // Load shader from files and bind default locations
RLAPI Shader LoadShaderFromMemory(std::string_view vsCode, std::string_view fsCode);                    // Load shader from code strings and bind default locations
RLAPI bool IsShaderValid(Shader shader);                                                                // Check if shader is valid (loaded on GPU)
RLAPI i32 GetShaderLocation(Shader shader, std::string_view uniformName);                               // Get shader uniform location
RLAPI i32 GetShaderLocationAttrib(Shader shader, std::string_view attribName);                          // Get shader attribute location
RLAPI void SetShaderValue(Shader shader, i32 locIndex, const void *value, i32 uniformType);             // Set shader uniform value
RLAPI void SetShaderValueV(Shader shader, i32 locIndex, const void *value, i32 uniformType, i32 count); // Set shader uniform value vector
RLAPI void SetShaderValueMatrix(Shader shader, i32 locIndex, mat4 mat);                                 // Set shader uniform value (matrix 4x4)
RLAPI void SetShaderValueTexture(Shader shader, i32 locIndex, Texture2D texture);                       // Set shader uniform value and bind the texture (sampler2d)
RLAPI void UnloadShader(Shader shader);                                                                 // Unload shader from GPU memory (VRAM)

// Screen-space-related functions
RLAPI Ray GetScreenToWorldRay(vec2 position, Camera camera);                          // Get a ray trace from screen position (i.e mouse)
RLAPI Ray GetScreenToWorldRayEx(vec2 position, Camera camera, i32 width, i32 height); // Get a ray trace from screen position (i.e mouse) in a viewport
RLAPI vec2 GetWorldToScreen(vec3 position, Camera camera);                            // Get screen space position for a 3d world space position
RLAPI vec2 GetWorldToScreenEx(vec3 position, Camera camera, i32 width, i32 height);   // Get sized screen space position for a 3d world space position
RLAPI vec2 GetWorldToScreen2D(vec2 position, Camera2D camera);                        // Get screen space position for a 2d camera world space position
RLAPI vec2 GetScreenToWorld2D(vec2 position, Camera2D camera);                        // Get world space position for a 2d camera screen space position
RLAPI mat4 GetCameraMatrix(Camera camera);                                            // Get camera transform matrix (view matrix)
RLAPI mat4 GetCameraMatrix2D(Camera2D camera);                                        // Get camera 2d transform matrix

// Timing-related functions
RLAPI void SetTargetFPS(i32 fps);                       // Set target FPS (maximum)
RLAPI f32 GetFrameTime();                             // Get time in seconds for last frame drawn (delta time)
RLAPI f64 GetTime();                                 // Get elapsed time in seconds since InitWindow()
RLAPI i32 GetFPS();                                     // Get current FPS

// Custom frame control functions
// NOTE: Those functions are intended for advanced users that want full control over the frame processing
// By default EndDrawing() does this job: draws everything + SwapScreenBuffer() + manage frame timing + PollInputEvents()
// To avoid that behaviour and control frame processes manually, enable in config.h: SUPPORT_CUSTOM_FRAME_CONTROL
RLAPI void SwapScreenBuffer();                          // Swap back buffer with front buffer (screen drawing)
RLAPI void PollInputEvents();                           // Register all input events
RLAPI void WaitTime(f64 seconds);                    // Wait for some time (halt program execution)

// Random values generation functions
RLAPI void SetRandomSeed(u32 seed);                         // Set the seed for the random number generator
RLAPI i32 GetRandomValue(i32 min, i32 max);                          // Get a random value between min and max (both included)
RLAPI i32 *LoadRandomSequence(u32 count, i32 min, i32 max); // Load random values sequence, no values repeated
RLAPI void UnloadRandomSequence(i32 *sequence);                      // Unload random values sequence

// Misc. functions
RLAPI void TakeScreenshot(std::string_view fileName);                // Takes a screenshot of current screen (filename extension defines format)
RLAPI void SetConfigFlags(u32 flags);                       // Set init configuration flags (view FLAGS)
RLAPI void OpenURL(std::string_view url);                            // Open URL with default system browser (if available)

// Logging system
RLAPI void SetTraceLogLevel(i32 logLevel);                           // Set the current threshold (minimum) log level
RLAPI void TraceLog(i32 logLevel, std::string_view text, ...);       // Show trace log messages (LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERROR...)
RLAPI void SetTraceLogCallback(TraceLogCallback callback);           // Set custom trace log

// Memory management, using internal allocators
RLAPI void *MemAlloc(u32 size);                        // Internal memory allocator
RLAPI void *MemRealloc(void *ptr, u32 size);           // Internal memory reallocator
RLAPI void MemFree(void *ptr);                                  // Internal memory free

// File system management functions
RLAPI u8 *LoadFileData(std::string_view fileName, i32 *dataSize);                     // Load file data as byte array (read)
RLAPI void UnloadFileData(u8 *data);                                                  // Unload file data allocated by LoadFileData()
RLAPI bool SaveFileData(std::string_view fileName, const void *data, i32 dataSize);              // Save data to file from byte array (write), returns true on success
RLAPI bool ExportDataAsCode(const u8 *data, i32 dataSize, std::string_view fileName); // Export data to code (.h), returns true on success
RLAPI std::string LoadFileText(std::string_view fileName);                                       // Load text data from file (read)
RLAPI bool SaveFileText(std::string_view fileName, std::string_view text);                       // Save text data to file (write), returns true on success

// File access custom callbacks
// WARNING: Callbacks setup is intended for advanced users
RLAPI void SetLoadFileDataCallback(LoadFileDataCallback callback);  // Set custom file binary data loader
RLAPI void SetSaveFileDataCallback(SaveFileDataCallback callback);  // Set custom file binary data saver
RLAPI void SetLoadFileTextCallback(LoadFileTextCallback callback);  // Set custom file text data loader
RLAPI void SetSaveFileTextCallback(SaveFileTextCallback callback);  // Set custom file text data saver

RLAPI i32 FileRename(std::string_view fileName, std::string_view fileRename);                                     // Rename file (if exists), returns 0 on success
RLAPI i32 FileRemove(std::string_view fileName);                                                                  // Remove file (if exists), returns 0 on success
RLAPI i32 FileCopy(std::string_view srcPath, std::string_view dstPath);                                           // Copy file from one path to another, dstPath created if it doesn't exist, returns 0 on success
RLAPI i32 FileMove(std::string_view srcPath, std::string_view dstPath);                                           // Move file from one directory to another, dstPath created if it doesn't exist, returns 0 on success
RLAPI i32 FileTextReplace(std::string_view fileName, std::string_view search, std::string_view replacement);      // Replace text in an existing file, returns 0 on success
RLAPI i32 FileTextFindIndex(std::string_view fileName, std::string_view search);                                  // Find text in existing file, returns -1 if index not found or index otherwise
RLAPI bool FileExists(std::string_view fileName);                                                                 // Check if file exists
RLAPI bool DirectoryExists(std::string_view dirPath);                                                             // Check if directory path exists
RLAPI bool IsFileExtension(std::string_view fileName, std::string_view ext);                                      // Check file extension (recommended include point: .png, .wav)
RLAPI bool IsFileHidden(std::string_view filePath);                                                               // Check if file path (file or directory) is hidden by OS
RLAPI i32 GetFileLength(std::string_view fileName);                                                               // Get file length in bytes (NOTE: GetFileSize() conflicts with windows.h)
RLAPI long GetFileModTime(std::string_view fileName);                                                             // Get file modification time (last write time)
RLAPI std::string GetFileExtension(std::string_view fileName);                                                    // Get file extension for a filename string (includes dot: '.png')
RLAPI std::string GetFileName(std::string_view filePath);                                                         // Get filename for a path string
RLAPI std::string GetFileNameWithoutExt(std::string_view filePath);                                               // Get filename string without extension
RLAPI std::string GetDirectoryPath(std::string_view filePath);                                                    // Get full path for a provided fileName with path
RLAPI std::string GetPrevDirectoryPath(std::string_view dirPath);                                                 // Get previous directory path for a provided path
RLAPI std::string GetWorkingDirectory();                                                                          // Get current working directory
RLAPI std::string GetApplicationDirectory();                                                                      // Get the directory of the running application
RLAPI i32 MakeDirectory(std::string_view dirPath);                                                                // Create directories (including full path requested), returns 0 on success
RLAPI i32 ChangeDirectory(std::string_view dirPath);                                                              // Change working directory, returns 0 on success
RLAPI bool IsPathFile(std::string_view path);                                                                     // Check if provided path points to a file
RLAPI bool IsPathDirectory(std::string_view path);                                                                // Check if provided path points to a directory
RLAPI bool IsPathAbsolute(std::string_view path);                                                                 // Check if provided path is an absolute path
RLAPI bool IsFileNameValid(std::string_view fileName);                                                            // Check if fileName is valid for the platform/OS
RLAPI FilePathList LoadDirectoryFiles(std::string_view dirPath);                                                  // Load directory filepaths, files and directories, no subdirs scan
RLAPI FilePathList LoadDirectoryFilesEx(std::string_view basePath, std::string_view filter, bool scanSubdirs);    // Load directory filepaths with extension filtering and subdir scan; some filters available: '*.*','FILES*','DIRS*'
RLAPI bool IsFileDropped();                                                                                       // Check if file has been dropped into window
RLAPI FilePathList LoadDroppedFiles();                                                                            // Load dropped filepaths
RLAPI u32 GetDirectoryFileCount(std::string_view dirPath);                                               // Get the file count in a directory
RLAPI u32 GetDirectoryFileCountEx(std::string_view basePath, std::string_view filter, bool scanSubdirs); // Get the file count in a directory with extension filtering and recursive directory scan. Use 'DIR' in the filter string to include directories in the result

// Compression/Encoding functionality
RLAPI u8 *CompressData(const u8 *data, i32 dataSize, i32 *compDataSize);              // Compress data (DEFLATE algorithm), memory must be MemFree()
RLAPI u8 *DecompressData(const u8 *compData, i32 compDataSize, i32 *dataSize);        // Decompress data (DEFLATE algorithm), memory must be MemFree()
RLAPI std::string EncodeDataBase64(const u8 *data, i32 dataSize, i32 *outputSize);               // Encode data to Base64 string
RLAPI u8 *DecodeDataBase64(std::string_view text, i32 *outputSize);                              // Decode Base64 string
RLAPI u32 ComputeCRC32(const u8 *data, i32 dataSize);                                   // Compute CRC32 hash code
RLAPI u32 *ComputeMD5(const u8 *data, i32 dataSize);                                    // Compute MD5 hash code, returns static int[4] (16 bytes)
RLAPI u32 *ComputeSHA1(const u8 *data, i32 dataSize);                                   // Compute SHA1 hash code, returns static int[5] (20 bytes)
RLAPI u32 *ComputeSHA256(const u8 *data, i32 dataSize);                                 // Compute SHA256 hash code, returns static int[8] (32 bytes)

// Automation events functionality
RLAPI AutomationEventList LoadAutomationEventList(std::string_view fileName);              // Load automation events list from file, empty view for an empty list, capacity = MAX_AUTOMATION_EVENTS
RLAPI void UnloadAutomationEventList(AutomationEventList list);                            // Unload automation events list from file
RLAPI bool ExportAutomationEventList(AutomationEventList list, std::string_view fileName); // Export automation events list as text file
RLAPI void SetAutomationEventList(AutomationEventList *list);                              // Set automation event list to record to
RLAPI void SetAutomationEventBaseFrame(i32 frame);                                         // Set automation event internal base frame to start recording
RLAPI void StartAutomationEventRecording();                                                // Start recording automation events (AutomationEventList must be set)
RLAPI void StopAutomationEventRecording();                                                 // Stop recording automation events
RLAPI void PlayAutomationEvent(AutomationEvent event);                                     // Play a recorded automation event

//------------------------------------------------------------------------------------
// Input Handling Functions (Module: core)
//------------------------------------------------------------------------------------

// Input-related functions: keyboard
RLAPI bool IsKeyPressed(i32 key);                             // Check if key has been pressed once
RLAPI bool IsKeyPressedRepeat(i32 key);                       // Check if key has been pressed again
RLAPI bool IsKeyDown(i32 key);                                // Check if key is being pressed
RLAPI bool IsKeyReleased(i32 key);                            // Check if key has been released once
RLAPI bool IsKeyUp(i32 key);                                  // Check if key is NOT being pressed
RLAPI i32 GetKeyPressed();                                    // Get key pressed (keycode), call it multiple times for keys queued, returns 0 when the queue is empty
RLAPI i32 GetCharPressed();                                   // Get char pressed (unicode), call it multiple times for chars queued, returns 0 when the queue is empty
RLAPI std::string GetKeyName(i32 key);                        // Get name of a QWERTY key on the current keyboard layout (eg returns string 'q' for KEY_A on an AZERTY keyboard)
RLAPI void SetExitKey(i32 key);                               // Set a custom key to exit program (default is ESC)

// Input-related functions: gamepads
RLAPI bool IsGamepadAvailable(i32 gamepad);                                                     // Check if gamepad is available
RLAPI std::string GetGamepadName(i32 gamepad);                                                  // Get gamepad internal name id
RLAPI bool IsGamepadButtonPressed(i32 gamepad, i32 button);                                     // Check if gamepad button has been pressed once
RLAPI bool IsGamepadButtonDown(i32 gamepad, i32 button);                                        // Check if gamepad button is being pressed
RLAPI bool IsGamepadButtonReleased(i32 gamepad, i32 button);                                    // Check if gamepad button has been released once
RLAPI bool IsGamepadButtonUp(i32 gamepad, i32 button);                                          // Check if gamepad button is NOT being pressed
RLAPI i32 GetGamepadButtonPressed();                                                            // Get the last gamepad button pressed
RLAPI i32 GetGamepadAxisCount(i32 gamepad);                                                     // Get axis count for a gamepad
RLAPI f32 GetGamepadAxisMovement(i32 gamepad, i32 axis);                                      // Get movement value for a gamepad axis
RLAPI i32 SetGamepadMappings(std::string_view mappings);                                        // Set internal gamepad mappings (SDL_GameControllerDB)
RLAPI void SetGamepadVibration(i32 gamepad, f32 leftMotor, f32 rightMotor, f32 duration); // Set gamepad vibration for both motors (duration in seconds)

// Input-related functions: mouse
RLAPI bool IsMouseButtonPressed(i32 button);                  // Check if mouse button has been pressed once
RLAPI bool IsMouseButtonDown(i32 button);                     // Check if mouse button is being pressed
RLAPI bool IsMouseButtonReleased(i32 button);                 // Check if mouse button has been released once
RLAPI bool IsMouseButtonUp(i32 button);                       // Check if mouse button is NOT being pressed
RLAPI i32 GetMouseX();                                        // Get mouse position X
RLAPI i32 GetMouseY();                                        // Get mouse position Y
RLAPI vec2 GetMousePosition();                                // Get mouse position XY
RLAPI vec2 GetMouseDelta();                                   // Get mouse delta between frames
RLAPI void SetMousePosition(i32 x, i32 y);                    // Set mouse position XY
RLAPI void SetMouseOffset(i32 offsetX, i32 offsetY);          // Set mouse offset
RLAPI void SetMouseScale(f32 scaleX, f32 scaleY);         // Set mouse scaling
RLAPI f32 GetMouseWheelMove();                              // Get mouse wheel movement for X or Y, whichever is larger
RLAPI vec2 GetMouseWheelMoveV();                              // Get mouse wheel movement for both X and Y
RLAPI void SetMouseCursor(i32 cursor);                        // Set mouse cursor

// Input-related functions: touch
RLAPI i32 GetTouchX();                                        // Get touch position X for touch point 0 (relative to screen size)
RLAPI i32 GetTouchY();                                        // Get touch position Y for touch point 0 (relative to screen size)
RLAPI vec2 GetTouchPosition(i32 index);                       // Get touch position XY for a touch point index (relative to screen size)
RLAPI i32 GetTouchPointId(i32 index);                         // Get touch point identifier for provided index
RLAPI i32 GetTouchPointCount();                               // Get number of touch points

//------------------------------------------------------------------------------------
// Gestures and Touch Handling Functions (Module: rgestures)
//------------------------------------------------------------------------------------
RLAPI void SetGesturesEnabled(u32 flags);            // Enable a set of gestures using flags
RLAPI bool IsGestureDetected(u32 gesture);           // Check if gesture has been detected
RLAPI i32 GetGestureDetected();                               // Get latest detected gesture
RLAPI f32 GetGestureHoldDuration();                         // Get gesture hold time in seconds
RLAPI vec2 GetGestureDragVector();                            // Get gesture drag vector
RLAPI f32 GetGestureDragAngle();                            // Get gesture drag angle
RLAPI vec2 GetGesturePinchVector();                           // Get gesture pinch delta
RLAPI f32 GetGesturePinchAngle();                           // Get gesture pinch angle

//------------------------------------------------------------------------------------
// Camera System Functions (Module: rcamera)
//------------------------------------------------------------------------------------
RLAPI void UpdateCamera(Camera *camera, i32 mode);                                    // Update camera position for selected mode
RLAPI void UpdateCameraPro(Camera *camera, vec3 movement, vec3 rotation, f32 zoom); // Update camera movement/rotation

//------------------------------------------------------------------------------------
// Basic Shapes Drawing Functions (Module: shapes)
//------------------------------------------------------------------------------------
// Set texture and rectangle to be used on shapes drawing
// NOTE: It can be useful when using basic shapes and one single font,
// defining a font char white rectangle would allow drawing everything in a single draw call
RLAPI void SetShapesTexture(Texture2D texture, Rectangle rec); // Set texture and rectangle to be used on shapes drawing
RLAPI Texture2D GetShapesTexture();                            // Get texture that is used for shapes drawing
RLAPI Rectangle GetShapesTextureRectangle();                   // Get texture source rectangle that is used for shapes drawing

// Basic shapes drawing functions
RLAPI void DrawPixel(i32 posX, i32 posY, Color color);                                                                                                   // Draw a pixel using geometry [Can be slow, use with care]
RLAPI void DrawPixelV(vec2 position, Color color);                                                                                                       // Draw a pixel using geometry (Vector version) [Can be slow, use with care]
RLAPI void DrawLine(i32 startPosX, i32 startPosY, i32 endPosX, i32 endPosY, Color color);                                                                // Draw a line
RLAPI void DrawLineV(vec2 startPos, vec2 endPos, Color color);                                                                                           // Draw a line (using gl lines)
RLAPI void DrawLineEx(vec2 startPos, vec2 endPos, f32 thick, Color color);                                                                             // Draw a line (using triangles/quads)
RLAPI void DrawLineStrip(const vec2 *points, i32 pointCount, Color color);                                                                               // Draw lines sequence (using gl lines)
RLAPI void DrawLineBezier(vec2 startPos, vec2 endPos, f32 thick, Color color);                                                                         // Draw line segment cubic-bezier in-out interpolation
RLAPI void DrawLineDashed(vec2 startPos, vec2 endPos, i32 dashSize, i32 spaceSize, Color color);                                                         // Draw a dashed line
RLAPI void DrawTriangle(vec2 v1, vec2 v2, vec2 v3, Color color);                                                                                         // Draw a color-filled triangle, counter-clockwise vertex order
RLAPI void DrawTriangleGradient(vec2 v1, vec2 v2, vec2 v3, Color c1, Color c2, Color c3);                                                                // Draw triangle with interpolated colors, counter-clockwise vertex/color order
RLAPI void DrawTriangleLines(vec2 v1, vec2 v2, vec2 v3, Color color);                                                                                    // Draw triangle outline, counter-clockwise vertex order
RLAPI void DrawTriangleLinesEx(vec2 v1, vec2 v2, vec2 v3, f32 thick, Color color);                                                                     // Draw triangle outline with line thickness, counter-clockwise vertex order
RLAPI void DrawTriangleFan(const vec2 *points, i32 pointCount, Color color);                                                                             // Draw a triangle fan defined by points (first vertex is the center)
RLAPI void DrawTriangleStrip(const vec2 *points, i32 pointCount, Color color);                                                                           // Draw a triangle strip defined by points
RLAPI void DrawRectangle(i32 posX, i32 posY, i32 width, i32 height, Color color);                                                                        // Draw a color-filled rectangle
RLAPI void DrawRectangleV(vec2 position, vec2 size, Color color);                                                                                        // Draw a color-filled rectangle (Vector version)
RLAPI void DrawRectangleRec(Rectangle rec, Color color);                                                                                                 // Draw a color-filled rectangle
RLAPI void DrawRectanglePro(Rectangle rec, vec2 origin, f32 rotation, Color color);                                                                    // Draw a color-filled rectangle with pro parameters
RLAPI void DrawRectangleGradientV(i32 posX, i32 posY, i32 width, i32 height, Color top, Color bottom);                                                   // Draw a vertical-gradient-filled rectangle
RLAPI void DrawRectangleGradientH(i32 posX, i32 posY, i32 width, i32 height, Color left, Color right);                                                   // Draw a horizontal-gradient-filled rectangle
RLAPI void DrawRectangleGradientEx(Rectangle rec, Color col1, Color col2, Color col3, Color col4);                                                       // Draw a gradient-filled rectangle with custom vertex colors, counter-clockwise color order
RLAPI void DrawRectangleLines(i32 posX, i32 posY, i32 width, i32 height, Color color);                                                                   // Draw rectangle outline
RLAPI void DrawRectangleLinesEx(Rectangle rec, f32 thick, Color color);                                                                                // Draw rectangle outline with line thickness
RLAPI void DrawRectangleRounded(Rectangle rec, f32 roundness, i32 segments, Color color);                                                              // Draw rectangle with rounded edges
RLAPI void DrawRectangleRoundedLines(Rectangle rec, f32 roundness, i32 segments, Color color);                                                         // Draw rectangle lines with rounded edges
RLAPI void DrawRectangleRoundedLinesEx(Rectangle rec, f32 roundness, i32 segments, f32 thick, Color color);                                          // Draw rectangle lines with rounded edges outline and line thickness
RLAPI void DrawPoly(vec2 center, i32 sides, f32 radius, f32 rotation, Color color);                                                                  // Draw a polygon of n sides
RLAPI void DrawPolyLines(vec2 center, i32 sides, f32 radius, f32 rotation, Color color);                                                             // Draw a polygon outline of n sides
RLAPI void DrawPolyLinesEx(vec2 center, i32 sides, f32 radius, f32 rotation, f32 thick, Color color);                                              // Draw a polygon outline of n sides with line thickness
RLAPI void DrawCircle(i32 centerX, i32 centerY, f32 radius, Color color);                                                                              // Draw a color-filled circle
RLAPI void DrawCircleV(vec2 center, f32 radius, Color color);                                                                                          // Draw a color-filled circle (Vector version)
RLAPI void DrawCircleGradient(vec2 center, f32 radius, Color inner, Color outer);                                                                      // Draw a gradient-filled circle
RLAPI void DrawCircleSector(vec2 center, f32 radius, f32 startAngle, f32 endAngle, i32 segments, Color color);                                     // Draw a piece of a circle
RLAPI void DrawCircleSectorLines(vec2 center, f32 radius, f32 startAngle, f32 endAngle, i32 segments, Color color);                                // Draw circle sector outline
RLAPI void DrawCircleSectorLinesEx(vec2 center, f32 radius, f32 startAngle, f32 endAngle, i32 segments, f32 thick, Color color);                 // Draw circle sector outline with thickness
RLAPI void DrawCircleLines(i32 centerX, i32 centerY, f32 radius, Color color);                                                                         // Draw circle outline
RLAPI void DrawCircleLinesV(vec2 center, f32 radius, Color color);                                                                                     // Draw circle outline (Vector version)
RLAPI void DrawCircleLinesEx(vec2 center, f32 radius, f32 thick, Color color);                                                                       // Draw circle outline with line thickness
RLAPI void DrawEllipse(i32 centerX, i32 centerY, f32 radiusH, f32 radiusV, Color color);                                                             // Draw ellipse
RLAPI void DrawEllipseV(vec2 center, f32 radiusH, f32 radiusV, Color color);                                                                         // Draw ellipse (Vector version)
RLAPI void DrawEllipseLines(i32 centerX, i32 centerY, f32 radiusH, f32 radiusV, Color color);                                                        // Draw ellipse outline
RLAPI void DrawEllipseLinesV(vec2 center, f32 radiusH, f32 radiusV, Color color);                                                                    // Draw ellipse outline (Vector version)
RLAPI void DrawEllipseLinesEx(vec2 center, f32 radiusH, f32 radiusV, f32 thick, Color color);                                                      // Draw ellipse outline with line thickness
RLAPI void DrawRing(vec2 center, f32 innerRadius, f32 outerRadius, f32 startAngle, f32 endAngle, i32 segments, Color color);                     // Draw ring
RLAPI void DrawRingLines(vec2 center, f32 innerRadius, f32 outerRadius, f32 startAngle, f32 endAngle, i32 segments, Color color);                // Draw ring outline
RLAPI void DrawRingLinesEx(vec2 center, f32 innerRadius, f32 outerRadius, f32 startAngle, f32 endAngle, i32 segments, f32 thick, Color color); // Draw ring outline with line thickness

// Splines drawing functions
RLAPI void DrawSplineLinear(const vec2 *points, i32 pointCount, f32 thick, Color color);             // Draw spline: Linear, minimum 2 points
RLAPI void DrawSplineBasis(const vec2 *points, i32 pointCount, f32 thick, Color color);              // Draw spline: B-Spline, minimum 4 points
RLAPI void DrawSplineCatmullRom(const vec2 *points, i32 pointCount, f32 thick, Color color);         // Draw spline: Catmull-Rom, minimum 4 points
RLAPI void DrawSplineBezierQuadratic(const vec2 *points, i32 pointCount, f32 thick, Color color);    // Draw spline: Quadratic Bezier, minimum 3 points (1 control point): [p1, c2, p3, c4...]
RLAPI void DrawSplineBezierCubic(const vec2 *points, i32 pointCount, f32 thick, Color color);        // Draw spline: Cubic Bezier, minimum 4 points (2 control points): [p1, c2, c3, p4, c5, c6...]
RLAPI void DrawSplineSegmentLinear(vec2 p1, vec2 p2, f32 thick, Color color);                        // Draw spline segment: Linear, 2 points
RLAPI void DrawSplineSegmentBasis(vec2 p1, vec2 p2, vec2 p3, vec2 p4, f32 thick, Color color);       // Draw spline segment: B-Spline, 4 points
RLAPI void DrawSplineSegmentCatmullRom(vec2 p1, vec2 p2, vec2 p3, vec2 p4, f32 thick, Color color);  // Draw spline segment: Catmull-Rom, 4 points
RLAPI void DrawSplineSegmentBezierQuadratic(vec2 p1, vec2 c2, vec2 p3, f32 thick, Color color);      // Draw spline segment: Quadratic Bezier, 2 points, 1 control point
RLAPI void DrawSplineSegmentBezierCubic(vec2 p1, vec2 c2, vec2 c3, vec2 p4, f32 thick, Color color); // Draw spline segment: Cubic Bezier, 2 points, 2 control points

// Spline segment point evaluation functions, for a provided t [0.0f .. 1.0f]
RLAPI vec2 GetSplinePointLinear(vec2 startPos, vec2 endPos, f32 t);                         // Get (evaluate) spline point: Linear
RLAPI vec2 GetSplinePointBasis(vec2 p1, vec2 p2, vec2 p3, vec2 p4, f32 t);                  // Get (evaluate) spline point: B-Spline
RLAPI vec2 GetSplinePointCatmullRom(vec2 p1, vec2 p2, vec2 p3, vec2 p4, f32 t);             // Get (evaluate) spline point: Catmull-Rom
RLAPI vec2 GetSplinePointBezierQuadratic(vec2 p1, vec2 c2, vec2 p3, f32 t);                 // Get (evaluate) spline point: Quadratic Bezier
RLAPI vec2 GetSplinePointBezierCubic(vec2 p1, vec2 c2, vec2 c3, vec2 p4, f32 t);            // Get (evaluate) spline point: Cubic Bezier

// Basic shapes collision detection functions
RLAPI bool CheckCollisionRecs(Rectangle rec1, Rectangle rec2);                                                    // Check collision between two rectangles
RLAPI bool CheckCollisionCircles(vec2 center1, f32 radius1, vec2 center2, f32 radius2);                       // Check collision between two circles
RLAPI bool CheckCollisionCircleRec(vec2 center, f32 radius, Rectangle rec);                                     // Check collision between circle and rectangle
RLAPI bool CheckCollisionCircleLine(vec2 center, f32 radius, vec2 p1, vec2 p2);                                 // Check if circle collides with a line created between two points [p1] and [p2]
RLAPI bool CheckCollisionPointRec(vec2 point, Rectangle rec);                                                     // Check if point is inside rectangle
RLAPI bool CheckCollisionPointCircle(vec2 point, vec2 center, f32 radius);                                      // Check if point is inside circle
RLAPI bool CheckCollisionPointTriangle(vec2 point, vec2 p1, vec2 p2, vec2 p3);                                    // Check if point is inside a triangle
RLAPI bool CheckCollisionPointLine(vec2 point, vec2 p1, vec2 p2, i32 threshold);                                  // Check if point belongs to line created between two points [p1] and [p2] with defined margin in pixels [threshold]
RLAPI bool CheckCollisionPointPoly(vec2 point, const vec2 *points, i32 pointCount);                               // Check if point is within a polygon described by array of vertices
RLAPI bool CheckCollisionLines(vec2 startPos1, vec2 endPos1, vec2 startPos2, vec2 endPos2, vec2 *collisionPoint); // Check the collision between two lines defined by two points each, returns collision point by reference
RLAPI Rectangle GetCollisionRec(Rectangle rec1, Rectangle rec2);                                                  // Get collision rectangle for two rectangles collision

//------------------------------------------------------------------------------------
// Texture Loading and Drawing Functions (Module: textures)
//------------------------------------------------------------------------------------

// Image loading functions
// NOTE: These functions do not require GPU access
RLAPI Image LoadImage(std::string_view fileName);                                                                         // Load image from file into CPU memory (RAM)
RLAPI Image LoadImageRaw(std::string_view fileName, i32 width, i32 height, i32 format, i32 headerSize);                   // Load image from RAW file data
RLAPI Image LoadImageAnim(std::string_view fileName, i32 *frames);                                                        // Load image sequence from file (frames appended to image.data)
RLAPI Image LoadImageAnimFromMemory(std::string_view fileType, const u8 *fileData, i32 dataSize, i32 *frames); // Load image sequence from memory buffer
RLAPI Image LoadImageFromMemory(std::string_view fileType, const u8 *fileData, i32 dataSize);                  // Load image from memory buffer, fileType refers to extension: i.e. '.png'
RLAPI Image LoadImageFromTexture(Texture2D texture);                                                                      // Load image from GPU texture data
RLAPI Image LoadImageFromScreen();                                                                                        // Load image from screen buffer (screenshot)
RLAPI bool IsImageValid(Image image);                                                                                     // Check if an image is valid (data and parameters)
RLAPI void UnloadImage(Image image);                                                                                      // Unload image from CPU memory (RAM)
RLAPI bool ExportImage(Image image, std::string_view fileName);                                                           // Export image data to file, returns true on success
RLAPI u8 *ExportImageToMemory(Image image, std::string_view fileType, i32 *fileSize);                          // Export image to memory buffer, memory must be MemFree()
RLAPI bool ExportImageAsCode(Image image, std::string_view fileName);                                                     // Export image as code file defining an array of bytes, returns true on success

// Image generation functions
RLAPI Image GenImageColor(i32 width, i32 height, Color color);                                                // Generate image: plain color
RLAPI Image GenImageGradientLinear(i32 width, i32 height, i32 direction, Color start, Color end);             // Generate image: linear gradient, direction in degrees [0..360], 0=Vertical gradient
RLAPI Image GenImageGradientRadial(i32 width, i32 height, f32 density, Color inner, Color outer);           // Generate image: radial gradient
RLAPI Image GenImageGradientSquare(i32 width, i32 height, f32 density, Color inner, Color outer);           // Generate image: square gradient
RLAPI Image GenImageChecked(i32 width, i32 height, i32 checksX, i32 checksY, Color col1, Color col2);         // Generate image: checked
RLAPI Image GenImageWhiteNoise(i32 width, i32 height, f32 factor);                                          // Generate image: white noise
RLAPI Image GenImagePerlinNoise(i32 width, i32 height, i32 offsetX, i32 offsetY, f32 scale);                // Generate image: perlin noise
RLAPI Image GenImageCellular(i32 width, i32 height, i32 tileSize);                                            // Generate image: cellular algorithm, bigger tileSize means bigger cells
RLAPI Image GenImageText(i32 width, i32 height, std::string_view text);                                       // Generate image: grayscale image from text data

// Image manipulation functions
RLAPI Image ImageCopy(Image image);                                                                            // Create an image duplicate (useful for transformations)
RLAPI Image ImageFromImage(Image image, Rectangle rec);                                                        // Create an image from another image piece
RLAPI Image ImageFromChannel(Image image, i32 selectedChannel);                                                // Create an image from a selected channel of another image (GRAYSCALE)
RLAPI Image ImageText(std::string_view text, i32 fontSize, Color color);                                       // Create an image from text (default font)
RLAPI Image ImageTextEx(Font font, std::string_view text, f32 fontSize, f32 spacing, Color tint);          // Create an image from text (custom sprite font)
RLAPI void ImageFormat(Image *image, i32 newFormat);                                                           // Convert image data to desired format
RLAPI void ImageToPOT(Image *image, Color fill);                                                               // Convert image to POT (power-of-two)
RLAPI void ImageCrop(Image *image, Rectangle crop);                                                            // Crop an image to a defined rectangle
RLAPI void ImageAlphaCrop(Image *image, f32 threshold);                                                      // Crop image depending on alpha value
RLAPI void ImageAlphaClear(Image *image, Color color, f32 threshold);                                        // Clear alpha channel to desired color
RLAPI void ImageAlphaMask(Image *image, Image alphaMask);                                                      // Apply alpha mask to image
RLAPI void ImageAlphaPremultiply(Image *image);                                                                // Premultiply alpha channel
RLAPI void ImageBlurGaussian(Image *image, i32 blurSize);                                                      // Apply Gaussian blur using a box blur approximation
RLAPI void ImageKernelConvolution(Image *image, const f32 *kernel, i32 kernelSize);                          // Apply custom square convolution kernel to image
RLAPI void ImageResize(Image *image, i32 newWidth, i32 newHeight);                                             // Resize image (Bicubic scaling algorithm)
RLAPI void ImageResizeNN(Image *image, i32 newWidth, i32 newHeight);                                           // Resize image (Nearest-Neighbor scaling algorithm)
RLAPI void ImageResizeCanvas(Image *image, i32 newWidth, i32 newHeight, i32 offsetX, i32 offsetY, Color fill); // Resize canvas and fill with color
RLAPI void ImageMipmaps(Image *image);                                                                         // Compute all mipmap levels for a provided image
RLAPI void ImageDither(Image *image, i32 rBpp, i32 gBpp, i32 bBpp, i32 aBpp);                                  // Dither image data to 16bpp or lower (Floyd-Steinberg dithering)
RLAPI void ImageFlipVertical(Image *image);                                                                    // Flip image vertically
RLAPI void ImageFlipHorizontal(Image *image);                                                                  // Flip image horizontally
RLAPI void ImageRotate(Image *image, i32 degrees);                                                             // Rotate image by input angle in degrees (-359 to 359)
RLAPI void ImageRotateCW(Image *image);                                                                        // Rotate image clockwise 90deg
RLAPI void ImageRotateCCW(Image *image);                                                                       // Rotate image counter-clockwise 90deg
RLAPI void ImageColorTint(Image *image, Color color);                                                          // Modify image color: tint
RLAPI void ImageColorInvert(Image *image);                                                                     // Modify image color: invert
RLAPI void ImageColorGrayscale(Image *image);                                                                  // Modify image color: grayscale
RLAPI void ImageColorContrast(Image *image, i32 contrast);                                                     // Modify image color: contrast (-100 to 100)
RLAPI void ImageColorBrightness(Image *image, i32 brightness);                                                 // Modify image color: brightness (-255 to 255)
RLAPI void ImageColorReplace(Image *image, Color color, Color replace);                                        // Modify image color: replace color
RLAPI Color *LoadImageColors(Image image);                                                                     // Load color data from image as a Color array (RGBA - 32bit)
RLAPI Color *LoadImagePalette(Image image, i32 maxPaletteSize, i32 *colorCount);                               // Load colors palette from image as a Color array (RGBA - 32bit)
RLAPI void UnloadImageColors(Color *colors);                                                                   // Unload color data loaded with LoadImageColors()
RLAPI void UnloadImagePalette(Color *colors);                                                                  // Unload colors palette loaded with LoadImagePalette()
RLAPI Rectangle GetImageAlphaBorder(Image image, f32 threshold);                                             // Get image alpha border rectangle
RLAPI Color GetImageColor(Image image, i32 x, i32 y);                                                          // Get image pixel color at (x, y) position

// Image drawing functions
// NOTE: Image software-rendering functions (CPU)
RLAPI void ImageClearBackground(Image *dst, Color color);                                                           // Clear image background with provided color
RLAPI void ImageDrawPixel(Image *dst, i32 posX, i32 posY, Color color);                                             // Draw pixel within an image
RLAPI void ImageDrawPixelV(Image *dst, vec2 position, Color color);                                                 // Draw pixel within an image (Vector version)
RLAPI void ImageDrawLine(Image *dst, i32 startPosX, i32 startPosY, i32 endPosX, i32 endPosY, Color color);          // Draw line within an image
RLAPI void ImageDrawLineV(Image *dst, vec2 start, vec2 end, Color color);                                           // Draw line within an image (Vector version)
RLAPI void ImageDrawLineEx(Image *dst, vec2 start, vec2 end, i32 thick, Color color);                               // Draw a line defining thickness within an image
RLAPI void ImageDrawLineStrip(Image *dst, const vec2 *points, i32 pointCount, Color color);                         // Draw a lines sequence within an image
RLAPI void ImageDrawTriangle(Image *dst, vec2 v1, vec2 v2, vec2 v3, Color color);                                   // Draw triangle within an image
RLAPI void ImageDrawTriangleGradient(Image *dst, vec2 v1, vec2 v2, vec2 v3, Color c1, Color c2, Color c3);          // Draw triangle with interpolated colors within an image
RLAPI void ImageDrawTriangleLines(Image *dst, vec2 v1, vec2 v2, vec2 v3, Color color);                              // Draw triangle outline within an image
RLAPI void ImageDrawTriangleFan(Image *dst, const vec2 *points, i32 pointCount, Color color);                       // Draw a triangle fan defined by points within an image (first vertex is the center)
RLAPI void ImageDrawTriangleStrip(Image *dst, const vec2 *points, i32 pointCount, Color color);                     // Draw a triangle strip defined by points within an image
RLAPI void ImageDrawRectangle(Image *dst, i32 posX, i32 posY, i32 width, i32 height, Color color);                  // Draw rectangle within an image
RLAPI void ImageDrawRectangleV(Image *dst, vec2 position, vec2 size, Color color);                                  // Draw rectangle within an image (Vector version)
RLAPI void ImageDrawRectangleRec(Image *dst, Rectangle rec, Color color);                                           // Draw rectangle within an image
RLAPI void ImageDrawRectanglePro(Image *dst, Rectangle rec, vec2 origin, f32 rotation, Color color);              // Draw a color-filled rectangle with pro parameters within and image
RLAPI void ImageDrawRectangleLines(Image *dst, i32 posX, i32 posY, i32 width, i32 height, Color color);             // Draw rectangle lines within an image
RLAPI void ImageDrawRectangleLinesEx(Image *dst, Rectangle rec, i32 thick, Color color);                            // Draw rectangle lines within an image with line thickness
RLAPI void ImageDrawRectangleGradientEx(Image *dst, Rectangle rec, Color col1, Color col2, Color col3, Color col4); // Draw rectangle with gradient colors within an image, counter-clockwise color order
RLAPI void ImageDrawCircle(Image *dst, i32 centerX, i32 centerY, i32 radius, Color color);                          // Draw a filled circle within an image
RLAPI void ImageDrawCircleV(Image *dst, vec2 center, i32 radius, Color color);                                      // Draw a filled circle within an image (Vector version)
RLAPI void ImageDrawCircleLines(Image *dst, i32 centerX, i32 centerY, i32 radius, Color color);                     // Draw circle outline within an image
RLAPI void ImageDrawCircleLinesV(Image *dst, vec2 center, i32 radius, Color color);                                 // Draw circle outline within an image (Vector version)
RLAPI void ImageDrawCircleGradient(Image *dst, vec2 center, f32 radius, Color inner, Color outer);                // Draw a gradient-filled circle within an image

RLAPI void ImageDrawImage(Image *dst, Image src, i32 posX, i32 posY, Color tint);                                                                                 // Draw an image within an image
RLAPI void ImageDrawImageEx(Image *dst, Image src, vec2 position, f32 rotation, f32 scale, Color tint);                                                       // Draw an image with scaling and rotation within an image
RLAPI void ImageDrawImageRec(Image *dst, Image src, Rectangle srcRec, vec2 position, Color tint);                                                                 // Draw a part of an image defined by a rectangle within an image
RLAPI void ImageDrawImagePro(Image *dst, Image src, Rectangle srcRec, Rectangle dstRec, vec2 origin, f32 rotation, Color tint);                                 // Draw a part of an image defined by a rectangle into destination rectangle, with scaling and rotation, within an image
RLAPI void ImageDrawText(Image *dst, std::string_view text, i32 posX, i32 posY, i32 fontSize, Color color);                                                       // Draw text (using default font) within an image (destination)
RLAPI void ImageDrawTextEx(Image *dst, Font font, std::string_view text, vec2 position, f32 fontSize, f32 spacing, Color tint);                               // Draw text (custom sprite font) within an image (destination)
RLAPI void ImageDrawTextPro(Image *dst, Font font, std::string_view text, vec2 position, vec2 origin, f32 rotation, f32 fontSize, f32 spacing, Color tint); // Draw text using Font and pro parameters (rotation)

// Texture loading functions
// NOTE: These functions require GPU access
RLAPI Texture2D LoadTexture(std::string_view fileName);                                                       // Load texture from file into GPU memory (VRAM)
RLAPI Texture2D LoadTextureFromImage(Image image);                                                            // Load texture from image data
RLAPI TextureCubemap LoadTextureCubemap(Image image, i32 layout);                                             // Load cubemap from image, multiple image cubemap layouts supported
RLAPI RenderTexture2D LoadRenderTexture(i32 width, i32 height);                                               // Load texture for rendering (framebuffer)
RLAPI RenderTexture2D LoadRenderTextureEx(i32 width, i32 height, i32 format);                                 // Load texture for rendering (framebuffer), with specific format
RLAPI bool IsTextureValid(Texture2D texture);                                                                 // Check if texture is valid (loaded in GPU)
RLAPI void UnloadTexture(Texture2D texture);                                                                  // Unload texture from GPU memory (VRAM)
RLAPI bool IsRenderTextureValid(RenderTexture2D target);                                                      // Check if render texture is valid (loaded in GPU)
RLAPI void UnloadRenderTexture(RenderTexture2D target);                                                       // Unload render texture from GPU memory (VRAM)
RLAPI void UpdateTexture(Texture2D texture, const void *pixels);                                              // Update GPU texture with new data (pixels should be able to fill texture)
RLAPI void UpdateTextureRec(Texture2D texture, Rectangle rec, const void *pixels);                            // Update GPU texture rectangle with new data (pixels and rec should fit in texture)

// Texture configuration functions
RLAPI void GenTextureMipmaps(Texture2D *texture);                                                        // Generate GPU mipmaps for a texture
RLAPI void SetTextureFilter(Texture2D texture, i32 filter);                                              // Set texture scaling filter mode
RLAPI void SetTextureWrap(Texture2D texture, i32 wrap);                                                  // Set texture wrapping mode

// Texture drawing functions
RLAPI void DrawTexture(Texture2D texture, i32 posX, i32 posY, Color tint);                                                         // Draw a Texture2D
RLAPI void DrawTextureV(Texture2D texture, vec2 position, Color tint);                                                             // Draw a Texture2D with position defined as vec2
RLAPI void DrawTextureEx(Texture2D texture, vec2 position, f32 rotation, f32 scale, Color tint);                               // Draw a Texture2D with rotation and scale
RLAPI void DrawTextureRec(Texture2D texture, Rectangle rec, vec2 position, Color tint);                                            // Draw a part of a texture defined by a rectangle
RLAPI void DrawTexturePro(Texture2D texture, Rectangle srcrec, Rectangle dstrec, vec2 origin, f32 rotation, Color tint);         // Draw a part of a texture defined by a source rectangle to destination rectangle, with scaling and rotation
RLAPI void DrawTextureNPatch(Texture2D texture, NPatchInfo nPatchInfo, Rectangle dstrec, vec2 origin, f32 rotation, Color tint); // Draw a texture (or part of it) that stretches or shrinks nicely

// Color/pixel related functions
RLAPI bool ColorIsEqual(Color col1, Color col2);                            // Check if two colors are equal
RLAPI Color Fade(Color color, f32 alpha);                                 // Get color with alpha applied, alpha goes from 0.0f to 1.0f
RLAPI i32 ColorToInt(Color color);                                          // Get hexadecimal value for a Color (0xRRGGBBAA)
RLAPI vec4 ColorNormalize(Color color);                                     // Get Color normalized as float [0..1]
RLAPI Color ColorFromNormalized(vec4 normalized);                           // Get Color from normalized values [0..1]
RLAPI vec3 ColorToHSV(Color color);                                         // Get HSV values for a Color, hue [0..360], saturation/value [0..1]
RLAPI Color ColorFromHSV(f32 hue, f32 saturation, f32 value);         // Get a Color from HSV values, hue [0..360], saturation/value [0..1]
RLAPI Color ColorTint(Color color, Color tint);                             // Get color multiplied with another color
RLAPI Color ColorBrightness(Color color, f32 factor);                     // Get color with brightness correction, brightness factor goes from -1.0f to 1.0f
RLAPI Color ColorContrast(Color color, f32 contrast);                     // Get color with contrast correction, contrast values between -1.0f and 1.0f
RLAPI Color ColorAlpha(Color color, f32 alpha);                           // Get color with alpha applied, alpha goes from 0.0f to 1.0f
RLAPI Color ColorAlphaBlend(Color dst, Color src, Color tint);              // Get src alpha-blended into dst color with tint
RLAPI Color ColorLerp(Color color1, Color color2, f32 factor);            // Get color lerp interpolation between two colors, factor [0.0f..1.0f]
RLAPI Color GetColor(u32 hexValue);                                // Get Color structure from hexadecimal value
RLAPI Color GetPixelColor(const void *srcPtr, i32 format);                  // Get Color from a source pixel pointer of certain format
RLAPI void SetPixelColor(void *dstPtr, Color color, i32 format);            // Set color formatted into destination pixel pointer
RLAPI i32 GetPixelDataSize(i32 width, i32 height, i32 format);              // Get pixel data size in bytes for certain format

//------------------------------------------------------------------------------------
// Font Loading and Text Drawing Functions (Module: text)
//------------------------------------------------------------------------------------

// Font loading/unloading functions
RLAPI Font GetFontDefault();                                                                                                                                    // Get the default Font
RLAPI Font LoadFont(std::string_view fileName);                                                                                                                 // Load font from file into GPU memory (VRAM)
RLAPI Font LoadFontEx(std::string_view fileName, i32 fontSize, const i32 *codepoints, i32 codepointCount);                                                      // Load font from file with defined codepoints and generation size, use NULL for codepoints and 0 for codepointCount to load the default character set, font size is provided in pixels height
RLAPI Font LoadFontFromImage(Image image, Color key, i32 firstChar);                                                                                            // Load font from Image (XNA style)
RLAPI Font LoadFontFromMemory(std::string_view fileType, const u8 *fileData, i32 dataSize, i32 fontSize, const i32 *codepoints, i32 codepointCount); // Load font from memory buffer, fileType refers to extension: i.e. '.ttf'
RLAPI bool IsFontValid(Font font);                                                                                                                              // Check if font is valid (font data loaded, WARNING: GPU texture not checked)
RLAPI GlyphInfo *LoadFontData(const u8 *fileData, i32 dataSize, i32 fontSize, const i32 *codepoints, i32 codepointCount, i32 type, i32 *glyphCount); // Load font data for further use
RLAPI Image GenImageFontAtlas(const GlyphInfo *glyphs, Rectangle **glyphRecs, i32 glyphCount, i32 fontSize, i32 padding, i32 packMethod);                       // Generate image font atlas using chars info
RLAPI void UnloadFontData(GlyphInfo *glyphs, i32 glyphCount);                                                                                                   // Unload font chars info data (RAM)
RLAPI void UnloadFont(Font font);                                                                                                                               // Unload font from GPU memory (VRAM)
RLAPI bool ExportFontAsCode(Font font, std::string_view fileName);                                                                                              // Export font as code file, returns true on success

// Text drawing functions
RLAPI void DrawFPS(i32 posX, i32 posY);                                                                                                          // Draw current FPS
RLAPI void DrawText(std::string_view text, i32 posX, i32 posY, i32 fontSize, Color color);                                                       // Draw text (using default font)
RLAPI void DrawTextEx(Font font, std::string_view text, vec2 position, f32 fontSize, f32 spacing, Color tint);                               // Draw text using font and additional parameters
RLAPI void DrawTextPro(Font font, std::string_view text, vec2 position, vec2 origin, f32 rotation, f32 fontSize, f32 spacing, Color tint); // Draw text using Font and pro parameters (rotation)
RLAPI void DrawTextCodepoint(Font font, i32 codepoint, vec2 position, f32 fontSize, Color tint);                                               // Draw one character (codepoint)
RLAPI void DrawTextCodepoints(Font font, const i32 *codepoints, i32 codepointCount, vec2 position, f32 fontSize, f32 spacing, Color tint);   // Draw multiple characters (codepoint)

// Text font info functions
RLAPI void SetTextLineSpacing(i32 spacing);                                                                    // Set vertical line spacing when drawing with line-breaks
RLAPI i32 MeasureText(std::string_view text, i32 fontSize);                                                    // Measure string width for default font
RLAPI vec2 MeasureTextEx(Font font, std::string_view text, f32 fontSize, f32 spacing);                     // Measure string size for Font
RLAPI vec2 MeasureTextCodepoints(Font font, const i32 *codepoints, i32 length, f32 fontSize, f32 spacing); // Measure string size for an existing array of codepoints for Font
RLAPI i32 GetGlyphIndex(Font font, i32 codepoint);                                                             // Get glyph index position in font for a codepoint (unicode character), fallback to '?' if not found
RLAPI GlyphInfo GetGlyphInfo(Font font, i32 codepoint);                                                        // Get glyph font info data for a codepoint (unicode character), fallback to '?' if not found
RLAPI Rectangle GetGlyphAtlasRec(Font font, i32 codepoint);                                                    // Get glyph rectangle in font atlas for a codepoint (unicode character), fallback to '?' if not found

// Text codepoints management functions (unicode characters)
RLAPI std::string LoadUTF8(const i32 *codepoints, i32 length);                                    // Load UTF-8 text encoded from codepoints array
RLAPI i32 *LoadCodepoints(std::string_view text, i32 *count);                                     // Load all codepoints from a UTF-8 text string, codepoints count returned by parameter
RLAPI void UnloadCodepoints(i32 *codepoints);                                                     // Unload codepoints data from memory
RLAPI i32 GetCodepointCount(std::string_view text);                                               // Get total number of codepoints in a UTF-8 encoded string
RLAPI i32 GetCodepoint(std::string_view text, i32 *codepointSize);                                // Get next codepoint in a UTF-8 encoded string, 0x3f('?') is returned on failure
RLAPI i32 GetCodepointNext(std::string_view text, i32 *codepointSize);                            // Get next codepoint in a UTF-8 encoded string, 0x3f('?') is returned on failure
RLAPI i32 GetCodepointPrevious(std::string_view text, i32 *codepointSize);                        // Get previous codepoint in a UTF-8 encoded string, 0x3f('?') is returned on failure
RLAPI std::string CodepointToUTF8(i32 codepoint);                                                 // Encode one codepoint into UTF-8 byte array

// Text strings management functions (no UTF-8 strings, only byte chars)
RLAPI std::vector<std::string> LoadTextLines(std::string_view text);                                                                     // Load text as separate lines
RLAPI bool TextIsEqual(std::string_view text1, std::string_view text2);                                                                  // Check if two text strings are equal
RLAPI u32 TextLength(std::string_view text);                                                                                    // Get text length, checks for '\0' ending
RLAPI std::string TextFormat(std::string_view text, ...);                                                                                // Text formatting with variables (sprintf() style)
RLAPI std::string TextSubtext(std::string_view text, i32 position, i32 length);                                                          // Get a piece of a text string
RLAPI std::string TextRemoveSpaces(std::string_view text);                                                                               // Remove text spaces, concat words
RLAPI std::string GetTextBetween(std::string_view text, std::string_view begin, std::string_view end);                                   // Get text between two strings
RLAPI std::string TextReplace(std::string_view text, std::string_view search, std::string_view replacement);                             // Replace text string with new string
RLAPI std::string TextReplaceBetween(std::string_view text, std::string_view begin, std::string_view end, std::string_view replacement); // Replace text between two specific strings
RLAPI std::string TextInsert(std::string_view text, std::string_view insert, i32 position);                                              // Insert text in a defined byte position
RLAPI std::string TextJoin(const std::vector<std::string> &textList, std::string_view delimiter);                                        // Join text strings with delimiter
RLAPI std::vector<std::string> TextSplit(std::string_view text, char delimiter);                                                         // Split text into multiple strings
RLAPI i32 TextFindIndex(std::string_view text, std::string_view search);                                                                 // Find first text occurrence within a string, -1 if not found
RLAPI std::string TextToUpper(std::string_view text);                                                                                    // Get upper case version of provided string
RLAPI std::string TextToLower(std::string_view text);                                                                                    // Get lower case version of provided string
RLAPI std::string TextToPascal(std::string_view text);                                                                                   // Get Pascal case notation version of provided string
RLAPI std::string TextToSnake(std::string_view text);                                                                                    // Get Snake case notation version of provided string
RLAPI std::string TextToCamel(std::string_view text);                                                                                    // Get Camel case notation version of provided string
RLAPI i32 TextToInteger(std::string_view text);                                                                                          // Get integer value from text
RLAPI f32 TextToFloat(std::string_view text);                                                                                          // Get float value from text

//------------------------------------------------------------------------------------
// Basic 3d Shapes Drawing Functions (Module: models)
//------------------------------------------------------------------------------------

// Basic geometric 3D shapes drawing functions
RLAPI void DrawLine3D(vec3 startPos, vec3 endPos, Color color);                                                         // Draw a line in 3D world space
RLAPI void DrawPoint3D(vec3 position, Color color);                                                                     // Draw a point in 3D space, actually a small line
RLAPI void DrawCircle3D(vec3 center, f32 radius, vec3 rotationAxis, f32 rotationAngle, Color color);                // Draw a circle in 3D world space
RLAPI void DrawTriangle3D(vec3 v1, vec3 v2, vec3 v3, Color color);                                                      // Draw a color-filled triangle, counter-clockwise vertex order
RLAPI void DrawTriangleStrip3D(const vec3 *points, i32 pointCount, Color color);                                        // Draw a triangle strip defined by points
RLAPI void DrawCube(vec3 position, f32 width, f32 height, f32 length, Color color);                               // Draw cube
RLAPI void DrawCubeV(vec3 position, vec3 size, Color color);                                                            // Draw cube (Vector version)
RLAPI void DrawCubeWires(vec3 position, f32 width, f32 height, f32 length, Color color);                          // Draw cube wires
RLAPI void DrawCubeWiresV(vec3 position, vec3 size, Color color);                                                       // Draw cube wires (Vector version)
RLAPI void DrawSphere(vec3 centerPos, f32 radius, Color color);                                                       // Draw sphere
RLAPI void DrawSphereEx(vec3 centerPos, f32 radius, i32 rings, i32 slices, Color color);                              // Draw sphere with defined rings and slices
RLAPI void DrawSphereWires(vec3 centerPos, f32 radius, i32 rings, i32 slices, Color color);                           // Draw sphere wires
RLAPI void DrawCylinder(vec3 position, f32 radiusTop, f32 radiusBottom, f32 height, i32 sides, Color color);      // Draw a cylinder/cone
RLAPI void DrawCylinderEx(vec3 startPos, vec3 endPos, f32 startRadius, f32 endRadius, i32 sides, Color color);      // Draw a cylinder with base at startPos and top at endPos
RLAPI void DrawCylinderWires(vec3 position, f32 radiusTop, f32 radiusBottom, f32 height, i32 sides, Color color); // Draw a cylinder/cone wires
RLAPI void DrawCylinderWiresEx(vec3 startPos, vec3 endPos, f32 startRadius, f32 endRadius, i32 sides, Color color); // Draw a cylinder wires with base at startPos and top at endPos
RLAPI void DrawCapsule(vec3 startPos, vec3 endPos, f32 radius, i32 rings, i32 slices, Color color);                   // Draw a capsule with the center of its sphere caps at startPos and endPos
RLAPI void DrawCapsuleWires(vec3 startPos, vec3 endPos, f32 radius, i32 rings, i32 slices, Color color);              // Draw capsule wireframe with the center of its sphere caps at startPos and endPos
RLAPI void DrawPlane(vec3 centerPos, vec2 size, Color color);                                                           // Draw a plane XZ
RLAPI void DrawRay(Ray ray, Color color);                                                                               // Draw a ray line
RLAPI void DrawGrid(i32 slices, f32 spacing);                                                                         // Draw a grid (centered at (0, 0, 0))

//------------------------------------------------------------------------------------
// Model 3d Loading and Drawing Functions (Module: models)
//------------------------------------------------------------------------------------

// Model management functions
RLAPI Model LoadModel(std::string_view fileName);                                                // Load model from files (meshes and materials)
RLAPI Model LoadModelFromMesh(Mesh mesh);                                                        // Load model from generated mesh (default material)
RLAPI bool IsModelValid(Model model);                                                            // Check if model is valid (loaded in GPU, VAO/VBOs)
RLAPI void UnloadModel(Model model);                                                             // Unload model (including meshes) from memory (RAM and/or VRAM)
RLAPI BoundingBox GetModelBoundingBox(Model model);                                              // Compute model bounding box limits (considers all meshes)

// Model drawing functions
RLAPI void DrawModel(Model model, vec3 position, f32 scale, Color tint);                                                                                // Draw a model (with texture if set)
RLAPI void DrawModelEx(Model model, vec3 position, vec3 rotationAxis, f32 rotationAngle, vec3 scale, Color tint);                                       // Draw a model with custom transform
RLAPI void DrawModelWires(Model model, vec3 position, f32 scale, Color tint);                                                                           // Draw a model wires (with texture if set)
RLAPI void DrawModelWiresEx(Model model, vec3 position, vec3 rotationAxis, f32 rotationAngle, vec3 scale, Color tint);                                  // Draw a model wires with custom transform
RLAPI void DrawBoundingBox(BoundingBox box, Color color);                                                                                                 // Draw bounding box (wires)
RLAPI void DrawBillboard(Camera camera, Texture2D texture, vec3 position, f32 scale, Color tint);                                                       // Draw a billboard texture
RLAPI void DrawBillboardRec(Camera camera, Texture2D texture, Rectangle rec, vec3 position, vec2 size, Color tint);                                       // Draw a billboard texture defined by rectangle
RLAPI void DrawBillboardPro(Camera camera, Texture2D texture, Rectangle rec, vec3 position, vec3 up, vec2 size, vec2 origin, f32 rotation, Color tint); // Draw a billboard texture defined by source rectangle with scaling and rotation

// Mesh management functions
RLAPI void UploadMesh(Mesh *mesh, bool dynamic);                                                   // Upload mesh vertex data in GPU and provide VAO/VBO ids
RLAPI void UpdateMeshBuffer(Mesh mesh, i32 index, const void *data, i32 dataSize, i32 offset);     // Update mesh vertex data in GPU for a specific buffer index
RLAPI void UnloadMesh(Mesh mesh);                                                                  // Unload mesh data from CPU and GPU
RLAPI void DrawMesh(Mesh mesh, Material material, mat4 transform);                                 // Draw a 3d mesh with material and transform
RLAPI void DrawMeshInstanced(Mesh mesh, Material material, const mat4 *transforms, i32 instances); // Draw multiple mesh instances with material and different transforms
RLAPI BoundingBox GetMeshBoundingBox(Mesh mesh);                                                   // Compute mesh bounding box limits
RLAPI void GenMeshTangents(Mesh *mesh);                                                            // Compute mesh tangents
RLAPI bool ExportMesh(Mesh mesh, std::string_view fileName);                                       // Export mesh data to file, returns true on success
RLAPI bool ExportMeshAsCode(Mesh mesh, std::string_view fileName);                                 // Export mesh as code file (.h) defining multiple arrays of vertex attributes

// Mesh generation functions
RLAPI Mesh GenMeshPoly(i32 sides, f32 radius);                                            // Generate polygonal mesh
RLAPI Mesh GenMeshPlane(f32 width, f32 length, i32 resX, i32 resZ);                     // Generate plane mesh (with subdivisions)
RLAPI Mesh GenMeshCube(f32 width, f32 height, f32 length);                            // Generate cuboid mesh
RLAPI Mesh GenMeshSphere(f32 radius, i32 rings, i32 slices);                              // Generate sphere mesh (standard sphere)
RLAPI Mesh GenMeshHemiSphere(f32 radius, i32 rings, i32 slices);                          // Generate half-sphere mesh (no bottom cap)
RLAPI Mesh GenMeshCylinder(f32 radius, f32 height, i32 slices);                         // Generate cylinder mesh
RLAPI Mesh GenMeshCone(f32 radius, f32 height, i32 slices);                             // Generate cone/pyramid mesh
RLAPI Mesh GenMeshTorus(f32 radius, f32 size, i32 radSeg, i32 sides);                   // Generate torus mesh
RLAPI Mesh GenMeshKnot(f32 radius, f32 size, i32 radSeg, i32 sides);                    // Generate trefoil knot mesh
RLAPI Mesh GenMeshHeightmap(Image heightmap, vec3 size);                                    // Generate heightmap mesh from image data
RLAPI Mesh GenMeshCubicmap(Image cubicmap, vec3 cubeSize);                                  // Generate cubes-based map mesh from image data

// Material loading/unloading functions
RLAPI Material *LoadMaterials(std::string_view fileName, i32 *materialCount);                    // Load materials from model file
RLAPI Material LoadMaterialDefault();                                                            // Load default material (Supports: DIFFUSE, SPECULAR, NORMAL maps)
RLAPI bool IsMaterialValid(Material material);                                                   // Check if material is valid (shader assigned, map textures loaded in GPU)
RLAPI void UnloadMaterial(Material material);                                                    // Unload material from GPU memory (VRAM)
RLAPI void SetMaterialTexture(Material *material, i32 mapType, Texture2D texture);               // Set texture for a material map type (MATERIAL_MAP_DIFFUSE, MATERIAL_MAP_SPECULAR...)
RLAPI void SetModelMeshMaterial(Model *model, i32 meshId, i32 materialId);                       // Set material for a mesh

// Model animations loading/unloading functions
RLAPI ModelAnimation *LoadModelAnimations(std::string_view fileName, i32 *animCount);                                                // Load model animations from file
RLAPI void UpdateModelAnimation(Model model, ModelAnimation anim, f32 frame);                                                      // Update model animation pose (vertex buffers and bone matrices)
RLAPI void UpdateModelAnimationEx(Model model, ModelAnimation animA, f32 frameA, ModelAnimation animB, f32 frameB, f32 blend); // Update model animation pose, blending two animations
RLAPI void UnloadModelAnimations(ModelAnimation *animations, i32 animCount);                                                         // Unload animation array data
RLAPI bool IsModelAnimationValid(Model model, ModelAnimation anim);                                                                  // Check model animation skeleton match

// Collision detection functions
RLAPI bool CheckCollisionSpheres(vec3 center1, f32 radius1, vec3 center2, f32 radius2); // Check collision between two spheres
RLAPI bool CheckCollisionBoxes(BoundingBox box1, BoundingBox box2);                         // Check collision between two bounding boxes
RLAPI bool CheckCollisionBoxSphere(BoundingBox box, vec3 center, f32 radius);             // Check collision between box and sphere
RLAPI RayCollision GetRayCollisionSphere(Ray ray, vec3 center, f32 radius);               // Get collision info between ray and sphere
RLAPI RayCollision GetRayCollisionBox(Ray ray, BoundingBox box);                            // Get collision info between ray and box
RLAPI RayCollision GetRayCollisionMesh(Ray ray, Mesh mesh, mat4 transform);                 // Get collision info between ray and mesh
RLAPI RayCollision GetRayCollisionTriangle(Ray ray, vec3 p1, vec3 p2, vec3 p3);             // Get collision info between ray and triangle
RLAPI RayCollision GetRayCollisionQuad(Ray ray, vec3 p1, vec3 p2, vec3 p3, vec3 p4);        // Get collision info between ray and quad

//------------------------------------------------------------------------------------
// Audio Loading and Playing Functions (Module: audio)
//------------------------------------------------------------------------------------
typedef void (*AudioCallback)(void *bufferData, u32 frames);

// Audio device management functions
RLAPI void InitAudioDevice();                                         // Initialize audio device and context
RLAPI void CloseAudioDevice();                                        // Close the audio device and context
RLAPI bool IsAudioDeviceReady();                                      // Check if audio device has been initialized successfully
RLAPI void SetMasterVolume(f32 volume);                             // Set master volume (listener)
RLAPI f32 GetMasterVolume();                                        // Get master volume (listener)

// Wave/Sound loading/unloading functions
RLAPI Wave LoadWave(std::string_view fileName);                                                        // Load wave data from file
RLAPI Wave LoadWaveFromMemory(std::string_view fileType, const u8 *fileData, i32 dataSize); // Load wave from memory buffer, fileType refers to extension: i.e. '.wav'
RLAPI bool IsWaveValid(const Wave& wave);                                                              // Check if wave data is valid (data loaded and parameters)
RLAPI Sound LoadSound(std::string_view fileName);                                                      // Load sound from file
RLAPI Sound LoadSoundFromWave(const Wave& wave);                                                       // Load sound from wave data
RLAPI Sound LoadSoundAlias(const Sound& source);                                                       // Load sound alias, new sound that shares the same sample data as the source sound, does not own the sound data
RLAPI bool IsSoundValid(const Sound& sound);                                                           // Check if sound is valid (data loaded and buffers initialized)
RLAPI void UpdateSound(const Sound& sound, const void *data, i32 frameCount);                          // Update sound buffer with new data (default data format: 32 bit float, stereo)
RLAPI void UnloadWave(const Wave& wave);                                                               // Unload wave data
RLAPI void UnloadSound(const Sound& sound);                                                            // Unload sound
RLAPI void UnloadSoundAlias(const Sound& alias);                                                       // Unload sound alias (does not deallocate sample data)
RLAPI bool ExportWave(const Wave& wave, std::string_view fileName);                                    // Export wave data to file, returns true on success
RLAPI bool ExportWaveAsCode(const Wave& wave, std::string_view fileName);                              // Export wave sample data to code (.h), returns true on success

// Wave/Sound management functions
RLAPI void PlaySound(const Sound& sound);                                        // Play a sound
RLAPI void StopSound(const Sound& sound);                                        // Stop playing a sound
RLAPI void PauseSound(const Sound& sound);                                       // Pause a sound
RLAPI void ResumeSound(const Sound& sound);                                      // Resume a paused sound
RLAPI bool IsSoundPlaying(const Sound& sound);                                   // Check if sound is currently playing
RLAPI void SetSoundVolume(const Sound& sound, f32 volume);                     // Set volume for a sound (1.0 is max level)
RLAPI void SetSoundPitch(const Sound& sound, f32 pitch);                       // Set pitch for a sound (1.0 is base level)
RLAPI void SetSoundPan(const Sound& sound, f32 pan);                           // Set pan for a sound (-1.0 left, 0.0 center, 1.0 right)
RLAPI Wave WaveCopy(const Wave& wave);                                           // Copy a wave to a new wave
RLAPI void WaveCrop(Wave *wave, i32 initFrame, i32 finalFrame);                  // Crop a wave to defined frames range
RLAPI void WaveFormat(Wave *wave, i32 sampleRate, i32 sampleSize, i32 channels); // Convert wave data to desired format
RLAPI f32 *LoadWaveSamples(const Wave& wave);                                  // Load samples data from wave as a 32bit float data array
RLAPI void UnloadWaveSamples(f32 *samples);                                    // Unload samples data loaded with LoadWaveSamples()

// Music management functions
RLAPI Music LoadMusicStream(std::string_view fileName);                                                    // Load music stream from file
RLAPI Music LoadMusicStreamFromMemory(std::string_view fileType, const u8 *data, i32 dataSize); // Load music stream from data
RLAPI bool IsMusicValid(Music music);                                                                      // Check if music stream is valid (context and buffers initialized)
RLAPI void UnloadMusicStream(Music music);                                                                 // Unload music stream
RLAPI void PlayMusicStream(Music music);                                                                   // Start music playing
RLAPI bool IsMusicStreamPlaying(const Music& music);                                                       // Check if music is playing
RLAPI void UpdateMusicStream(Music music);                                                                 // Update buffers for music streaming
RLAPI void StopMusicStream(Music music);                                                                   // Stop music playing
RLAPI void PauseMusicStream(Music music);                                                                  // Pause music playing
RLAPI void ResumeMusicStream(Music music);                                                                 // Resume playing paused music
RLAPI void SeekMusicStream(Music music, f32 position);                                                   // Seek music to a position (in seconds)
RLAPI void SetMusicVolume(const Music& music, f32 volume);                                               // Set volume for music (1.0 is max level)
RLAPI void SetMusicPitch(const Music& music, f32 pitch);                                                 // Set pitch for music (1.0 is base level)
RLAPI void SetMusicPan(const Music& music, f32 pan);                                                     // Set pan for music (-1.0 left, 0.0 center, 1.0 right)
RLAPI f32 GetMusicTimeLength(const Music& music);                                                        // Get music time length (in seconds)
RLAPI f32 GetMusicTimePlayed(const Music& music);                                                        // Get current music time played (in seconds)

// AudioStream management functions
RLAPI AudioStream LoadAudioStream(u32 sampleRate, u32 sampleSize, u32 channels); // Load audio stream (to stream raw audio pcm data)
RLAPI bool IsAudioStreamValid(const AudioStream& stream);                                                   // Check if an audio stream is valid (buffers initialized)
RLAPI void UnloadAudioStream(const AudioStream& stream);                                                    // Unload audio stream and free memory
RLAPI void UpdateAudioStream(const AudioStream& stream, const void *data, i32 frameCount);                  // Update audio stream buffers with data
RLAPI bool IsAudioStreamProcessed(const AudioStream& stream);                                               // Check if any audio stream buffers requires refill
RLAPI void PlayAudioStream(const AudioStream& stream);                                                      // Play audio stream
RLAPI void PauseAudioStream(const AudioStream& stream);                                                     // Pause audio stream
RLAPI void ResumeAudioStream(const AudioStream& stream);                                                    // Resume audio stream
RLAPI bool IsAudioStreamPlaying(const AudioStream& stream);                                                 // Check if audio stream is playing
RLAPI void StopAudioStream(const AudioStream& stream);                                                      // Stop audio stream
RLAPI void SetAudioStreamVolume(const AudioStream& stream, f32 volume);                                   // Set volume for audio stream (1.0 is max level)
RLAPI void SetAudioStreamPitch(const AudioStream& stream, f32 pitch);                                     // Set pitch for audio stream (1.0 is base level)
RLAPI void SetAudioStreamPan(const AudioStream& stream, f32 pan);                                         // Set pan for audio stream (-1.0 left, 0.0 center, 1.0 right)
RLAPI void SetAudioStreamBufferSizeDefault(i32 size);                                                       // Default size for new audio streams
RLAPI void SetAudioStreamCallback(const AudioStream& stream, AudioCallback callback);                       // Audio thread callback to request new data
RLAPI void AttachAudioStreamProcessor(const AudioStream& stream, AudioCallback processor);                  // Attach audio stream processor to stream, receives frames x 2 samples as 'float' (stereo)
RLAPI void DetachAudioStreamProcessor(const AudioStream& stream, AudioCallback processor);                  // Detach audio stream processor from stream
RLAPI void AttachAudioMixedProcessor(AudioCallback processor);                                              // Attach audio stream processor to the entire audio pipeline, receives frames x 2 samples as 'float' (stereo)
RLAPI void DetachAudioMixedProcessor(AudioCallback processor);                                              // Detach audio stream processor from the entire audio pipeline

#endif // RAYLIB_H
