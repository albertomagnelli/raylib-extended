/**********************************************************************************************
*
*   rtextures - Basic functions to load and draw textures
*
*   CONFIGURATION:
*       #define SUPPORT_MODULE_RTEXTURES    1
*           rtextures module is included in the build
*
*       #define SUPPORT_FILEFORMAT_BMP      1
*       #define SUPPORT_FILEFORMAT_PNG      1
*       #define SUPPORT_FILEFORMAT_TGA      0
*       #define SUPPORT_FILEFORMAT_JPG      0
*       #define SUPPORT_FILEFORMAT_GIF      1
*       #define SUPPORT_FILEFORMAT_QOI      1
*       #define SUPPORT_FILEFORMAT_PEP      1
*       #define SUPPORT_FILEFORMAT_PSD      0
*       #define SUPPORT_FILEFORMAT_HDR      0
*       #define SUPPORT_FILEFORMAT_PIC      0
*       #define SUPPORT_FILEFORMAT_PNM      0
*       #define SUPPORT_FILEFORMAT_DDS      1
*       #define SUPPORT_FILEFORMAT_PKM      0
*       #define SUPPORT_FILEFORMAT_KTX      0
*       #define SUPPORT_FILEFORMAT_PVR      0
*       #define SUPPORT_FILEFORMAT_ASTC     0
*           Selected desired fileformats to be supported for image data loading. Some of those formats are
*           supported by default, to remove support, #define as 0 in this module or your build system
*
*       #define SUPPORT_IMAGE_EXPORT        1
*           Support image export in multiple file formats
*
*       #define SUPPORT_IMAGE_GENERATION    1
*           Support procedural image generation functionality (gradient, spot, perlin-noise, cellular)
*
*   DEPENDENCIES:
*       stb_image        - Multiple image formats loading (JPEG, PNG, BMP, TGA, PSD, GIF, PIC)
*                          NOTE: stb_image has been slightly modified to support Android platform
*       stb_image_resize - Multiple image resize algorithms
*
*
*   LICENSE: zlib/libpng
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

#include "raylib.h"             // Declares module functions

#include "config.h"             // Defines module configuration flags

#include "logging.h"         // Levelbasiertes Logging (log(), logMessage())

#if SUPPORT_MODULE_RTEXTURES

#include "rlgl.h"               // OpenGL abstraction layer to multiple versions

#include <stdlib.h>             // Required for: malloc(), calloc(), free()
#include <string.h>             // Required for: strlen() [Used in ImageTextEx()], strcmp() [Used in LoadImageFromMemory()/LoadImageAnimFromMemory()]
#include <math.h>               // Required for: fabsf() [Used in DrawTextureRec()]
#include <stdio.h>              // Required for: sprintf() [Used in ExportImageAsCode()]
#include <limits.h>             // Required for: INT_MAX

// Support only desired texture formats on stb_image
#if !SUPPORT_FILEFORMAT_BMP
    #define STBI_NO_BMP
#endif
#if !SUPPORT_FILEFORMAT_PNG
    #define STBI_NO_PNG
#endif
#if !SUPPORT_FILEFORMAT_TGA
    #define STBI_NO_TGA
#endif
#if !SUPPORT_FILEFORMAT_JPG
    #define STBI_NO_JPEG        // Image format .jpg and .jpeg
#endif
#if !SUPPORT_FILEFORMAT_PSD
    #define STBI_NO_PSD
#endif
#if !SUPPORT_FILEFORMAT_GIF
    #define STBI_NO_GIF
#endif
#if !SUPPORT_FILEFORMAT_PIC
    #define STBI_NO_PIC
#endif
#if !SUPPORT_FILEFORMAT_HDR
    #define STBI_NO_HDR
#endif
#if !SUPPORT_FILEFORMAT_PNM
    #define STBI_NO_PNM
#endif

#if SUPPORT_FILEFORMAT_DDS
    #define RLTEXGPU_SUPPORT_DDS
#endif
#if SUPPORT_FILEFORMAT_PKM
    #define RLTEXGPU_SUPPORT_PKM
#endif
#if SUPPORT_FILEFORMAT_KTX
    #define RLTEXGPU_SUPPORT_KTX
#endif
#if SUPPORT_FILEFORMAT_PVR
    #define RLTEXGPU_SUPPORT_PVR
#endif
#if SUPPORT_FILEFORMAT_ASTC
    #define RLTEXGPU_SUPPORT_ASTC
#endif

// Image fileformats not supported by default
#if (SUPPORT_FILEFORMAT_BMP || \
     SUPPORT_FILEFORMAT_PNG || \
     SUPPORT_FILEFORMAT_TGA || \
     SUPPORT_FILEFORMAT_JPG || \
     SUPPORT_FILEFORMAT_PSD || \
     SUPPORT_FILEFORMAT_GIF || \
     SUPPORT_FILEFORMAT_HDR || \
     SUPPORT_FILEFORMAT_PIC || \
     SUPPORT_FILEFORMAT_PNM)

    #if defined(__GNUC__) // GCC and Clang
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wunused-function"
    #endif

    #define STBI_MALLOC malloc
    #define STBI_FREE free
    #define STBI_REALLOC realloc

    #define STBI_NO_THREAD_LOCALS

    #if defined(__TINYC__)
        #define STBI_NO_SIMD
    #endif

    #define STB_IMAGE_IMPLEMENTATION
    #include "external/stb_image.h"         // Required for: stbi_load_from_file()
                                            // NOTE: Used to read image data (multiple formats support)

    #if defined(__GNUC__) // GCC and Clang
        #pragma GCC diagnostic pop
    #endif
#endif

#if (SUPPORT_FILEFORMAT_DDS || \
     SUPPORT_FILEFORMAT_PKM || \
     SUPPORT_FILEFORMAT_KTX || \
     SUPPORT_FILEFORMAT_PVR || \
     SUPPORT_FILEFORMAT_ASTC)

    #if defined(__GNUC__) // GCC and Clang
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wunused-function"
    #endif

    #define RLTEXGPU_MALLOC malloc
    #define RLTEXGPU_FREE free
    // NOTE: printf-style varargs macro required by rltexgpu.h, TraceLog() forwards to the logging system
    #define RLTEXGPU_LOG(...) TraceLog(LOG_WARNING, "IMAGE: " __VA_ARGS__)
    #define RLTEXGPU_SHOW_LOG_INFO
    #define RLTEXGPU_IMPLEMENTATION
    #include "external/rltexgpu.h"         // Required for: rl_load_xxx_from_memory()
                                           // NOTE: Used to read compressed textures data (multiple formats support)
    #if defined(__GNUC__) // GCC and Clang
        #pragma GCC diagnostic pop
    #endif
#endif

#if SUPPORT_FILEFORMAT_QOI
    #define QOI_MALLOC malloc
    #define QOI_FREE free

    #if defined(_MSC_VER)               // Disable some MSVC warning
        #pragma warning(push)
        #pragma warning(disable : 4267)
    #endif

    #define QOI_IMPLEMENTATION
    #include "external/qoi.h"

    #if defined(_MSC_VER)
        #pragma warning(pop)            // Disable MSVC warning suppression
    #endif
#endif

#if SUPPORT_FILEFORMAT_PEP
	#define PEP_MALLOC malloc
	#define PEP_REALLOC realloc
	#define PEP_FREE free

    #define PEP_IMPLEMENTATION
    #include "external/pep.h"
#endif

#if SUPPORT_IMAGE_EXPORT
    #define STBIW_MALLOC malloc
    #define STBIW_FREE free
    #define STBIW_REALLOC realloc

    #define STB_IMAGE_WRITE_IMPLEMENTATION
    #include "external/stb_image_write.h"   // Required for: stbi_write_*()
#endif

#if SUPPORT_IMAGE_GENERATION
    #define STB_PERLIN_IMPLEMENTATION
    #include "external/stb_perlin.h"        // Required for: stb_perlin_fbm_noise3
#endif

#define STBIR_MALLOC(size,c) ((void)(c), malloc(size))
#define STBIR_FREE(ptr,c) ((void)(c), free(ptr))

#if defined(__GNUC__) // GCC and Clang
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-function"
#endif

#if defined(__TINYC__)
    #define STBIR_NO_SIMD
#endif
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "external/stb_image_resize2.h"     // Required for: stbir_resize_uint8_linear() [ImageResize()]

#if defined(__GNUC__) // GCC and Clang
    #pragma GCC diagnostic pop
#endif

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
#ifndef PIXELFORMAT_UNCOMPRESSED_R5G5B5A1_ALPHA_THRESHOLD
    #define PIXELFORMAT_UNCOMPRESSED_R5G5B5A1_ALPHA_THRESHOLD  50    // Threshold over 255 to set alpha as 0
#endif

#ifndef GAUSSIAN_BLUR_ITERATIONS
    #define GAUSSIAN_BLUR_ITERATIONS  4    // Number of box blur iterations to approximate gaussian blur
#endif

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// ...

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
// It's lonely here...

//----------------------------------------------------------------------------------
// Other Modules Functions Declaration (required by text)
//----------------------------------------------------------------------------------
extern void LoadFontDefault();          // [Module: text] Loads default font, required by ImageDrawText()

//----------------------------------------------------------------------------------
// Module Internal Functions Declaration
//----------------------------------------------------------------------------------
static f32 HalfToFloat(const u16 x);
static u16 FloatToHalf(const f32 x);
static vec4 *LoadImageDataNormalized(const Image image);     // Load pixel data from image as vec4 array (float normalized)

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------
// Load image from file into CPU memory (RAM)
Image LoadImage(std::string_view fileNameView)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();

    auto image = Image{ nullptr };

#if SUPPORT_FILEFORMAT_PNG || \
    SUPPORT_FILEFORMAT_BMP || \
    SUPPORT_FILEFORMAT_TGA || \
    SUPPORT_FILEFORMAT_JPG || \
    SUPPORT_FILEFORMAT_GIF || \
    SUPPORT_FILEFORMAT_PIC || \
    SUPPORT_FILEFORMAT_HDR || \
    SUPPORT_FILEFORMAT_PNM || \
    SUPPORT_FILEFORMAT_PSD

    #define STBI_REQUIRED
#endif

    // Loading file to memory
    auto dataSize = 0;
    auto fileData = LoadFileData(fileName, &dataSize);

    // Loading image from memory data
    if (fileData != nullptr)
    {
        image = LoadImageFromMemory(GetFileExtension(fileName), fileData, dataSize);

        UnloadFileData(fileData);
    }

    return image;
}

// Load an image from RAW file data
Image LoadImageRaw(std::string_view fileNameView, i32 width, i32 height, i32 format, const i32 headerSize)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();

    auto image = Image{ nullptr };

    auto dataSize = 0;
    auto fileData = LoadFileData(fileName, &dataSize);

    if (fileData != nullptr)
    {
        u8 *dataPtr = fileData;
        const auto size = GetPixelDataSize(width, height, format);

        if (size <= dataSize) // Security check
        {
            // Offset file data to expected raw image by header size
            if ((headerSize > 0) && ((headerSize + size) <= dataSize)) dataPtr += headerSize;

            image.data = calloc(size, 1);   // Allocate required memory in bytes
            memcpy(image.data, dataPtr, size); // Copy required data to image
            image.width = width;
            image.height = height;
            image.mipmaps = 1;
            image.format = format;
        }

        UnloadFileData(fileData);
    }

    return image;
}

// Load animated image data
//  - Image.data buffer includes all frames: [image#0][image#1][image#2][...]
//  - Number of frames is returned through 'frames' parameter
//  - All frames are returned in RGBA format
//  - Frames delay data is discarded
Image LoadImageAnim(std::string_view fileNameView, i32 *frames)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();

    auto image = Image{ nullptr };
    auto frameCount = 0;

#if SUPPORT_FILEFORMAT_GIF
    if (IsFileExtension(fileName, ".gif"))
    {
        auto dataSize = 0;
        auto fileData = LoadFileData(fileName, &dataSize);

        if (fileData != nullptr)
        {
            auto comp = 0;
            i32 *delays = nullptr;
            image.data = stbi_load_gif_from_memory(fileData, dataSize, &delays, &image.width, &image.height, &frameCount, &comp, 4);

            image.mipmaps = 1;
            image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

            UnloadFileData(fileData);
            free(delays);        // NOTE: Frames delays are discarded
        }
    }
#else
    if (false) { }
#endif
    else
    {
        image = LoadImage(fileName);
        frameCount = 1;
    }

    *frames = frameCount;
    return image;
}

// Load animated image data from memory
//  - Image.data buffer includes all frames: [image#0][image#1][image#2][...]
//  - Number of frames is returned through 'frames' parameter
//  - All frames are returned in RGBA format
//  - Frames delay data is discarded
Image LoadImageAnimFromMemory(std::string_view fileTypeView, const u8 *fileData, const i32 dataSize, i32 *frames)
{
    const std::string fileTypeStr(fileTypeView);
    const char *fileType = fileTypeStr.c_str();

    auto image = Image{ nullptr };
    auto frameCount = 0;

    if (fileTypeView.empty() || (fileData == nullptr) || (dataSize == 0)) return image; // Security check

#if SUPPORT_FILEFORMAT_GIF
    if ((strcmp(fileType, ".gif") == 0) || (strcmp(fileType, ".GIF") == 0))
    {
        if (fileData != nullptr)
        {
            auto comp = 0;
            i32 *delays = nullptr;
            image.data = stbi_load_gif_from_memory(fileData, dataSize, &delays, &image.width, &image.height, &frameCount, &comp, 4);

            image.mipmaps = 1;
            image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

            free(delays);        // NOTE: Frames delays are discarded
        }
    }
#else
    if (false) { }
#endif
    else
    {
        image = LoadImageFromMemory(fileType, fileData, dataSize);
        frameCount = 1;
    }

    *frames = frameCount;
    return image;
}

// Load image from memory buffer, fileType refers to extension: i.e. ".png"
// WARNING: File extension must be provided in lower-case
Image LoadImageFromMemory(std::string_view fileTypeView, const u8 *fileData, const i32 dataSize)
{
    const std::string fileTypeStr(fileTypeView);
    const char *fileType = fileTypeStr.c_str();

    auto image = Image{ nullptr };

    // Security checks for input data
    if ((fileData == nullptr) || (dataSize == 0)) { log(LogLevel::warn, "IMAGE: Invalid file data"); return image; }
    if (fileTypeView.empty()) { log(LogLevel::warn, "IMAGE: Missing file extension"); return image; }

    if ((false)
#if SUPPORT_FILEFORMAT_PNG
        || (strcmp(fileType, ".png") == 0) || (strcmp(fileType, ".PNG") == 0)
#endif
#if SUPPORT_FILEFORMAT_BMP
        || (strcmp(fileType, ".bmp") == 0) || (strcmp(fileType, ".BMP") == 0)
#endif
#if SUPPORT_FILEFORMAT_TGA
        || (strcmp(fileType, ".tga") == 0) || (strcmp(fileType, ".TGA") == 0)
#endif
#if SUPPORT_FILEFORMAT_JPG
        || (strcmp(fileType, ".jpg") == 0) || (strcmp(fileType, ".jpeg") == 0)
        || (strcmp(fileType, ".JPG") == 0) || (strcmp(fileType, ".JPEG") == 0)
#endif
#if SUPPORT_FILEFORMAT_GIF
        || (strcmp(fileType, ".gif") == 0) || (strcmp(fileType, ".GIF") == 0)
#endif
#if SUPPORT_FILEFORMAT_PIC
        || (strcmp(fileType, ".pic") == 0) || (strcmp(fileType, ".PIC") == 0)
#endif
#if SUPPORT_FILEFORMAT_PNM
        || (strcmp(fileType, ".ppm") == 0) || (strcmp(fileType, ".pgm") == 0)
        || (strcmp(fileType, ".PPM") == 0) || (strcmp(fileType, ".PGM") == 0)
#endif
#if SUPPORT_FILEFORMAT_PSD
        || (strcmp(fileType, ".psd") == 0) || (strcmp(fileType, ".PSD") == 0)
#endif
        )
    {
#if defined(STBI_REQUIRED)
        // NOTE: Using stb_image to load images (Supports multiple image formats)

        if (fileData != nullptr)
        {
            auto comp = 0;
            image.data = stbi_load_from_memory(fileData, dataSize, &image.width, &image.height, &comp, 0);

            if (image.data != nullptr)
            {
                image.mipmaps = 1;

                if (comp == 1) image.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;
                else if (comp == 2) image.format = PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA;
                else if (comp == 3) image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8;
                else if (comp == 4) image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
            }
        }
#endif
    }
#if SUPPORT_FILEFORMAT_HDR
    else if ((strcmp(fileType, ".hdr") == 0) || (strcmp(fileType, ".HDR") == 0))
    {
#if defined(STBI_REQUIRED)
        if (fileData != nullptr)
        {
            auto comp = 0;
            image.data = stbi_loadf_from_memory(fileData, dataSize, &image.width, &image.height, &comp, 0);

            image.mipmaps = 1;

            if (comp == 1) image.format = PIXELFORMAT_UNCOMPRESSED_R32;
            else if (comp == 3) image.format = PIXELFORMAT_UNCOMPRESSED_R32G32B32;
            else if (comp == 4) image.format = PIXELFORMAT_UNCOMPRESSED_R32G32B32A32;
            else
            {
                log(LogLevel::warn, "IMAGE: HDR file format not supported");
                UnloadImage(image);
                image = Image{};
            }
        }
#endif
    }
#endif
#if SUPPORT_FILEFORMAT_QOI
    else if ((strcmp(fileType, ".qoi") == 0) || (strcmp(fileType, ".QOI") == 0))
    {
        if (fileData != nullptr)
        {
            auto desc = qoi_desc{ 0 };
            image.data = qoi_decode(fileData, dataSize, &desc, 0);   // NOTE: Using channels from file data
            image.width = static_cast<i32>(desc.width);
            image.height = static_cast<i32>(desc.height);
            image.format = (desc.channels == 4)? PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 : PIXELFORMAT_UNCOMPRESSED_R8G8B8;
            image.mipmaps = 1;
        }
    }
#endif
#if SUPPORT_FILEFORMAT_PEP
    else if ((strcmp(fileType, ".pep") == 0) || (strcmp(fileType, ".PEP") == 0))
    {
        if (fileData != nullptr)
        {
            // Deserialize file data into pep structure
            pep p = pep_deserialize(fileData, dataSize);

            // Decompress pep data for image struct
            // Decompression options:
            //  - First color transparent, 3rd parameter: 1
            //  - Premultiply alpha on loading, 4th parameter: 1
            u32 *pepData = pep_decompress(&p, pep_rgba, 1, 1);

            image.data = pepData;
            image.width = p.width;
            image.height = p.height;
            image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
            image.mipmaps = 1;

            pep_free(&p);
        }
    }
#endif
#if SUPPORT_FILEFORMAT_DDS
    else if ((strcmp(fileType, ".dds") == 0) || (strcmp(fileType, ".DDS") == 0))
    {
        image.data = rl_load_dds_from_memory(fileData, dataSize, &image.width, &image.height, &image.format, &image.mipmaps);
    }
#endif
#if SUPPORT_FILEFORMAT_PKM
    else if ((strcmp(fileType, ".pkm") == 0) || (strcmp(fileType, ".PKM") == 0))
    {
        image.data = rl_load_pkm_from_memory(fileData, dataSize, &image.width, &image.height, &image.format, &image.mipmaps);
    }
#endif
#if SUPPORT_FILEFORMAT_KTX
    else if ((strcmp(fileType, ".ktx") == 0) || (strcmp(fileType, ".KTX") == 0))
    {
        image.data = rl_load_ktx_from_memory(fileData, dataSize, &image.width, &image.height, &image.format, &image.mipmaps);
    }
#endif
#if SUPPORT_FILEFORMAT_PVR
    else if ((strcmp(fileType, ".pvr") == 0) || (strcmp(fileType, ".PVR") == 0))
    {
        image.data = rl_load_pvr_from_memory(fileData, dataSize, &image.width, &image.height, &image.format, &image.mipmaps);
    }
#endif
#if SUPPORT_FILEFORMAT_ASTC
    else if ((strcmp(fileType, ".astc") == 0) || (strcmp(fileType, ".ASTC") == 0))
    {
        image.data = rl_load_astc_from_memory(fileData, dataSize, &image.width, &image.height, &image.format, &image.mipmaps);
    }
#endif
    else log(LogLevel::warn, "IMAGE: Data format not supported");

    if (image.data != nullptr) log(LogLevel::info, "IMAGE: Data loaded successfully ({}x{} | {} | {} mipmaps)", image.width, image.height, rlGetPixelFormatName(image.format).c_str(), image.mipmaps);
    else log(LogLevel::warn, "IMAGE: Failed to load image data");

    return image;
}

// Load image from GPU texture data
// NOTE: Compressed texture formats not supported
Image LoadImageFromTexture(const Texture2D texture)
{
    auto image = Image{ nullptr };

    if (texture.format < PIXELFORMAT_COMPRESSED_DXT1_RGB)
    {
        image.data = rlReadTexturePixels(texture.id, texture.width, texture.height, texture.format);

        if (image.data != nullptr)
        {
            image.width = texture.width;
            image.height = texture.height;
#if defined(FBO_READ_TEXTURE_AS_RGBA)
            // WARNING: On OpenGL ES 2.0/WebGL 1.0, there is no glGetTexImage() so,
            // texture data is retrieved by creating an RGBA fbo, binding the texture
            // to the fbo color attachment, and reading it as RGBA data with glReadPixels()
            // Returned data *should* be RGBA but it seems on some platforms (RPI, WASM)
            // original texture format is retrieved, so adding a define for this patch
            image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
#else
            image.format = texture.format;
#endif
            image.mipmaps = 1;

            log(LogLevel::info, "TEXTURE: [ID {}] Pixel data retrieved successfully", texture.id);
        }
        else log(LogLevel::warn, "TEXTURE: [ID {}] Failed to retrieve pixel data", texture.id);
    }
    else log(LogLevel::warn, "TEXTURE: [ID {}] Failed to retrieve compressed pixel data", texture.id);

    return image;
}

// Load image from screen buffer (screenshot)
Image LoadImageFromScreen()
{
    auto image = Image{ nullptr };

    image.width = GetRenderWidth();
    image.height = GetRenderHeight();
    image.mipmaps = 1;
    image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    image.data = rlReadScreenPixels(image.width, image.height);

    return image;
}

// Check if an image is ready
bool IsImageValid(const Image image)
{
    auto result = false;

    if ((image.data != nullptr) &&     // Validate pixel data available
        (image.width > 0) &&        // Validate image width
        (image.height > 0) &&       // Validate image height
        (image.format > 0) &&       // Validate image format
        (image.mipmaps > 0)) result = true; // Validate image mipmaps (at least 1 for basic mipmap level)

    return result;
}

// Unload image from CPU memory (RAM)
void UnloadImage(Image image)
{
    free(image.data);
}

// Export image data to file
// NOTE: File format depends on fileName extension
bool ExportImage(const Image image, std::string_view fileNameView)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();

    auto result = 0;

    if ((image.width == 0) || (image.height == 0) || (image.data == nullptr)) return result; // Security check

#if SUPPORT_IMAGE_EXPORT
    auto channels = 4;
    auto allocatedData = false;
    auto imgData = static_cast<u8 *>(image.data);

    if (image.format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE) channels = 1;
    else if (image.format == PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA) channels = 2;
    else if (image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8) channels = 3;
    else if (image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) channels = 4;
    else
    {
        // NOTE: Getting Color array as RGBA unsigned char values
        imgData = reinterpret_cast<u8 *>(LoadImageColors(image));
        allocatedData = true;
    }

    if (false) { } // Required to attach following 'else' cases
#if SUPPORT_FILEFORMAT_PNG
    else if (IsFileExtension(fileName, ".png"))
    {
        auto dataSize = 0;
        u8 *fileData = stbi_write_png_to_mem(static_cast<const u8 *>(imgData), image.width*channels, image.width, image.height, channels, &dataSize);
        result = SaveFileData(fileName, fileData, dataSize);
        free(fileData);
    }
#endif
#if SUPPORT_FILEFORMAT_BMP
    else if (IsFileExtension(fileName, ".bmp")) result = stbi_write_bmp(fileName, image.width, image.height, channels, imgData);
#endif
#if SUPPORT_FILEFORMAT_TGA
    else if (IsFileExtension(fileName, ".tga")) result = stbi_write_tga(fileName, image.width, image.height, channels, imgData);
#endif
#if SUPPORT_FILEFORMAT_JPG
    else if (IsFileExtension(fileName, ".jpg") ||
             IsFileExtension(fileName, ".jpeg")) result = stbi_write_jpg(fileName, image.width, image.height, channels, imgData, 90);  // JPG quality: between 1 and 100
#endif
#if SUPPORT_FILEFORMAT_QOI
    else if (IsFileExtension(fileName, ".qoi"))
    {
        channels = 0;
        if (image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8) channels = 3;
        else if (image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) channels = 4;
        else log(LogLevel::warn, "IMAGE: Image pixel format must be R8G8B8 or R8G8B8A8");

        if ((channels == 3) || (channels == 4))
        {
            auto desc = qoi_desc{ 0 };
            desc.width = image.width;
            desc.height = image.height;
            desc.channels = channels;
            desc.colorspace = QOI_SRGB;

            result = qoi_write(fileName, imgData, &desc);
        }
    }
#endif
#if SUPPORT_FILEFORMAT_PEP
    else if (IsFileExtension(fileName, ".pep"))
    {
        if (image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8)
        {
            // NOTE: Only RGBA 32bit (8bit per channel) image format supported
            pep p = pep_compress(image.data, image.width, image.height, pep_rgba, pep_8bit);

            u32 pepDataSize = 0;
            u8 *pepData = pep_serialize(&p, &pepDataSize);

            result = SaveFileData(fileName, pepData, pepDataSize);

            PEP_FREE(pepData);
            pep_free(&p);
        }
        else log(LogLevel::warn, "IMAGE: Image can not be exported, .pep requires RGBA 32bit input");
    }
#endif
#if SUPPORT_FILEFORMAT_KTX
    else if (IsFileExtension(fileName, ".ktx"))
    {
        result = rl_save_ktx(fileName, image.data, image.width, image.height, image.format, image.mipmaps);
    }
#endif
    else if (IsFileExtension(fileName, ".raw"))
    {
        // Export raw pixel data (without header)
        // NOTE: It's up to the user to track image parameters
        result = SaveFileData(fileName, image.data, GetPixelDataSize(image.width, image.height, image.format));
    }
    else log(LogLevel::warn, "IMAGE: Export image format requested not supported");

    if (allocatedData) free(imgData);
#endif // SUPPORT_IMAGE_EXPORT

    if (result != 0) log(LogLevel::info, "FILEIO: [{}] Image exported successfully", fileName);
    else log(LogLevel::warn, "FILEIO: [{}] Failed to export image", fileName);

    return result;
}

// Export image to memory buffer
u8 *ExportImageToMemory(const Image image, const std::string_view fileType, i32 *dataSize)
{
    u8 *fileData = nullptr;
    *dataSize = 0;

    if ((image.width == 0) || (image.height == 0) || (image.data == nullptr)) return fileData; // Security check

    auto channels = 4;

    if (image.format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE) channels = 1;
    else if (image.format == PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA) channels = 2;
    else if (image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8) channels = 3;
    else if (image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) channels = 4;

#if SUPPORT_IMAGE_EXPORT
    if ((fileType == ".png") || (fileType == ".PNG"))
    {
        fileData = stbi_write_png_to_mem(static_cast<const u8 *>(image.data), image.width*channels, image.width, image.height, channels, dataSize);
    }
#else
    log(LogLevel::warn, "IMAGE: To export image, enable flag SUPPORT_IMAGE_EXPORT");
#endif

    return fileData;
}

// Export image as code file (.h) defining an array of bytes
bool ExportImageAsCode(const Image image, std::string_view fileNameView)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();

    auto result = false;

#ifndef TEXT_BYTES_PER_LINE
    #define TEXT_BYTES_PER_LINE     20
#endif

    const auto dataSize = GetPixelDataSize(image.width, image.height, image.format);

    // NOTE: Text data buffer size is estimated considering image data size in bytes
    // and requiring 6 char bytes for every byte: "0x00, "
    auto txtData = static_cast<char *>(calloc(dataSize*6 + 2000, sizeof(char)));

    auto byteCount = 0;
    byteCount += sprintf(txtData + byteCount, "////////////////////////////////////////////////////////////////////////////////////////\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// ImageAsCode exporter v1.0 - Image pixel data exported as an array of bytes         //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// more info and bugs-report:  github.com/raysan5/raylib                              //\n");
    byteCount += sprintf(txtData + byteCount, "// feedback and support:       ray[at]raylib.com                                      //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// Copyright (c) 2018-2026 Ramon Santamaria (@raysan5)                                //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "////////////////////////////////////////////////////////////////////////////////////////\n\n");

    // Get file name from path and convert variable name to uppercase
    char varFileName[256] = { 0 };
    snprintf(varFileName, 256, "%s", GetFileNameWithoutExt(fileName).c_str()); // NOTE: Using function provided by [rcore] module
    for (i32 i = 0; varFileName[i] != '\0'; i++) if ((varFileName[i] >= 'a') && (varFileName[i] <= 'z')) { varFileName[i] = static_cast<char>(varFileName[i] - 32); }

    // Add image information
    byteCount += sprintf(txtData + byteCount, "// Image data information\n");
    byteCount += sprintf(txtData + byteCount, "#define %s_WIDTH    %i\n", varFileName, image.width);
    byteCount += sprintf(txtData + byteCount, "#define %s_HEIGHT   %i\n", varFileName, image.height);
    byteCount += sprintf(txtData + byteCount, "#define %s_FORMAT   %i          // raylib internal pixel format\n\n", varFileName, image.format);

    byteCount += sprintf(txtData + byteCount, "static unsigned char %s_DATA[%i] = { ", varFileName, dataSize);
    for (i32 i = 0; i < dataSize - 1; i++) byteCount += sprintf(txtData + byteCount, ((i%TEXT_BYTES_PER_LINE == 0)? "0x%x,\n" : "0x%x, "), (static_cast<u8 *>(image.data))[i]);
    byteCount += sprintf(txtData + byteCount, "0x%x };\n", (static_cast<u8 *>(image.data))[dataSize - 1]);

    // NOTE: Text data size exported is determined by '\0' (NULL) character
    result = SaveFileText(fileName, txtData);

    free(txtData);

    if (result != 0) log(LogLevel::info, "FILEIO: [{}] Image as code exported successfully", fileName);
    else log(LogLevel::warn, "FILEIO: [{}] Failed to export image as code", fileName);

    return result;
}

//------------------------------------------------------------------------------------
// Image generation functions
//------------------------------------------------------------------------------------
// Generate image: plain color
Image GenImageColor(i32 width, i32 height, const Color color)
{
    const auto dataSize = GetPixelDataSize(width, height, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    // NOTE: calloc(0) is implementation-defined (may return NULL), so allocate at least one pixel
    auto pixels = static_cast<Color *>(calloc((dataSize > 0) ? dataSize : 1, 1));

    for (i32 i = 0; (pixels != nullptr) && (i < width*height); i++) pixels[i] = color;

    Image image = {
        .data = pixels,
        .width = width,
        .height = height,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };

    return image;
}

#if SUPPORT_IMAGE_GENERATION
// Generate image: linear gradient
// The direction value specifies the direction of the gradient (in degrees)
// with 0 being vertical (from top to bottom), 90 being horizontal (from left to right)
// The gradient effectively rotates counter-clockwise by the specified amount
Image GenImageGradientLinear(i32 width, i32 height, const i32 direction, const Color start, const Color end)
{
    auto pixels = static_cast<Color *>(calloc(width*height, sizeof(Color)));

    const f32 radianDirection = static_cast<f32>(90 - direction)/180.f*3.14159f;
    const f32 cosDir = cosf(radianDirection);
    const f32 sinDir = sinf(radianDirection);

    // Calculate how far the top-left pixel is along the gradient direction from the center of said gradient
    const f32 startingPos = 0.5f - (cosDir*static_cast<f32>(width)/2) - (sinDir*static_cast<f32>(height)/2);

    // With directions that lie in the first or third quadrant (i.e. from top-left to
    // bottom-right or vice-versa), pixel (0, 0) is the farthest point on the gradient
    // (i.e. the pixel which should become one of the gradient's ends color); while for
    // directions that lie in the second or fourth quadrant, that point is pixel (width, 0)
    const f32 maxPosValue = ((signbit(sinDir) != 0) == (signbit(cosDir) != 0))? fabsf(startingPos) : fabsf(startingPos + static_cast<f32>(width)*cosDir);
    for (i32 i = 0; i < width; i++)
    {
        for (i32 j = 0; j < height; j++)
        {
            // Calculate the relative position of the pixel along the gradient direction
            const f32 pos = (startingPos + (static_cast<f32>(i)*cosDir + static_cast<f32>(j)*sinDir))/maxPosValue;

            f32 factor = pos;
            factor = (factor > 1.0f)? 1.0f : factor;    // Clamp to [-1,1]
            factor = (factor < -1.0f)? -1.0f : factor;  // Clamp to [-1,1]
            factor = factor/2.0f + 0.5f;

            // Generate the color for this pixel
            pixels[j*width + i].r = static_cast<i32>(static_cast<f32>(end.r)*factor + static_cast<f32>(start.r)*(1.0f - factor));
            pixels[j*width + i].g = static_cast<i32>(static_cast<f32>(end.g)*factor + static_cast<f32>(start.g)*(1.0f - factor));
            pixels[j*width + i].b = static_cast<i32>(static_cast<f32>(end.b)*factor + static_cast<f32>(start.b)*(1.0f - factor));
            pixels[j*width + i].a = static_cast<i32>(static_cast<f32>(end.a)*factor + static_cast<f32>(start.a)*(1.0f - factor));
        }
    }

    Image image = {
        .data = pixels,
        .width = width,
        .height = height,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };

    return image;
}

// Generate image: radial gradient
Image GenImageGradientRadial(i32 width, i32 height, const f32 density, const Color inner, const Color outer)
{
    auto pixels = static_cast<Color *>(calloc(width*height, sizeof(Color)));
    const f32 radius = (width < height)? static_cast<f32>(width)/2.0f : static_cast<f32>(height)/2.0f;

    const f32 centerX = static_cast<f32>(width)/2.0f;
    const f32 centerY = static_cast<f32>(height)/2.0f;

    for (i32 y = 0; y < height; y++)
    {
        for (i32 x = 0; x < width; x++)
        {
            const f32 dist = hypotf(static_cast<f32>(x) - centerX, static_cast<f32>(y) - centerY);
            f32 factor = (dist - radius*density)/(radius*(1.0f - density));

            factor = fmax(factor, 0.0f);
            factor = fmin(factor, 1.f); // Distance can be bigger than radius, so it needs to be checked

            pixels[y*width + x].r = static_cast<i32>(static_cast<f32>(outer.r)*factor + static_cast<f32>(inner.r)*(1.0f - factor));
            pixels[y*width + x].g = static_cast<i32>(static_cast<f32>(outer.g)*factor + static_cast<f32>(inner.g)*(1.0f - factor));
            pixels[y*width + x].b = static_cast<i32>(static_cast<f32>(outer.b)*factor + static_cast<f32>(inner.b)*(1.0f - factor));
            pixels[y*width + x].a = static_cast<i32>(static_cast<f32>(outer.a)*factor + static_cast<f32>(inner.a)*(1.0f - factor));
        }
    }

    Image image = {
        .data = pixels,
        .width = width,
        .height = height,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };

    return image;
}

// Generate image: square gradient
Image GenImageGradientSquare(i32 width, i32 height, const f32 density, const Color inner, const Color outer)
{
    auto pixels = static_cast<Color *>(calloc(width*height, sizeof(Color)));

    const f32 centerX = static_cast<f32>(width)/2.0f;
    const f32 centerY = static_cast<f32>(height)/2.0f;

    for (i32 y = 0; y < height; y++)
    {
        for (i32 x = 0; x < width; x++)
        {
            // Calculate the Manhattan distance from the center
            const f32 distX = fabsf(static_cast<f32>(x) - centerX);
            const f32 distY = fabsf(static_cast<f32>(y) - centerY);

            // Normalize the distances by the dimensions of the gradient rectangle
            const f32 normalizedDistX = distX/centerX;
            const f32 normalizedDistY = distY/centerY;

            // Calculate the total normalized Manhattan distance
            const f32 manhattanDist = fmaxf(normalizedDistX, normalizedDistY);

            // Subtract the density from the manhattanDist, then divide by (1 - density)
            // This makes the gradient start from the center when density is 0, and from the edge when density is 1
            f32 factor = (manhattanDist - density)/(1.0f - density);

            // Clamp the factor between 0 and 1
            factor = fminf(fmaxf(factor, 0.0f), 1.0f);

            // Blend the colors based on the calculated factor
            pixels[y*width + x].r = static_cast<i32>(static_cast<f32>(outer.r)*factor + static_cast<f32>(inner.r)*(1.0f - factor));
            pixels[y*width + x].g = static_cast<i32>(static_cast<f32>(outer.g)*factor + static_cast<f32>(inner.g)*(1.0f - factor));
            pixels[y*width + x].b = static_cast<i32>(static_cast<f32>(outer.b)*factor + static_cast<f32>(inner.b)*(1.0f - factor));
            pixels[y*width + x].a = static_cast<i32>(static_cast<f32>(outer.a)*factor + static_cast<f32>(inner.a)*(1.0f - factor));
        }
    }

    Image image = {
        .data = pixels,
        .width = width,
        .height = height,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };

    return image;
}

// Generate image: checked
Image GenImageChecked(i32 width, i32 height, const i32 checksX, const i32 checksY, const Color col1, const Color col2)
{
    auto pixels = static_cast<Color *>(calloc(width*height, sizeof(Color)));

    for (i32 y = 0; y < height; y++)
    {
        for (i32 x = 0; x < width; x++)
        {
            if ((x/checksX + y/checksY)%2 == 0) pixels[y*width + x] = col1;
            else pixels[y*width + x] = col2;
        }
    }

    Image image = {
        .data = pixels,
        .width = width,
        .height = height,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };

    return image;
}

// Generate image: white noise
// NOTE: It requires GetRandomValue(), defined in [rcore]
Image GenImageWhiteNoise(i32 width, i32 height, const f32 factor)
{
    auto pixels = static_cast<Color *>(calloc(width*height, sizeof(Color)));

    for (i32 i = 0; i < width*height; i++)
    {
        if (GetRandomValue(0, 99) < static_cast<i32>(factor*100.0f)) pixels[i] = WHITE;
        else pixels[i] = BLACK;
    }

    Image image = {
        .data = pixels,
        .width = width,
        .height = height,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };

    return image;
}

// Generate image: perlin noise
Image GenImagePerlinNoise(i32 width, i32 height, const i32 offsetX, const i32 offsetY, const f32 scale)
{
    auto pixels = static_cast<Color *>(calloc(width*height, sizeof(Color)));

    const f32 aspectRatio = static_cast<f32>(width)/static_cast<f32>(height);

    for (i32 y = 0; y < height; y++)
    {
        for (i32 x = 0; x < width; x++)
        {
            f32 nx = static_cast<f32>(x + offsetX)*(scale/static_cast<f32>(width));
            f32 ny = static_cast<f32>(y + offsetY)*(scale/static_cast<f32>(height));

            // Apply aspect ratio compensation to wider side
            if (width > height) nx *= aspectRatio;
            else ny /= aspectRatio;

            // Basic perlin noise implementation (not used)
            //float p = (stb_perlin_noise3(nx, ny, 0.0f, 0, 0, 0);

            // Calculate a better perlin noise using fbm (fractal brownian motion)
            // Typical values to start playing with:
            //   lacunarity = ~2.0   -- spacing between successive octaves (use exactly 2.0 for wrapping output)
            //   gain       =  0.5   -- relative weighting applied to each successive octave
            //   octaves    =  6     -- number of "octaves" of noise3() to sum
            f32 p = stb_perlin_fbm_noise3(nx, ny, 1.0f, 2.0f, 0.5f, 6);

            // Clamp between -1.0f and 1.0f
            if (p < -1.0f) p = -1.0f;
            if (p > 1.0f) p = 1.0f;

            // Data needs to be normalized from [-1..1] to [0..1]
            const f32 np = (p + 1.0f)/2.0f;

            const auto intensity = static_cast<u8>(np*255.0f);
            pixels[y*width + x] = Color{ intensity, intensity, intensity, 255 };
        }
    }

    Image image = {
        .data = pixels,
        .width = width,
        .height = height,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };

    return image;
}

// Generate image: cellular algorithm. Bigger tileSize means bigger cells
Image GenImageCellular(i32 width, i32 height, const i32 tileSize)
{
    auto pixels = static_cast<Color *>(calloc(width*height, sizeof(Color)));

    const i32 seedsPerRow = width/tileSize;
    const i32 seedsPerCol = height/tileSize;
    const i32 seedCount = seedsPerRow*seedsPerCol;

    auto seeds = static_cast<vec2 *>(calloc(seedCount, sizeof(vec2)));

    for (i32 i = 0; i < seedCount; i++)
    {
        const i32 y = (i/seedsPerRow)*tileSize + GetRandomValue(0, tileSize - 1);
        const i32 x = (i%seedsPerRow)*tileSize + GetRandomValue(0, tileSize - 1);
        seeds[i] = vec2{ static_cast<f32>(x), static_cast<f32>(y) };
    }

    for (i32 y = 0; y < height; y++)
    {
        const i32 tileY = y/tileSize;

        for (i32 x = 0; x < width; x++)
        {
            const i32 tileX = x/tileSize;

            auto minDistance = 65536.0f; //(float)strtod("Inf", NULL);

            // Check all adjacent tiles
            for (i32 i = -1; i < 2; i++)
            {
                if ((tileX + i < 0) || (tileX + i >= seedsPerRow)) continue;

                for (i32 j = -1; j < 2; j++)
                {
                    if ((tileY + j < 0) || (tileY + j >= seedsPerCol)) continue;

                    const vec2 neighborSeed = seeds[(tileY + j)*seedsPerRow + tileX + i];

                    const f32 dist = static_cast<f32>(hypot(x - static_cast<i32>(neighborSeed.x), y - static_cast<i32>(neighborSeed.y)));
                    minDistance = fmin(minDistance, dist);
                }
            }

            // This approach seems to give good results at all tile sizes
            auto intensity = static_cast<i32>(minDistance*256.0f/static_cast<f32>(tileSize));
            if (intensity > 255) intensity = 255;

            const auto intensityUC = static_cast<u8>(intensity);
            pixels[y*width + x] = Color{ intensityUC, intensityUC, intensityUC, 255 };
        }
    }

    free(seeds);

    Image image = {
        .data = pixels,
        .width = width,
        .height = height,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };

    return image;
}

// Generate image: grayscale image from text data
Image GenImageText(i32 width, i32 height, std::string_view textView)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();

    auto image = Image{ nullptr };

    const i32 imageSize = width*height;
    image.width = width;
    image.height = height;
    image.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;
    image.data = calloc(imageSize, 1);
    image.mipmaps = 1;

    if (text != nullptr)
    {
        const auto textLength = static_cast<i32>(strlen(text));
        memcpy(image.data, text, (textLength > imageSize)? imageSize : textLength);
    }

    return image;
}
#endif // SUPPORT_IMAGE_GENERATION

//------------------------------------------------------------------------------------
// Image manipulation functions
//------------------------------------------------------------------------------------
// Copy an image to a new image
Image ImageCopy(const Image image)
{
    auto newImage = Image{ nullptr };

    i32 width = image.width;
    i32 height = image.height;
    auto size = 0;

    for (i32 i = 0; i < image.mipmaps; i++)
    {
        size += GetPixelDataSize(width, height, image.format);

        width /= 2;
        height /= 2;

        // Security check for NPOT textures
        if (width < 1) width = 1;
        if (height < 1) height = 1;
    }

    // NOTE: calloc(0) is implementation-defined (may return NULL), so allocate at least one byte
    newImage.data = calloc((size > 0) ? size : 1, 1);

    if (newImage.data != nullptr)
    {
        // NOTE: Size must be provided in bytes
        memcpy(newImage.data, image.data, size);

        newImage.width = image.width;
        newImage.height = image.height;
        newImage.mipmaps = image.mipmaps;
        newImage.format = image.format;
    }

    return newImage;
}

// Create an image from another image piece
Image ImageFromImage(const Image image, const Rectangle rec)
{
    auto result = Image{ nullptr };

    // Security check to avoid program crash
    if ((image.data == nullptr) || (image.width == 0) || (image.height == 0)) return result;

    if (image.format < PIXELFORMAT_COMPRESSED_DXT1_RGB)
    {
        // Basic rectangle validation: size smaller than image size
        if ((rec.x >= 0) && (rec.y >= 0) && (rec.width > 0) && (rec.height > 0) &&
            ((static_cast<i32>(rec.x) + static_cast<i32>(rec.width)) <= image.width) &&
            ((static_cast<i32>(rec.y) + static_cast<i32>(rec.height)) <= image.height))
        {
            const auto bytesPerPixel = GetPixelDataSize(1, 1, image.format);

            result.width = static_cast<i32>(rec.width);
            result.height = static_cast<i32>(rec.height);
            result.data = calloc(static_cast<i32>(rec.width)*static_cast<i32>(rec.height)*bytesPerPixel, 1);
            result.format = image.format;
            result.mipmaps = 1;

            for (i32 y = 0; y < static_cast<i32>(rec.height); y++)
            {
                memcpy((static_cast<u8 *>(result.data)) + y*static_cast<i32>(rec.width)*bytesPerPixel,
                    (static_cast<u8 *>(image.data)) + ((y + static_cast<i32>(rec.y))*image.width + static_cast<i32>(rec.x))*bytesPerPixel,
                    static_cast<i32>(rec.width)*bytesPerPixel);
            }
        }
        else log(LogLevel::warn, "IMAGE: ImageFromImage(), rectangle provided not valid");
    }
    else log(LogLevel::warn, "IMAGE: Image manipulation not supported for compressed formats");

    return result;
}

// Crop an image to area defined by a rectangle
// NOTE: Security checks are performed in case rectangle goes out of bounds
void ImageCrop(Image *image, Rectangle crop)
{
    // Security check to avoid program crash
    if ((image->data == nullptr) || (image->width == 0) || (image->height == 0)) return;

    crop.x = floorf(crop.x);
    crop.y = floorf(crop.y);
    crop.width = floorf(crop.width);
    crop.height = floorf(crop.height);

    // Security checks to validate crop rectangle
    if (crop.x < 0) { crop.width += crop.x; crop.x = 0; }
    if (crop.y < 0) { crop.height += crop.y; crop.y = 0; }
    if ((crop.x + crop.width) > static_cast<f32>(image->width)) crop.width = static_cast<f32>(image->width) - crop.x;
    if ((crop.y + crop.height) > static_cast<f32>(image->height)) crop.height = static_cast<f32>(image->height) - crop.y;
    if ((crop.x > static_cast<f32>(image->width)) || (crop.y > static_cast<f32>(image->height)) || (crop.width < 1) || (crop.height < 1))
    {
        log(LogLevel::warn, "IMAGE: Failed to crop, rectangle out of bounds");
        return;
    }

    const auto cropX = static_cast<i32>(crop.x);
    const auto cropY = static_cast<i32>(crop.y);
    const auto cropWidth = static_cast<i32>(crop.width);
    const auto cropHeight = static_cast<i32>(crop.height);

    if (image->mipmaps > 1) log(LogLevel::warn, "Image manipulation only applied to base mipmap level");
    if (image->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) log(LogLevel::warn, "Image manipulation not supported for compressed formats");
    else
    {
        const auto bytesPerPixel = GetPixelDataSize(1, 1, image->format);

        auto croppedData = static_cast<u8 *>(calloc(cropWidth*cropHeight*bytesPerPixel, 1));

        // OPTION 1: Move cropped data line-by-line
        for (i32 y = cropY, offsetSize = 0; y < cropY + cropHeight; y++)
        {
            memcpy(croppedData + offsetSize, (static_cast<u8 *>(image->data)) + (y*image->width + cropX)*bytesPerPixel, cropWidth*bytesPerPixel);
            offsetSize += (cropWidth*bytesPerPixel);
        }

        /*
        // OPTION 2: Move cropped data pixel-by-pixel or byte-by-byte
        for (int y = cropY; y < cropY + cropHeight; y++)
        {
            for (int x = cropX; x < cropX + cropWidth; x++)
            {
                //memcpy(croppedData + ((y - cropY)*cropWidth + (x - cropX))*bytesPerPixel, ((unsigned char *)image->data) + (y*image->width + x)*bytesPerPixel, bytesPerPixel);
                for (int i = 0; i < bytesPerPixel; i++) croppedData[((y - cropY)*cropWidth + (x - cropX))*bytesPerPixel + i] = ((unsigned char *)image->data)[(y*image->width + x)*bytesPerPixel + i];
            }
        }
        */

        free(image->data);
        image->data = croppedData;
        image->width = cropWidth;
        image->height = cropHeight;
    }
}

// Convert image data to desired format
void ImageFormat(Image *image, const i32 newFormat)
{
    // Security check to avoid program crash
    if ((image->data == nullptr) || (image->width == 0) || (image->height == 0)) return;

    if ((newFormat != 0) && (image->format != newFormat))
    {
        if ((image->format < PIXELFORMAT_COMPRESSED_DXT1_RGB) && (newFormat < PIXELFORMAT_COMPRESSED_DXT1_RGB))
        {
            auto pixels = LoadImageDataNormalized(*image);   // Supports 8 to 32 bit per channel

            free(image->data);      // WARNING! Loosing mipmaps data --> Regenerated at the end
            image->data = nullptr;
            image->format = newFormat;

            switch (image->format)
            {
                case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE:
                {
                    image->data = static_cast<u8 *>(calloc(image->width*image->height, sizeof(u8)));

                    for (i32 i = 0; i < image->width*image->height; i++)
                    {
                        (static_cast<u8 *>(image->data))[i] = static_cast<u8>((pixels[i].x*0.299f + pixels[i].y*0.587f + pixels[i].z*0.114f)*255.0f);
                    }

                } break;
                case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:
                {
                    image->data = static_cast<u8 *>(calloc(image->width*image->height*2, sizeof(u8)));

                    for (i32 i = 0, k = 0; i < image->width*image->height*2; i += 2, k++)
                    {
                        (static_cast<u8 *>(image->data))[i] = static_cast<u8>((pixels[k].x*0.299f + pixels[k].y*0.587f + pixels[k].z*0.114f)*255.0f);
                        (static_cast<u8 *>(image->data))[i + 1] = static_cast<u8>(pixels[k].w*255.0f);
                    }

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R5G6B5:
                {
                    image->data = static_cast<u16 *>(calloc(image->width*image->height, sizeof(u16)));

                    u8 r = 0;
                    u8 g = 0;
                    u8 b = 0;

                    for (i32 i = 0; i < image->width*image->height; i++)
                    {
                        r = static_cast<u8>(round(pixels[i].x*31.0f));
                        g = static_cast<u8>(round(pixels[i].y*63.0f));
                        b = static_cast<u8>(round(pixels[i].z*31.0f));

                        (static_cast<u16 *>(image->data))[i] = static_cast<u16>(r) << 11 | static_cast<u16>(g) << 5 | static_cast<u16>(b);
                    }

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R8G8B8:
                {
                    image->data = static_cast<u8 *>(calloc(image->width*image->height*3, sizeof(u8)));

                    for (i32 i = 0, k = 0; i < image->width*image->height*3; i += 3, k++)
                    {
                        (static_cast<u8 *>(image->data))[i] = static_cast<u8>(pixels[k].x*255.0f);
                        (static_cast<u8 *>(image->data))[i + 1] = static_cast<u8>(pixels[k].y*255.0f);
                        (static_cast<u8 *>(image->data))[i + 2] = static_cast<u8>(pixels[k].z*255.0f);
                    }
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
                {
                    image->data = static_cast<u16 *>(calloc(image->width*image->height, sizeof(u16)));

                    u8 r = 0;
                    u8 g = 0;
                    u8 b = 0;
                    u8 a = 0;

                    for (i32 i = 0; i < image->width*image->height; i++)
                    {
                        r = static_cast<u8>(round(pixels[i].x*31.0f));
                        g = static_cast<u8>(round(pixels[i].y*31.0f));
                        b = static_cast<u8>(round(pixels[i].z*31.0f));
                        a = (pixels[i].w > (static_cast<f32>(PIXELFORMAT_UNCOMPRESSED_R5G5B5A1_ALPHA_THRESHOLD)/255.0f))? 1 : 0;

                        (static_cast<u16 *>(image->data))[i] = static_cast<u16>(r) << 11 | static_cast<u16>(g) << 6 | static_cast<u16>(b) << 1 | static_cast<u16>(a);
                    }

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4:
                {
                    image->data = static_cast<u16 *>(calloc(image->width*image->height, sizeof(u16)));

                    u8 r = 0;
                    u8 g = 0;
                    u8 b = 0;
                    u8 a = 0;

                    for (i32 i = 0; i < image->width*image->height; i++)
                    {
                        r = static_cast<u8>(round(pixels[i].x*15.0f));
                        g = static_cast<u8>(round(pixels[i].y*15.0f));
                        b = static_cast<u8>(round(pixels[i].z*15.0f));
                        a = static_cast<u8>(round(pixels[i].w*15.0f));

                        (static_cast<u16 *>(image->data))[i] = static_cast<u16>(r) << 12 | static_cast<u16>(g) << 8 | static_cast<u16>(b) << 4 | static_cast<u16>(a);
                    }

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8:
                {
                    image->data = static_cast<u8 *>(calloc(image->width*image->height*4, sizeof(u8)));

                    for (i32 i = 0, k = 0; i < image->width*image->height*4; i += 4, k++)
                    {
                        (static_cast<u8 *>(image->data))[i] = static_cast<u8>(pixels[k].x*255.0f);
                        (static_cast<u8 *>(image->data))[i + 1] = static_cast<u8>(pixels[k].y*255.0f);
                        (static_cast<u8 *>(image->data))[i + 2] = static_cast<u8>(pixels[k].z*255.0f);
                        (static_cast<u8 *>(image->data))[i + 3] = static_cast<u8>(pixels[k].w*255.0f);
                    }
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32:
                {
                    // WARNING: Image is converted to GRAYSCALE equivalent 32bit

                    image->data = static_cast<f32 *>(calloc(image->width*image->height, sizeof(f32)));

                    for (i32 i = 0; i < image->width*image->height; i++)
                    {
                        (static_cast<f32 *>(image->data))[i] = (pixels[i].x*0.299f + pixels[i].y*0.587f + pixels[i].z*0.114f);
                    }
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32G32B32:
                {
                    image->data = static_cast<f32 *>(calloc(image->width*image->height*3, sizeof(f32)));

                    for (i32 i = 0, k = 0; i < image->width*image->height*3; i += 3, k++)
                    {
                        (static_cast<f32 *>(image->data))[i] = pixels[k].x;
                        (static_cast<f32 *>(image->data))[i + 1] = pixels[k].y;
                        (static_cast<f32 *>(image->data))[i + 2] = pixels[k].z;
                    }
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32:
                {
                    image->data = static_cast<f32 *>(calloc(image->width*image->height*4, sizeof(f32)));

                    for (i32 i = 0, k = 0; i < image->width*image->height*4; i += 4, k++)
                    {
                        (static_cast<f32 *>(image->data))[i] = pixels[k].x;
                        (static_cast<f32 *>(image->data))[i + 1] = pixels[k].y;
                        (static_cast<f32 *>(image->data))[i + 2] = pixels[k].z;
                        (static_cast<f32 *>(image->data))[i + 3] = pixels[k].w;
                    }
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R16:
                {
                    // WARNING: Image is converted to GRAYSCALE equivalent 16bit

                    image->data = static_cast<u16 *>(calloc(image->width*image->height, sizeof(u16)));

                    for (i32 i = 0; i < image->width*image->height; i++)
                    {
                        (static_cast<u16 *>(image->data))[i] = FloatToHalf((pixels[i].x*0.299f + pixels[i].y*0.587f + pixels[i].z*0.114f));
                    }
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R16G16B16:
                {
                    image->data = static_cast<u16 *>(calloc(image->width*image->height*3, sizeof(u16)));

                    for (i32 i = 0, k = 0; i < image->width*image->height*3; i += 3, k++)
                    {
                        (static_cast<u16 *>(image->data))[i] = FloatToHalf(pixels[k].x);
                        (static_cast<u16 *>(image->data))[i + 1] = FloatToHalf(pixels[k].y);
                        (static_cast<u16 *>(image->data))[i + 2] = FloatToHalf(pixels[k].z);
                    }
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R16G16B16A16:
                {
                    image->data = static_cast<u16 *>(calloc(image->width*image->height*4, sizeof(u16)));

                    for (i32 i = 0, k = 0; i < image->width*image->height*4; i += 4, k++)
                    {
                        (static_cast<u16 *>(image->data))[i] = FloatToHalf(pixels[k].x);
                        (static_cast<u16 *>(image->data))[i + 1] = FloatToHalf(pixels[k].y);
                        (static_cast<u16 *>(image->data))[i + 2] = FloatToHalf(pixels[k].z);
                        (static_cast<u16 *>(image->data))[i + 3] = FloatToHalf(pixels[k].w);
                    }
                } break;
                default: break;
            }

            free(pixels);
            pixels = nullptr;

            // In case original image had mipmaps, generate mipmaps for formatted image
            // NOTE: Original mipmaps are replaced by new ones, if custom mipmaps were used, they are lost
            if (image->mipmaps > 1)
            {
                image->mipmaps = 1;
                if (image->data != nullptr) ImageMipmaps(image);
            }
        }
        else log(LogLevel::warn, "IMAGE: Data format is compressed, can not be converted");
    }
}

// Create an image from text (default font)
Image ImageText(std::string_view textView, i32 fontSize, const Color color)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();

    auto imText = Image{ nullptr };
#if SUPPORT_MODULE_RTEXT
    constexpr auto defaultFontSize = 10;   // Default Font chars height in pixel
    if (fontSize < defaultFontSize) fontSize = defaultFontSize;
    const i32 spacing = fontSize/defaultFontSize;
    imText = ImageTextEx(GetFontDefault(), text, static_cast<f32>(fontSize), static_cast<f32>(spacing), color);   // WARNING: Module required: rtext
#else
    imText = GenImageColor(200, 60, BLACK);     // Generating placeholder black image rectangle
    log(LogLevel::warn, "IMAGE: ImageTextEx() requires module: rtext");
#endif
    return imText;
}

// Create an image from text (custom sprite font)
// WARNING: Module required: rtext
Image ImageTextEx(const Font font, std::string_view textView, const f32 fontSize, const f32 spacing, const Color tint)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();

    auto imText = Image{ nullptr };
#if SUPPORT_MODULE_RTEXT
    if (text == nullptr) return imText;

    const auto textLength = static_cast<i32>(strlen(text)); // Get length of text in bytes
    auto textOffsetX = 0;                                   // Image drawing position X
    auto textOffsetY = 0;                                   // Offset between lines (on linebreak '\n')
    auto lineIndex = 0;                                     // Current line index (on linebreak '\n')

    // NOTE: Text image is generated at font base size, later scaled to desired font size
    const auto imSize = MeasureTextEx(font, text, static_cast<f32>(font.baseSize), spacing);// WARNING: Module required: rtext
    const auto textSize = MeasureTextEx(font, text, fontSize, spacing);

    auto lineBreakCount = 0;         // Number of line breaks ('\n') in text
    for (i32 i = 0; i < textLength; i++)
    {
        if (text[i] == '\n') lineBreakCount++;
    }

    auto lineSpacing = 0.0f;       // Line spacing, as used by MeasureTextEx()
    if (lineBreakCount > 0) lineSpacing = (textSize.y - fontSize)/static_cast<f32>(lineBreakCount) - fontSize;

    const f32 lineStep = static_cast<f32>(font.baseSize) + lineSpacing*static_cast<f32>(font.baseSize)/fontSize;   // Line advance at base size

    // Create image to store text
    imText = GenImageColor(static_cast<i32>(imSize.x), font.baseSize + static_cast<i32>(static_cast<f32>(lineBreakCount)*lineStep), BLANK);

    for (i32 i = 0; i < textLength;)
    {
        // Get next codepoint from byte string and glyph index in font
        auto codepointByteCount = 0;
        const auto codepoint = GetCodepointNext(&text[i], &codepointByteCount);    // WARNING: Module required: rtext
        const auto index = GetGlyphIndex(font, codepoint);                         // WARNING: Module required: rtext

        if (codepoint == '\n')
        {
            lineIndex++;
            textOffsetY = static_cast<i32>(static_cast<f32>(lineIndex)*lineStep);
            textOffsetX = 0;
        }
        else
        {
            if ((codepoint != ' ') && (codepoint != '\t'))
            {
                const auto rec = Rectangle{ static_cast<f32>(textOffsetX + font.glyphs[index].offsetX), static_cast<f32>(textOffsetY + font.glyphs[index].offsetY), font.recs[index].width, font.recs[index].height };
                ImageDrawImagePro(&imText, font.glyphs[index].image, Rectangle{ 0, 0, static_cast<f32>(font.glyphs[index].image.width), static_cast<f32>(font.glyphs[index].image.height) },
                    rec, vec2{ 0 }, 0.0f, tint);
            }

            if (font.glyphs[index].advanceX == 0) textOffsetX += static_cast<i32>(font.recs[index].width + spacing);
            else textOffsetX += font.glyphs[index].advanceX + static_cast<i32>(spacing);
        }

        i += codepointByteCount;   // Move text bytes counter to next codepoint
    }

    // Scale image depending on text size
    if (fontSize != static_cast<f32>(font.baseSize))
    {
        const f32 scaleFactor = fontSize/static_cast<f32>(font.baseSize);
        log(LogLevel::info, "IMAGE: Text scaled by factor: {:.6f}", scaleFactor);

        // Using nearest-neighbor scaling algorithm for default font
        // TODO: Support other scaling mechanism, use flag?
        if (font.texture.id == GetFontDefault().texture.id) ImageResizeNN(&imText, static_cast<i32>(static_cast<f32>(imText.width)*scaleFactor), static_cast<i32>(static_cast<f32>(imText.height)*scaleFactor));
        else ImageResize(&imText, static_cast<i32>(static_cast<f32>(imText.width)*scaleFactor), static_cast<i32>(static_cast<f32>(imText.height)*scaleFactor));
    }
#else
    imText = GenImageColor(200, 60, BLACK);     // Generating placeholder black image rectangle
    log(LogLevel::warn, "IMAGE: ImageTextEx() requires module: rtext");
#endif
    return imText;
}

// Create an image from a selected channel of another image
Image ImageFromChannel(const Image image, i32 selectedChannel)
{
    auto result = Image{ nullptr };

    if ((image.data == nullptr) || (image.width == 0) || (image.height == 0)) return result; // Security check

    // Check selected channel is valid
    if (selectedChannel < 0)
    {
        log(LogLevel::warn, "Channel cannot be negative. Setting channel to 0.");
        selectedChannel = 0;
    }

    if (image.format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE ||
        image.format == PIXELFORMAT_UNCOMPRESSED_R32 ||
        image.format == PIXELFORMAT_UNCOMPRESSED_R16)
    {
        if (selectedChannel > 0)
        {
            log(LogLevel::warn, "This image has only 1 channel. Setting channel to it.");
            selectedChannel = 0;
        }
    }
    else if (image.format == PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA)
    {
        if (selectedChannel > 1)
        {
            log(LogLevel::warn, "This image has only 2 channels. Setting channel to alpha.");
            selectedChannel = 1;
        }
    }
    else if (image.format == PIXELFORMAT_UNCOMPRESSED_R5G6B5 ||
             image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8 ||
             image.format == PIXELFORMAT_UNCOMPRESSED_R32G32B32 ||
             image.format == PIXELFORMAT_UNCOMPRESSED_R16G16B16)
    {
        if (selectedChannel > 2)
        {
            log(LogLevel::warn, "This image has only 3 channels. Setting channel to red.");
            selectedChannel = 0;
        }
    }

    // Check for RGBA formats
    if (selectedChannel > 3)
    {
        log(LogLevel::warn, "ImageFromChannel supports channels 0 to 3 (RGBA). Setting channel to alpha.");
        selectedChannel = 3;
    }

    // TODO: Support R16 and R32 channel sizes, not only 8-bit
    result.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;
    result.height = image.height;
    result.width = image.width;
    result.mipmaps = 1;

    auto pixels = static_cast<u8 *>(calloc(image.width*image.height, sizeof(u8))); // Values from 0 to 255

    if (image.format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) log(LogLevel::warn, "IMAGE: Pixel data retrieval not supported for compressed image formats");
    else
    {
        for (i32 i = 0, k = 0; i < image.width*image.height; i++)
        {
            f32 pixelValue = -1;
            switch (image.format)
            {
                case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE:
                {
                    pixelValue = static_cast<f32>((static_cast<u8 *>(image.data))[i + selectedChannel])/255.0f;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:
                {
                    pixelValue = static_cast<f32>((static_cast<u8 *>(image.data))[k + selectedChannel])/255.0f;
                    k += 2;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
                {
                    const u16 pixel = (static_cast<u16 *>(image.data))[i];

                    if (selectedChannel == 0) pixelValue = static_cast<f32>((pixel & 0b1111100000000000) >> 11)*(1.0f/31);
                    else if (selectedChannel == 1) pixelValue = static_cast<f32>((pixel & 0b0000011111000000) >> 6)*(1.0f/31);
                    else if (selectedChannel == 2) pixelValue = static_cast<f32>((pixel & 0b0000000000111110) >> 1)*(1.0f/31);
                    else if (selectedChannel == 3) pixelValue = ((pixel & 0b0000000000000001) == 0)? 0.0f : 1.0f;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R5G6B5:
                {
                    const u16 pixel = (static_cast<u16 *>(image.data))[i];

                    if (selectedChannel == 0) pixelValue = static_cast<f32>((pixel & 0b1111100000000000) >> 11)*(1.0f/31);
                    else if (selectedChannel == 1) pixelValue = static_cast<f32>((pixel & 0b0000011111100000) >> 5)*(1.0f/63);
                    else if (selectedChannel == 2) pixelValue = static_cast<f32>(pixel & 0b0000000000011111)*(1.0f/31);

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4:
                {
                    const u16 pixel = (static_cast<u16 *>(image.data))[i];

                    if (selectedChannel == 0) pixelValue = static_cast<f32>((pixel & 0b1111000000000000) >> 12)*(1.0f/15);
                    else if (selectedChannel == 1) pixelValue = static_cast<f32>((pixel & 0b0000111100000000) >> 8)*(1.0f/15);
                    else if (selectedChannel == 2) pixelValue = static_cast<f32>((pixel & 0b0000000011110000) >> 4)*(1.0f/15);
                    else if (selectedChannel == 3) pixelValue = static_cast<f32>(pixel & 0b0000000000001111)*(1.0f/15);

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8:
                {
                    pixelValue = static_cast<f32>((static_cast<u8 *>(image.data))[k + selectedChannel])/255.0f;
                    k += 4;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R8G8B8:
                {
                    pixelValue = static_cast<f32>((static_cast<u8 *>(image.data))[k + selectedChannel])/255.0f;
                    k += 3;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32:
                {
                    pixelValue = (static_cast<f32 *>(image.data))[k];
                    k += 1;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32G32B32:
                {
                    pixelValue = (static_cast<f32 *>(image.data))[k + selectedChannel];
                    k += 3;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32:
                {
                    pixelValue = (static_cast<f32 *>(image.data))[k + selectedChannel];
                    k += 4;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R16:
                {
                    pixelValue = HalfToFloat((static_cast<u16 *>(image.data))[k]);
                    k += 1;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R16G16B16:
                {
                    pixelValue = HalfToFloat((static_cast<u16 *>(image.data))[k+selectedChannel]);
                    k += 3;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R16G16B16A16:
                {
                    pixelValue = HalfToFloat((static_cast<u16 *>(image.data))[k + selectedChannel]);
                    k += 4;

                } break;
                default: break;
            }

            pixels[i] = static_cast<u8>(pixelValue*255);
        }
    }

    result.data = pixels;

    return result;
}

// Resize and image to new size using Nearest-Neighbor scaling algorithm
void ImageResizeNN(Image *image, const i32 newWidth, const i32 newHeight)
{
    // Security check to avoid program crash
    if ((image->data == nullptr) || (image->width == 0) || (image->height == 0)) return;

    auto pixels = LoadImageColors(*image);
    auto output = static_cast<Color *>(calloc(newWidth*newHeight, sizeof(Color)));

    // EDIT: added +1 to account for an early rounding problem
    const i32 xRatio = ((image->width << 16)/newWidth) + 1;
    const i32 yRatio = ((image->height << 16)/newHeight) + 1;

    auto x2 = 0;
    auto y2 = 0;
    for (i32 y = 0; y < newHeight; y++)
    {
        for (i32 x = 0; x < newWidth; x++)
        {
            x2 = ((x*xRatio) >> 16);
            y2 = ((y*yRatio) >> 16);

            output[(y*newWidth) + x] = pixels[(y2*image->width) + x2] ;
        }
    }

    const i32 format = image->format;

    free(image->data);

    image->data = output;
    image->width = newWidth;
    image->height = newHeight;
    image->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    ImageFormat(image, format);  // Reformat 32bit RGBA image to original format

    UnloadImageColors(pixels);
}

// Resize and image to new size
// NOTE: Uses stb default scaling filters (both bicubic):
// STBIR_DEFAULT_FILTER_UPSAMPLE    STBIR_FILTER_CATMULLROM
// STBIR_DEFAULT_FILTER_DOWNSAMPLE  STBIR_FILTER_MITCHELL   (high-quality Catmull-Rom)
void ImageResize(Image *image, const i32 newWidth, const i32 newHeight)
{
    // Security check to avoid program crash
    if ((image->data == nullptr) || (image->width == 0) || (image->height == 0)) return;

    // Check if fast path can be used on image scaling
    // It can be for 8 bit per channel images with 1 to 4 channels per pixel
    if ((image->format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE) ||
        (image->format == PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA) ||
        (image->format == PIXELFORMAT_UNCOMPRESSED_R8G8B8) ||
        (image->format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8))
    {
        const auto bytesPerPixel = GetPixelDataSize(1, 1, image->format);
        auto output = static_cast<u8 *>(calloc(newWidth*newHeight*bytesPerPixel, 1));

        switch (image->format)
        {
            case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE: stbir_resize_uint8_linear(static_cast<u8 *>(image->data), image->width, image->height, 0, output, newWidth, newHeight, 0, static_cast<stbir_pixel_layout>(1)); break;
            case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA: stbir_resize_uint8_linear(static_cast<u8 *>(image->data), image->width, image->height, 0, output, newWidth, newHeight, 0, static_cast<stbir_pixel_layout>(2)); break;
            case PIXELFORMAT_UNCOMPRESSED_R8G8B8: stbir_resize_uint8_linear(static_cast<u8 *>(image->data), image->width, image->height, 0, output, newWidth, newHeight, 0, static_cast<stbir_pixel_layout>(3)); break;
            case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8: stbir_resize_uint8_linear(static_cast<u8 *>(image->data), image->width, image->height, 0, output, newWidth, newHeight, 0, static_cast<stbir_pixel_layout>(4)); break;
            default: break;
        }

        free(image->data);
        image->data = output;
        image->width = newWidth;
        image->height = newHeight;
    }
    else
    {
        // Get data as Color pixels array to work with it
        auto pixels = LoadImageColors(*image);
        auto output = static_cast<Color *>(calloc(newWidth*newHeight, sizeof(Color)));

        // NOTE: Color data is cast to (unsigned char *), there shouldn't been any problem...
        stbir_resize_uint8_linear(reinterpret_cast<u8 *>(pixels), image->width, image->height, 0, reinterpret_cast<u8 *>(output), newWidth, newHeight, 0, static_cast<stbir_pixel_layout>(4));

        const i32 format = image->format;

        UnloadImageColors(pixels);
        free(image->data);

        image->data = output;
        image->width = newWidth;
        image->height = newHeight;
        image->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

        ImageFormat(image, format);  // Reformat 32bit RGBA image to original format
    }
}

// Resize canvas and fill with color
// NOTE: Resize offset is relative to the top-left corner of the original image
void ImageResizeCanvas(Image *image, const i32 newWidth, const i32 newHeight, const i32 offsetX, const i32 offsetY, const Color fill)
{
    // Security check to avoid program crash
    if ((image->data == nullptr) || (image->width == 0) || (image->height == 0)) return;

    if (image->mipmaps > 1) log(LogLevel::warn, "Image manipulation only applied to base mipmap level");
    if (image->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) log(LogLevel::warn, "Image manipulation not supported for compressed formats");
    else if ((newWidth != image->width) || (newHeight != image->height))
    {
        auto srcRec = Rectangle{ 0, 0, static_cast<f32>(image->width), static_cast<f32>(image->height) };
        auto dstPos = vec2{ static_cast<f32>(offsetX), static_cast<f32>(offsetY) };

        if (offsetX < 0)
        {
            srcRec.x = static_cast<f32>(-offsetX);
            srcRec.width += static_cast<f32>(offsetX);
            dstPos.x = 0;
        }
        else if ((offsetX + image->width) > newWidth) srcRec.width = static_cast<f32>(newWidth - offsetX);

        if (offsetY < 0)
        {
            srcRec.y = static_cast<f32>(-offsetY);
            srcRec.height += static_cast<f32>(offsetY);
            dstPos.y = 0;
        }
        else if ((offsetY + image->height) > newHeight) srcRec.height = static_cast<f32>(newHeight - offsetY);

        if (static_cast<f32>(newWidth) < srcRec.width) srcRec.width = static_cast<f32>(newWidth);
        if (static_cast<f32>(newHeight) < srcRec.height) srcRec.height = static_cast<f32>(newHeight);

        const auto bytesPerPixel = GetPixelDataSize(1, 1, image->format);
        auto resizedData = static_cast<u8 *>(calloc(newWidth*newHeight*bytesPerPixel, 1));

        // Fill resized canvas with fill color
        // Set first pixel with image->format
        SetPixelColor(resizedData, fill, image->format);

        // Fill remaining bytes of first row
        for (i32 x = 1; x < newWidth; x++)
        {
            memcpy(resizedData + x*bytesPerPixel, resizedData, bytesPerPixel);
        }
        // Copy the first row into the other rows
        for (i32 y = 1; y < newHeight; y++)
        {
            memcpy(resizedData + y*newWidth*bytesPerPixel, resizedData, newWidth*bytesPerPixel);
        }

        // Copy old image to resized canvas
        i32 dstOffsetSize = (static_cast<i32>(dstPos.y)*newWidth + static_cast<i32>(dstPos.x))*bytesPerPixel;

        for (i32 y = 0; y < static_cast<i32>(srcRec.height); y++)
        {
            memcpy(resizedData + dstOffsetSize, (static_cast<u8 *>(image->data)) + ((y + static_cast<i32>(srcRec.y))*image->width + static_cast<i32>(srcRec.x))*bytesPerPixel, static_cast<i32>(srcRec.width)*bytesPerPixel);
            dstOffsetSize += (newWidth*bytesPerPixel);
        }

        free(image->data);
        image->data = resizedData;
        image->width = newWidth;
        image->height = newHeight;
    }
}

// Convert image to POT (power-of-two)
// NOTE: It could be useful on OpenGL ES 2.0 (RPI, HTML5)
void ImageToPOT(Image *image, const Color fill)
{
    // Security check to avoid program crash
    if ((image->data == nullptr) || (image->width == 0) || (image->height == 0)) return;

    // Calculate next power-of-two values
    // NOTE: Add the required amount of pixels at the right and bottom sides of image...
    const i32 potWidth = static_cast<i32>(powf(2, ceilf(logf(static_cast<f32>(image->width))/logf(2))));
    const i32 potHeight = static_cast<i32>(powf(2, ceilf(logf(static_cast<f32>(image->height))/logf(2))));

    // Check if POT texture generation is required (if texture is not already POT)
    if ((potWidth != image->width) || (potHeight != image->height)) ImageResizeCanvas(image, potWidth, potHeight, 0, 0, fill);
}

// Crop image depending on alpha value
// NOTE: Threshold is defined as a percentage: 0.0f -> 1.0f
void ImageAlphaCrop(Image *image, const f32 threshold)
{
    // Security check to avoid program crash
    if ((image->data == nullptr) || (image->width == 0) || (image->height == 0)) return;

    const auto crop = GetImageAlphaBorder(*image, threshold);

    // Crop if rectangle is valid
    if ((static_cast<i32>(crop.width) != 0) && (static_cast<i32>(crop.height) != 0)) ImageCrop(image, crop);
}

// Clear alpha channel to desired color
// NOTE: Threshold defines the alpha limit, 0.0f to 1.0f
void ImageAlphaClear(const Image *image, const Color color, const f32 threshold)
{
    // Security check to avoid program crash
    if ((image->data == nullptr) || (image->width == 0) || (image->height == 0)) return;

    if (image->mipmaps > 1) log(LogLevel::warn, "Image manipulation only applied to base mipmap level");
    if (image->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) log(LogLevel::warn, "Image manipulation not supported for compressed formats");
    else
    {
        switch (image->format)
        {
            case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:
            {
                const auto thresholdValue = static_cast<u8>(threshold*255.0f);
                for (i32 i = 1; i < image->width*image->height*2; i += 2)
                {
                    if ((static_cast<u8 *>(image->data))[i] <= thresholdValue)
                    {
                        (static_cast<u8 *>(image->data))[i - 1] = color.r;
                        (static_cast<u8 *>(image->data))[i] = color.a;
                    }
                }
            } break;
            case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
            {
                const u8 thresholdValue = ((threshold < 0.5f)? 0 : 1);

                const u8 r = static_cast<u8>(round(static_cast<f32>(color.r)/255.0*31.0f));
                const u8 g = static_cast<u8>(round(static_cast<f32>(color.g)/255.0*31.0f));
                const u8 b = static_cast<u8>(round(static_cast<f32>(color.b)/255.0*31.0f));
                const u8 a = (color.a < 128)? 0 : 1;

                for (i32 i = 0; i < image->width*image->height; i++)
                {
                    if (((static_cast<u16 *>(image->data))[i] & 0b0000000000000001) <= thresholdValue)
                    {
                        (static_cast<u16 *>(image->data))[i] = static_cast<u16>(r) << 11 | static_cast<u16>(g) << 6 | static_cast<u16>(b) << 1 | static_cast<u16>(a);
                    }
                }
            } break;
            case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4:
            {
                const auto thresholdValue = static_cast<u8>(threshold*15.0f);

                const u8 r = static_cast<u8>(round(static_cast<f32>(color.r)/255.0*15.0f));
                const u8 g = static_cast<u8>(round(static_cast<f32>(color.g)/255.0*15.0f));
                const u8 b = static_cast<u8>(round(static_cast<f32>(color.b)/255.0*15.0f));
                const u8 a = static_cast<u8>(round(static_cast<f32>(color.a)/255.0*15.0f));

                for (i32 i = 0; i < image->width*image->height; i++)
                {
                    if (((static_cast<u16 *>(image->data))[i] & 0x000f) <= thresholdValue)
                    {
                        (static_cast<u16 *>(image->data))[i] = static_cast<u16>(r) << 12 | static_cast<u16>(g) << 8 | static_cast<u16>(b) << 4 | static_cast<u16>(a);
                    }
                }
            } break;
            case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8:
            {
                const auto thresholdValue = static_cast<u8>(threshold*255.0f);
                for (i32 i = 3; i < image->width*image->height*4; i += 4)
                {
                    if ((static_cast<u8 *>(image->data))[i] <= thresholdValue)
                    {
                        (static_cast<u8 *>(image->data))[i - 3] = color.r;
                        (static_cast<u8 *>(image->data))[i - 2] = color.g;
                        (static_cast<u8 *>(image->data))[i - 1] = color.b;
                        (static_cast<u8 *>(image->data))[i] = color.a;
                    }
                }
            } break;
            case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32:
            {
                for (i32 i = 3; i < image->width*image->height*4; i += 4)
                {
                    if ((static_cast<f32 *>(image->data))[i] <= threshold)
                    {
                        (static_cast<f32 *>(image->data))[i - 3] = static_cast<f32>(color.r)/255.0f;
                        (static_cast<f32 *>(image->data))[i - 2] = static_cast<f32>(color.g)/255.0f;
                        (static_cast<f32 *>(image->data))[i - 1] = static_cast<f32>(color.b)/255.0f;
                        (static_cast<f32 *>(image->data))[i] = static_cast<f32>(color.a)/255.0f;
                    }
                }
            } break;
            case PIXELFORMAT_UNCOMPRESSED_R16G16B16A16:
            {
                for (i32 i = 3; i < image->width*image->height*4; i += 4)
                {
                    if (HalfToFloat((static_cast<u16 *>(image->data))[i]) <= threshold)
                    {
                        (static_cast<u16 *>(image->data))[i - 3] = FloatToHalf(static_cast<f32>(color.r)/255.0f);
                        (static_cast<u16 *>(image->data))[i - 2] = FloatToHalf(static_cast<f32>(color.g)/255.0f);
                        (static_cast<u16 *>(image->data))[i - 1] = FloatToHalf(static_cast<f32>(color.b)/255.0f);
                        (static_cast<u16 *>(image->data))[i] = FloatToHalf(static_cast<f32>(color.a)/255.0f);
                    }
                }
            } break;
            default: break;
        }
    }
}

// Apply alpha mask to image
// NOTE 1: Returned image is GRAY_ALPHA (16bit) or RGBA (32bit)
// NOTE 2: alphaMask should be same size as image
void ImageAlphaMask(Image *image, const Image alphaMask)
{
    if ((image->width != alphaMask.width) || (image->height != alphaMask.height))
    {
        log(LogLevel::warn, "IMAGE: Alpha mask must be same size as image");
    }
    else if (image->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB)
    {
        log(LogLevel::warn, "IMAGE: Alpha mask can not be applied to compressed data formats");
    }
    else
    {
        // Force mask to be Grayscale
        auto mask = ImageCopy(alphaMask);
        if (mask.format != PIXELFORMAT_UNCOMPRESSED_GRAYSCALE) ImageFormat(&mask, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE);

        // In case image is only grayscale, add alpha channel
        if (image->format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE)
        {
            auto data = static_cast<u8 *>(calloc(image->width*image->height*2, 1));

            // Apply alpha mask to alpha channel
            for (i32 i = 0, k = 0; (i < mask.width*mask.height) || (i < image->width*image->height); i++, k += 2)
            {
                data[k] = (static_cast<u8 *>(image->data))[i];
                data[k + 1] = (static_cast<u8 *>(mask.data))[i];
            }

            free(image->data);
            image->data = data;
            image->format = PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA;
        }
        else
        {
            // Convert image to RGBA
            if (image->format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) ImageFormat(image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

            // Apply alpha mask to alpha channel
            for (i32 i = 0, k = 3; (i < mask.width*mask.height) || (i < image->width*image->height); i++, k += 4)
            {
                (static_cast<u8 *>(image->data))[k] = (static_cast<u8 *>(mask.data))[i];
            }
        }

        UnloadImage(mask);
    }
}

// Premultiply alpha channel
void ImageAlphaPremultiply(Image *image)
{
    // Security check to avoid program crash
    if ((image->data == nullptr) || (image->width == 0) || (image->height == 0)) return;

    auto alpha = 0.0f;
    auto pixels = LoadImageColors(*image);

    for (i32 i = 0; i < image->width*image->height; i++)
    {
        if (pixels[i].a == 0)
        {
            pixels[i].r = 0;
            pixels[i].g = 0;
            pixels[i].b = 0;
        }
        else if (pixels[i].a < 255)
        {
            alpha = static_cast<f32>(pixels[i].a)/255.0f;
            pixels[i].r = static_cast<u8>(static_cast<f32>(pixels[i].r)*alpha);
            pixels[i].g = static_cast<u8>(static_cast<f32>(pixels[i].g)*alpha);
            pixels[i].b = static_cast<u8>(static_cast<f32>(pixels[i].b)*alpha);
        }
    }

    free(image->data);

    const i32 format = image->format;
    image->data = pixels;
    image->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    ImageFormat(image, format);
}

// Apply box blur to image
void ImageBlurGaussian(Image *image, const i32 blurSize)
{
    // Security check to avoid program crash
    if ((image->data == nullptr) || (image->width == 0) || (image->height == 0)) return;

    ImageAlphaPremultiply(image);

    auto pixels = LoadImageColors(*image);

    // Loop switches between pixelsCopy1 and pixelsCopy2
    auto pixelsCopy1 = static_cast<vec4 *>(calloc(image->height*image->width, sizeof(vec4)));
    auto pixelsCopy2 = static_cast<vec4 *>(calloc(image->height*image->width, sizeof(vec4)));

    for (i32 i = 0; i < (image->height*image->width); i++)
    {
        pixelsCopy1[i].x = pixels[i].r;
        pixelsCopy1[i].y = pixels[i].g;
        pixelsCopy1[i].z = pixels[i].b;
        pixelsCopy1[i].w = pixels[i].a;
    }

    // Repeated convolution of rectangular window signal by itself converges to a gaussian distribution
    for (i32 j = 0; j < GAUSSIAN_BLUR_ITERATIONS; j++)
    {
        // Horizontal motion blur
        for (i32 row = 0; row < image->height; row++)
        {
            auto avgR = 0.0f;
            auto avgG = 0.0f;
            auto avgB = 0.0f;
            auto avgAlpha = 0.0f;
            i32 convolutionSize = blurSize;

            for (i32 i = 0; i < blurSize; i++)
            {
                avgR += pixelsCopy1[row*image->width + i].x;
                avgG += pixelsCopy1[row*image->width + i].y;
                avgB += pixelsCopy1[row*image->width + i].z;
                avgAlpha += pixelsCopy1[row*image->width + i].w;
            }

            for (i32 x = 0; x < image->width; x++)
            {
                if (x-blurSize-1 >= 0)
                {
                    avgR -= pixelsCopy1[row*image->width + x-blurSize-1].x;
                    avgG -= pixelsCopy1[row*image->width + x-blurSize-1].y;
                    avgB -= pixelsCopy1[row*image->width + x-blurSize-1].z;
                    avgAlpha -= pixelsCopy1[row*image->width + x-blurSize-1].w;
                    convolutionSize--;
                }

                if (x+blurSize < image->width)
                {
                    avgR += pixelsCopy1[row*image->width + x+blurSize].x;
                    avgG += pixelsCopy1[row*image->width + x+blurSize].y;
                    avgB += pixelsCopy1[row*image->width + x+blurSize].z;
                    avgAlpha += pixelsCopy1[row*image->width + x+blurSize].w;
                    convolutionSize++;
                }

                pixelsCopy2[row*image->width + x].x = avgR/static_cast<f32>(convolutionSize);
                pixelsCopy2[row*image->width + x].y = avgG/static_cast<f32>(convolutionSize);
                pixelsCopy2[row*image->width + x].z = avgB/static_cast<f32>(convolutionSize);
                pixelsCopy2[row*image->width + x].w = avgAlpha/static_cast<f32>(convolutionSize);
            }
        }

        // Vertical motion blur
        for (i32 col = 0; col < image->width; col++)
        {
            auto avgR = 0.0f;
            auto avgG = 0.0f;
            auto avgB = 0.0f;
            auto avgAlpha = 0.0f;
            i32 convolutionSize = blurSize;

            for (i32 i = 0; i < blurSize; i++)
            {
                avgR += pixelsCopy2[i*image->width + col].x;
                avgG += pixelsCopy2[i*image->width + col].y;
                avgB += pixelsCopy2[i*image->width + col].z;
                avgAlpha += pixelsCopy2[i*image->width + col].w;
            }

            for (i32 y = 0; y < image->height; y++)
            {
                if (y-blurSize-1 >= 0)
                {
                    avgR -= pixelsCopy2[(y-blurSize-1)*image->width + col].x;
                    avgG -= pixelsCopy2[(y-blurSize-1)*image->width + col].y;
                    avgB -= pixelsCopy2[(y-blurSize-1)*image->width + col].z;
                    avgAlpha -= pixelsCopy2[(y-blurSize-1)*image->width + col].w;
                    convolutionSize--;
                }
                if (y+blurSize < image->height)
                {
                    avgR += pixelsCopy2[(y+blurSize)*image->width + col].x;
                    avgG += pixelsCopy2[(y+blurSize)*image->width + col].y;
                    avgB += pixelsCopy2[(y+blurSize)*image->width + col].z;
                    avgAlpha += pixelsCopy2[(y+blurSize)*image->width + col].w;
                    convolutionSize++;
                }

                pixelsCopy1[y*image->width + col].x = static_cast<u8>(avgR/static_cast<f32>(convolutionSize));
                pixelsCopy1[y*image->width + col].y = static_cast<u8>(avgG/static_cast<f32>(convolutionSize));
                pixelsCopy1[y*image->width + col].z = static_cast<u8>(avgB/static_cast<f32>(convolutionSize));
                pixelsCopy1[y*image->width + col].w = static_cast<u8>(avgAlpha/static_cast<f32>(convolutionSize));
            }
        }
    }

    // Reverse premultiply
    for (i32 i = 0; i < (image->width)*(image->height); i++)
    {
        if (pixelsCopy1[i].w == 0.0f)
        {
            pixels[i].r = 0;
            pixels[i].g = 0;
            pixels[i].b = 0;
            pixels[i].a = 0;
        }
        else if (pixelsCopy1[i].w <= 255.0f)
        {
            const f32 alpha = pixelsCopy1[i].w/255.0f;
            pixels[i].r = static_cast<u8>(fminf(pixelsCopy1[i].x/alpha, 255.0));
            pixels[i].g = static_cast<u8>(fminf(pixelsCopy1[i].y/alpha, 255.0));
            pixels[i].b = static_cast<u8>(fminf(pixelsCopy1[i].z/alpha, 255.0));
            pixels[i].a = static_cast<u8>(pixelsCopy1[i].w);
        }
    }

    const i32 format = image->format;
    free(image->data);
    free(pixelsCopy1);
    free(pixelsCopy2);

    image->data = pixels;
    image->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    ImageFormat(image, format);
}

// Apply custom square convolution kernel to image
// NOTE: The convolution kernel matrix is expected to be square
void ImageKernelConvolution(Image *image, const f32 *kernel, const i32 kernelSize)
{
    if ((image->data == nullptr) || (image->width == 0) || (image->height == 0) || kernel == nullptr) return;

    const i32 kernelWidth = static_cast<i32>(sqrtf(static_cast<f32>(kernelSize)));

    if (kernelWidth*kernelWidth != kernelSize)
    {
        log(LogLevel::warn, "IMAGE: Convolution kernel must be square to be applied");
        return;
    }

    auto pixels = LoadImageColors(*image);
    auto result = static_cast<Color *>(calloc((image->height)*(image->width), sizeof(Color)));

    i32 startRange = 0, endRange = 0;

    if (kernelWidth%2 == 0)
    {
        startRange = -kernelWidth/2;
        endRange = kernelWidth/2;
    }
    else
    {
        startRange = -kernelWidth/2;
        endRange = kernelWidth/2 + 1;
    }

    for (i32 x = 0; x < image->height; x++)
    {
        for (i32 y = 0; y < image->width; y++)
        {
            auto rRes = 0.0f;
            auto gRes = 0.0f;
            auto bRes = 0.0f;
            auto aRes = 0.0f;

            for (i32 xk = startRange; xk < endRange; xk++)
            {
                // Clamp neighbour row and column to image bounds, edge pixels are repeated
                i32 row = x + xk;
                if (row < 0) row = 0;
                if (row > (image->height - 1)) row = image->height - 1;

                for (i32 yk = startRange; yk < endRange; yk++)
                {
                    i32 col = y + yk;
                    if (col < 0) col = 0;
                    if (col > (image->width - 1)) col = image->width - 1;

                    const i32 xkabs = xk + kernelWidth/2;
                    const i32 ykabs = yk + kernelWidth/2;
                    const i32 imgindex = image->width*row + col;

                    rRes += (static_cast<f32>(pixels[imgindex].r))/255.0f*kernel[kernelWidth*xkabs + ykabs];
                    gRes += (static_cast<f32>(pixels[imgindex].g))/255.0f*kernel[kernelWidth*xkabs + ykabs];
                    bRes += (static_cast<f32>(pixels[imgindex].b))/255.0f*kernel[kernelWidth*xkabs + ykabs];
                    aRes += (static_cast<f32>(pixels[imgindex].a))/255.0f*kernel[kernelWidth*xkabs + ykabs];
                }
            }

            if (rRes < 0.0f) rRes = 0.0f;
            if (gRes < 0.0f) gRes = 0.0f;
            if (bRes < 0.0f) bRes = 0.0f;
            if (aRes < 0.0f) aRes = 0.0f;

            if (rRes > 1.0f) rRes = 1.0f;
            if (gRes > 1.0f) gRes = 1.0f;
            if (bRes > 1.0f) bRes = 1.0f;
            if (aRes > 1.0f) aRes = 1.0f;

            result[image->width*x + y].r = static_cast<u8>(rRes*255.0f);
            result[image->width*x + y].g = static_cast<u8>(gRes*255.0f);
            result[image->width*x + y].b = static_cast<u8>(bRes*255.0f);
            result[image->width*x + y].a = static_cast<u8>(aRes*255.0f);
        }
    }

    const i32 format = image->format;
    free(image->data);
    free(pixels);

    image->data = result;
    image->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    ImageFormat(image, format);
}

// Generate all mipmap levels for a provided image
// NOTE 1: Supports POT and NPOT images
// NOTE 2: image.data is scaled to include mipmap levels
// NOTE 3: Mipmaps format is the same as base image
void ImageMipmaps(Image *image)
{
    // Security check to avoid program crash
    if ((image->data == nullptr) || (image->width == 0) || (image->height == 0)) return;

    auto mipCount = 1;                                                    // Required mipmap levels count (including base level)
    i32 mipWidth = image->width;                                         // Base image width
    i32 mipHeight = image->height;                                       // Base image height
    auto mipSize = GetPixelDataSize(mipWidth, mipHeight, image->format);  // Image data size (in bytes)

    // Count mipmap levels required
    while ((mipWidth != 1) || (mipHeight != 1))
    {
        if (mipWidth != 1) mipWidth /= 2;
        if (mipHeight != 1) mipHeight /= 2;

        // Security check for NPOT textures
        if (mipWidth < 1) mipWidth = 1;
        if (mipHeight < 1) mipHeight = 1;

        log(LogLevel::debug, "IMAGE: Next mipmap level: {} x {} - current size {}", mipWidth, mipHeight, mipSize);

        mipCount++;
        mipSize += GetPixelDataSize(mipWidth, mipHeight, image->format);       // Add mipmap size (in bytes)
    }

    if (image->mipmaps < mipCount)
    {
        // Create second buffer and copy data manually to it
        void *temp = calloc(mipSize, 1);
        memcpy(temp, image->data, GetPixelDataSize(image->width, image->height, image->format));
        free(image->data);
        image->data = temp;

        // Pointer to allocated memory point where store next mipmap level data
        auto nextmip = static_cast<u8 *>(image->data);

        mipWidth = image->width;
        mipHeight = image->height;
        mipSize = GetPixelDataSize(mipWidth, mipHeight, image->format);
        auto imCopy = ImageCopy(*image);

        for (i32 i = 1; i < mipCount; i++)
        {
            nextmip += mipSize;

            mipWidth /= 2;
            mipHeight /= 2;

            // Security check for NPOT textures
            if (mipWidth < 1) mipWidth = 1;
            if (mipHeight < 1) mipHeight = 1;

            mipSize = GetPixelDataSize(mipWidth, mipHeight, image->format);

            if (i < image->mipmaps) continue;

            log(LogLevel::debug, "IMAGE: Generating mipmap level: {} ({} x {}) - size: {} - offset: {}", i, mipWidth, mipHeight, mipSize, static_cast<const void *>(nextmip));
            ImageResize(&imCopy, mipWidth, mipHeight); // Uses internally Mitchell cubic downscale filter
            memcpy(nextmip, imCopy.data, mipSize);
        }

        UnloadImage(imCopy);

        image->mipmaps = mipCount;
    }
    else log(LogLevel::warn, "IMAGE: Mipmaps already available");
}

// Dither image data to 16bpp or lower (Floyd-Steinberg dithering)
// NOTE: In case selected bpp do not represent a known 16bit format,
// dithered data is stored in the LSB part of the unsigned short
void ImageDither(Image *image, const i32 rBpp, const i32 gBpp, const i32 bBpp, const i32 aBpp)
{
    // Security check to avoid program crash
    if ((image->data == nullptr) || (image->width == 0) || (image->height == 0)) return;

    if (image->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB)
    {
        log(LogLevel::warn, "IMAGE: Compressed data formats can not be dithered");
        return;
    }

    if ((rBpp + gBpp + bBpp + aBpp) > 16)
    {
        log(LogLevel::warn, "IMAGE: Unsupported dithering bpps ({}bpp), only 16bpp or lower modes supported", (rBpp+gBpp+bBpp+aBpp));
    }
    else
    {
        auto pixels = LoadImageColors(*image);

        free(image->data);      // free old image data

        if ((image->format != PIXELFORMAT_UNCOMPRESSED_R8G8B8) && (image->format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8))
        {
            log(LogLevel::warn, "IMAGE: Format is already 16bpp or lower, dithering could have no effect");
        }

        // Define new image format, check if desired bpp match internal known format
        if ((rBpp == 5) && (gBpp == 6) && (bBpp == 5) && (aBpp == 0)) image->format = PIXELFORMAT_UNCOMPRESSED_R5G6B5;
        else if ((rBpp == 5) && (gBpp == 5) && (bBpp == 5) && (aBpp == 1)) image->format = PIXELFORMAT_UNCOMPRESSED_R5G5B5A1;
        else if ((rBpp == 4) && (gBpp == 4) && (bBpp == 4) && (aBpp == 4)) image->format = PIXELFORMAT_UNCOMPRESSED_R4G4B4A4;
        else
        {
            image->format = 0;
            log(LogLevel::warn, "IMAGE: Unsupported dithered OpenGL internal format: {}bpp (R{}G{}B{}A{})", (rBpp+gBpp+bBpp+aBpp), rBpp, gBpp, bBpp, aBpp);
        }

        // NOTE: Storing the dithered data as unsigned short (16bpp)
        image->data = static_cast<u16 *>(calloc(image->width*image->height, sizeof(u16)));

        Color oldPixel = WHITE;
        Color newPixel = WHITE;

        auto rError = 0;
        auto gError = 0;
        auto bError = 0;
        u16 rPixel = 0;   // Used for 16bit pixel composition
        u16 gPixel = 0;
        u16 bPixel = 0;
        u16 aPixel = 0;

        #define MIN(a,b) (((a)<(b))?(a):(b))

        for (i32 y = 0; y < image->height; y++)
        {
            for (i32 x = 0; x < image->width; x++)
            {
                oldPixel = pixels[y*image->width + x];

                // NOTE: New pixel obtained by bits truncate, it would be better to round values (check ImageFormat())
                newPixel.r = oldPixel.r >> (8 - rBpp);     // R bits
                newPixel.g = oldPixel.g >> (8 - gBpp);     // G bits
                newPixel.b = oldPixel.b >> (8 - bBpp);     // B bits
                newPixel.a = oldPixel.a >> (8 - aBpp);     // A bits (not used on dithering)

                // NOTE: Error must be computed between new and old pixel but using same number of bits,
                // to know how much color precision has been lost
                rError = static_cast<i32>(oldPixel.r) - (newPixel.r << (8 - rBpp));
                gError = static_cast<i32>(oldPixel.g) - (newPixel.g << (8 - gBpp));
                bError = static_cast<i32>(oldPixel.b) - (newPixel.b << (8 - bBpp));

                pixels[y*image->width + x] = newPixel;

                // NOTE: Some cases are out of the array and should be ignored
                if (x < (image->width - 1))
                {
                    pixels[y*image->width + x+1].r = MIN(static_cast<i32>(pixels[y*image->width + x+1].r) + static_cast<i32>((static_cast<f32>(rError)*7.0f/16)), 0xff);
                    pixels[y*image->width + x+1].g = MIN(static_cast<i32>(pixels[y*image->width + x+1].g) + static_cast<i32>((static_cast<f32>(gError)*7.0f/16)), 0xff);
                    pixels[y*image->width + x+1].b = MIN(static_cast<i32>(pixels[y*image->width + x+1].b) + static_cast<i32>((static_cast<f32>(bError)*7.0f/16)), 0xff);
                }

                if ((x > 0) && (y < (image->height - 1)))
                {
                    pixels[(y+1)*image->width + x-1].r = MIN(static_cast<i32>(pixels[(y+1)*image->width + x-1].r) + static_cast<i32>((static_cast<f32>(rError)*3.0f/16)), 0xff);
                    pixels[(y+1)*image->width + x-1].g = MIN(static_cast<i32>(pixels[(y+1)*image->width + x-1].g) + static_cast<i32>((static_cast<f32>(gError)*3.0f/16)), 0xff);
                    pixels[(y+1)*image->width + x-1].b = MIN(static_cast<i32>(pixels[(y+1)*image->width + x-1].b) + static_cast<i32>((static_cast<f32>(bError)*3.0f/16)), 0xff);
                }

                if (y < (image->height - 1))
                {
                    pixels[(y+1)*image->width + x].r = MIN(static_cast<i32>(pixels[(y+1)*image->width + x].r) + static_cast<i32>((static_cast<f32>(rError)*5.0f/16)), 0xff);
                    pixels[(y+1)*image->width + x].g = MIN(static_cast<i32>(pixels[(y+1)*image->width + x].g) + static_cast<i32>((static_cast<f32>(gError)*5.0f/16)), 0xff);
                    pixels[(y+1)*image->width + x].b = MIN(static_cast<i32>(pixels[(y+1)*image->width + x].b) + static_cast<i32>((static_cast<f32>(bError)*5.0f/16)), 0xff);
                }

                if ((x < (image->width - 1)) && (y < (image->height - 1)))
                {
                    pixels[(y+1)*image->width + x+1].r = MIN(static_cast<i32>(pixels[(y+1)*image->width + x+1].r) + static_cast<i32>((static_cast<f32>(rError)*1.0f/16)), 0xff);
                    pixels[(y+1)*image->width + x+1].g = MIN(static_cast<i32>(pixels[(y+1)*image->width + x+1].g) + static_cast<i32>((static_cast<f32>(gError)*1.0f/16)), 0xff);
                    pixels[(y+1)*image->width + x+1].b = MIN(static_cast<i32>(pixels[(y+1)*image->width + x+1].b) + static_cast<i32>((static_cast<f32>(bError)*1.0f/16)), 0xff);
                }

                rPixel = static_cast<u16>(newPixel.r);
                gPixel = static_cast<u16>(newPixel.g);
                bPixel = static_cast<u16>(newPixel.b);
                aPixel = static_cast<u16>(newPixel.a);

                (static_cast<u16 *>(image->data))[y*image->width + x] = (rPixel << (gBpp + bBpp + aBpp)) | (gPixel << (bBpp + aBpp)) | (bPixel << aBpp) | aPixel;
            }
        }

        UnloadImageColors(pixels);
    }
}

// Flip image vertically
void ImageFlipVertical(Image *image)
{
    // Security check to avoid program crash
    if ((image->data == nullptr) || (image->width == 0) || (image->height == 0)) return;

    if (image->mipmaps > 1) log(LogLevel::warn, "Image manipulation only applied to base mipmap level");
    if (image->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) log(LogLevel::warn, "Image manipulation not supported for compressed formats");
    else
    {
        const auto bytesPerPixel = GetPixelDataSize(1, 1, image->format);
        auto flippedData = static_cast<u8 *>(calloc(image->width*image->height*bytesPerPixel, 1));

        for (i32 i = (image->height - 1), offsetSize = 0; i >= 0; i--)
        {
            memcpy(flippedData + offsetSize, (static_cast<u8 *>(image->data)) + i*image->width*bytesPerPixel, image->width*bytesPerPixel);
            offsetSize += image->width*bytesPerPixel;
        }

        free(image->data);
        image->data = flippedData;
    }
}

// Flip image horizontally
void ImageFlipHorizontal(Image *image)
{
    // Security check to avoid program crash
    if ((image->data == nullptr) || (image->width == 0) || (image->height == 0)) return;

    if (image->mipmaps > 1) log(LogLevel::warn, "Image manipulation only applied to base mipmap level");
    if (image->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) log(LogLevel::warn, "Image manipulation not supported for compressed formats");
    else
    {
        const auto bytesPerPixel = GetPixelDataSize(1, 1, image->format);
        auto flippedData = static_cast<u8 *>(calloc(image->width*image->height*bytesPerPixel, 1));

        for (i32 y = 0; y < image->height; y++)
        {
            for (i32 x = 0; x < image->width; x++)
            {
                // OPTION 1: Move pixels with memcpy()
                //memcpy(flippedData + (y*image->width + x)*bytesPerPixel, ((unsigned char *)image->data) + (y*image->width + (image->width - 1 - x))*bytesPerPixel, bytesPerPixel);

                // OPTION 2: Copy data pixel by pixel
                for (i32 i = 0; i < bytesPerPixel; i++) flippedData[(y*image->width + x)*bytesPerPixel + i] = (static_cast<u8 *>(image->data))[(y*image->width + (image->width - 1 - x))*bytesPerPixel + i];
            }
        }

        free(image->data);
        image->data = flippedData;

        /*
        // OPTION 3: Faster implementation (specific for 32bit pixels)
        // NOTE: It does not require additional allocations
        u32 *ptr = (u32 *)image->data;
        for (int y = 0; y < image->height; y++)
        {
            for (int x = 0; x < image->width/2; x++)
            {
                u32 backup = ptr[y*image->width + x];
                ptr[y*image->width + x] = ptr[y*image->width + (image->width - 1 - x)];
                ptr[y*image->width + (image->width - 1 - x)] = backup;
            }
        }
        */
    }
}

// Rotate image in degrees
void ImageRotate(Image *image, const i32 degrees)
{
    // Security check to avoid program crash
    if ((image->data == nullptr) || (image->width == 0) || (image->height == 0)) return;

    if (image->mipmaps > 1) log(LogLevel::warn, "Image manipulation only applied to base mipmap level");
    if (image->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) log(LogLevel::warn, "Image manipulation not supported for compressed formats");
    else
    {
        const f32 rad = static_cast<f32>(degrees)*PI/180.0f;
        const f32 sinRadius = sinf(rad);
        const f32 cosRadius = cosf(rad);

        const auto width = static_cast<i32>(fabsf(static_cast<f32>(image->width)*cosRadius) + fabsf(static_cast<f32>(image->height)*sinRadius));
        const auto height = static_cast<i32>(fabsf(static_cast<f32>(image->height)*cosRadius) + fabsf(static_cast<f32>(image->width)*sinRadius));

        const auto dataSize = GetPixelDataSize(width, height, image->format);
        const auto bytesPerPixel = GetPixelDataSize(1, 1, image->format);
        auto rotatedData = static_cast<u8 *>(calloc(dataSize, 1));

        if (rotatedData != nullptr)
        {
            for (i32 y = 0; y < height; y++)
            {
                for (i32 x = 0; x < width; x++)
                {
                    const f32 oldX = ((static_cast<f32>(x) + 0.5f - static_cast<f32>(width)/2.0f)*cosRadius + (static_cast<f32>(y) + 0.5f - static_cast<f32>(height)/2.0f)*sinRadius) + static_cast<f32>(image->width)/2.0f - 0.5f;
                    const f32 oldY = ((static_cast<f32>(y) + 0.5f - static_cast<f32>(height)/2.0f)*cosRadius - (static_cast<f32>(x) + 0.5f - static_cast<f32>(width)/2.0f)*sinRadius) + static_cast<f32>(image->height)/2.0f - 0.5f;

                    if ((oldX >= -0.5f) && (oldX < static_cast<f32>(image->width) - 0.5f) && (oldY >= -0.5f) && (oldY < static_cast<f32>(image->height) - 0.5f))
                    {
                        const f32 sampleX = (oldX < 0)? 0 : oldX;
                        const f32 sampleY = (oldY < 0)? 0 : oldY;
                        const auto x1 = static_cast<i32>(floorf(sampleX));
                        const auto y1 = static_cast<i32>(floorf(sampleY));
                        const i32 x2 = MIN(x1 + 1, image->width - 1);
                        const i32 y2 = MIN(y1 + 1, image->height - 1);

                        const f32 px = sampleX - static_cast<f32>(x1);
                        const f32 py = sampleY - static_cast<f32>(y1);

                        for (i32 i = 0; i < bytesPerPixel; i++)
                        {
                            const f32 f1 = (static_cast<u8 *>(image->data))[(y1*image->width + x1)*bytesPerPixel + i];
                            const f32 f2 = (static_cast<u8 *>(image->data))[(y1*image->width + x2)*bytesPerPixel + i];
                            const f32 f3 = (static_cast<u8 *>(image->data))[(y2*image->width + x1)*bytesPerPixel + i];
                            const f32 f4 = (static_cast<u8 *>(image->data))[(y2*image->width + x2)*bytesPerPixel + i];

                            const f32 val = f1*(1 - px)*(1 - py) + f2*px*(1 - py) + f3*(1 - px)*py + f4*px*py;

                            rotatedData[(y*width + x)*bytesPerPixel + i] = static_cast<u8>(val + 0.5f);
                        }
                    }
                }
            }

            free(image->data);
            image->data = rotatedData;
            image->width = width;
            image->height = height;
        }
    }
}

// Rotate image clockwise 90deg
void ImageRotateCW(Image *image)
{
    // Security check to avoid program crash
    if ((image->data == nullptr) || (image->width == 0) || (image->height == 0)) return;

    if (image->mipmaps > 1) log(LogLevel::warn, "Image manipulation only applied to base mipmap level");
    if (image->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) log(LogLevel::warn, "Image manipulation not supported for compressed formats");
    else
    {
        const auto bytesPerPixel = GetPixelDataSize(1, 1, image->format);
        auto rotatedData = static_cast<u8 *>(calloc(image->width*image->height*bytesPerPixel, 1));

        for (i32 y = 0; y < image->height; y++)
        {
            for (i32 x = 0; x < image->width; x++)
            {
                //memcpy(rotatedData + (x*image->height + (image->height - y - 1))*bytesPerPixel, ((unsigned char *)image->data) + (y*image->width + x)*bytesPerPixel, bytesPerPixel);
                for (i32 i = 0; i < bytesPerPixel; i++) rotatedData[(x*image->height + (image->height - y - 1))*bytesPerPixel + i] = (static_cast<u8 *>(image->data))[(y*image->width + x)*bytesPerPixel + i];
            }
        }

        free(image->data);
        image->data = rotatedData;
        const i32 width = image->width;
        const i32 height = image-> height;

        image->width = height;
        image->height = width;
    }
}

// Rotate image counter-clockwise 90deg
void ImageRotateCCW(Image *image)
{
    // Security check to avoid program crash
    if ((image->data == nullptr) || (image->width == 0) || (image->height == 0)) return;

    if (image->mipmaps > 1) log(LogLevel::warn, "Image manipulation only applied to base mipmap level");
    if (image->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) log(LogLevel::warn, "Image manipulation not supported for compressed formats");
    else
    {
        const auto bytesPerPixel = GetPixelDataSize(1, 1, image->format);
        auto rotatedData = static_cast<u8 *>(calloc(image->width*image->height*bytesPerPixel, 1));

        for (i32 y = 0; y < image->height; y++)
        {
            for (i32 x = 0; x < image->width; x++)
            {
                //memcpy(rotatedData + (x*image->height + y))*bytesPerPixel, ((unsigned char *)image->data) + (y*image->width + (image->width - x - 1))*bytesPerPixel, bytesPerPixel);
                for (i32 i = 0; i < bytesPerPixel; i++) rotatedData[(x*image->height + y)*bytesPerPixel + i] = (static_cast<u8 *>(image->data))[(y*image->width + (image->width - x - 1))*bytesPerPixel + i];
            }
        }

        free(image->data);
        image->data = rotatedData;
        const i32 width = image->width;
        const i32 height = image-> height;

        image->width = height;
        image->height = width;
    }
}

// Modify image color: tint
void ImageColorTint(Image *image, const Color color)
{
    // Security check to avoid program crash
    if ((image->data == nullptr) || (image->width == 0) || (image->height == 0)) return;

    auto pixels = LoadImageColors(*image);

    for (i32 i = 0; i < image->width*image->height; i++)
    {
        const u8 r = static_cast<u8>((static_cast<i32>(pixels[i].r)*static_cast<i32>(color.r))/255);
        const u8 g = static_cast<u8>((static_cast<i32>(pixels[i].g)*static_cast<i32>(color.g))/255);
        const u8 b = static_cast<u8>((static_cast<i32>(pixels[i].b)*static_cast<i32>(color.b))/255);
        const u8 a = static_cast<u8>((static_cast<i32>(pixels[i].a)*static_cast<i32>(color.a))/255);

        pixels[i].r = r;
        pixels[i].g = g;
        pixels[i].b = b;
        pixels[i].a = a;
    }

    const i32 format = image->format;
    free(image->data);

    image->data = pixels;
    image->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    ImageFormat(image, format);
}

// Modify image color: invert
void ImageColorInvert(Image *image)
{
    // Security check to avoid program crash
    if ((image->data == nullptr) || (image->width == 0) || (image->height == 0)) return;

    auto pixels = LoadImageColors(*image);

    for (i32 i = 0; i < image->width*image->height; i++)
    {
        pixels[i].r = 255 - pixels[i].r;
        pixels[i].g = 255 - pixels[i].g;
        pixels[i].b = 255 - pixels[i].b;
    }

    const i32 format = image->format;
    free(image->data);

    image->data = pixels;
    image->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    ImageFormat(image, format);
}

// Modify image color: grayscale
void ImageColorGrayscale(Image *image)
{
    ImageFormat(image, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE);
}

// Modify image color: contrast
// NOTE: Contrast values between -100 and 100
void ImageColorContrast(Image *image, i32 contrast)
{
    // Security check to avoid program crash
    if ((image->data == nullptr) || (image->width == 0) || (image->height == 0)) return;

    if (contrast < -100) contrast = -100;
    if (contrast > 100) contrast = 100;

    f32 factor = (100.0f + static_cast<f32>(contrast))/100.0f;
    factor *= factor;

    auto pixels = LoadImageColors(*image);

    for (i32 i = 0; i < image->width*image->height; i++)
    {
        f32 pR = static_cast<f32>(pixels[i].r)/255.0f;
        pR -= 0.5f;
        pR *= factor;
        pR += 0.5f;
        pR *= 255;
        if (pR < 0) pR = 0;
        if (pR > 255) pR = 255;

        f32 pG = static_cast<f32>(pixels[i].g)/255.0f;
        pG -= 0.5f;
        pG *= factor;
        pG += 0.5f;
        pG *= 255;
        if (pG < 0) pG = 0;
        if (pG > 255) pG = 255;

        f32 pB = static_cast<f32>(pixels[i].b)/255.0f;
        pB -= 0.5f;
        pB *= factor;
        pB += 0.5f;
        pB *= 255;
        if (pB < 0) pB = 0;
        if (pB > 255) pB = 255;

        pixels[i].r = static_cast<u8>(pR);
        pixels[i].g = static_cast<u8>(pG);
        pixels[i].b = static_cast<u8>(pB);
    }

    const i32 format = image->format;
    free(image->data);

    image->data = pixels;
    image->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    ImageFormat(image, format);
}

// Modify image color: brightness
// NOTE: Brightness values between -255 and 255
void ImageColorBrightness(Image *image, i32 brightness)
{
    // Security check to avoid program crash
    if ((image->data == nullptr) || (image->width == 0) || (image->height == 0)) return;

    if (brightness < -255) brightness = -255;
    if (brightness > 255) brightness = 255;

    auto pixels = LoadImageColors(*image);

    for (i32 i = 0; i < image->width*image->height; i++)
    {
        i32 cR = pixels[i].r + brightness;
        i32 cG = pixels[i].g + brightness;
        i32 cB = pixels[i].b + brightness;

        if (cR < 0) cR = 0;
        if (cR > 255) cR = 255;

        if (cG < 0) cG = 0;
        if (cG > 255) cG = 255;

        if (cB < 0) cB = 0;
        if (cB > 255) cB = 255;

        pixels[i].r = static_cast<u8>(cR);
        pixels[i].g = static_cast<u8>(cG);
        pixels[i].b = static_cast<u8>(cB);
    }

    const i32 format = image->format;
    free(image->data);

    image->data = pixels;
    image->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    ImageFormat(image, format);
}

// Modify image color: replace color
void ImageColorReplace(Image *image, const Color color, const Color replace)
{
    // Security check to avoid program crash
    if ((image->data == nullptr) || (image->width == 0) || (image->height == 0)) return;

    auto pixels = LoadImageColors(*image);

    for (i32 i = 0; i < image->width*image->height; i++)
    {
        if ((pixels[i].r == color.r) &&
            (pixels[i].g == color.g) &&
            (pixels[i].b == color.b) &&
            (pixels[i].a == color.a))
        {
            pixels[i].r = replace.r;
            pixels[i].g = replace.g;
            pixels[i].b = replace.b;
            pixels[i].a = replace.a;
        }
    }

    const i32 format = image->format;
    free(image->data);

    image->data = pixels;
    image->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    // Only convert back to original format if it supported alpha
    if ((format == PIXELFORMAT_UNCOMPRESSED_R8G8B8) ||
        (format == PIXELFORMAT_UNCOMPRESSED_R5G6B5) ||
        (format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE) ||
        (format == PIXELFORMAT_UNCOMPRESSED_R32G32B32) ||
        (format == PIXELFORMAT_UNCOMPRESSED_R16G16B16) ||
        (format == PIXELFORMAT_COMPRESSED_DXT1_RGB) ||
        (format == PIXELFORMAT_COMPRESSED_ETC1_RGB) ||
        (format == PIXELFORMAT_COMPRESSED_ETC2_RGB) ||
        (format == PIXELFORMAT_COMPRESSED_PVRT_RGB)) ImageFormat(image, format);
}

// Load color data from image as a Color array (RGBA - 32bit)
// NOTE: Memory allocated should be freed using UnloadImageColors();
Color *LoadImageColors(const Image image)
{
    if ((image.width == 0) || (image.height == 0)) return nullptr; // Security check

    auto pixels = static_cast<Color *>(calloc(image.width*image.height, sizeof(Color)));

    if (image.format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) log(LogLevel::warn, "IMAGE: Pixel data retrieval not supported for compressed image formats");
    else
    {
        if ((image.format == PIXELFORMAT_UNCOMPRESSED_R32) ||
            (image.format == PIXELFORMAT_UNCOMPRESSED_R32G32B32) ||
            (image.format == PIXELFORMAT_UNCOMPRESSED_R32G32B32A32)) log(LogLevel::warn, "IMAGE: Pixel format converted from 32bit to 8bit per channel");

        if ((image.format == PIXELFORMAT_UNCOMPRESSED_R16) ||
            (image.format == PIXELFORMAT_UNCOMPRESSED_R16G16B16) ||
            (image.format == PIXELFORMAT_UNCOMPRESSED_R16G16B16A16)) log(LogLevel::warn, "IMAGE: Pixel format converted from 16bit to 8bit per channel");

        for (i32 i = 0, k = 0; i < image.width*image.height; i++)
        {
            switch (image.format)
            {
                case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE:
                {
                    pixels[i].r = (static_cast<u8 *>(image.data))[i];
                    pixels[i].g = (static_cast<u8 *>(image.data))[i];
                    pixels[i].b = (static_cast<u8 *>(image.data))[i];
                    pixels[i].a = 255;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:
                {
                    pixels[i].r = (static_cast<u8 *>(image.data))[k];
                    pixels[i].g = (static_cast<u8 *>(image.data))[k];
                    pixels[i].b = (static_cast<u8 *>(image.data))[k];
                    pixels[i].a = (static_cast<u8 *>(image.data))[k + 1];

                    k += 2;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
                {
                    const u16 pixel = (static_cast<u16 *>(image.data))[i];

                    pixels[i].r = static_cast<u8>(static_cast<f32>((pixel & 0b1111100000000000) >> 11)*255.0f/31);
                    pixels[i].g = static_cast<u8>(static_cast<f32>((pixel & 0b0000011111000000) >> 6)*255.0f/31);
                    pixels[i].b = static_cast<u8>(static_cast<f32>((pixel & 0b0000000000111110) >> 1)*255.0f/31);
                    pixels[i].a = static_cast<u8>((pixel & 0b0000000000000001)*255);

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R5G6B5:
                {
                    const u16 pixel = (static_cast<u16 *>(image.data))[i];

                    pixels[i].r = static_cast<u8>(static_cast<f32>((pixel & 0b1111100000000000) >> 11)*255.0f/31);
                    pixels[i].g = static_cast<u8>(static_cast<f32>((pixel & 0b0000011111100000) >> 5)*255.0f/63);
                    pixels[i].b = static_cast<u8>(static_cast<f32>(pixel & 0b0000000000011111)*255.0f/31);
                    pixels[i].a = 255;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4:
                {
                    const u16 pixel = (static_cast<u16 *>(image.data))[i];

                    pixels[i].r = static_cast<u8>(static_cast<f32>((pixel & 0b1111000000000000) >> 12)*(255.0f/15.0f));
                    pixels[i].g = static_cast<u8>(static_cast<f32>((pixel & 0b0000111100000000) >> 8)*(255.0f/15.0f));
                    pixels[i].b = static_cast<u8>(static_cast<f32>((pixel & 0b0000000011110000) >> 4)*(255.0f/15.0f));
                    pixels[i].a = static_cast<u8>(static_cast<f32>(pixel & 0b0000000000001111)*(255.0f/15.0f));

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8:
                {
                    pixels[i].r = (static_cast<u8 *>(image.data))[k];
                    pixels[i].g = (static_cast<u8 *>(image.data))[k + 1];
                    pixels[i].b = (static_cast<u8 *>(image.data))[k + 2];
                    pixels[i].a = (static_cast<u8 *>(image.data))[k + 3];

                    k += 4;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R8G8B8:
                {
                    pixels[i].r = (static_cast<u8 *>(image.data))[k];
                    pixels[i].g = (static_cast<u8 *>(image.data))[k + 1];
                    pixels[i].b = (static_cast<u8 *>(image.data))[k + 2];
                    pixels[i].a = 255;

                    k += 3;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32:
                {
                    pixels[i].r = static_cast<u8>((static_cast<f32 *>(image.data))[k]*255.0f);
                    pixels[i].g = 0;
                    pixels[i].b = 0;
                    pixels[i].a = 255;

                    k += 1;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32G32B32:
                {
                    pixels[i].r = static_cast<u8>((static_cast<f32 *>(image.data))[k]*255.0f);
                    pixels[i].g = static_cast<u8>((static_cast<f32 *>(image.data))[k + 1]*255.0f);
                    pixels[i].b = static_cast<u8>((static_cast<f32 *>(image.data))[k + 2]*255.0f);
                    pixels[i].a = 255;

                    k += 3;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32:
                {
                    pixels[i].r = static_cast<u8>((static_cast<f32 *>(image.data))[k]*255.0f);
                    pixels[i].g = static_cast<u8>((static_cast<f32 *>(image.data))[k + 1]*255.0f);
                    pixels[i].b = static_cast<u8>((static_cast<f32 *>(image.data))[k + 2]*255.0f);
                    pixels[i].a = static_cast<u8>((static_cast<f32 *>(image.data))[k + 3]*255.0f);

                    k += 4;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R16:
                {
                    pixels[i].r = static_cast<u8>(HalfToFloat((static_cast<u16 *>(image.data))[k])*255.0f);
                    pixels[i].g = 0;
                    pixels[i].b = 0;
                    pixels[i].a = 255;

                    k += 1;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R16G16B16:
                {
                    pixels[i].r = static_cast<u8>(HalfToFloat((static_cast<u16 *>(image.data))[k])*255.0f);
                    pixels[i].g = static_cast<u8>(HalfToFloat((static_cast<u16 *>(image.data))[k + 1])*255.0f);
                    pixels[i].b = static_cast<u8>(HalfToFloat((static_cast<u16 *>(image.data))[k + 2])*255.0f);
                    pixels[i].a = 255;

                    k += 3;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R16G16B16A16:
                {
                    pixels[i].r = static_cast<u8>(HalfToFloat((static_cast<u16 *>(image.data))[k])*255.0f);
                    pixels[i].g = static_cast<u8>(HalfToFloat((static_cast<u16 *>(image.data))[k + 1])*255.0f);
                    pixels[i].b = static_cast<u8>(HalfToFloat((static_cast<u16 *>(image.data))[k + 2])*255.0f);
                    pixels[i].a = static_cast<u8>(HalfToFloat((static_cast<u16 *>(image.data))[k + 3])*255.0f);

                    k += 4;
                } break;
                default: break;
            }
        }
    }

    return pixels;
}

// Load colors palette from image as a Color array (RGBA - 32bit)
// NOTE: Memory allocated should be freed using UnloadImagePalette()
Color *LoadImagePalette(const Image image, const i32 maxPaletteSize, i32 *colorCount)
{
    #define COLOR_EQUAL(col1, col2) ((col1.r == col2.r)&&(col1.g == col2.g)&&(col1.b == col2.b)&&(col1.a == col2.a))

    auto palCount = 0;
    Color *palette = nullptr;
    auto pixels = LoadImageColors(image);

    if (pixels != nullptr)
    {
        palette = static_cast<Color *>(calloc(maxPaletteSize, sizeof(Color)));

        for (i32 i = 0; i < maxPaletteSize; i++) palette[i] = BLANK;   // Set all colors to BLANK

        for (i32 i = 0; i < image.width*image.height; i++)
        {
            if (pixels[i].a > 0)
            {
                auto colorInPalette = false;

                // Check if the color is already on palette
                for (i32 j = 0; j < maxPaletteSize; j++)
                {
                    if (COLOR_EQUAL(pixels[i], palette[j]))
                    {
                        colorInPalette = true;
                        break;
                    }
                }

                // Store color if not on the palette
                if (!colorInPalette)
                {
                    palette[palCount] = pixels[i];      // Add pixels[i] to palette
                    palCount++;

                    // Reached the limit of colors supported by palette
                    if (palCount >= maxPaletteSize)
                    {
                        i = image.width*image.height;   // Finish palette get
                        log(LogLevel::warn, "IMAGE: Palette is greater than {} colors", maxPaletteSize);
                    }
                }
            }
        }

        UnloadImageColors(pixels);
    }

    *colorCount = palCount;

    return palette;
}

// Unload color data loaded with LoadImageColors()
void UnloadImageColors(Color *colors)
{
    free(colors);
}

// Unload colors palette loaded with LoadImagePalette()
void UnloadImagePalette(Color *colors)
{
    free(colors);
}

// Get image alpha border rectangle
// NOTE: Threshold is defined as a percentage: 0.0f -> 1.0f
Rectangle GetImageAlphaBorder(const Image image, const f32 threshold)
{
    auto crop = Rectangle{ 0 };

    auto pixels = LoadImageColors(image);

    if (pixels != nullptr)
    {
        auto xMin = 65536;   // Define a big enough number
        auto xMax = 0;
        auto yMin = 65536;
        auto yMax = 0;

        for (i32 y = 0; y < image.height; y++)
        {
            for (i32 x = 0; x < image.width; x++)
            {
                if (pixels[y*image.width + x].a > static_cast<u8>(threshold*255.0f))
                {
                    if (x < xMin) xMin = x;
                    if (x > xMax) xMax = x;
                    if (y < yMin) yMin = y;
                    if (y > yMax) yMax = y;
                }
            }
        }

        // Check for empty blank image
        if ((xMin != 65536) && (xMax != 65536))
        {
            crop = Rectangle{ static_cast<f32>(xMin), static_cast<f32>(yMin), static_cast<f32>((xMax + 1) - xMin), static_cast<f32>((yMax + 1) - yMin) };
        }

        UnloadImageColors(pixels);
    }

    return crop;
}

// Get image pixel color at (x, y) position
Color GetImageColor(const Image image, const i32 x, const i32 y)
{
    auto color = Color{ 0 };

    if ((x >=0) && (x < image.width) && (y >= 0) && (y < image.height))
    {
        switch (image.format)
        {
            case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE:
            {
                color.r = (static_cast<u8 *>(image.data))[y*image.width + x];
                color.g = (static_cast<u8 *>(image.data))[y*image.width + x];
                color.b = (static_cast<u8 *>(image.data))[y*image.width + x];
                color.a = 255;

            } break;
            case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:
            {
                color.r = (static_cast<u8 *>(image.data))[(y*image.width + x)*2];
                color.g = (static_cast<u8 *>(image.data))[(y*image.width + x)*2];
                color.b = (static_cast<u8 *>(image.data))[(y*image.width + x)*2];
                color.a = (static_cast<u8 *>(image.data))[(y*image.width + x)*2 + 1];

            } break;
            case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
            {
                const u16 pixel = (static_cast<u16 *>(image.data))[y*image.width + x];

                color.r = static_cast<u8>(static_cast<f32>((pixel & 0b1111100000000000) >> 11)*255.0f/31);
                color.g = static_cast<u8>(static_cast<f32>((pixel & 0b0000011111000000) >> 6)*255.0f/31);
                color.b = static_cast<u8>(static_cast<f32>((pixel & 0b0000000000111110) >> 1)*255.0f/31);
                color.a = static_cast<u8>((pixel & 0b0000000000000001)*255);

            } break;
            case PIXELFORMAT_UNCOMPRESSED_R5G6B5:
            {
                const u16 pixel = (static_cast<u16 *>(image.data))[y*image.width + x];

                color.r = static_cast<u8>(static_cast<f32>((pixel & 0b1111100000000000) >> 11)*255.0f/31);
                color.g = static_cast<u8>(static_cast<f32>((pixel & 0b0000011111100000) >> 5)*255.0f/63);
                color.b = static_cast<u8>(static_cast<f32>(pixel & 0b0000000000011111)*255.0f/31);
                color.a = 255;

            } break;
            case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4:
            {
                const u16 pixel = (static_cast<u16 *>(image.data))[y*image.width + x];

                color.r = static_cast<u8>(static_cast<f32>((pixel & 0b1111000000000000) >> 12)*(255.0f/15.0f));
                color.g = static_cast<u8>(static_cast<f32>((pixel & 0b0000111100000000) >> 8)*(255.0f/15.0f));
                color.b = static_cast<u8>(static_cast<f32>((pixel & 0b0000000011110000) >> 4)*(255.0f/15.0f));
                color.a = static_cast<u8>(static_cast<f32>(pixel & 0b0000000000001111)*(255.0f/15.0f));

            } break;
            case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8:
            {
                color.r = (static_cast<u8 *>(image.data))[(y*image.width + x)*4];
                color.g = (static_cast<u8 *>(image.data))[(y*image.width + x)*4 + 1];
                color.b = (static_cast<u8 *>(image.data))[(y*image.width + x)*4 + 2];
                color.a = (static_cast<u8 *>(image.data))[(y*image.width + x)*4 + 3];

            } break;
            case PIXELFORMAT_UNCOMPRESSED_R8G8B8:
            {
                color.r = (static_cast<u8 *>(image.data))[(y*image.width + x)*3];
                color.g = (static_cast<u8 *>(image.data))[(y*image.width + x)*3 + 1];
                color.b = (static_cast<u8 *>(image.data))[(y*image.width + x)*3 + 2];
                color.a = 255;

            } break;
            case PIXELFORMAT_UNCOMPRESSED_R32:
            {
                color.r = static_cast<u8>((static_cast<f32 *>(image.data))[y*image.width + x]*255.0f);
                color.g = 0;
                color.b = 0;
                color.a = 255;

            } break;
            case PIXELFORMAT_UNCOMPRESSED_R32G32B32:
            {
                color.r = static_cast<u8>((static_cast<f32 *>(image.data))[(y*image.width + x)*3]*255.0f);
                color.g = static_cast<u8>((static_cast<f32 *>(image.data))[(y*image.width + x)*3 + 1]*255.0f);
                color.b = static_cast<u8>((static_cast<f32 *>(image.data))[(y*image.width + x)*3 + 2]*255.0f);
                color.a = 255;

            } break;
            case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32:
            {
                color.r = static_cast<u8>((static_cast<f32 *>(image.data))[(y*image.width + x)*4]*255.0f);
                color.g = static_cast<u8>((static_cast<f32 *>(image.data))[(y*image.width + x)*4 + 1]*255.0f);
                color.b = static_cast<u8>((static_cast<f32 *>(image.data))[(y*image.width + x)*4 + 2]*255.0f);
                color.a = static_cast<u8>((static_cast<f32 *>(image.data))[(y*image.width + x)*4 + 3]*255.0f);

            } break;
            case PIXELFORMAT_UNCOMPRESSED_R16:
            {
                color.r = static_cast<u8>(HalfToFloat((static_cast<u16 *>(image.data))[y*image.width + x])*255.0f);
                color.g = 0;
                color.b = 0;
                color.a = 255;

            } break;
            case PIXELFORMAT_UNCOMPRESSED_R16G16B16:
            {
                color.r = static_cast<u8>(HalfToFloat((static_cast<u16 *>(image.data))[(y*image.width + x)*3])*255.0f);
                color.g = static_cast<u8>(HalfToFloat((static_cast<u16 *>(image.data))[(y*image.width + x)*3 + 1])*255.0f);
                color.b = static_cast<u8>(HalfToFloat((static_cast<u16 *>(image.data))[(y*image.width + x)*3 + 2])*255.0f);
                color.a = 255;

            } break;
            case PIXELFORMAT_UNCOMPRESSED_R16G16B16A16:
            {
                color.r = static_cast<u8>(HalfToFloat((static_cast<u16 *>(image.data))[(y*image.width + x)*4])*255.0f);
                color.g = static_cast<u8>(HalfToFloat((static_cast<u16 *>(image.data))[(y*image.width + x)*4 + 1])*255.0f);
                color.b = static_cast<u8>(HalfToFloat((static_cast<u16 *>(image.data))[(y*image.width + x)*4 + 2])*255.0f);
                color.a = static_cast<u8>(HalfToFloat((static_cast<u16 *>(image.data))[(y*image.width + x)*4 + 3])*255.0f);

            } break;
            default: log(LogLevel::warn, "Compressed image format does not support color reading"); break;
        }
    }
    else log(LogLevel::warn, "Requested image pixel ({}, {}) out of bounds", x, y);

    return color;
}

//------------------------------------------------------------------------------------
// Image drawing functions
//------------------------------------------------------------------------------------
// Clear image background with given color
void ImageClearBackground(const Image *dst, const Color color)
{
    // Security check to avoid program crash
    if ((dst->data == nullptr) || (dst->width == 0) || (dst->height == 0)) return;

    // Fill in first pixel based on image format
    ImageDrawPixel(dst, 0, 0, color);

    auto pSrcPixel = static_cast<u8 *>(dst->data);
    const auto bytesPerPixel = GetPixelDataSize(1, 1, dst->format);
    const i32 totalPixels = dst->width*dst->height;

    // Repeat the first pixel data throughout the image,
    // doubling the pixels copied on each iteration
    for (i32 i = 1; i < totalPixels; i *= 2)
    {
        const i32 pixelsToCopy = MIN(i, totalPixels - i);
        memcpy(pSrcPixel + i*bytesPerPixel, pSrcPixel, pixelsToCopy*bytesPerPixel);
    }
}

// Draw pixel within an image
// NOTE: Compressed image formats not supported
void ImageDrawPixel(const Image *dst, const i32 x, const i32 y, const Color color)
{
    // Security check to avoid program crash
    if ((dst->data == nullptr) || (x < 0) || (x >= dst->width) || (y < 0) || (y >= dst->height)) return;

    switch (dst->format)
    {
        case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE:
        {
            // NOTE: Calculate grayscale equivalent color
            const auto coln = vec3{ static_cast<f32>(color.r)/255.0f, static_cast<f32>(color.g)/255.0f, static_cast<f32>(color.b)/255.0f };
            const auto gray = static_cast<u8>((coln.x*0.299f + coln.y*0.587f + coln.z*0.114f)*255.0f);

            (static_cast<u8 *>(dst->data))[y*dst->width + x] = gray;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:
        {
            // NOTE: Calculate grayscale equivalent color
            const auto coln = vec3{ static_cast<f32>(color.r)/255.0f, static_cast<f32>(color.g)/255.0f, static_cast<f32>(color.b)/255.0f };
            const auto gray = static_cast<u8>((coln.x*0.299f + coln.y*0.587f + coln.z*0.114f)*255.0f);

            (static_cast<u8 *>(dst->data))[(y*dst->width + x)*2] = gray;
            (static_cast<u8 *>(dst->data))[(y*dst->width + x)*2 + 1] = color.a;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R5G6B5:
        {
            // NOTE: Calculate R5G6B5 equivalent color
            const auto coln = vec3{ static_cast<f32>(color.r)/255.0f, static_cast<f32>(color.g)/255.0f, static_cast<f32>(color.b)/255.0f };

            const auto r = static_cast<u8>(round(coln.x*31.0f));
            const auto g = static_cast<u8>(round(coln.y*63.0f));
            const auto b = static_cast<u8>(round(coln.z*31.0f));

            (static_cast<u16 *>(dst->data))[y*dst->width + x] = static_cast<u16>(r) << 11 | static_cast<u16>(g) << 5 | static_cast<u16>(b);

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
        {
            // NOTE: Calculate R5G5B5A1 equivalent color
            const auto coln = vec4{ static_cast<f32>(color.r)/255.0f, static_cast<f32>(color.g)/255.0f, static_cast<f32>(color.b)/255.0f, static_cast<f32>(color.a)/255.0f };

            const auto r = static_cast<u8>(round(coln.x*31.0f));
            const auto g = static_cast<u8>(round(coln.y*31.0f));
            const auto b = static_cast<u8>(round(coln.z*31.0f));
            const u8 a = (coln.w > (static_cast<f32>(PIXELFORMAT_UNCOMPRESSED_R5G5B5A1_ALPHA_THRESHOLD)/255.0f))? 1 : 0;

            (static_cast<u16 *>(dst->data))[y*dst->width + x] = static_cast<u16>(r) << 11 | static_cast<u16>(g) << 6 | static_cast<u16>(b) << 1 | static_cast<u16>(a);

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4:
        {
            // NOTE: Calculate R5G5B5A1 equivalent color
            const auto coln = vec4{ static_cast<f32>(color.r)/255.0f, static_cast<f32>(color.g)/255.0f, static_cast<f32>(color.b)/255.0f, static_cast<f32>(color.a)/255.0f };

            const auto r = static_cast<u8>(round(coln.x*15.0f));
            const auto g = static_cast<u8>(round(coln.y*15.0f));
            const auto b = static_cast<u8>(round(coln.z*15.0f));
            const auto a = static_cast<u8>(round(coln.w*15.0f));

            (static_cast<u16 *>(dst->data))[y*dst->width + x] = static_cast<u16>(r) << 12 | static_cast<u16>(g) << 8 | static_cast<u16>(b) << 4 | static_cast<u16>(a);

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8:
        {
            (static_cast<u8 *>(dst->data))[(y*dst->width + x)*3] = color.r;
            (static_cast<u8 *>(dst->data))[(y*dst->width + x)*3 + 1] = color.g;
            (static_cast<u8 *>(dst->data))[(y*dst->width + x)*3 + 2] = color.b;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8:
        {
            (static_cast<u8 *>(dst->data))[(y*dst->width + x)*4] = color.r;
            (static_cast<u8 *>(dst->data))[(y*dst->width + x)*4 + 1] = color.g;
            (static_cast<u8 *>(dst->data))[(y*dst->width + x)*4 + 2] = color.b;
            (static_cast<u8 *>(dst->data))[(y*dst->width + x)*4 + 3] = color.a;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R32:
        {
            // NOTE: Calculate grayscale equivalent color (normalized to 32bit)
            const auto coln = vec3{ static_cast<f32>(color.r)/255.0f, static_cast<f32>(color.g)/255.0f, static_cast<f32>(color.b)/255.0f };

            (static_cast<f32 *>(dst->data))[y*dst->width + x] = coln.x*0.299f + coln.y*0.587f + coln.z*0.114f;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R32G32B32:
        {
            // NOTE: Calculate R32G32B32 equivalent color (normalized to 32bit)
            const auto coln = vec3{ static_cast<f32>(color.r)/255.0f, static_cast<f32>(color.g)/255.0f, static_cast<f32>(color.b)/255.0f };

            (static_cast<f32 *>(dst->data))[(y*dst->width + x)*3] = coln.x;
            (static_cast<f32 *>(dst->data))[(y*dst->width + x)*3 + 1] = coln.y;
            (static_cast<f32 *>(dst->data))[(y*dst->width + x)*3 + 2] = coln.z;
        } break;
        case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32:
        {
            // NOTE: Calculate R32G32B32A32 equivalent color (normalized to 32bit)
            const auto coln = vec4{ static_cast<f32>(color.r)/255.0f, static_cast<f32>(color.g)/255.0f, static_cast<f32>(color.b)/255.0f, static_cast<f32>(color.a)/255.0f };

            (static_cast<f32 *>(dst->data))[(y*dst->width + x)*4] = coln.x;
            (static_cast<f32 *>(dst->data))[(y*dst->width + x)*4 + 1] = coln.y;
            (static_cast<f32 *>(dst->data))[(y*dst->width + x)*4 + 2] = coln.z;
            (static_cast<f32 *>(dst->data))[(y*dst->width + x)*4 + 3] = coln.w;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R16:
        {
            // NOTE: Calculate grayscale equivalent color (normalized to 32bit)
            const auto coln = vec3{ static_cast<f32>(color.r)/255.0f, static_cast<f32>(color.g)/255.0f, static_cast<f32>(color.b)/255.0f };

            (static_cast<u16 *>(dst->data))[y*dst->width + x] = FloatToHalf(coln.x*0.299f + coln.y*0.587f + coln.z*0.114f);

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R16G16B16:
        {
            // NOTE: Calculate R32G32B32 equivalent color (normalized to 32bit)
            const auto coln = vec3{ static_cast<f32>(color.r)/255.0f, static_cast<f32>(color.g)/255.0f, static_cast<f32>(color.b)/255.0f };

            (static_cast<u16 *>(dst->data))[(y*dst->width + x)*3] = FloatToHalf(coln.x);
            (static_cast<u16 *>(dst->data))[(y*dst->width + x)*3 + 1] = FloatToHalf(coln.y);
            (static_cast<u16 *>(dst->data))[(y*dst->width + x)*3 + 2] = FloatToHalf(coln.z);
        } break;
        case PIXELFORMAT_UNCOMPRESSED_R16G16B16A16:
        {
            // NOTE: Calculate R32G32B32A32 equivalent color (normalized to 32bit)
            const auto coln = vec4{ static_cast<f32>(color.r)/255.0f, static_cast<f32>(color.g)/255.0f, static_cast<f32>(color.b)/255.0f, static_cast<f32>(color.a)/255.0f };

            (static_cast<u16 *>(dst->data))[(y*dst->width + x)*4] = FloatToHalf(coln.x);
            (static_cast<u16 *>(dst->data))[(y*dst->width + x)*4 + 1] = FloatToHalf(coln.y);
            (static_cast<u16 *>(dst->data))[(y*dst->width + x)*4 + 2] = FloatToHalf(coln.z);
            (static_cast<u16 *>(dst->data))[(y*dst->width + x)*4 + 3] = FloatToHalf(coln.w);

        } break;
        default: break;
    }
}

// Draw pixel within an image (Vector version)
void ImageDrawPixelV(const Image *dst, const vec2 position, const Color color)
{
    ImageDrawPixel(dst, static_cast<i32>(position.x), static_cast<i32>(position.y), color);
}

// Draw line within an image
void ImageDrawLine(const Image *dst, const i32 startPosX, const i32 startPosY, const i32 endPosX, const i32 endPosY, const Color color)
{
    // Calculate differences in coordinates
    i32 shortLen = endPosY - startPosY;
    i32 longLen = endPosX - startPosX;
    auto yLonger = false;

    // Determine if the line is more vertical than horizontal
    if (abs(shortLen) > abs(longLen))
    {
        // Swap the lengths if the line is more vertical
        const i32 temp = shortLen;
        shortLen = longLen;
        longLen = temp;
        yLonger = true;
    }

    // Initialize variables for drawing loop
    i32 endVal = longLen + 1;
    auto sgnInc = 1;

    // Adjust direction increment based on longLen sign
    if (longLen < 0)
    {
        longLen = -longLen;
        sgnInc = -1;
        endVal -= 2;
    }

    // Calculate fixed-point increment for shorter length
    const i32 decInc = (longLen == 0)? 0 : (shortLen << 16)/longLen;

    // Draw the line pixel by pixel
    if (yLonger)
    {
        // If line is more vertical, iterate over y-axis
		// Init j with 0.5 in 16-bit fixed point (1 << 15) for better rounding.
        for (i32 i = 0, j = (1 << 15); i != endVal; i += sgnInc, j += decInc)
        {
            // Calculate pixel position and draw it
            ImageDrawPixel(dst, startPosX + (j >> 16), startPosY + i, color);
        }
    }
    else
    {
        // If line is more horizontal, iterate over x-axis
        for (i32 i = 0, j = (1 << 15); i != endVal; i += sgnInc, j += decInc)
        {
            // Calculate pixel position and draw it
            ImageDrawPixel(dst, startPosX + i, startPosY + (j >> 16), color);
        }
    }
}

// Draw line within an image (Vector version)
void ImageDrawLineV(const Image *dst, const vec2 start, const vec2 end, const Color color)
{
    // Round start and end positions to nearest integer coordinates
    // NOTE: round() is used instead of "(i32)(x + 0.5f)": the latter truncates towards
    // zero and therefore rounds negative coordinates the wrong way (-1.2 became 0, not -1)
    const auto x1 = static_cast<i32>(round(start.x));
    const auto y1 = static_cast<i32>(round(start.y));
    const auto x2 = static_cast<i32>(round(end.x));
    const auto y2 = static_cast<i32>(round(end.y));

    // Draw a vertical line using ImageDrawLine function
    ImageDrawLine(dst, x1, y1, x2, y2, color);
}

// Draw a line defining thickness within an image
void ImageDrawLineEx(const Image *dst, const vec2 start, const vec2 end, const i32 thick, const Color color)
{
    // Round start and end positions to nearest integer coordinates
    // NOTE: round() is used instead of "(i32)(x + 0.5f)": the latter truncates towards
    // zero and therefore rounds negative coordinates the wrong way (-1.2 became 0, not -1)
    const auto x1 = static_cast<i32>(round(start.x));
    const auto y1 = static_cast<i32>(round(start.y));
    const auto x2 = static_cast<i32>(round(end.x));
    const auto y2 = static_cast<i32>(round(end.y));

    // Calculate differences in x and y coordinates
    const i32 dx = x2 - x1;
    const i32 dy = y2 - y1;

    // Determine if the line is more horizontal or vertical
    if ((dx != 0) && (abs(dy/dx) < 1))
    {
        // Line is more horizontal

        // How many additional lines to draw
        const i32 wy = thick - 1;

        // Draw the main line and lower half
        for (i32 i = 0; i <= ((wy+1)/2); i++)
        {
            ImageDrawLine(dst, x1, y1 + i, x2, y2 + i, color);
        }

        // Draw the upper half
        for (i32 i = 1; i <= (wy/2); i++)
        {
            ImageDrawLine(dst, x1, y1 - i, x2, y2 - i, color);
        }
    }
    else if (dy != 0)
    {
        // Line is more vertical or perfectly horizontal

        // How many additional lines to draw
        const i32 wx = thick - 1;

        //Draw the main line and right half
        for (i32 i = 0; i <= ((wx+1)/2); i++)
        {
            ImageDrawLine(dst, x1 + i, y1, x2 + i, y2, color);
        }

        // Draw the left half
        for (i32 i = 1; i <= (wx/2); i++)
        {
            ImageDrawLine(dst, x1 - i, y1, x2 - i, y2, color);
        }
    }
}

// Draw a lines sequence within an image
void ImageDrawLineStrip(const Image *dst, const vec2 *points, const i32 pointCount, const Color color)
{
    for (i32 i = 0; i < pointCount - 1; i++)
    {
        ImageDrawLineV(dst, points[i], points[i + 1], color);
    }
}

// Draw triangle within an image
void ImageDrawTriangle(const Image *dst, const vec2 v1, const vec2 v2, const vec2 v3, const Color color)
{
    // Calculate the 2D bounding box of the triangle
    // Determine the minimum and maximum x and y coordinates of the triangle vertices
    auto xMin = static_cast<i32>((v1.x < v2.x)? ((v1.x < v3.x)? v1.x : v3.x) : ((v2.x < v3.x)? v2.x : v3.x));
    auto yMin = static_cast<i32>((v1.y < v2.y)? ((v1.y < v3.y)? v1.y : v3.y) : ((v2.y < v3.y)? v2.y : v3.y));
    auto xMax = static_cast<i32>((v1.x > v2.x)? ((v1.x > v3.x)? v1.x : v3.x) : ((v2.x > v3.x)? v2.x : v3.x));
    auto yMax = static_cast<i32>((v1.y > v2.y)? ((v1.y > v3.y)? v1.y : v3.y) : ((v2.y > v3.y)? v2.y : v3.y));

    // Clamp the bounding box to the image dimensions
    if (xMin < 0) xMin = 0;
    if (yMin < 0) yMin = 0;
    if (xMax > dst->width) xMax = dst->width;
    if (yMax > dst->height) yMax = dst->height;

    // Check the order of the vertices to determine if it's a front or back face
    // NOTE: if signedArea is equal to 0, the face is degenerate
    const f32 signedArea = (v2.x - v1.x)*(v3.y - v1.y) - (v3.x - v1.x)*(v2.y - v1.y);
    const bool isBackFace = (signedArea > 0);

    // Barycentric interpolation setup
    // Calculate the step increments for the barycentric coordinates
    i32 w1XStep = static_cast<i32>(v3.y - v2.y), w1YStep = static_cast<i32>(v2.x - v3.x);
    i32 w2XStep = static_cast<i32>(v1.y - v3.y), w2YStep = static_cast<i32>(v3.x - v1.x);
    i32 w3XStep = static_cast<i32>(v2.y - v1.y), w3YStep = static_cast<i32>(v1.x - v2.x);

    // If the triangle is a back face, invert the steps
    if (isBackFace)
    {
        w1XStep = -w1XStep, w1YStep = -w1YStep;
        w2XStep = -w2XStep, w2YStep = -w2YStep;
        w3XStep = -w3XStep, w3YStep = -w3YStep;
    }

    // Calculate the initial barycentric coordinates for the top-left point of the bounding box
    auto w1Row = static_cast<i32>((static_cast<f32>(xMin) - v2.x)*static_cast<f32>(w1XStep) + static_cast<f32>(w1YStep)*(static_cast<f32>(yMin) - v2.y));
    auto w2Row = static_cast<i32>((static_cast<f32>(xMin) - v3.x)*static_cast<f32>(w2XStep) + static_cast<f32>(w2YStep)*(static_cast<f32>(yMin) - v3.y));
    auto w3Row = static_cast<i32>((static_cast<f32>(xMin) - v1.x)*static_cast<f32>(w3XStep) + static_cast<f32>(w3YStep)*(static_cast<f32>(yMin) - v1.y));

    // Rasterization loop
    // Iterate through each pixel in the bounding box
    for (i32 y = yMin; y <= yMax; y++)
    {
        i32 w1 = w1Row;
        i32 w2 = w2Row;
        i32 w3 = w3Row;

        for (i32 x = xMin; x <= xMax; x++)
        {
            // Check if the pixel is inside the triangle using barycentric coordinates
            // If it is, the pixel can be drawn with the given color
            if ((w1 | w2 | w3) >= 0) ImageDrawPixel(dst, x, y, color);

            // Increment the barycentric coordinates for the next pixel
            w1 += w1XStep;
            w2 += w2XStep;
            w3 += w3XStep;
        }

        // Move to the next row in the bounding box
        w1Row += w1YStep;
        w2Row += w2YStep;
        w3Row += w3YStep;
    }
}

// Draw triangle with interpolated colors within an image
void ImageDrawTriangleGradient(const Image *dst, const vec2 v1, const vec2 v2, const vec2 v3, const Color c1, const Color c2, const Color c3)
{
    // Calculate the 2D bounding box of the triangle
    // Determine the minimum and maximum x and y coordinates of the triangle vertices
    auto xMin = static_cast<i32>((v1.x < v2.x)? ((v1.x < v3.x)? v1.x : v3.x) : ((v2.x < v3.x)? v2.x : v3.x));
    auto yMin = static_cast<i32>((v1.y < v2.y)? ((v1.y < v3.y)? v1.y : v3.y) : ((v2.y < v3.y)? v2.y : v3.y));
    auto xMax = static_cast<i32>((v1.x > v2.x)? ((v1.x > v3.x)? v1.x : v3.x) : ((v2.x > v3.x)? v2.x : v3.x));
    auto yMax = static_cast<i32>((v1.y > v2.y)? ((v1.y > v3.y)? v1.y : v3.y) : ((v2.y > v3.y)? v2.y : v3.y));

    // Clamp the bounding box to the image dimensions
    if (xMin < 0) xMin = 0;
    if (yMin < 0) yMin = 0;
    if (xMax > dst->width) xMax = dst->width;
    if (yMax > dst->height) yMax = dst->height;

    // Check the order of the vertices to determine if it's a front or back face
    // NOTE: if signedArea is equal to 0, the face is degenerate
    const f32 signedArea = (v2.x - v1.x)*(v3.y - v1.y) - (v3.x - v1.x)*(v2.y - v1.y);
    const bool isBackFace = (signedArea > 0);

    // Barycentric interpolation setup
    // Calculate the step increments for the barycentric coordinates
    i32 w1XStep = static_cast<i32>(v3.y - v2.y), w1YStep = static_cast<i32>(v2.x - v3.x);
    i32 w2XStep = static_cast<i32>(v1.y - v3.y), w2YStep = static_cast<i32>(v3.x - v1.x);
    i32 w3XStep = static_cast<i32>(v2.y - v1.y), w3YStep = static_cast<i32>(v1.x - v2.x);

    // If the triangle is a back face, invert the steps
    if (isBackFace)
    {
        w1XStep = -w1XStep, w1YStep = -w1YStep;
        w2XStep = -w2XStep, w2YStep = -w2YStep;
        w3XStep = -w3XStep, w3YStep = -w3YStep;
    }

    // Calculate the initial barycentric coordinates for the top-left point of the bounding box
    auto w1Row = static_cast<i32>((static_cast<f32>(xMin) - v2.x)*static_cast<f32>(w1XStep) + static_cast<f32>(w1YStep)*(static_cast<f32>(yMin) - v2.y));
    auto w2Row = static_cast<i32>((static_cast<f32>(xMin) - v3.x)*static_cast<f32>(w2XStep) + static_cast<f32>(w2YStep)*(static_cast<f32>(yMin) - v3.y));
    auto w3Row = static_cast<i32>((static_cast<f32>(xMin) - v1.x)*static_cast<f32>(w3XStep) + static_cast<f32>(w3YStep)*(static_cast<f32>(yMin) - v1.y));

    // Calculate the inverse of the sum of the barycentric coordinates for normalization
    const f32 wInvSum = 255.0f/static_cast<f32>((w1Row + w2Row + w3Row));

    // Rasterization loop
    // Iterate through each pixel in the bounding box
    for (i32 y = yMin; y <= yMax; y++)
    {
        i32 w1 = w1Row;
        i32 w2 = w2Row;
        i32 w3 = w3Row;

        for (i32 x = xMin; x <= xMax; x++)
        {
            // Check if the pixel is inside the triangle using barycentric coordinates
            if ((w1 | w2 | w3) >= 0)
            {
                // Compute the normalized barycentric coordinates
                const u8 aW1 = static_cast<u8>(static_cast<f32>(w1)*wInvSum);
                const u8 aW2 = static_cast<u8>(static_cast<f32>(w2)*wInvSum);
                const u8 aW3 = static_cast<u8>(static_cast<f32>(w3)*wInvSum);

                // Interpolate the color using the barycentric coordinates
                auto finalColor = Color{ 0 };
                finalColor.r = (c1.r*aW1 + c2.r*aW2 + c3.r*aW3)/255;
                finalColor.g = (c1.g*aW1 + c2.g*aW2 + c3.g*aW3)/255;
                finalColor.b = (c1.b*aW1 + c2.b*aW2 + c3.b*aW3)/255;
                finalColor.a = (c1.a*aW1 + c2.a*aW2 + c3.a*aW3)/255;

                // Draw the pixel with the interpolated color
                ImageDrawPixel(dst, x, y, finalColor);
            }

            // Increment the barycentric coordinates for the next pixel
            w1 += w1XStep;
            w2 += w2XStep;
            w3 += w3XStep;
        }

        // Move to the next row in the bounding box
        w1Row += w1YStep;
        w2Row += w2YStep;
        w3Row += w3YStep;
    }
}

// Draw triangle outline within an image
void ImageDrawTriangleLines(const Image *dst, const vec2 v1, const vec2 v2, const vec2 v3, const Color color)
{
    ImageDrawLine(dst, static_cast<i32>(v1.x), static_cast<i32>(v1.y), static_cast<i32>(v2.x), static_cast<i32>(v2.y), color);
    ImageDrawLine(dst, static_cast<i32>(v2.x), static_cast<i32>(v2.y), static_cast<i32>(v3.x), static_cast<i32>(v3.y), color);
    ImageDrawLine(dst, static_cast<i32>(v3.x), static_cast<i32>(v3.y), static_cast<i32>(v1.x), static_cast<i32>(v1.y), color);
}

// Draw a triangle fan defined by points within an image (first vertex is the center)
void ImageDrawTriangleFan(const Image *dst, const vec2 *points, const i32 pointCount, const Color color)
{
    if (pointCount >= 3)
    {
        for (i32 i = 1; i < pointCount - 1; i++)
        {
            ImageDrawTriangle(dst, points[0], points[i], points[i + 1], color);
        }
    }
}

// Draw a triangle strip defined by points within an image
void ImageDrawTriangleStrip(const Image *dst, const vec2 *points, const i32 pointCount, const Color color)
{
    if (pointCount >= 3)
    {
        for (i32 i = 2; i < pointCount; i++)
        {
            if ((i%2) == 0) ImageDrawTriangle(dst, points[i], points[i - 2], points[i - 1], color);
            else ImageDrawTriangle(dst, points[i], points[i - 1], points[i - 2], color);
        }
    }
}

// Draw rectangle within an image
void ImageDrawRectangle(const Image *dst, const i32 posX, const i32 posY, const i32 width, const i32 height, const Color color)
{
    ImageDrawRectangleRec(dst, Rectangle{ static_cast<f32>(posX), static_cast<f32>(posY), static_cast<f32>(width), static_cast<f32>(height) }, color);
}

// Draw rectangle within an image (Vector version)
void ImageDrawRectangleV(const Image *dst, const vec2 position, const vec2 size, const Color color)
{
    ImageDrawRectangle(dst, static_cast<i32>(position.x), static_cast<i32>(position.y), static_cast<i32>(size.x), static_cast<i32>(size.y), color);
}

// Draw rectangle within an image
void ImageDrawRectangleRec(const Image *dst, Rectangle rec, const Color color)
{
    // Security check to avoid program crash
    if ((dst->data == nullptr) || (dst->width == 0) || (dst->height == 0)) return;

    // Security check to avoid drawing out of bounds in case of bad user data
    if (rec.x < 0) { rec.width += rec.x; rec.x = 0; }
    if (rec.y < 0) { rec.height += rec.y; rec.y = 0; }
    if (rec.width < 0) rec.width = 0;
    if (rec.height < 0) rec.height = 0;

    // Clamp the size the the image bounds
    if ((rec.x + rec.width) >= static_cast<f32>(dst->width)) rec.width = static_cast<f32>(dst->width) - rec.x;
    if ((rec.y + rec.height) >= static_cast<f32>(dst->height)) rec.height = static_cast<f32>(dst->height) - rec.y;

    // Check if the rect is even inside the image
    if ((rec.x >= static_cast<f32>(dst->width)) || (rec.y >= static_cast<f32>(dst->height))) return;
    if (((rec.x + rec.width) <= 0) || (rec.y + rec.height <= 0)) return;

    const auto sy = static_cast<i32>(rec.y);
    const auto sx = static_cast<i32>(rec.x);

    const auto bytesPerPixel = GetPixelDataSize(1, 1, dst->format);

    // Fill in the first pixel of the first row based on image format
    ImageDrawPixel(dst, sx, sy, color);

    const i32 bytesOffset = ((sy*dst->width) + sx)*bytesPerPixel;
    u8 *pSrcPixel = static_cast<u8 *>(dst->data) + bytesOffset;

    // Repeat the first pixel data throughout the row
    for (i32 x = 1; x < static_cast<i32>(rec.width); x *= 2)
    {
        const i32 pixelsToCopy = MIN(x, static_cast<i32>(rec.width) - x);
        memcpy(pSrcPixel + x*bytesPerPixel, pSrcPixel, pixelsToCopy*bytesPerPixel);
    }

    // Repeat the first row data for all other rows
    const i32 bytesPerRow = bytesPerPixel*static_cast<i32>(rec.width);
    for (i32 y = 1; y < static_cast<i32>(rec.height); y++)
    {
        memcpy(pSrcPixel + (y*dst->width)*bytesPerPixel, pSrcPixel, bytesPerRow);
    }
}

// Draw a color-filled rectangle with pro parameters within and image
void ImageDrawRectanglePro(const Image *dst, const Rectangle rec, const vec2 origin, const f32 rotation, const Color color)
{
    // Security checks to avoid program crash
    if ((dst == nullptr) || (dst->data == nullptr) || (rec.width <= 0) || (rec.height <= 0)) return;

    const f32 cosAngle = cosf(rotation*DEG2RAD);
    const f32 sinAngle = sinf(rotation*DEG2RAD);

    // Rotation origin in world/image coordinates
    const f32 ox = rec.x + origin.x;
    const f32 oy = rec.y + origin.y;

    // Rectangle corners relative to rotation origin
    const f32 x1 = -origin.x;
    const f32 y1 = -origin.y;

    const f32 x2 = rec.width - origin.x;
    const f32 y2 = rec.height - origin.y;

    // Rotate the four corners to calculate the bounding box
    const f32 cornersX[4] = { x1, x2, x2, x1 };
    const f32 cornersY[4] = { y1, y1, y2, y2 };

    f32 minX = 65536;
    f32 minY = 65536;
    f32 maxX = -65536;
    f32 maxY = -65536;

    for (i32 i = 0; i < 4; i++)
    {
        f32 rx = cornersX[i]*cosAngle - cornersY[i]*sinAngle;
        f32 ry = cornersX[i]*sinAngle + cornersY[i]*cosAngle;

        rx += ox;
        ry += oy;

        if (rx < minX) minX = rx;
        if (ry < minY) minY = ry;
        if (rx > maxX) maxX = rx;
        if (ry > maxY) maxY = ry;
    }

    // Convert bounding box to integer pixel bounds
    auto x0 = static_cast<i32>(floorf(minX));
    auto y0 = static_cast<i32>(floorf(minY));
    auto xEnd = static_cast<i32>(ceilf(maxX));
    auto yEnd = static_cast<i32>(ceilf(maxY));

    // Limit drawing to image bounds
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (xEnd > dst->width) xEnd = dst->width;
    if (yEnd > dst->height) yEnd = dst->height;

    // Safety check
    if ((x0 >= xEnd) || (y0 >= yEnd)) return;

    for (i32 y = y0; y < yEnd; y++)
    {
        for (i32 x = x0; x < xEnd; x++)
        {
            // Pixel center in world coordinates
            const f32 px = static_cast<f32>(x) + 0.5f - ox;
            const f32 py = static_cast<f32>(y) + 0.5f - oy;

            // Inverse-rotate pixel into rectangle local space
            const f32 localX = px*cosAngle + py*sinAngle;
            const f32 localY = -px*sinAngle + py*cosAngle;

            // Check whether pixel lies inside the rectangle
            if ((localX >= -origin.x) && (localX < (rec.width - origin.x)) &&
                (localY >= -origin.y) && (localY < (rec.height - origin.y)))
            {
                // NOTE: Pixel format conversion managed by function
                ImageDrawPixel(dst, x, y, color);
            }
        }
    }
}

// Draw rectangle lines within an image
void ImageDrawRectangleLines(const Image *dst, const i32 posX, const i32 posY, const i32 width, const i32 height, const Color color)
{
    const auto rec = Rectangle{ static_cast<f32>(posX), static_cast<f32>(posY), static_cast<f32>(width), static_cast<f32>(height) };
    ImageDrawRectangleLinesEx(dst, rec, 1, color);
}

// Draw rectangle lines within an image with line thickness
void ImageDrawRectangleLinesEx(const Image *dst, const Rectangle rec, const i32 thick, const Color color)
{
    ImageDrawRectangle(dst, static_cast<i32>(rec.x), static_cast<i32>(rec.y), static_cast<i32>(rec.width), thick, color);
    ImageDrawRectangle(dst, static_cast<i32>(rec.x), static_cast<i32>(rec.y + static_cast<f32>(thick)), thick, static_cast<i32>(rec.height - static_cast<f32>(thick)*2), color);
    ImageDrawRectangle(dst, static_cast<i32>(rec.x + rec.width - static_cast<f32>(thick)), static_cast<i32>(rec.y + static_cast<f32>(thick)), thick, static_cast<i32>(rec.height - static_cast<f32>(thick)*2), color);
    ImageDrawRectangle(dst, static_cast<i32>(rec.x), static_cast<i32>(rec.y + rec.height - static_cast<f32>(thick)), static_cast<i32>(rec.width), thick, color);
}

// Draw rectangle with gradient colors within an image, counter-clockwise color order
void ImageDrawRectangleGradientEx(const Image *dst, const Rectangle rec, const Color col1, const Color col2, const Color col3, const Color col4)
{
    // Security checks to avoid program crash
    if ((dst == nullptr) || (dst->data == nullptr) || (rec.width <= 0) || (rec.height <= 0)) return;

    auto x0 = static_cast<i32>(floorf(rec.x));
    auto y0 = static_cast<i32>(floorf(rec.y));
    auto x1 = static_cast<i32>(ceilf(rec.x + rec.width));
    auto y1 = static_cast<i32>(ceilf(rec.y + rec.height));

    // Limit draw to image bounds
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > dst->width) x1 = dst->width;
    if (y1 > dst->height) y1 = dst->height;

    // Safety check
    if ((x0 >= x1) || (y0 >= y1)) return;

    for (i32 y = y0; y < y1; y++)
    {
        f32 ty = (static_cast<f32>(y) - rec.y)/rec.height;
        if (ty < 0.0f) ty = 0.0f;
        if (ty > 1.0f) ty = 1.0f;

        for (i32 x = x0; x < x1; x++)
        {
            f32 tx = (static_cast<f32>(x) - rec.x)/rec.width;
            if (tx < 0.0f) tx = 0.0f;
            if (tx > 1.0f) tx = 1.0f;

            // Bilinear interpolation weights
            const f32 w1 = (1.0f - tx)*(1.0f - ty);
            const f32 w2 = (1.0f - tx)*ty;
            const f32 w3 = tx*ty;
            const f32 w4 = tx*(1.0f - ty);

            const Color color{
                static_cast<u8>(static_cast<f32>(col1.r)*w1 + static_cast<f32>(col2.r)*w2 + static_cast<f32>(col3.r)*w3 + static_cast<f32>(col4.r)*w4),
                static_cast<u8>(static_cast<f32>(col1.g)*w1 + static_cast<f32>(col2.g)*w2 + static_cast<f32>(col3.g)*w3 + static_cast<f32>(col4.g)*w4),
                static_cast<u8>(static_cast<f32>(col1.b)*w1 + static_cast<f32>(col2.b)*w2 + static_cast<f32>(col3.b)*w3 + static_cast<f32>(col4.b)*w4),
                static_cast<u8>(static_cast<f32>(col1.a)*w1 + static_cast<f32>(col2.a)*w2 + static_cast<f32>(col3.a)*w3 + static_cast<f32>(col4.a)*w4)
            };

            // Using ImageDrawPixel() to let it manage color format conversion
            ImageDrawPixel(dst, x, y, color);
        }
    }
}

// Draw circle within an image
void ImageDrawCircle(const Image *dst, const i32 centerX, const i32 centerY, const i32 radius, const Color color)
{
    auto x = 0;
    i32 y = radius;
    i32 decesionParameter = 3 - 2*radius;

    while (y >= x)
    {
        ImageDrawRectangle(dst, centerX - x, centerY + y, x*2 + 1, 1, color);
        ImageDrawRectangle(dst, centerX - x, centerY - y, x*2 + 1, 1, color);
        ImageDrawRectangle(dst, centerX - y, centerY + x, y*2 + 1, 1, color);
        ImageDrawRectangle(dst, centerX - y, centerY - x, y*2 + 1, 1, color);
        x++;

        if (decesionParameter > 0)
        {
            y--;
            decesionParameter = decesionParameter + 4*(x - y) + 10;
        }
        else decesionParameter = decesionParameter + 4*x + 6;
    }
}

// Draw circle within an image (Vector version)
void ImageDrawCircleV(const Image *dst, const vec2 center, const i32 radius, const Color color)
{
    ImageDrawCircle(dst, static_cast<i32>(center.x), static_cast<i32>(center.y), radius, color);
}

// Draw circle outline within an image
void ImageDrawCircleLines(const Image *dst, const i32 centerX, const i32 centerY, const i32 radius, const Color color)
{
    auto x = 0;
    i32 y = radius;
    i32 decesionParameter = 3 - 2*radius;

    while (y >= x)
    {
        ImageDrawPixel(dst, centerX + x, centerY + y, color);
        ImageDrawPixel(dst, centerX - x, centerY + y, color);
        ImageDrawPixel(dst, centerX + x, centerY - y, color);
        ImageDrawPixel(dst, centerX - x, centerY - y, color);
        ImageDrawPixel(dst, centerX + y, centerY + x, color);
        ImageDrawPixel(dst, centerX - y, centerY + x, color);
        ImageDrawPixel(dst, centerX + y, centerY - x, color);
        ImageDrawPixel(dst, centerX - y, centerY - x, color);
        x++;

        if (decesionParameter > 0)
        {
            y--;
            decesionParameter = decesionParameter + 4*(x - y) + 10;
        }
        else decesionParameter = decesionParameter + 4*x + 6;
    }
}

// Draw circle outline within an image (Vector version)
void ImageDrawCircleLinesV(const Image *dst, const vec2 center, const i32 radius, const Color color)
{
    ImageDrawCircleLines(dst, static_cast<i32>(center.x), static_cast<i32>(center.y), radius, color);
}

// Draw a gradient-filled circle within an image
void ImageDrawCircleGradient(const Image *dst, const vec2 center, const f32 radius, const Color inner, const Color outer)
{
    // Security checks to avoid program crash
    if ((dst == nullptr) || (dst->data == nullptr) || (radius <= 0.0f)) return;

    auto x0 = static_cast<i32>(floorf(center.x - radius));
    auto y0 = static_cast<i32>(floorf(center.y - radius));
    auto x1 = static_cast<i32>(ceilf(center.x + radius));
    auto y1 = static_cast<i32>(ceilf(center.y + radius));

    // Limit drawing to image bounds
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > dst->width)  x1 = dst->width;
    if (y1 > dst->height) y1 = dst->height;

    if ((x0 >= x1) || (y0 >= y1)) return;

    const f32 radiusSq = radius*radius;

    for (i32 y = y0; y < y1; y++)
    {
        for (i32 x = x0; x < x1; x++)
        {
            // Center relative to circle center
            const f32 dx = (static_cast<f32>(x) + 0.5f) - center.x;
            const f32 dy = (static_cast<f32>(y) + 0.5f) - center.y;

            const f32 distSq = dx*dx + dy*dy;

            // Skip pixels outside the circle
            if (distSq > radiusSq) continue;

            // Linear interpolation from inner to outer color
            // TODO: Consider other types of interpolation for better results?
            const f32 t = sqrtf(distSq)/radius;
            const Color color{
                static_cast<u8>(static_cast<f32>(inner.r) + static_cast<f32>((outer.r - inner.r))*t),
                static_cast<u8>(static_cast<f32>(inner.g) + static_cast<f32>((outer.g - inner.g))*t),
                static_cast<u8>(static_cast<f32>(inner.b) + static_cast<f32>((outer.b - inner.b))*t),
                static_cast<u8>(static_cast<f32>(inner.a) + static_cast<f32>((outer.a - inner.a))*t)
            };

            // NOTE: Pixel format conversion managed by function
            ImageDrawPixel(dst, x, y, color);
        }
    }
}

// Draw an image within an image
void ImageDrawImage(const Image *dst, const Image src, const i32 posX, const i32 posY, const Color tint)
{
    const auto srcRec = Rectangle{ 0.0f, 0.0f, static_cast<f32>(src.width), static_cast<f32>(src.height) };
    const auto dstRec = Rectangle{ static_cast<f32>(posX), static_cast<f32>(posY), srcRec.width, srcRec.height };
    ImageDrawImagePro(dst, src, srcRec, dstRec, vec2{ 0 }, 0.0f, tint);
}

// Draw an image with scaling and rotation within an image
// NOTE: Rotation applied from top-left corner origin
void ImageDrawImageEx(const Image *dst, const Image src, const vec2 position, const f32 rotation, const f32 scale, const Color tint)
{
    // Security checks to avoid program crash
    if ((dst == nullptr) || (dst->data == nullptr) || (dst->width <= 0) || (dst->height <= 0) || (src.width <= 0) || (src.height <= 0)) return;

    const f32 cosA = cosf(rotation*DEG2RAD);
    const f32 sinA = sinf(rotation*DEG2RAD);

    // Rotate around the source image top-left corner
    const f32 cornersX[4] = { 0.0f, static_cast<f32>(src.width)*scale, static_cast<f32>(src.width)*scale, 0.0f };
    const f32 cornersY[4] = { 0.0f, 0.0f, static_cast<f32>(src.height)*scale, static_cast<f32>(src.height)*scale };

    f32 minX = 65536;
    f32 minY = 65536;
    f32 maxX = -65536;
    f32 maxY = -65536;

    // Calculate the rotated bounding box
    for (i32 i = 0; i < 4; i++)
    {
        f32 rx = cornersX[i]*cosA - cornersY[i]*sinA;
        f32 ry = cornersX[i]*sinA + cornersY[i]*cosA;

        rx += position.x;
        ry += position.y;

        if (rx < minX) minX = rx;
        if (ry < minY) minY = ry;
        if (rx > maxX) maxX = rx;
        if (ry > maxY) maxY = ry;
    }

    auto x0 = static_cast<i32>(floorf(minX));
    auto y0 = static_cast<i32>(floorf(minY));
    auto x1 = static_cast<i32>(ceilf(maxX));
    auto y1 = static_cast<i32>(ceilf(maxY));

    // Limit drawing to destination bounds
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > dst->width) x1 = dst->width;
    if (y1 > dst->height) y1 = dst->height;

    // Safety check
    if ((x0 >= x1) || (y0 >= y1)) return;

    for (i32 y = y0; y < y1; y++)
    {
        for (i32 x = x0; x < x1; x++)
        {
            // Destination pixel center relative to image position
            const f32 dx = (static_cast<f32>(x) + 0.5f) - position.x;
            const f32 dy = (static_cast<f32>(y) + 0.5f) - position.y;

            // Inverse-rotate into the scaled source space
            f32 sx = dx*cosA + dy*sinA;
            f32 sy = -dx*sinA + dy*cosA;

            // Revert scaling
            sx /= scale;
            sy /= scale;

            // Skip pixels outside the source image
            if ((sx < 0.0f) || (sy < 0.0f) || (sx >= static_cast<f32>(src.width)) || (sy >= static_cast<f32>(src.height))) continue;

            // Nearest-neighbor source sampling
            const auto srcX = static_cast<i32>(sx);
            const auto srcY = static_cast<i32>(sy);

            const auto srcColor = GetImageColor(src, srcX, srcY);

            // Skip fully transparent pixels
            if (srcColor.a == 0) continue;

            // Read destination pixel and blend source over it, applying tint
            // NOTE: ColorAlphaBlend() takes destination alpha into account,
            // same as ImageDrawImagePro(), required to draw on transparent images
            const auto dstColor = GetImageColor(*dst, x, y);

            ImageDrawPixel(dst, x, y, ColorAlphaBlend(dstColor, srcColor, tint));
        }
    }
}

// Draw a part of an image defined by a rectangle within an image
void ImageDrawImageRec(const Image *dst, const Image src, const Rectangle srcRec, const vec2 position, const Color tint)
{
    const auto dstRec = Rectangle{ position.x, position.y, srcRec.width, srcRec.height };
    ImageDrawImagePro(dst, src, srcRec, dstRec, vec2{ 0 }, 0.0f, tint);
}

// Draw a part of an image defined by a rectangle into destination rectangle, with scaling and rotation, within an image
// TODO: REVIEW: ImageDrawImagePro(), implement origin and rotation for image drawing
void ImageDrawImagePro(const Image *dst, Image src, Rectangle srcRec, Rectangle dstRec, const vec2 origin, const f32 rotation, const Color tint)
{
    // Security checks to avoid program crash
    if ((dst == nullptr) || (dst->data == nullptr) || (dst->width == 0) || (dst->height == 0) ||
        (src.data == nullptr) || (src.width == 0) || (src.height == 0)) return;

    if (dst->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) log(LogLevel::warn, "Image drawing not supported for compressed formats");
    else
    {
        auto srcMod = Image{ nullptr };       // Source copy (in case it was required)
        Image *srcPtr = &src;             // Pointer to source image
        auto useSrcMod = false;           // Track source copy required

        // Source rectangle out-of-bounds security checks
        if (srcRec.x < 0) { srcRec.width += srcRec.x; srcRec.x = 0; }
        if (srcRec.y < 0) { srcRec.height += srcRec.y; srcRec.y = 0; }
        if ((srcRec.x + srcRec.width) > static_cast<f32>(src.width)) srcRec.width = static_cast<f32>(src.width) - srcRec.x;
        if ((srcRec.y + srcRec.height) > static_cast<f32>(src.height)) srcRec.height = static_cast<f32>(src.height) - srcRec.y;

        // Check if source rectangle needs to be resized to destination rectangle
        // In that case, make a copy of source, and apply all required transform
        if ((static_cast<i32>(srcRec.width) != static_cast<i32>(dstRec.width)) || (static_cast<i32>(srcRec.height) != static_cast<i32>(dstRec.height)))
        {
            srcMod = ImageFromImage(src, srcRec);                                                  // Create image from another image
            ImageResize(&srcMod, static_cast<i32>(dstRec.width), static_cast<i32>(dstRec.height)); // Resize to destination rectangle
            srcRec = Rectangle{ 0, 0, static_cast<f32>(srcMod.width), static_cast<f32>(srcMod.height) };

            srcPtr = &srcMod;
            useSrcMod = true;
        }

        // Destination rectangle out-of-bounds security checks
        if (dstRec.x < 0)
        {
            srcRec.x -= dstRec.x;
            srcRec.width += dstRec.x;
            dstRec.x = 0;
        }
        else if ((dstRec.x + srcRec.width) > static_cast<f32>(dst->width)) srcRec.width = static_cast<f32>(dst->width) - dstRec.x;

        if (dstRec.y < 0)
        {
            srcRec.y -= dstRec.y;
            srcRec.height += dstRec.y;
            dstRec.y = 0;
        }
        else if ((dstRec.y + srcRec.height) > static_cast<f32>(dst->height)) srcRec.height = static_cast<f32>(dst->height) - dstRec.y;

        if (static_cast<f32>(dst->width) < srcRec.width) srcRec.width = static_cast<f32>(dst->width);
        if (static_cast<f32>(dst->height) < srcRec.height) srcRec.height = static_cast<f32>(dst->height);

        // This blitting method is quite fast! The process followed is:
        // for every pixel -> [get_src_format/get_dst_format -> blend -> format_to_dst]
        // Some optimization ideas:
        //    [x] Avoid creating source copy if not required (no resize required)
        //    [x] Optimize ImageResize() for pixel format (alternative: ImageResizeNN())
        //    [x] Optimize ColorAlphaBlend() to avoid processing (alpha = 0) and (alpha = 1)
        //    [x] Optimize ColorAlphaBlend() for faster operations (maybe avoiding divs?)
        //    [x] Consider fast path: no alpha blending required cases (src has no alpha)
        //    [x] Consider fast path: same src/dst format with no alpha -> direct line copy
        //    [-] GetPixelColor(): Get vec4 instead of Color, easier for ColorAlphaBlend()
        //    [-] Support 16bit and 32bit (float) channels drawing

        auto colSrc = Color{ 0 };
        auto colDst = Color{ 0 };
        auto colBlend = Color{ 0 };
        auto blendRequired = true;

        // Fast path: Avoid blend if source has no alpha to blend
        if ((tint.a == 255) &&
            ((srcPtr->format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE) ||
            (srcPtr->format == PIXELFORMAT_UNCOMPRESSED_R5G6B5) ||
            (srcPtr->format == PIXELFORMAT_UNCOMPRESSED_R8G8B8) ||
            (srcPtr->format == PIXELFORMAT_UNCOMPRESSED_R32) ||
            (srcPtr->format == PIXELFORMAT_UNCOMPRESSED_R32G32B32) ||
            (srcPtr->format == PIXELFORMAT_UNCOMPRESSED_R16) ||
            (srcPtr->format == PIXELFORMAT_UNCOMPRESSED_R16G16B16)))
            blendRequired = false;

        const auto strideDst = GetPixelDataSize(dst->width, 1, dst->format);
        const i32 bytesPerPixelDst = strideDst/(dst->width);

        const auto strideSrc = GetPixelDataSize(srcPtr->width, 1, srcPtr->format);
        const i32 bytesPerPixelSrc = strideSrc/(srcPtr->width);

        u8 *pSrcBase = static_cast<u8 *>(srcPtr->data) + (static_cast<i32>(srcRec.y)*srcPtr->width + static_cast<i32>(srcRec.x))*bytesPerPixelSrc;
        u8 *pDstBase = static_cast<u8 *>(dst->data) + (static_cast<i32>(dstRec.y)*dst->width + static_cast<i32>(dstRec.x))*bytesPerPixelDst;

        for (i32 y = 0; y < static_cast<i32>(srcRec.height); y++)
        {
            u8 *pSrc = pSrcBase;
            u8 *pDst = pDstBase;

            // Fast path: Avoid moving pixel by pixel if no blend required and same format
            if (!blendRequired && (srcPtr->format == dst->format)) memcpy(pDst, pSrc, static_cast<i32>(srcRec.width)*bytesPerPixelSrc);
            else
            {
                for (i32 x = 0; x < static_cast<i32>(srcRec.width); x++)
                {
                    colSrc = GetPixelColor(pSrc, srcPtr->format);
                    colDst = GetPixelColor(pDst, dst->format);

                    // Fast path: Avoid blend if source has no alpha to blend
                    if (blendRequired) colBlend = ColorAlphaBlend(colDst, colSrc, tint);
                    else colBlend = colSrc;

                    SetPixelColor(pDst, colBlend, dst->format);

                    pDst += bytesPerPixelDst;
                    pSrc += bytesPerPixelSrc;
                }
            }

            pSrcBase += strideSrc;
            pDstBase += strideDst;
        }

        if (useSrcMod) UnloadImage(srcMod);     // Unload source modified image

        if ((dst->mipmaps > 1) && (src.mipmaps > 1))
        {
            Image mipmapDst = *dst;
            mipmapDst.data = static_cast<char *>(mipmapDst.data) + GetPixelDataSize(mipmapDst.width, mipmapDst.height, mipmapDst.format);
            mipmapDst.width /= 2;
            mipmapDst.height /= 2;
            mipmapDst.mipmaps--;

            Image mipmapSrc = src;
            mipmapSrc.data = static_cast<char *>(mipmapSrc.data) + GetPixelDataSize(mipmapSrc.width, mipmapSrc.height, mipmapSrc.format);
            mipmapSrc.width /= 2;
            mipmapSrc.height /= 2;
            mipmapSrc.mipmaps--;

            Rectangle mipmapSrcRec = srcRec;
            mipmapSrcRec.width /= 2;
            mipmapSrcRec.height /= 2;
            mipmapSrcRec.x /= 2;
            mipmapSrcRec.y /= 2;

            Rectangle mipmapDstRec = dstRec;
            mipmapDstRec.width /= 2;
            mipmapDstRec.height /= 2;
            mipmapDstRec.x /= 2;
            mipmapDstRec.y /= 2;

            ImageDrawImagePro(&mipmapDst, mipmapSrc, mipmapSrcRec, mipmapDstRec, vec2{ 0 }, 0.0f, tint);
        }
    }
}

// Draw text (default font) within an image (destination)
void ImageDrawText(const Image *dst, std::string_view textView, const i32 posX, const i32 posY, const i32 fontSize, const Color color)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();

#if SUPPORT_MODULE_RTEXT
    // Make sure default font is loaded to be used on image text drawing
    if (GetFontDefault().texture.id == 0) LoadFontDefault();

    const auto position = vec2{ static_cast<f32>(posX), static_cast<f32>(posY) };
    ImageDrawTextEx(dst, GetFontDefault(), text, position, static_cast<f32>(fontSize), 1.0f, color);   // WARNING: Module required: rtext
#else
    log(LogLevel::warn, "IMAGE: ImageDrawText() requires module: rtext");
#endif
}

// Draw text (custom sprite font) within an image (destination)
void ImageDrawTextEx(const Image *dst, const Font font, std::string_view textView, const vec2 position, const f32 fontSize, const f32 spacing, const Color tint)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();

    const auto imText = ImageTextEx(font, text, fontSize, spacing, tint);

    const auto srcRec = Rectangle{ 0.0f, 0.0f, static_cast<f32>(imText.width), static_cast<f32>(imText.height) };
    const auto dstRec = Rectangle{ position.x, position.y, static_cast<f32>(imText.width), static_cast<f32>(imText.height) };

    ImageDrawImagePro(dst, imText, srcRec, dstRec, vec2{ 0 }, 0.0f, WHITE);

    UnloadImage(imText);
}

// Draw text using Font and pro parameters (rotation)
void ImageDrawTextPro(const Image *dst, const Font font, std::string_view textView, const vec2 position, const vec2 origin, const f32 rotation, const f32 fontSize, const f32 spacing, const Color tint)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();

    // TODO: NEW: Implement ImageDrawTextPro()
}

//------------------------------------------------------------------------------------
// Texture loading functions
//------------------------------------------------------------------------------------
// Load texture from file into GPU memory (VRAM)
Texture2D LoadTexture(std::string_view fileNameView)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();

    auto texture = Texture2D{ 0 };

    const auto image = LoadImage(fileName);

    if (image.data != nullptr)
    {
        texture = LoadTextureFromImage(image);
        UnloadImage(image);
    }

    return texture;
}

// Load a texture from image data
// NOTE: image is not unloaded, it must be done manually
Texture2D LoadTextureFromImage(const Image image)
{
    auto texture = Texture2D{ 0 };

    if ((image.width != 0) && (image.height != 0))
    {
        texture.id = rlLoadTexture(image.data, image.width, image.height, image.format, image.mipmaps);
    }
    else log(LogLevel::warn, "IMAGE: Data is not valid to load texture");

    texture.width = image.width;
    texture.height = image.height;
    texture.mipmaps = image.mipmaps;
    texture.format = image.format;

    return texture;
}

// Load cubemap from image, multiple image cubemap layouts supported
TextureCubemap LoadTextureCubemap(const Image image, i32 layout)
{
    auto cubemap = TextureCubemap{ 0 };

    if (layout == CUBEMAP_LAYOUT_AUTO_DETECT) // Try to automatically guess layout type
    {
        // Check image width/height to determine the type of cubemap provided
        if (image.width > image.height)
        {
            if ((image.width/6) == image.height) { layout = CUBEMAP_LAYOUT_LINE_HORIZONTAL; cubemap.width = image.width/6; }
            else if ((image.width/4) == (image.height/3)) { layout = CUBEMAP_LAYOUT_CROSS_FOUR_BY_THREE; cubemap.width = image.width/4; }
        }
        else if (image.height > image.width)
        {
            if ((image.height/6) == image.width) { layout = CUBEMAP_LAYOUT_LINE_VERTICAL; cubemap.width = image.height/6; }
            else if ((image.width/3) == (image.height/4)) { layout = CUBEMAP_LAYOUT_CROSS_THREE_BY_FOUR; cubemap.width = image.width/3; }
        }
    }
    else
    {
        if (layout == CUBEMAP_LAYOUT_LINE_VERTICAL) cubemap.width = image.height/6;
        if (layout == CUBEMAP_LAYOUT_LINE_HORIZONTAL) cubemap.width = image.width/6;
        if (layout == CUBEMAP_LAYOUT_CROSS_THREE_BY_FOUR) cubemap.width = image.width/3;
        if (layout == CUBEMAP_LAYOUT_CROSS_FOUR_BY_THREE) cubemap.width = image.width/4;
    }

    cubemap.height = cubemap.width;

    // Layout provided or already auto-detected
    if (layout != CUBEMAP_LAYOUT_AUTO_DETECT)
    {
        const i32 size = cubemap.width;

        auto faces = Image{ nullptr };                // Vertical column image
        Rectangle faceRecs[6]{};                  // Face source rectangles

        for (i32 i = 0; i < 6; i++) faceRecs[i] = Rectangle{ 0, 0, static_cast<f32>(size), static_cast<f32>(size) };

        if (layout == CUBEMAP_LAYOUT_LINE_VERTICAL)
        {
            faces = ImageCopy(image);       // Image data already follows expected convention
        }
        /*else if (layout == CUBEMAP_LAYOUT_PANORAMA)
        {
            // TODO: Implement panorama by converting image to square faces...
            // REF: https://github.com/denivip/panorama/blob/master/panorama.cpp
        } */
        else
        {
            if (layout == CUBEMAP_LAYOUT_LINE_HORIZONTAL) for (i32 i = 0; i < 6; i++) faceRecs[i].x = static_cast<f32>(size)*static_cast<f32>(i);
            else if (layout == CUBEMAP_LAYOUT_CROSS_THREE_BY_FOUR)
            {
                faceRecs[0].x = static_cast<f32>(size); faceRecs[0].y = static_cast<f32>(size);
                faceRecs[1].x = static_cast<f32>(size); faceRecs[1].y = static_cast<f32>(size)*3;
                faceRecs[2].x = static_cast<f32>(size); faceRecs[2].y = 0;
                faceRecs[3].x = static_cast<f32>(size); faceRecs[3].y = static_cast<f32>(size)*2;
                faceRecs[4].x = 0; faceRecs[4].y = static_cast<f32>(size);
                faceRecs[5].x = static_cast<f32>(size)*2; faceRecs[5].y = static_cast<f32>(size);
            }
            else if (layout == CUBEMAP_LAYOUT_CROSS_FOUR_BY_THREE)
            {
                faceRecs[0].x = static_cast<f32>(size)*2; faceRecs[0].y = static_cast<f32>(size);
                faceRecs[1].x = 0; faceRecs[1].y = static_cast<f32>(size);
                faceRecs[2].x = static_cast<f32>(size); faceRecs[2].y = 0;
                faceRecs[3].x = static_cast<f32>(size); faceRecs[3].y = static_cast<f32>(size)*2;
                faceRecs[4].x = static_cast<f32>(size); faceRecs[4].y = static_cast<f32>(size);
                faceRecs[5].x = static_cast<f32>(size)*3; faceRecs[5].y = static_cast<f32>(size);
            }

            // Convert image data to 6 faces in a vertical column, that's the optimum layout for loading
            // NOTE: Image formatting does not work with compressed textures
            faces = GenImageColor(size, size*6, MAGENTA);
            ImageFormat(&faces, image.format);

            auto mipmapped = ImageCopy(image);
            if (image.mipmaps > 1)
            {
                ImageMipmaps(&mipmapped);
                ImageMipmaps(&faces);
            }

            for (i32 i = 0; i < 6; i++) ImageDrawImagePro(&faces, mipmapped, faceRecs[i], Rectangle{ 0, static_cast<f32>(size)*static_cast<f32>(i), static_cast<f32>(size), static_cast<f32>(size) }, vec2{ 0 }, 0.0f, WHITE);

            UnloadImage(mipmapped);
        }

        // NOTE: Cubemap data is expected to be provided as 6 images in a single data array,
        // one after the other (that's a vertical image), following convention: +X, -X, +Y, -Y, +Z, -Z
        cubemap.id = rlLoadTextureCubemap(faces.data, size, faces.format, faces.mipmaps);

        if (cubemap.id != 0)
        {
            cubemap.format = faces.format;
            cubemap.mipmaps = faces.mipmaps;
        }
        else log(LogLevel::warn, "IMAGE: Failed to load cubemap image");

        UnloadImage(faces);
    }
    else log(LogLevel::warn, "IMAGE: Failed to detect cubemap image layout");

    return cubemap;
}

// Load RGBA texture for rendering (framebuffer)
RenderTexture2D LoadRenderTexture(const i32 width, const i32 height)
{
    return LoadRenderTextureEx(width, height, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
}

// Load texture for rendering (framebuffer), with specific format
// NOTE: Render texture is loaded by default with RGBA color attachment and depth RenderBuffer
RLAPI RenderTexture2D LoadRenderTextureEx(i32 width, i32 height, i32 format)
{
    auto target = RenderTexture2D{ 0 };

    if (format >= PIXELFORMAT_COMPRESSED_DXT1_RGB)
    {
        log(LogLevel::warn, "FBO: Render texture format not supported");
        return target;
    }

    target.id = rlLoadFramebuffer(); // Load an empty framebuffer

    if (target.id > 0)
    {
        rlEnableFramebuffer(target.id);

        // Create color texture (default to RGBA)
        target.texture.id = rlLoadTexture(nullptr, width, height, format, 1);
        target.texture.width = width;
        target.texture.height = height;
        target.texture.format = format;
        target.texture.mipmaps = 1;

        // Create depth renderbuffer/texture
        target.depth.id = rlLoadTextureDepth(width, height, true);
        target.depth.width = width;
        target.depth.height = height;
        target.depth.format = 19;       //DEPTH_COMPONENT_24BIT?
        target.depth.mipmaps = 1;

        // Attach color texture and depth renderbuffer/texture to FBO
        rlFramebufferAttach(target.id, target.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
        rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);

        // Check if fbo is complete with attachments (valid)
        if (rlFramebufferComplete(target.id)) log(LogLevel::info, "FBO: [ID {}] Framebuffer object created successfully", target.id);

        rlDisableFramebuffer();
    }
    else log(LogLevel::warn, "FBO: Framebuffer object can not be created");

    return target;
}

// Check if texture is valid (loaded in GPU)
bool IsTextureValid(const Texture2D texture)
{
    auto result = false;

    if ((texture.id > 0) &&         // Validate OpenGL id (texture uploaded to GPU)
        (texture.width > 0) &&      // Validate texture width
        (texture.height > 0) &&     // Validate texture height
        (texture.format > 0) &&     // Validate texture pixel format
        (texture.mipmaps > 0)) result = true; // Validate texture mipmaps (at least 1 for basic mipmap level)

    return result;
}

// Unload texture from GPU memory (VRAM)
void UnloadTexture(const Texture2D texture)
{
    if (texture.id > 0)
    {
        rlUnloadTexture(texture.id);

        log(LogLevel::info, "TEXTURE: [ID {}] Unloaded texture data from VRAM (GPU)", texture.id);
    }
}

// Check if render texture is valid (loaded in GPU)
bool IsRenderTextureValid(const RenderTexture2D target)
{
    auto result = false;

    if ((target.id > 0) &&                  // Validate OpenGL id (loaded on GPU)
        IsTextureValid(target.depth) &&     // Validate FBO depth texture/renderbuffer attachment
        IsTextureValid(target.texture)) result = true; // Validate FBO texture attachment

    return result;
}

// Unload render texture from GPU memory (VRAM)
void UnloadRenderTexture(const RenderTexture2D target)
{
    if (target.id > 0)
    {
        if (target.texture.id > 0)
        {
            // Color texture attached to FBO is deleted
            rlUnloadTexture(target.texture.id);
        }

        // NOTE: Depth texture/renderbuffer is automatically
        // queried and deleted before deleting framebuffer
        rlUnloadFramebuffer(target.id);
    }
}

// Update GPU texture with new data
// NOTE 1: pixels data must match texture.format
// NOTE 2: pixels data must contain at least as many pixels as texture
void UpdateTexture(const Texture2D texture, const void *pixels)
{
    rlUpdateTexture(texture.id, 0, 0, texture.width, texture.height, texture.format, pixels);
}

// Update GPU texture rectangle with new data
// NOTE 1: pixels data must match texture.format
// NOTE 2: pixels data must contain as many pixels as rec contains
// NOTE 3: rec must fit completely within texture's width and height
void UpdateTextureRec(const Texture2D texture, const Rectangle rec, const void *pixels)
{
    rlUpdateTexture(texture.id, static_cast<i32>(rec.x), static_cast<i32>(rec.y), static_cast<i32>(rec.width), static_cast<i32>(rec.height), texture.format, pixels);
}

//------------------------------------------------------------------------------------
// Texture configuration functions
//------------------------------------------------------------------------------------
// Generate GPU mipmaps for a texture
void GenTextureMipmaps(Texture2D *texture)
{
    // NOTE: NPOT textures support check inside function
    // On WebGL (OpenGL ES 2.0) NPOT textures support is limited
    rlGenTextureMipmaps(texture->id, texture->width, texture->height, texture->format, &texture->mipmaps);
}

// Set texture scaling filter mode
void SetTextureFilter(const Texture2D texture, const i32 filter)
{
    switch (filter)
    {
        case TEXTURE_FILTER_POINT:
        {
            if (texture.mipmaps > 1)
            {
                // RL_TEXTURE_FILTER_MIP_NEAREST - tex filter: POINT, mipmaps filter: POINT (sharp switching between mipmaps)
                rlTextureParameters(texture.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_MIP_NEAREST);

                // RL_TEXTURE_FILTER_NEAREST - tex filter: POINT (no filter), no mipmaps
                rlTextureParameters(texture.id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_NEAREST);
            }
            else
            {
                // RL_TEXTURE_FILTER_NEAREST - tex filter: POINT (no filter), no mipmaps
                rlTextureParameters(texture.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_NEAREST);
                rlTextureParameters(texture.id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_NEAREST);
            }
        } break;
        case TEXTURE_FILTER_BILINEAR:
        {
            if (texture.mipmaps > 1)
            {
                // RL_TEXTURE_FILTER_LINEAR_MIP_NEAREST - tex filter: BILINEAR, mipmaps filter: POINT (sharp switching between mipmaps)
                // Alternative: RL_TEXTURE_FILTER_NEAREST_MIP_LINEAR - tex filter: POINT, mipmaps filter: BILINEAR (smooth transition between mipmaps)
                rlTextureParameters(texture.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_LINEAR_MIP_NEAREST);

                // RL_TEXTURE_FILTER_LINEAR - tex filter: BILINEAR, no mipmaps
                rlTextureParameters(texture.id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_LINEAR);
            }
            else
            {
                // RL_TEXTURE_FILTER_LINEAR - tex filter: BILINEAR, no mipmaps
                rlTextureParameters(texture.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_LINEAR);
                rlTextureParameters(texture.id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_LINEAR);
            }
        } break;
        case TEXTURE_FILTER_TRILINEAR:
        {
            if (texture.mipmaps > 1)
            {
                // RL_TEXTURE_FILTER_MIP_LINEAR - tex filter: BILINEAR, mipmaps filter: BILINEAR (smooth transition between mipmaps)
                rlTextureParameters(texture.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_MIP_LINEAR);

                // RL_TEXTURE_FILTER_LINEAR - tex filter: BILINEAR, no mipmaps
                rlTextureParameters(texture.id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_LINEAR);
            }
            else
            {
                log(LogLevel::warn, "TEXTURE: [ID {}] No mipmaps available for TRILINEAR texture filtering", texture.id);

                // RL_TEXTURE_FILTER_LINEAR - tex filter: BILINEAR, no mipmaps
                rlTextureParameters(texture.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_LINEAR);
                rlTextureParameters(texture.id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_LINEAR);
            }
        } break;
        case TEXTURE_FILTER_ANISOTROPIC_4X: rlTextureParameters(texture.id, RL_TEXTURE_FILTER_ANISOTROPIC, 4); break;
        case TEXTURE_FILTER_ANISOTROPIC_8X: rlTextureParameters(texture.id, RL_TEXTURE_FILTER_ANISOTROPIC, 8); break;
        case TEXTURE_FILTER_ANISOTROPIC_16X: rlTextureParameters(texture.id, RL_TEXTURE_FILTER_ANISOTROPIC, 16); break;
        default: break;
    }
}

// Set texture wrapping mode
void SetTextureWrap(const Texture2D texture, const i32 wrap)
{
    switch (wrap)
    {
        case TEXTURE_WRAP_REPEAT:
        {
            // NOTE: It only works if NPOT textures are supported, i.e. OpenGL ES 2.0 could not support it
            rlTextureParameters(texture.id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_REPEAT);
            rlTextureParameters(texture.id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_REPEAT);
        } break;
        case TEXTURE_WRAP_CLAMP:
        {
            rlTextureParameters(texture.id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_CLAMP);
            rlTextureParameters(texture.id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);
        } break;
        case TEXTURE_WRAP_MIRROR_REPEAT:
        {
            rlTextureParameters(texture.id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_MIRROR_REPEAT);
            rlTextureParameters(texture.id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_MIRROR_REPEAT);
        } break;
        case TEXTURE_WRAP_MIRROR_CLAMP:
        {
            rlTextureParameters(texture.id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_MIRROR_CLAMP);
            rlTextureParameters(texture.id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_MIRROR_CLAMP);
        } break;
        default: break;
    }
}

//------------------------------------------------------------------------------------
// Texture drawing functions
//------------------------------------------------------------------------------------
// Draw a texture
void DrawTexture(const Texture2D texture, const i32 posX, const i32 posY, const Color tint)
{
    DrawTextureEx(texture, vec2{ static_cast<f32>(posX), static_cast<f32>(posY) }, 0.0f, 1.0f, tint);
}

// Draw a texture with position defined as vec2
void DrawTextureV(const Texture2D texture, const vec2 position, const Color tint)
{
    DrawTextureEx(texture, position, 0.0f, 1.0f, tint);
}

// Draw a texture with rotation and scale
// NOTE: Rotation applied from top-left corner origin
void DrawTextureEx(const Texture2D texture, const vec2 position, const f32 rotation, const f32 scale, const Color tint)
{
    const auto srcrec = Rectangle{ 0.0f, 0.0f, static_cast<f32>(texture.width), static_cast<f32>(texture.height) };
    const auto dstrec = Rectangle{ position.x, position.y, static_cast<f32>(texture.width)*scale, static_cast<f32>(texture.height)*scale };
    const auto origin = vec2{ 0.0f, 0.0f };

    DrawTexturePro(texture, srcrec, dstrec, origin, rotation, tint);
}

// Draw a part of a texture (defined by a rectangle)
void DrawTextureRec(const Texture2D texture, const Rectangle rec, const vec2 position, const Color tint)
{
    const auto dstrec = Rectangle{ position.x, position.y, fabsf(rec.width), fabsf(rec.height) };
    const auto origin = vec2{ 0.0f, 0.0f };

    DrawTexturePro(texture, rec, dstrec, origin, 0.0f, tint);
}

// Draw a part of a texture (defined by a rectangle) with 'pro' parameters
// NOTE: origin is relative to destination rectangle size
void DrawTexturePro(const Texture2D texture, Rectangle srcrec, Rectangle dstrec, const vec2 origin, const f32 rotation, const Color tint)
{
    // Check if texture is valid
    if (texture.id > 0)
    {
        const auto width = static_cast<f32>(texture.width);
        const auto height = static_cast<f32>(texture.height);

        auto flipX = false;

        if (srcrec.width < 0) { flipX = true; srcrec.width *= -1; }
        if (srcrec.height < 0) srcrec.y -= srcrec.height;

        if (dstrec.width < 0) dstrec.width *= -1;
        if (dstrec.height < 0) dstrec.height *= -1;

        vec2 topLeft{};
        vec2 topRight{};
        vec2 bottomLeft{};
        vec2 bottomRight{};

        // Only calculate rotation if needed
        if (rotation == 0.0f)
        {
            const f32 x = dstrec.x - origin.x;
            const f32 y = dstrec.y - origin.y;
            topLeft = vec2{ x, y };
            topRight = vec2{ x + dstrec.width, y };
            bottomLeft = vec2{ x, y + dstrec.height };
            bottomRight = vec2{ x + dstrec.width, y + dstrec.height };
        }
        else
        {
            const f32 sinRotation = sinf(rotation*DEG2RAD);
            const f32 cosRotation = cosf(rotation*DEG2RAD);
            const f32 x = dstrec.x;
            const f32 y = dstrec.y;
            const f32 dx = -origin.x;
            const f32 dy = -origin.y;

            topLeft.x = x + dx*cosRotation - dy*sinRotation;
            topLeft.y = y + dx*sinRotation + dy*cosRotation;

            topRight.x = x + (dx + dstrec.width)*cosRotation - dy*sinRotation;
            topRight.y = y + (dx + dstrec.width)*sinRotation + dy*cosRotation;

            bottomLeft.x = x + dx*cosRotation - (dy + dstrec.height)*sinRotation;
            bottomLeft.y = y + dx*sinRotation + (dy + dstrec.height)*cosRotation;

            bottomRight.x = x + (dx + dstrec.width)*cosRotation - (dy + dstrec.height)*sinRotation;
            bottomRight.y = y + (dx + dstrec.width)*sinRotation + (dy + dstrec.height)*cosRotation;
        }

        rlSetTexture(texture.id);
        rlBegin(RL_QUADS);

            rlColor4ub(tint.r, tint.g, tint.b, tint.a);
            rlNormal3f(0.0f, 0.0f, 1.0f);                          // Normal vector pointing towards viewer

            // Top-left corner for texture and quad
            if (flipX) rlTexCoord2f((srcrec.x + srcrec.width)/width, srcrec.y/height);
            else rlTexCoord2f(srcrec.x/width, srcrec.y/height);
            rlVertex2f(topLeft.x, topLeft.y);

            // Bottom-left corner for texture and quad
            if (flipX) rlTexCoord2f((srcrec.x + srcrec.width)/width, (srcrec.y + srcrec.height)/height);
            else rlTexCoord2f(srcrec.x/width, (srcrec.y + srcrec.height)/height);
            rlVertex2f(bottomLeft.x, bottomLeft.y);

            // Bottom-right corner for texture and quad
            if (flipX) rlTexCoord2f(srcrec.x/width, (srcrec.y + srcrec.height)/height);
            else rlTexCoord2f((srcrec.x + srcrec.width)/width, (srcrec.y + srcrec.height)/height);
            rlVertex2f(bottomRight.x, bottomRight.y);

            // Top-right corner for texture and quad
            if (flipX) rlTexCoord2f(srcrec.x/width, srcrec.y/height);
            else rlTexCoord2f((srcrec.x + srcrec.width)/width, srcrec.y/height);
            rlVertex2f(topRight.x, topRight.y);

        rlEnd();
        rlSetTexture(0);

        // NOTE: Vertex position can be transformed using matrices
        // but the process is way more costly than calculating
        // the vertex positions manually, like done above
        // Old implementation is left here for educational purposes,
        // in case someone wants to do some performance test
        /*
        rlSetTexture(texture.id);
        rlPushMatrix();
            rlTranslatef(dstrec.x, dstrec.y, 0.0f);
            if (rotation != 0.0f) rlRotatef(rotation, 0.0f, 0.0f, 1.0f);
            rlTranslatef(-origin.x, -origin.y, 0.0f);

            rlBegin(RL_QUADS);
                rlColor4ub(tint.r, tint.g, tint.b, tint.a);
                rlNormal3f(0.0f, 0.0f, 1.0f);                          // Normal vector pointing towards viewer

                // Bottom-left corner for texture and quad
                if (flipX) rlTexCoord2f((srcrec.x + srcrec.width)/width, srcrec.y/height);
                else rlTexCoord2f(srcrec.x/width, srcrec.y/height);
                rlVertex2f(0.0f, 0.0f);

                // Bottom-right corner for texture and quad
                if (flipX) rlTexCoord2f((srcrec.x + srcrec.width)/width, (srcrec.y + srcrec.height)/height);
                else rlTexCoord2f(srcrec.x/width, (srcrec.y + srcrec.height)/height);
                rlVertex2f(0.0f, dstrec.height);

                // Top-right corner for texture and quad
                if (flipX) rlTexCoord2f(srcrec.x/width, (srcrec.y + srcrec.height)/height);
                else rlTexCoord2f((srcrec.x + srcrec.width)/width, (srcrec.y + srcrec.height)/height);
                rlVertex2f(dstrec.width, dstrec.height);

                // Top-left corner for texture and quad
                if (flipX) rlTexCoord2f(srcrec.x/width, srcrec.y/height);
                else rlTexCoord2f((srcrec.x + srcrec.width)/width, srcrec.y/height);
                rlVertex2f(dstrec.width, 0.0f);
            rlEnd();
        rlPopMatrix();
        rlSetTexture(0);
        */
    }
}

// Draw a texture (or part of it) that stretches or shrinks nicely using n-patch info
void DrawTextureNPatch(const Texture2D texture, NPatchInfo nPatchInfo, const Rectangle dstrec, const vec2 origin, const f32 rotation, const Color tint)
{
    if (texture.id > 0)
    {
        const auto width = static_cast<f32>(texture.width);
        const auto height = static_cast<f32>(texture.height);

        f32 patchWidth = (static_cast<i32>(dstrec.width) <= 0)? 0.0f : dstrec.width;
        f32 patchHeight = (static_cast<i32>(dstrec.height) <= 0)? 0.0f : dstrec.height;

        if (nPatchInfo.source.width < 0) nPatchInfo.source.x -= nPatchInfo.source.width;
        if (nPatchInfo.source.height < 0) nPatchInfo.source.y -= nPatchInfo.source.height;
        if (nPatchInfo.layout == NPATCH_THREE_PATCH_HORIZONTAL) patchHeight = nPatchInfo.source.height;
        if (nPatchInfo.layout == NPATCH_THREE_PATCH_VERTICAL) patchWidth = nPatchInfo.source.width;

        auto drawCenter = true;
        auto drawMiddle = true;
        auto leftBorder = static_cast<f32>(nPatchInfo.left);
        auto topBorder = static_cast<f32>(nPatchInfo.top);
        auto rightBorder = static_cast<f32>(nPatchInfo.right);
        auto bottomBorder = static_cast<f32>(nPatchInfo.bottom);

        // Adjust the lateral (left and right) border widths in case patchWidth < texture.width
        if (patchWidth <= (leftBorder + rightBorder) && nPatchInfo.layout != NPATCH_THREE_PATCH_VERTICAL)
        {
            drawCenter = false;
            leftBorder = (leftBorder/(leftBorder + rightBorder))*patchWidth;
            rightBorder = patchWidth - leftBorder;
        }

        // Adjust the lateral (top and bottom) border heights in case patchHeight < texture.height
        if (patchHeight <= (topBorder + bottomBorder) && nPatchInfo.layout != NPATCH_THREE_PATCH_HORIZONTAL)
        {
            drawMiddle = false;
            topBorder = (topBorder/(topBorder + bottomBorder))*patchHeight;
            bottomBorder = patchHeight - topBorder;
        }

        vec2 vertA{};
        vec2 vertB{};
        vec2 vertC{};
        vec2 vertD{};
        vertA.x = 0.0f;                             // Outer left
        vertA.y = 0.0f;                             // Outer top
        vertB.x = leftBorder;                       // Inner left
        vertB.y = topBorder;                        // Inner top
        vertC.x = patchWidth  - rightBorder;        // Inner right
        vertC.y = patchHeight - bottomBorder;       // Inner bottom
        vertD.x = patchWidth;                       // Outer right
        vertD.y = patchHeight;                      // Outer bottom

        vec2 coordA{};
        vec2 coordB{};
        vec2 coordC{};
        vec2 coordD{};
        coordA.x = nPatchInfo.source.x/width;
        coordA.y = nPatchInfo.source.y/height;
        coordB.x = (nPatchInfo.source.x + leftBorder)/width;
        coordB.y = (nPatchInfo.source.y + topBorder)/height;
        coordC.x = (nPatchInfo.source.x + nPatchInfo.source.width  - rightBorder)/width;
        coordC.y = (nPatchInfo.source.y + nPatchInfo.source.height - bottomBorder)/height;
        coordD.x = (nPatchInfo.source.x + nPatchInfo.source.width)/width;
        coordD.y = (nPatchInfo.source.y + nPatchInfo.source.height)/height;

        rlSetTexture(texture.id);

        rlPushMatrix();
            rlTranslatef(dstrec.x, dstrec.y, 0.0f);
            rlRotatef(rotation, 0.0f, 0.0f, 1.0f);
            rlTranslatef(-origin.x, -origin.y, 0.0f);

            rlBegin(RL_QUADS);
                rlColor4ub(tint.r, tint.g, tint.b, tint.a);
                rlNormal3f(0.0f, 0.0f, 1.0f);               // Normal vector pointing towards viewer

                if (nPatchInfo.layout == NPATCH_NINE_PATCH)
                {
                    // ------------------------------------------------------------
                    // TOP-LEFT QUAD
                    rlTexCoord2f(coordA.x, coordB.y); rlVertex2f(vertA.x, vertB.y);  // Bottom-left corner for texture and quad
                    rlTexCoord2f(coordB.x, coordB.y); rlVertex2f(vertB.x, vertB.y);  // Bottom-right corner for texture and quad
                    rlTexCoord2f(coordB.x, coordA.y); rlVertex2f(vertB.x, vertA.y);  // Top-right corner for texture and quad
                    rlTexCoord2f(coordA.x, coordA.y); rlVertex2f(vertA.x, vertA.y);  // Top-left corner for texture and quad
                    if (drawCenter)
                    {
                        // TOP-CENTER QUAD
                        rlTexCoord2f(coordB.x, coordB.y); rlVertex2f(vertB.x, vertB.y);  // Bottom-left corner for texture and quad
                        rlTexCoord2f(coordC.x, coordB.y); rlVertex2f(vertC.x, vertB.y);  // Bottom-right corner for texture and quad
                        rlTexCoord2f(coordC.x, coordA.y); rlVertex2f(vertC.x, vertA.y);  // Top-right corner for texture and quad
                        rlTexCoord2f(coordB.x, coordA.y); rlVertex2f(vertB.x, vertA.y);  // Top-left corner for texture and quad
                    }
                    // TOP-RIGHT QUAD
                    rlTexCoord2f(coordC.x, coordB.y); rlVertex2f(vertC.x, vertB.y);  // Bottom-left corner for texture and quad
                    rlTexCoord2f(coordD.x, coordB.y); rlVertex2f(vertD.x, vertB.y);  // Bottom-right corner for texture and quad
                    rlTexCoord2f(coordD.x, coordA.y); rlVertex2f(vertD.x, vertA.y);  // Top-right corner for texture and quad
                    rlTexCoord2f(coordC.x, coordA.y); rlVertex2f(vertC.x, vertA.y);  // Top-left corner for texture and quad
                    if (drawMiddle)
                    {
                        // ------------------------------------------------------------
                        // MIDDLE-LEFT QUAD
                        rlTexCoord2f(coordA.x, coordC.y); rlVertex2f(vertA.x, vertC.y);  // Bottom-left corner for texture and quad
                        rlTexCoord2f(coordB.x, coordC.y); rlVertex2f(vertB.x, vertC.y);  // Bottom-right corner for texture and quad
                        rlTexCoord2f(coordB.x, coordB.y); rlVertex2f(vertB.x, vertB.y);  // Top-right corner for texture and quad
                        rlTexCoord2f(coordA.x, coordB.y); rlVertex2f(vertA.x, vertB.y);  // Top-left corner for texture and quad
                        if (drawCenter)
                        {
                            // MIDDLE-CENTER QUAD
                            rlTexCoord2f(coordB.x, coordC.y); rlVertex2f(vertB.x, vertC.y);  // Bottom-left corner for texture and quad
                            rlTexCoord2f(coordC.x, coordC.y); rlVertex2f(vertC.x, vertC.y);  // Bottom-right corner for texture and quad
                            rlTexCoord2f(coordC.x, coordB.y); rlVertex2f(vertC.x, vertB.y);  // Top-right corner for texture and quad
                            rlTexCoord2f(coordB.x, coordB.y); rlVertex2f(vertB.x, vertB.y);  // Top-left corner for texture and quad
                        }

                        // MIDDLE-RIGHT QUAD
                        rlTexCoord2f(coordC.x, coordC.y); rlVertex2f(vertC.x, vertC.y);  // Bottom-left corner for texture and quad
                        rlTexCoord2f(coordD.x, coordC.y); rlVertex2f(vertD.x, vertC.y);  // Bottom-right corner for texture and quad
                        rlTexCoord2f(coordD.x, coordB.y); rlVertex2f(vertD.x, vertB.y);  // Top-right corner for texture and quad
                        rlTexCoord2f(coordC.x, coordB.y); rlVertex2f(vertC.x, vertB.y);  // Top-left corner for texture and quad
                    }

                    // ------------------------------------------------------------
                    // BOTTOM-LEFT QUAD
                    rlTexCoord2f(coordA.x, coordD.y); rlVertex2f(vertA.x, vertD.y);  // Bottom-left corner for texture and quad
                    rlTexCoord2f(coordB.x, coordD.y); rlVertex2f(vertB.x, vertD.y);  // Bottom-right corner for texture and quad
                    rlTexCoord2f(coordB.x, coordC.y); rlVertex2f(vertB.x, vertC.y);  // Top-right corner for texture and quad
                    rlTexCoord2f(coordA.x, coordC.y); rlVertex2f(vertA.x, vertC.y);  // Top-left corner for texture and quad
                    if (drawCenter)
                    {
                        // BOTTOM-CENTER QUAD
                        rlTexCoord2f(coordB.x, coordD.y); rlVertex2f(vertB.x, vertD.y);  // Bottom-left corner for texture and quad
                        rlTexCoord2f(coordC.x, coordD.y); rlVertex2f(vertC.x, vertD.y);  // Bottom-right corner for texture and quad
                        rlTexCoord2f(coordC.x, coordC.y); rlVertex2f(vertC.x, vertC.y);  // Top-right corner for texture and quad
                        rlTexCoord2f(coordB.x, coordC.y); rlVertex2f(vertB.x, vertC.y);  // Top-left corner for texture and quad
                    }

                    // BOTTOM-RIGHT QUAD
                    rlTexCoord2f(coordC.x, coordD.y); rlVertex2f(vertC.x, vertD.y);  // Bottom-left corner for texture and quad
                    rlTexCoord2f(coordD.x, coordD.y); rlVertex2f(vertD.x, vertD.y);  // Bottom-right corner for texture and quad
                    rlTexCoord2f(coordD.x, coordC.y); rlVertex2f(vertD.x, vertC.y);  // Top-right corner for texture and quad
                    rlTexCoord2f(coordC.x, coordC.y); rlVertex2f(vertC.x, vertC.y);  // Top-left corner for texture and quad
                }
                else if (nPatchInfo.layout == NPATCH_THREE_PATCH_VERTICAL)
                {
                    // TOP QUAD
                    // -----------------------------------------------------------
                    // Texture coords                 Vertices
                    rlTexCoord2f(coordA.x, coordB.y); rlVertex2f(vertA.x, vertB.y);  // Bottom-left corner for texture and quad
                    rlTexCoord2f(coordD.x, coordB.y); rlVertex2f(vertD.x, vertB.y);  // Bottom-right corner for texture and quad
                    rlTexCoord2f(coordD.x, coordA.y); rlVertex2f(vertD.x, vertA.y);  // Top-right corner for texture and quad
                    rlTexCoord2f(coordA.x, coordA.y); rlVertex2f(vertA.x, vertA.y);  // Top-left corner for texture and quad
                    if (drawCenter)
                    {
                        // MIDDLE QUAD
                        // -----------------------------------------------------------
                        // Texture coords                 Vertices
                        rlTexCoord2f(coordA.x, coordC.y); rlVertex2f(vertA.x, vertC.y);  // Bottom-left corner for texture and quad
                        rlTexCoord2f(coordD.x, coordC.y); rlVertex2f(vertD.x, vertC.y);  // Bottom-right corner for texture and quad
                        rlTexCoord2f(coordD.x, coordB.y); rlVertex2f(vertD.x, vertB.y);  // Top-right corner for texture and quad
                        rlTexCoord2f(coordA.x, coordB.y); rlVertex2f(vertA.x, vertB.y);  // Top-left corner for texture and quad
                    }
                    // BOTTOM QUAD
                    // -----------------------------------------------------------
                    // Texture coords                 Vertices
                    rlTexCoord2f(coordA.x, coordD.y); rlVertex2f(vertA.x, vertD.y);  // Bottom-left corner for texture and quad
                    rlTexCoord2f(coordD.x, coordD.y); rlVertex2f(vertD.x, vertD.y);  // Bottom-right corner for texture and quad
                    rlTexCoord2f(coordD.x, coordC.y); rlVertex2f(vertD.x, vertC.y);  // Top-right corner for texture and quad
                    rlTexCoord2f(coordA.x, coordC.y); rlVertex2f(vertA.x, vertC.y);  // Top-left corner for texture and quad
                }
                else if (nPatchInfo.layout == NPATCH_THREE_PATCH_HORIZONTAL)
                {
                    // LEFT QUAD
                    // -----------------------------------------------------------
                    // Texture coords                 Vertices
                    rlTexCoord2f(coordA.x, coordD.y); rlVertex2f(vertA.x, vertD.y);  // Bottom-left corner for texture and quad
                    rlTexCoord2f(coordB.x, coordD.y); rlVertex2f(vertB.x, vertD.y);  // Bottom-right corner for texture and quad
                    rlTexCoord2f(coordB.x, coordA.y); rlVertex2f(vertB.x, vertA.y);  // Top-right corner for texture and quad
                    rlTexCoord2f(coordA.x, coordA.y); rlVertex2f(vertA.x, vertA.y);  // Top-left corner for texture and quad
                    if (drawCenter)
                    {
                        // CENTER QUAD
                        // -----------------------------------------------------------
                        // Texture coords                 Vertices
                        rlTexCoord2f(coordB.x, coordD.y); rlVertex2f(vertB.x, vertD.y);  // Bottom-left corner for texture and quad
                        rlTexCoord2f(coordC.x, coordD.y); rlVertex2f(vertC.x, vertD.y);  // Bottom-right corner for texture and quad
                        rlTexCoord2f(coordC.x, coordA.y); rlVertex2f(vertC.x, vertA.y);  // Top-right corner for texture and quad
                        rlTexCoord2f(coordB.x, coordA.y); rlVertex2f(vertB.x, vertA.y);  // Top-left corner for texture and quad
                    }
                    // RIGHT QUAD
                    // -----------------------------------------------------------
                    // Texture coords                 Vertices
                    rlTexCoord2f(coordC.x, coordD.y); rlVertex2f(vertC.x, vertD.y);  // Bottom-left corner for texture and quad
                    rlTexCoord2f(coordD.x, coordD.y); rlVertex2f(vertD.x, vertD.y);  // Bottom-right corner for texture and quad
                    rlTexCoord2f(coordD.x, coordA.y); rlVertex2f(vertD.x, vertA.y);  // Top-right corner for texture and quad
                    rlTexCoord2f(coordC.x, coordA.y); rlVertex2f(vertC.x, vertA.y);  // Top-left corner for texture and quad
                }
            rlEnd();
        rlPopMatrix();

        rlSetTexture(0);
    }
}

// Check if two colors are equal
bool ColorIsEqual(const Color col1, const Color col2)
{
    auto result = false;

    if ((col1.r == col2.r) && (col1.g == col2.g) && (col1.b == col2.b) && (col1.a == col2.a)) result = true;

    return result;
}

// Get color with alpha applied, alpha goes from 0.0f to 1.0f
Color Fade(const Color color, f32 alpha)
{
    Color result = color;

    if (alpha < 0.0f) alpha = 0.0f;
    else if (alpha > 1.0f) alpha = 1.0f;

    result.a = static_cast<u8>(255.0f*alpha);

    return result;
}

// Get hexadecimal value for a Color
i32 ColorToInt(const Color color)
{
    auto result = 0;

    result = static_cast<i32>((static_cast<u32>(color.r) << 24) |
                   (static_cast<u32>(color.g) << 16) |
                   (static_cast<u32>(color.b) << 8) |
                    static_cast<u32>(color.a));

    return result;
}

// Get color normalized as float [0..1]
vec4 ColorNormalize(const Color color)
{
    vec4 result;

    result.x = static_cast<f32>(color.r)/255.0f;
    result.y = static_cast<f32>(color.g)/255.0f;
    result.z = static_cast<f32>(color.b)/255.0f;
    result.w = static_cast<f32>(color.a)/255.0f;

    return result;
}

// Get color from normalized values [0..1]
Color ColorFromNormalized(const vec4 normalized)
{
    Color result;

    result.r = static_cast<u8>(normalized.x*255.0f);
    result.g = static_cast<u8>(normalized.y*255.0f);
    result.b = static_cast<u8>(normalized.z*255.0f);
    result.a = static_cast<u8>(normalized.w*255.0f);

    return result;
}

// Get HSV values for a Color
// NOTE: Hue is returned as degrees [0..360]
vec3 ColorToHSV(const Color color)
{
    vec3 hsv{};
    const auto rgb = vec3{ static_cast<f32>(color.r)/255.0f, static_cast<f32>(color.g)/255.0f, static_cast<f32>(color.b)/255.0f };
    auto min = 0.0f;
    auto max = 0.0f;
    auto delta = 0.0f;

    min = (rgb.x < rgb.y)? rgb.x : rgb.y;
    min = (min < rgb.z)? min : rgb.z;

    max = (rgb.x > rgb.y)? rgb.x : rgb.y;
    max = (max > rgb.z)? max : rgb.z;

    hsv.z = max; // Value
    delta = max - min;

    if (delta < 0.00001f)
    {
        hsv.y = 0.0f;
        hsv.x = 0.0f; // Undefined, maybe NAN?
        return hsv;
    }

    if (max > 0.0f)
    {
        // NOTE: If max is 0, this divide would cause a crash
        hsv.y = (delta/max); // Saturation
    }
    else
    {
        // NOTE: If max is 0, then r = g = b = 0, s = 0, h is undefined
        hsv.y = 0.0f;
        hsv.x = NAN; // Undefined
        return hsv;
    }

    // NOTE: Comparing float values could not work properly
    if (rgb.x >= max) hsv.x = (rgb.y - rgb.z)/delta; // Between yellow & magenta
    else
    {
        if (rgb.y >= max) hsv.x = 2.0f + (rgb.z - rgb.x)/delta; // Between cyan & yellow
        else hsv.x = 4.0f + (rgb.x - rgb.y)/delta;              // Between magenta & cyan
    }

    hsv.x *= 60.0f; // Convert to degrees

    if (hsv.x < 0.0f) hsv.x += 360.0f;

    return hsv;
}

// Get a Color from HSV values
// Implementation reference: https://en.wikipedia.org/wiki/HSL_and_HSV#Alternative_HSV_conversion
// NOTE: Color->HSV->Color conversion will not yield exactly the same color due to rounding errors
// Hue is provided in degrees: [0..360]
// Saturation/Value are provided normalized: [0.0f..1.0f]
Color ColorFromHSV(const f32 hue, const f32 saturation, const f32 value)
{
    auto color = Color{ 0, 0, 0, 255 };

    // Red channel
    f32 k = fmodf((5.0f + hue/60.0f), 6);
    f32 t = 4.0f - k;
    k = (t < k)? t : k;
    k = (k < 1)? k : 1;
    k = (k > 0)? k : 0;
    color.r = static_cast<u8>((value - value*saturation*k)*255.0f);

    // Green channel
    k = fmodf((3.0f + hue/60.0f), 6);
    t = 4.0f - k;
    k = (t < k)? t : k;
    k = (k < 1)? k : 1;
    k = (k > 0)? k : 0;
    color.g = static_cast<u8>((value - value*saturation*k)*255.0f);

    // Blue channel
    k = fmodf((1.0f + hue/60.0f), 6);
    t = 4.0f - k;
    k = (t < k)? t : k;
    k = (k < 1)? k : 1;
    k = (k > 0)? k : 0;
    color.b = static_cast<u8>((value - value*saturation*k)*255.0f);

    return color;
}

// Get color multiplied with another color
Color ColorTint(const Color color, const Color tint)
{
    Color result = color;

    const u8 r = static_cast<u8>((static_cast<i32>(color.r)*static_cast<i32>(tint.r))/255);
    const u8 g = static_cast<u8>((static_cast<i32>(color.g)*static_cast<i32>(tint.g))/255);
    const u8 b = static_cast<u8>((static_cast<i32>(color.b)*static_cast<i32>(tint.b))/255);
    const u8 a = static_cast<u8>((static_cast<i32>(color.a)*static_cast<i32>(tint.a))/255);

    result.r = r;
    result.g = g;
    result.b = b;
    result.a = a;

    return result;
}

// Get color with brightness correction, brightness factor goes from -1.0f to 1.0f
Color ColorBrightness(const Color color, f32 factor)
{
    Color result = color;

    if (factor > 1.0f) factor = 1.0f;
    else if (factor < -1.0f) factor = -1.0f;

    auto red = static_cast<f32>(color.r);
    auto green = static_cast<f32>(color.g);
    auto blue = static_cast<f32>(color.b);

    if (factor < 0.0f)
    {
        factor = 1.0f + factor;
        red *= factor;
        green *= factor;
        blue *= factor;
    }
    else
    {
        red = (255 - red)*factor + red;
        green = (255 - green)*factor + green;
        blue = (255 - blue)*factor + blue;
    }

    result.r = static_cast<u8>(red);
    result.g = static_cast<u8>(green);
    result.b = static_cast<u8>(blue);

    return result;
}

// Get color with contrast correction
// NOTE: Contrast values between -1.0f and 1.0f
Color ColorContrast(const Color color, f32 contrast)
{
    Color result = color;

    if (contrast < -1.0f) contrast = -1.0f;
    else if (contrast > 1.0f) contrast = 1.0f;

    contrast = (1.0f + contrast);
    contrast *= contrast;

    f32 pR = static_cast<f32>(color.r)/255.0f;
    pR -= 0.5f;
    pR *= contrast;
    pR += 0.5f;
    pR *= 255;
    if (pR < 0) pR = 0;
    else if (pR > 255) pR = 255;

    f32 pG = static_cast<f32>(color.g)/255.0f;
    pG -= 0.5f;
    pG *= contrast;
    pG += 0.5f;
    pG *= 255;
    if (pG < 0) pG = 0;
    else if (pG > 255) pG = 255;

    f32 pB = static_cast<f32>(color.b)/255.0f;
    pB -= 0.5f;
    pB *= contrast;
    pB += 0.5f;
    pB *= 255;
    if (pB < 0) pB = 0;
    else if (pB > 255) pB = 255;

    result.r = static_cast<u8>(pR);
    result.g = static_cast<u8>(pG);
    result.b = static_cast<u8>(pB);

    return result;
}

// Get color with alpha applied, alpha goes from 0.0f to 1.0f
Color ColorAlpha(const Color color, f32 alpha)
{
    Color result = color;

    if (alpha < 0.0f) alpha = 0.0f;
    else if (alpha > 1.0f) alpha = 1.0f;

    result.a = static_cast<u8>(255.0f*alpha);

    return result;
}

// Get src alpha-blended into dst color with tint
Color ColorAlphaBlend(const Color dst, Color src, const Color tint)
{
    Color result = WHITE;

    // Apply color tint to source color
    src.r = static_cast<u8>((static_cast<u32>(src.r)*(static_cast<u32>(tint.r)+1)) >> 8);
    src.g = static_cast<u8>((static_cast<u32>(src.g)*(static_cast<u32>(tint.g)+1)) >> 8);
    src.b = static_cast<u8>((static_cast<u32>(src.b)*(static_cast<u32>(tint.b)+1)) >> 8);
    src.a = static_cast<u8>((static_cast<u32>(src.a)*(static_cast<u32>(tint.a)+1)) >> 8);

//#define COLORALPHABLEND_FLOAT
#define COLORALPHABLEND_INTEGERS
#if defined(COLORALPHABLEND_INTEGERS)
    if (src.a == 0) result = dst;
    else if (src.a == 255) result = src;
    else
    {
        const u32 alpha = static_cast<u32>(src.a) + 1; // Shifting by 8 (dividing by 256), so need to take that excess into account
        result.a = static_cast<u8>((alpha*256 + static_cast<u32>(dst.a)*(256 - alpha)) >> 8);

        if (result.a > 0)
        {
            result.r = static_cast<u8>(((static_cast<u32>(src.r)*alpha*256 + static_cast<u32>(dst.r)*static_cast<u32>(dst.a)*(256 - alpha))/result.a) >> 8);
            result.g = static_cast<u8>(((static_cast<u32>(src.g)*alpha*256 + static_cast<u32>(dst.g)*static_cast<u32>(dst.a)*(256 - alpha))/result.a) >> 8);
            result.b = static_cast<u8>(((static_cast<u32>(src.b)*alpha*256 + static_cast<u32>(dst.b)*static_cast<u32>(dst.a)*(256 - alpha))/result.a) >> 8);
        }
    }
#endif
#if defined(COLORALPHABLEND_FLOAT)
    if (src.a == 0) result = dst;
    else if (src.a == 255) result = src;
    else
    {
        auto fdst = ColorNormalize(dst);
        auto fsrc = ColorNormalize(src);
        auto ftint = ColorNormalize(tint);
        vec4 fout{};

        fout.w = fsrc.w + fdst.w*(1.0f - fsrc.w);

        if (fout.w > 0.0f)
        {
            fout.x = (fsrc.x*fsrc.w + fdst.x*fdst.w*(1 - fsrc.w))/fout.w;
            fout.y = (fsrc.y*fsrc.w + fdst.y*fdst.w*(1 - fsrc.w))/fout.w;
            fout.z = (fsrc.z*fsrc.w + fdst.z*fdst.w*(1 - fsrc.w))/fout.w;
        }

        result = Color{ static_cast<u8>((fout.x*255.0f)), static_cast<u8>((fout.y*255.0f)), static_cast<u8>((fout.z*255.0f)), static_cast<u8>((fout.w*255.0f)) };
    }
#endif

    return result;
}

// Get color lerp interpolation between two colors, factor [0.0f..1.0f]
Color ColorLerp(const Color color1, const Color color2, f32 factor)
{
    auto color = Color{ 0 };

    if (factor < 0.0f) factor = 0.0f;
    else if (factor > 1.0f) factor = 1.0f;

    color.r = static_cast<u8>((1.0f - factor)*static_cast<f32>(color1.r) + factor*static_cast<f32>(color2.r));
    color.g = static_cast<u8>((1.0f - factor)*static_cast<f32>(color1.g) + factor*static_cast<f32>(color2.g));
    color.b = static_cast<u8>((1.0f - factor)*static_cast<f32>(color1.b) + factor*static_cast<f32>(color2.b));
    color.a = static_cast<u8>((1.0f - factor)*static_cast<f32>(color1.a) + factor*static_cast<f32>(color2.a));

    return color;
}

// Get a Color struct from hexadecimal value
Color GetColor(const u32 hexValue)
{
    Color color;

    color.r = static_cast<u8>(hexValue >> 24) & 0xff;
    color.g = static_cast<u8>(hexValue >> 16) & 0xff;
    color.b = static_cast<u8>(hexValue >> 8) & 0xff;
    color.a = static_cast<u8>(hexValue) & 0xff;

    return color;
}

// Get color from a pixel from certain format
Color GetPixelColor(const void *srcPtr, const i32 format)
{
    auto color = Color{ 0 };

    switch (format)
    {
        case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE: color = Color{ (static_cast<const u8 *>(srcPtr))[0], (static_cast<const u8 *>(srcPtr))[0], (static_cast<const u8 *>(srcPtr))[0], 255 }; break;
        case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA: color = Color{ (static_cast<const u8 *>(srcPtr))[0], (static_cast<const u8 *>(srcPtr))[0], (static_cast<const u8 *>(srcPtr))[0], (static_cast<const u8 *>(srcPtr))[1] }; break;
        case PIXELFORMAT_UNCOMPRESSED_R5G6B5:
        {
            color.r = static_cast<u8>(((static_cast<const u16 *>(srcPtr))[0] >> 11)*255/31);
            color.g = static_cast<u8>((((static_cast<const u16 *>(srcPtr))[0] >> 5) & 0b0000000000111111)*255/63);
            color.b = static_cast<u8>(((static_cast<const u16 *>(srcPtr))[0] & 0b0000000000011111)*255/31);
            color.a = 255;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
        {
            color.r = static_cast<u8>(((static_cast<const u16 *>(srcPtr))[0] >> 11)*255/31);
            color.g = static_cast<u8>((((static_cast<const u16 *>(srcPtr))[0] >> 6) & 0b0000000000011111)*255/31);
            color.b = static_cast<u8>((((static_cast<const u16 *>(srcPtr))[0] >> 1) & 0b0000000000011111)*255/31);
            color.a = ((static_cast<const u16 *>(srcPtr))[0] & 0b0000000000000001)? 255 : 0;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4:
        {
            color.r = static_cast<u8>(((static_cast<const u16 *>(srcPtr))[0] >> 12)*255/15);
            color.g = static_cast<u8>((((static_cast<const u16 *>(srcPtr))[0] >> 8) & 0b0000000000001111)*255/15);
            color.b = static_cast<u8>((((static_cast<const u16 *>(srcPtr))[0] >> 4) & 0b0000000000001111)*255/15);
            color.a = static_cast<u8>(((static_cast<const u16 *>(srcPtr))[0] & 0b0000000000001111)*255/15);

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8: color = Color{ (static_cast<const u8 *>(srcPtr))[0], (static_cast<const u8 *>(srcPtr))[1], (static_cast<const u8 *>(srcPtr))[2], (static_cast<const u8 *>(srcPtr))[3] }; break;
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8: color = Color{ (static_cast<const u8 *>(srcPtr))[0], (static_cast<const u8 *>(srcPtr))[1], (static_cast<const u8 *>(srcPtr))[2], 255 }; break;
        case PIXELFORMAT_UNCOMPRESSED_R32:
        {
            // NOTE: Pixel normalized float value is converted to [0..255]
            color.r = static_cast<u8>((static_cast<const f32 *>(srcPtr))[0]*255.0f);
            color.g = static_cast<u8>((static_cast<const f32 *>(srcPtr))[0]*255.0f);
            color.b = static_cast<u8>((static_cast<const f32 *>(srcPtr))[0]*255.0f);
            color.a = 255;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R32G32B32:
        {
            // NOTE: Pixel normalized float value is converted to [0..255]
            color.r = static_cast<u8>((static_cast<const f32 *>(srcPtr))[0]*255.0f);
            color.g = static_cast<u8>((static_cast<const f32 *>(srcPtr))[1]*255.0f);
            color.b = static_cast<u8>((static_cast<const f32 *>(srcPtr))[2]*255.0f);
            color.a = 255;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32:
        {
            // NOTE: Pixel normalized float value is converted to [0..255]
            color.r = static_cast<u8>((static_cast<const f32 *>(srcPtr))[0]*255.0f);
            color.g = static_cast<u8>((static_cast<const f32 *>(srcPtr))[1]*255.0f);
            color.b = static_cast<u8>((static_cast<const f32 *>(srcPtr))[2]*255.0f);
            color.a = static_cast<u8>((static_cast<const f32 *>(srcPtr))[3]*255.0f);

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R16:
        {
            // NOTE: Pixel normalized float value is converted to [0..255]
            color.r = static_cast<u8>(HalfToFloat((static_cast<const u16 *>(srcPtr))[0])*255.0f);
            color.g = static_cast<u8>(HalfToFloat((static_cast<const u16 *>(srcPtr))[0])*255.0f);
            color.b = static_cast<u8>(HalfToFloat((static_cast<const u16 *>(srcPtr))[0])*255.0f);
            color.a = 255;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R16G16B16:
        {
            // NOTE: Pixel normalized float value is converted to [0..255]
            color.r = static_cast<u8>(HalfToFloat((static_cast<const u16 *>(srcPtr))[0])*255.0f);
            color.g = static_cast<u8>(HalfToFloat((static_cast<const u16 *>(srcPtr))[1])*255.0f);
            color.b = static_cast<u8>(HalfToFloat((static_cast<const u16 *>(srcPtr))[2])*255.0f);
            color.a = 255;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R16G16B16A16:
        {
            // NOTE: Pixel normalized float value is converted to [0..255]
            color.r = static_cast<u8>(HalfToFloat((static_cast<const u16 *>(srcPtr))[0])*255.0f);
            color.g = static_cast<u8>(HalfToFloat((static_cast<const u16 *>(srcPtr))[1])*255.0f);
            color.b = static_cast<u8>(HalfToFloat((static_cast<const u16 *>(srcPtr))[2])*255.0f);
            color.a = static_cast<u8>(HalfToFloat((static_cast<const u16 *>(srcPtr))[3])*255.0f);

        } break;
        default: break;
    }

    return color;
}

// Set pixel color formatted into destination pointer
void SetPixelColor(void *dstPtr, const Color color, const i32 format)
{
    switch (format)
    {
        case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE:
        {
            // NOTE: Calculate grayscale equivalent color
            const auto coln = vec3{ static_cast<f32>(color.r)/255.0f, static_cast<f32>(color.g)/255.0f, static_cast<f32>(color.b)/255.0f };
            const auto gray = static_cast<u8>((coln.x*0.299f + coln.y*0.587f + coln.z*0.114f)*255.0f);

            (static_cast<u8 *>(dstPtr))[0] = gray;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:
        {
            // NOTE: Calculate grayscale equivalent color
            const auto coln = vec3{ static_cast<f32>(color.r)/255.0f, static_cast<f32>(color.g)/255.0f, static_cast<f32>(color.b)/255.0f };
            const auto gray = static_cast<u8>((coln.x*0.299f + coln.y*0.587f + coln.z*0.114f)*255.0f);

            (static_cast<u8 *>(dstPtr))[0] = gray;
            (static_cast<u8 *>(dstPtr))[1] = color.a;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R5G6B5:
        {
            // NOTE: Calculate R5G6B5 equivalent color
            const auto coln = vec3{ static_cast<f32>(color.r)/255.0f, static_cast<f32>(color.g)/255.0f, static_cast<f32>(color.b)/255.0f };

            const auto r = static_cast<u8>(round(coln.x*31.0f));
            const auto g = static_cast<u8>(round(coln.y*63.0f));
            const auto b = static_cast<u8>(round(coln.z*31.0f));

            (static_cast<u16 *>(dstPtr))[0] = static_cast<u16>(r) << 11 | static_cast<u16>(g) << 5 | static_cast<u16>(b);

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
        {
            // NOTE: Calculate R5G5B5A1 equivalent color
            const auto coln = vec4{ static_cast<f32>(color.r)/255.0f, static_cast<f32>(color.g)/255.0f, static_cast<f32>(color.b)/255.0f, static_cast<f32>(color.a)/255.0f };

            const auto r = static_cast<u8>(round(coln.x*31.0f));
            const auto g = static_cast<u8>(round(coln.y*31.0f));
            const auto b = static_cast<u8>(round(coln.z*31.0f));
            const u8 a = (coln.w > (static_cast<f32>(PIXELFORMAT_UNCOMPRESSED_R5G5B5A1_ALPHA_THRESHOLD)/255.0f))? 1 : 0;

            (static_cast<u16 *>(dstPtr))[0] = static_cast<u16>(r) << 11 | static_cast<u16>(g) << 6 | static_cast<u16>(b) << 1 | static_cast<u16>(a);

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4:
        {
            // NOTE: Calculate R5G5B5A1 equivalent color
            const auto coln = vec4{ static_cast<f32>(color.r)/255.0f, static_cast<f32>(color.g)/255.0f, static_cast<f32>(color.b)/255.0f, static_cast<f32>(color.a)/255.0f };

            const auto r = static_cast<u8>(round(coln.x*15.0f));
            const auto g = static_cast<u8>(round(coln.y*15.0f));
            const auto b = static_cast<u8>(round(coln.z*15.0f));
            const auto a = static_cast<u8>(round(coln.w*15.0f));

            (static_cast<u16 *>(dstPtr))[0] = static_cast<u16>(r) << 12 | static_cast<u16>(g) << 8 | static_cast<u16>(b) << 4 | static_cast<u16>(a);

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8:
        {
            (static_cast<u8 *>(dstPtr))[0] = color.r;
            (static_cast<u8 *>(dstPtr))[1] = color.g;
            (static_cast<u8 *>(dstPtr))[2] = color.b;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8:
        {
            (static_cast<u8 *>(dstPtr))[0] = color.r;
            (static_cast<u8 *>(dstPtr))[1] = color.g;
            (static_cast<u8 *>(dstPtr))[2] = color.b;
            (static_cast<u8 *>(dstPtr))[3] = color.a;

        } break;
        default: break;
    }
}

// Get pixel data size in bytes for certain format
// NOTE: Size can be requested for Image or Texture data
i32 GetPixelDataSize(const i32 width, const i32 height, const i32 format)
{
    auto dataSize = 0;       // Size in bytes
    auto bpp = 0;            // Bits per pixel
    
    if ((width <= 0) || (height <= 0)) return dataSize;
    
    // TODO: Use rlGetPixelDataSize() directly instead of custom implementation?
    // WARNING: raylib pixel format enum must be aligned with rlgl pixel format
    //dataSize = rlGetPixelDataSize(width, height, format);

    switch (format)
    {
        case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE: bpp = 8; break;
        case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:
        case PIXELFORMAT_UNCOMPRESSED_R5G6B5:
        case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
        case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4: bpp = 16; break;
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8: bpp = 32; break;
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8: bpp = 24; break;
        case PIXELFORMAT_UNCOMPRESSED_R32: bpp = 32; break;
        case PIXELFORMAT_UNCOMPRESSED_R32G32B32: bpp = 32*3; break;
        case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32: bpp = 32*4; break;
        case PIXELFORMAT_UNCOMPRESSED_R16: bpp = 16; break;
        case PIXELFORMAT_UNCOMPRESSED_R16G16B16: bpp = 16*3; break;
        case PIXELFORMAT_UNCOMPRESSED_R16G16B16A16: bpp = 16*4; break;
        case PIXELFORMAT_COMPRESSED_DXT1_RGB:
        case PIXELFORMAT_COMPRESSED_DXT1_RGBA:
        case PIXELFORMAT_COMPRESSED_ETC1_RGB:
        case PIXELFORMAT_COMPRESSED_ETC2_RGB:
        case PIXELFORMAT_COMPRESSED_PVRT_RGB:
        case PIXELFORMAT_COMPRESSED_PVRT_RGBA: bpp = 4; break;
        case PIXELFORMAT_COMPRESSED_DXT3_RGBA:
        case PIXELFORMAT_COMPRESSED_DXT5_RGBA:
        case PIXELFORMAT_COMPRESSED_ETC2_EAC_RGBA:
        case PIXELFORMAT_COMPRESSED_ASTC_4x4_RGBA: bpp = 8; break;
        case PIXELFORMAT_COMPRESSED_ASTC_8x8_RGBA: bpp = 2; break;
        default: break;
    }

    const unsigned long long dataSizeBytes = (static_cast<unsigned long long>(width)*height*bpp) >> 3;  // Get size in bytes (dividing by 8)

    if (dataSizeBytes < INT_MAX)
    {
        dataSize = static_cast<i32>(dataSizeBytes);

        // Most compressed formats works on 4x4 blocks,
        // if texture is smaller, minimum dataSize is 8 or 16
        if ((width < 4) && (height < 4))
        {
            if ((format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) && (format < PIXELFORMAT_COMPRESSED_DXT3_RGBA)) dataSize = 8;
            else if ((format >= PIXELFORMAT_COMPRESSED_DXT3_RGBA) && (format < PIXELFORMAT_COMPRESSED_ASTC_8x8_RGBA)) dataSize = 16;
        }
    }
    else
    {
        // NOTE: In case required image data larger than 2GB, no memory allocated at all (NULL)
        log(LogLevel::warn, "Requested image size is larger than 2GB, it can not be allocated");
    }

    return dataSize;
}

//----------------------------------------------------------------------------------
// Module Internal Functions Definition
//----------------------------------------------------------------------------------
// Convert half-float (stored as unsigned short) to float
// REF: https://stackoverflow.com/questions/1659440/32-bit-to-16-bit-floating-point-conversion/60047308#60047308
static f32 HalfToFloat(const u16 x)
{
    auto result = 0.0f;

    union {
        f32 fm;
        u32 ui;
    } uni;

    const u32 e = (x & 0x7c00) >> 10; // Exponent
    const u32 m = (x & 0x03ff) << 13; // Mantissa
    uni.fm = static_cast<f32>(m);
    const u32 v = uni.ui >> 23;                                                                                                   // Evil log2 bit hack to count leading zeros in denormalized format
    uni.ui = (x & 0x8000) << 16 | (e != 0)*((e + 112) << 23 | m) | ((e == 0)&(m != 0))*((v - 37) << 23 | ((m << (150 - v)) & 0x007fe000)); // sign : normalized : denormalized

    result = uni.fm;

    return result;
}

// Convert float to half-float (stored as unsigned short)
static u16 FloatToHalf(const f32 x)
{
    u16 result = 0;

    union {
        f32 fm;
        u32 ui;
    } uni;
    uni.fm = x;

    const u32 b = uni.ui + 0x00001000;    // Round-to-nearest-even: add last bit after truncated mantissa
    const u32 e = (b & 0x7f800000) >> 23; // Exponent
    const u32 m = b & 0x007fffff;         // Mantissa; in line below: 0x007ff000 = 0x00800000-0x00001000 = decimal indicator flag - initial rounding

    result = (b & 0x80000000) >> 16 | (e > 112)*((((e - 112) << 10) & 0x7c00) | m >> 13) | ((e < 113) & (e > 101))*((((0x007ff000 + m) >> (125 - e)) + 1) >> 1) | (e > 143)*0x7fff; // sign : normalized : denormalized : saturate

    return result;
}

// Get pixel data from image as vec4 array (float normalized)
static vec4 *LoadImageDataNormalized(const Image image)
{
    auto pixels = static_cast<vec4 *>(calloc(image.width*image.height, sizeof(vec4)));

    if (image.format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) log(LogLevel::warn, "IMAGE: Pixel data retrieval not supported for compressed image formats");
    else
    {
        for (i32 i = 0, k = 0; i < image.width*image.height; i++)
        {
            switch (image.format)
            {
                case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE:
                {
                    pixels[i].x = static_cast<f32>((static_cast<u8 *>(image.data))[i])/255.0f;
                    pixels[i].y = static_cast<f32>((static_cast<u8 *>(image.data))[i])/255.0f;
                    pixels[i].z = static_cast<f32>((static_cast<u8 *>(image.data))[i])/255.0f;
                    pixels[i].w = 1.0f;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:
                {
                    pixels[i].x = static_cast<f32>((static_cast<u8 *>(image.data))[k])/255.0f;
                    pixels[i].y = static_cast<f32>((static_cast<u8 *>(image.data))[k])/255.0f;
                    pixels[i].z = static_cast<f32>((static_cast<u8 *>(image.data))[k])/255.0f;
                    pixels[i].w = static_cast<f32>((static_cast<u8 *>(image.data))[k + 1])/255.0f;

                    k += 2;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
                {
                    const u16 pixel = (static_cast<u16 *>(image.data))[i];

                    pixels[i].x = static_cast<f32>((pixel & 0b1111100000000000) >> 11)*(1.0f/31);
                    pixels[i].y = static_cast<f32>((pixel & 0b0000011111000000) >> 6)*(1.0f/31);
                    pixels[i].z = static_cast<f32>((pixel & 0b0000000000111110) >> 1)*(1.0f/31);
                    pixels[i].w = ((pixel & 0b0000000000000001) == 0)? 0.0f : 1.0f;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R5G6B5:
                {
                    const u16 pixel = (static_cast<u16 *>(image.data))[i];

                    pixels[i].x = static_cast<f32>((pixel & 0b1111100000000000) >> 11)*(1.0f/31);
                    pixels[i].y = static_cast<f32>((pixel & 0b0000011111100000) >> 5)*(1.0f/63);
                    pixels[i].z = static_cast<f32>(pixel & 0b0000000000011111)*(1.0f/31);
                    pixels[i].w = 1.0f;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4:
                {
                    const u16 pixel = (static_cast<u16 *>(image.data))[i];

                    pixels[i].x = static_cast<f32>((pixel & 0b1111000000000000) >> 12)*(1.0f/15);
                    pixels[i].y = static_cast<f32>((pixel & 0b0000111100000000) >> 8)*(1.0f/15);
                    pixels[i].z = static_cast<f32>((pixel & 0b0000000011110000) >> 4)*(1.0f/15);
                    pixels[i].w = static_cast<f32>(pixel & 0b0000000000001111)*(1.0f/15);

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8:
                {
                    pixels[i].x = static_cast<f32>((static_cast<u8 *>(image.data))[k])/255.0f;
                    pixels[i].y = static_cast<f32>((static_cast<u8 *>(image.data))[k + 1])/255.0f;
                    pixels[i].z = static_cast<f32>((static_cast<u8 *>(image.data))[k + 2])/255.0f;
                    pixels[i].w = static_cast<f32>((static_cast<u8 *>(image.data))[k + 3])/255.0f;

                    k += 4;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R8G8B8:
                {
                    pixels[i].x = static_cast<f32>((static_cast<u8 *>(image.data))[k])/255.0f;
                    pixels[i].y = static_cast<f32>((static_cast<u8 *>(image.data))[k + 1])/255.0f;
                    pixels[i].z = static_cast<f32>((static_cast<u8 *>(image.data))[k + 2])/255.0f;
                    pixels[i].w = 1.0f;

                    k += 3;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32:
                {
                    pixels[i].x = (static_cast<f32 *>(image.data))[k];
                    pixels[i].y = 0.0f;
                    pixels[i].z = 0.0f;
                    pixels[i].w = 1.0f;

                    k += 1;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32G32B32:
                {
                    pixels[i].x = (static_cast<f32 *>(image.data))[k];
                    pixels[i].y = (static_cast<f32 *>(image.data))[k + 1];
                    pixels[i].z = (static_cast<f32 *>(image.data))[k + 2];
                    pixels[i].w = 1.0f;

                    k += 3;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32:
                {
                    pixels[i].x = (static_cast<f32 *>(image.data))[k];
                    pixels[i].y = (static_cast<f32 *>(image.data))[k + 1];
                    pixels[i].z = (static_cast<f32 *>(image.data))[k + 2];
                    pixels[i].w = (static_cast<f32 *>(image.data))[k + 3];

                    k += 4;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R16:
                {
                    pixels[i].x = HalfToFloat((static_cast<u16 *>(image.data))[k]);
                    pixels[i].y = 0.0f;
                    pixels[i].z = 0.0f;
                    pixels[i].w = 1.0f;

                    k += 1;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R16G16B16:
                {
                    pixels[i].x = HalfToFloat((static_cast<u16 *>(image.data))[k]);
                    pixels[i].y = HalfToFloat((static_cast<u16 *>(image.data))[k + 1]);
                    pixels[i].z = HalfToFloat((static_cast<u16 *>(image.data))[k + 2]);
                    pixels[i].w = 1.0f;

                    k += 3;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R16G16B16A16:
                {
                    pixels[i].x = HalfToFloat((static_cast<u16 *>(image.data))[k]);
                    pixels[i].y = HalfToFloat((static_cast<u16 *>(image.data))[k + 1]);
                    pixels[i].z = HalfToFloat((static_cast<u16 *>(image.data))[k + 2]);
                    pixels[i].w = HalfToFloat((static_cast<u16 *>(image.data))[k + 3]);

                    k += 4;
                } break;
                default: break;
            }
        }
    }

    return pixels;
}

#endif // SUPPORT_MODULE_RTEXTURES
