/**********************************************************************************************
*
*   rtext - Basic functions to load fonts and draw text
*
*   CONFIGURATION:
*       #define SUPPORT_MODULE_RTEXT        1
*           rtext module is included in the build
*
*       #define SUPPORT_FILEFORMAT_FNT      1
*       #define SUPPORT_FILEFORMAT_TTF      1
*       #define SUPPORT_FILEFORMAT_BDF      0
*           Selected desired fileformats to be supported for loading. Some of those formats are
*           supported by default, to remove support, #define as 0 in this module or your build system
*
*       #define MAX_TEXT_BUFFER_LENGTH   1024
*           Text functions using static buffer max size
*
*       #define MAX_TEXTSPLIT_COUNT       128
*           TextSplit() function static substrings pointers array (pointing to static buffer)
*
*       #define FONT_ATLAS_CORNER_REC_SIZE  3
*           On font atlas image generation [GenImageFontAtlas()], add a NxN pixels white rectangle
*           at the bottom-right corner of the atlas. It can be useful to for shapes drawing, to allow
*           drawing text and shapes with a single draw call [SetShapesTexture()]
*
*   DEPENDENCIES:
*       stb_truetype  - Load TTF file and rasterize characters data
*       stb_rect_pack - Rectangles packing algorithms, required for font atlas generation
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

#include "raylib.h"         // Declares module functions

#include "config.h"         // Defines module configuration flags

#include "logging.h"         // Levelbasiertes Logging (log(), logMessage())

#if SUPPORT_MODULE_RTEXT

#include "rlgl.h"           // OpenGL abstraction layer to OpenGL 1.1, 2.1, 3.3+ or ES2 -> Only DrawTextPro()

#include <stdlib.h>         // Required for: malloc(), free()
#include <stdio.h>          // Required for: vsprintf(), snprintf()
#include <string.h>         // Required for: strcmp(), strstr(), strncpy(), sscanf() [Used in LoadBMFont()]
#include <stdarg.h>         // Required for: va_list, va_start(), vsprintf(), va_end() [Used in TextFormat()]
#include <ctype.h>          // Required for: toupper(), tolower() [Used in TextToUpper(), TextToLower()]

#if SUPPORT_FILEFORMAT_TTF || SUPPORT_FILEFORMAT_BDF
    #if defined(__GNUC__) // GCC and Clang
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wunused-function"
    #endif

    #define STB_RECT_PACK_IMPLEMENTATION
    #include "external/stb_rect_pack.h"     // Required for: ttf/bdf font rectangles packaging

    #include <math.h>   // Required for: ttf/bdf font rectangles packaging

    #if defined(__GNUC__) // GCC and Clang
        #pragma GCC diagnostic pop
    #endif
#endif

#if SUPPORT_FILEFORMAT_TTF
    #if defined(__GNUC__) // GCC and Clang
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wunused-function"
    #endif

    #define STBTT_malloc(x,u) ((void)(u),malloc(x))
    #define STBTT_free(x,u) ((void)(u),free(x))

    #define STBTT_STATIC
    #define STB_TRUETYPE_IMPLEMENTATION
    #include "external/stb_truetype.h"      // Required for: ttf font data reading

    #if defined(__GNUC__) // GCC and Clang
        #pragma GCC diagnostic pop
    #endif
#endif

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
#ifndef MAX_TEXT_BUFFER_LENGTH
    #define MAX_TEXT_BUFFER_LENGTH     1024         // Size of internal static buffers used on some functions:
                                                    // TextFormat(), TextSubtext(), TextToUpper(), TextToLower(), TextToPascal(), TextSplit()
#endif
#ifndef MAX_TEXTSPLIT_COUNT
    #define MAX_TEXTSPLIT_COUNT         128         // Maximum number of substrings to split: TextSplit()
#endif

#ifndef FONT_ATLAS_CORNER_REC_SIZE
    // On font atlas image generation [GenImageFontAtlas()], add a 3x3 pixels white rectangle
    // at the bottom-right corner of the atlas. It can be useful for shapes drawing, to allow
    // drawing text and shapes with a single draw call [SetShapesTexture()]
    #define FONT_ATLAS_CORNER_REC_SIZE    3         // Size of white rectangle drawn on font atlas on font loading
#endif

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
//...

//----------------------------------------------------------------------------------
// Global variables
//----------------------------------------------------------------------------------
// Default font provided by raylib
// NOTE: Default font is loaded on InitWindow() and disposed on CloseWindow() [module: core]
static auto defaultFont = Font{ 0 };

// Text vertical line spacing in pixels (between lines)
static auto textLineSpacing = 2;

//----------------------------------------------------------------------------------
// Other Modules Functions Declaration (required by text)
//----------------------------------------------------------------------------------
//...

//----------------------------------------------------------------------------------
// Module Internal Functions Declaration
//----------------------------------------------------------------------------------
#if SUPPORT_FILEFORMAT_FNT
static Font LoadBMFont(const char *fileName);   // Load a BMFont file (AngelCode font file)
#endif
#if SUPPORT_FILEFORMAT_BDF
static GlyphInfo *LoadFontDataBDF(const u8 *fileData, const i32 dataSize, const i32 *codepoints, i32 codepointCount, const i32 *outFontSize);
#endif

extern void LoadFontDefault();
extern void UnloadFontDefault();

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------
// Load raylib default font
extern void LoadFontDefault()
{
    #define BIT_CHECK(a,b) ((a) & (1u << (b)))

    // Check to see if the font for an image has already been allocated,
    // and if no need to upload, then return
    if (defaultFont.glyphs != nullptr) return;

    // NOTE: Using UTF-8 encoding table for Unicode U+0000..U+00FF Basic Latin + Latin-1 Supplement
    // REF: http://www.utf8-chartable.de/unicode-utf8-table.pl

    defaultFont.glyphCount = 224; // Number of glyphs included in our default font
    defaultFont.glyphPadding = 0; // Characters padding

    // Default font is directly defined here (data generated from a sprite font image)
    // This way, reconstructing Font without creating large global variables
    // This data is automatically allocated to Stack and automatically deallocated at the end of this function
    const u32 defaultFontData[512] = {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00200020, 0x0001b000, 0x00000000, 0x00000000, 0x8ef92520, 0x00020a00, 0x7dbe8000, 0x1f7df45f,
        0x4a2bf2a0, 0x0852091e, 0x41224000, 0x10041450, 0x2e292020, 0x08220812, 0x41222000, 0x10041450, 0x10f92020, 0x3efa084c, 0x7d22103c, 0x107df7de,
        0xe8a12020, 0x08220832, 0x05220800, 0x10450410, 0xa4a3f000, 0x08520832, 0x05220400, 0x10450410, 0xe2f92020, 0x0002085e, 0x7d3e0281, 0x107df41f,
        0x00200000, 0x8001b000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xc0000fbe, 0xfbf7e00f, 0x5fbf7e7d, 0x0050bee8, 0x440808a2, 0x0a142fe8, 0x50810285, 0x0050a048,
        0x49e428a2, 0x0a142828, 0x40810284, 0x0048a048, 0x10020fbe, 0x09f7ebaf, 0xd89f3e84, 0x0047a04f, 0x09e48822, 0x0a142aa1, 0x50810284, 0x0048a048,
        0x04082822, 0x0a142fa0, 0x50810285, 0x0050a248, 0x00008fbe, 0xfbf42021, 0x5f817e7d, 0x07d09ce8, 0x00008000, 0x00000fe0, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x000c0180,
        0xdfbf4282, 0x0bfbf7ef, 0x42850505, 0x004804bf, 0x50a142c6, 0x08401428, 0x42852505, 0x00a808a0, 0x50a146aa, 0x08401428, 0x42852505, 0x00081090,
        0x5fa14a92, 0x0843f7e8, 0x7e792505, 0x00082088, 0x40a15282, 0x08420128, 0x40852489, 0x00084084, 0x40a16282, 0x0842022a, 0x40852451, 0x00088082,
        0xc0bf4282, 0xf843f42f, 0x7e85fc21, 0x3e0900bf, 0x00000000, 0x00000004, 0x00000000, 0x000c0180, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x04000402, 0x41482000, 0x00000000, 0x00000800,
        0x04000404, 0x4100203c, 0x00000000, 0x00000800, 0xf7df7df0, 0x514bef85, 0xbefbefbe, 0x04513bef, 0x14414500, 0x494a2885, 0xa28a28aa, 0x04510820,
        0xf44145f0, 0x474a289d, 0xa28a28aa, 0x04510be0, 0x14414510, 0x494a2884, 0xa28a28aa, 0x02910a00, 0xf7df7df0, 0xd14a2f85, 0xbefbe8aa, 0x011f7be0,
        0x00000000, 0x00400804, 0x20080000, 0x00000000, 0x00000000, 0x00600f84, 0x20080000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0xac000000, 0x00000f01, 0x00000000, 0x00000000, 0x24000000, 0x00000f01, 0x00000000, 0x06000000, 0x24000000, 0x00000f01, 0x00000000, 0x09108000,
        0x24fa28a2, 0x00000f01, 0x00000000, 0x013e0000, 0x2242252a, 0x00000f52, 0x00000000, 0x038a8000, 0x2422222a, 0x00000f29, 0x00000000, 0x010a8000,
        0x2412252a, 0x00000f01, 0x00000000, 0x010a8000, 0x24fbe8be, 0x00000f01, 0x00000000, 0x0ebe8000, 0xac020000, 0x00000f01, 0x00000000, 0x00048000,
        0x0003e000, 0x00000f00, 0x00000000, 0x00008000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000038, 0x8443b80e, 0x00203a03,
        0x02bea080, 0xf0000020, 0xc452208a, 0x04202b02, 0xf8029122, 0x07f0003b, 0xe44b388e, 0x02203a02, 0x081e8a1c, 0x0411e92a, 0xf4420be0, 0x01248202,
        0xe8140414, 0x05d104ba, 0xe7c3b880, 0x00893a0a, 0x283c0e1c, 0x04500902, 0xc4400080, 0x00448002, 0xe8208422, 0x04500002, 0x80400000, 0x05200002,
        0x083e8e00, 0x04100002, 0x804003e0, 0x07000042, 0xf8008400, 0x07f00003, 0x80400000, 0x04000022, 0x00000000, 0x00000000, 0x80400000, 0x04000002,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00800702, 0x1848a0c2, 0x84010000, 0x02920921, 0x01042642, 0x00005121, 0x42023f7f, 0x00291002,
        0xefc01422, 0x7efdfbf7, 0xefdfa109, 0x03bbbbf7, 0x28440f12, 0x42850a14, 0x20408109, 0x01111010, 0x28440408, 0x42850a14, 0x2040817f, 0x01111010,
        0xefc78204, 0x7efdfbf7, 0xe7cf8109, 0x011111f3, 0x2850a932, 0x42850a14, 0x2040a109, 0x01111010, 0x2850b840, 0x42850a14, 0xefdfbf79, 0x03bbbbf7,
        0x001fa020, 0x00000000, 0x00001000, 0x00000000, 0x00002070, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x08022800, 0x00012283, 0x02430802, 0x01010001, 0x8404147c, 0x20000144, 0x80048404, 0x00823f08, 0xdfbf4284, 0x7e03f7ef, 0x142850a1, 0x0000210a,
        0x50a14684, 0x528a1428, 0x142850a1, 0x03efa17a, 0x50a14a9e, 0x52521428, 0x142850a1, 0x02081f4a, 0x50a15284, 0x4a221428, 0xf42850a1, 0x03efa14b,
        0x50a16284, 0x4a521428, 0x042850a1, 0x0228a17a, 0xdfbf427c, 0x7e8bf7ef, 0xf7efdfbf, 0x03efbd0b, 0x00000000, 0x04000000, 0x00000000, 0x00000008,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00200508, 0x00840400, 0x11458122, 0x00014210,
        0x00514294, 0x51420800, 0x20a22a94, 0x0050a508, 0x00200000, 0x00000000, 0x00050000, 0x08000000, 0xfefbefbe, 0xfbefbefb, 0xfbeb9114, 0x00fbefbe,
        0x20820820, 0x8a28a20a, 0x8a289114, 0x3e8a28a2, 0xfefbefbe, 0xfbefbe0b, 0x8a289114, 0x008a28a2, 0x228a28a2, 0x08208208, 0x8a289114, 0x088a28a2,
        0xfefbefbe, 0xfbefbefb, 0xfa2f9114, 0x00fbefbe, 0x00000000, 0x00000040, 0x00000000, 0x00000000, 0x00000000, 0x00000020, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00210100, 0x00000004, 0x00000000, 0x00000000, 0x14508200, 0x00001402, 0x00000000, 0x00000000,
        0x00000010, 0x00000020, 0x00000000, 0x00000000, 0xa28a28be, 0x00002228, 0x00000000, 0x00000000, 0xa28a28aa, 0x000022e8, 0x00000000, 0x00000000,
        0xa28a28aa, 0x000022a8, 0x00000000, 0x00000000, 0xa28a28aa, 0x000022e8, 0x00000000, 0x00000000, 0xbefbefbe, 0x00003e2f, 0x00000000, 0x00000000,
        0x00000004, 0x00002028, 0x00000000, 0x00000000, 0x80000000, 0x00003e0f, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 };

    constexpr auto charsHeight = 10;
    constexpr auto charsDivisor = 1; // Every char is separated from the consecutive by a 1 pixel divisor, horizontally and vertically

    const i32 charsWidth[224] = { 3, 1, 4, 6, 5, 7, 6, 2, 3, 3, 5, 5, 2, 4, 1, 7, 5, 2, 5, 5, 5, 5, 5, 5, 5, 5, 1, 1, 3, 4, 3, 6,
                            7, 6, 6, 6, 6, 6, 6, 6, 6, 3, 5, 6, 5, 7, 6, 6, 6, 6, 6, 6, 7, 6, 7, 7, 6, 6, 6, 2, 7, 2, 3, 5,
                            2, 5, 5, 5, 5, 5, 4, 5, 5, 1, 2, 5, 2, 5, 5, 5, 5, 5, 5, 5, 4, 5, 5, 5, 5, 5, 5, 3, 1, 3, 4, 4,
                            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                            1, 1, 5, 5, 5, 7, 1, 5, 3, 7, 3, 5, 4, 1, 7, 4, 3, 5, 3, 3, 2, 5, 6, 1, 2, 2, 3, 5, 6, 6, 6, 6,
                            6, 6, 6, 6, 6, 6, 7, 6, 6, 6, 6, 6, 3, 3, 3, 3, 7, 6, 6, 6, 6, 6, 6, 5, 6, 6, 6, 6, 6, 6, 4, 6,
                            5, 5, 5, 5, 5, 5, 9, 5, 5, 5, 5, 5, 2, 2, 3, 3, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 3, 5 };

    // Re-construct image from defaultFontData and generate OpenGL texture
    //----------------------------------------------------------------------
    const Image imFont = {
        .data = calloc(128*128, 2),  // 2 bytes per pixel (gray + alpha)
        .width = 128,
        .height = 128,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA
    };

    // Fill image.data with defaultFontData (convert from bit to pixel!)
    for (i32 i = 0, counter = 0; i < imFont.width*imFont.height; i += 32)
    {
        for (i32 j = 31; j >= 0; j--)
        {
            if (BIT_CHECK(defaultFontData[counter], j))
            {
                // NOTE: Unreferencing data as short, so,
                // considering data as little-endian (alpha + gray)
                (static_cast<u16 *>(imFont.data))[i + j] = 0xffff;
            }
            else
            {
                (static_cast<u8 *>(imFont.data))[(i + j)*sizeof(short)] = 0xff;
                (static_cast<u8 *>(imFont.data))[(i + j)*sizeof(short) + 1] = 0x00;
            }
        }

        counter++;
    }

    defaultFont.texture = LoadTextureFromImage(imFont);

    // Check again if font glyph data has been already loaded
    // to avoid reallocating the glyphs and rects
    if (defaultFont.glyphs != nullptr)
    {
        UnloadImage(imFont);
        return;
    }

    // Reconstruct charSet using charsWidth[], charsHeight, charsDivisor, glyphCount
    //------------------------------------------------------------------------------
    // Allocate space for our characters info data
    // NOTE: This memory must be freed at end! --> Done by CloseWindow()
    defaultFont.glyphs = static_cast<GlyphInfo *>(calloc(defaultFont.glyphCount, sizeof(GlyphInfo)));
    defaultFont.recs = static_cast<Rectangle *>(calloc(defaultFont.glyphCount, sizeof(Rectangle)));

    auto currentLine = 0;
    i32 currentPosX = charsDivisor;
    i32 testPosX = charsDivisor;

    for (i32 i = 0; i < defaultFont.glyphCount; i++)
    {
        defaultFont.glyphs[i].value = 32 + i;  // First char is 32

        defaultFont.recs[i].x = static_cast<f32>(currentPosX);
        defaultFont.recs[i].y = static_cast<f32>(charsDivisor + currentLine*(charsHeight + charsDivisor));
        defaultFont.recs[i].width = static_cast<f32>(charsWidth[i]);
        defaultFont.recs[i].height = static_cast<f32>(charsHeight);

        testPosX += static_cast<i32>(defaultFont.recs[i].width + static_cast<f32>(charsDivisor));

        if (testPosX >= imFont.width)
        {
            currentLine++;
            currentPosX = 2*charsDivisor + charsWidth[i];
            testPosX = currentPosX;

            defaultFont.recs[i].x = static_cast<f32>(charsDivisor);
            defaultFont.recs[i].y = static_cast<f32>(charsDivisor + currentLine*(charsHeight + charsDivisor));
        }
        else currentPosX = testPosX;

        // NOTE: On default font character offsets and xAdvance are not required
        defaultFont.glyphs[i].offsetX = 0;
        defaultFont.glyphs[i].offsetY = 0;
        defaultFont.glyphs[i].advanceX = 0;

        // Fill character image data from fontClear data
        defaultFont.glyphs[i].image = ImageFromImage(imFont, defaultFont.recs[i]);
    }

    UnloadImage(imFont);

    defaultFont.baseSize = static_cast<i32>(defaultFont.recs[0].height);

    log(LogLevel::info, "FONT: Default font loaded successfully ({} glyphs)", defaultFont.glyphCount);
}

// Unload raylib default font
extern void UnloadFontDefault()
{
    for (i32 i = 0; i < defaultFont.glyphCount; i++) UnloadImage(defaultFont.glyphs[i].image);
    UnloadTexture(defaultFont.texture);
    free(defaultFont.glyphs);
    free(defaultFont.recs);
    defaultFont.glyphCount = 0;
    defaultFont.glyphs = nullptr;
    defaultFont.recs = nullptr;
}

// Get the default font
Font GetFontDefault()
{
    return defaultFont;
}

// Load Font from file into GPU memory (VRAM)
Font LoadFont(std::string_view fileNameView)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();

    // Default values for ttf font generation
#ifndef FONT_TTF_DEFAULT_SIZE
    #define FONT_TTF_DEFAULT_SIZE           32      // TTF font generation default char size (char-height)
#endif
#ifndef FONT_TTF_DEFAULT_NUMCHARS
    #define FONT_TTF_DEFAULT_NUMCHARS       95      // TTF font generation default charset: 95 glyphs (ASCII 32..126)
#endif
#ifndef FONT_TTF_DEFAULT_FIRST_CHAR
    #define FONT_TTF_DEFAULT_FIRST_CHAR     32      // TTF font generation default first char for image sprite font (32-Space)
#endif
#ifndef FONT_TTF_DEFAULT_CHARS_PADDING
    #define FONT_TTF_DEFAULT_CHARS_PADDING   4      // TTF font generation default glyphs padding
#endif

    auto font = Font{ 0 };

#if SUPPORT_FILEFORMAT_TTF
    if (IsFileExtension(fileName, ".ttf") || IsFileExtension(fileName, ".otf")) font = LoadFontEx(fileName, FONT_TTF_DEFAULT_SIZE, nullptr, FONT_TTF_DEFAULT_NUMCHARS);
    else
#endif
#if SUPPORT_FILEFORMAT_FNT
    if (IsFileExtension(fileName, ".fnt")) font = LoadBMFont(fileName);
    else
#endif
#if SUPPORT_FILEFORMAT_BDF
    if (IsFileExtension(fileName, ".bdf")) font = LoadFontEx(fileName, FONT_TTF_DEFAULT_SIZE, nullptr, FONT_TTF_DEFAULT_NUMCHARS);
    else
#endif
    {
        const auto image = LoadImage(fileName);
        if (image.data != nullptr) font = LoadFontFromImage(image, MAGENTA, FONT_TTF_DEFAULT_FIRST_CHAR);
        else font = GetFontDefault();
        UnloadImage(image);
    }

    if (font.texture.id == 0) log(LogLevel::warn, "FONT: [{}] Failed to load font texture -> Using default font", fileName);
    else
    {
        SetTextureFilter(font.texture, TEXTURE_FILTER_POINT); // By default, set point filter (the best performance)
        log(LogLevel::info, "FONT: Data loaded successfully ({} pixel size | {} glyphs)", font.baseSize, font.glyphCount);
    }

    return font;
}

// Load font from file with defined codepoints and generation size
// NOTE: NULL for codepoints and 0 for codepointCount to load the default character set (32..126),
// font size is provided in pixels height
Font LoadFontEx(std::string_view fileNameView, const i32 fontSize, const i32 *codepoints, const i32 codepointCount)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();

    auto font = Font{ 0 };

    // Loading file to memory
    auto dataSize = 0;
    auto fileData = LoadFileData(fileName, &dataSize);

    if (fileData != nullptr)
    {
        // Loading font from memory data
        font = LoadFontFromMemory(GetFileExtension(fileName), fileData, dataSize, fontSize, codepoints, codepointCount);

        UnloadFileData(fileData);
    }

    return font;
}

// Load an Image font file (XNA style)
Font LoadFontFromImage(Image image, const Color key, const i32 firstChar)
{
#ifndef MAX_GLYPHS_FROM_IMAGE
    #define MAX_GLYPHS_FROM_IMAGE   256     // Maximum number of glyphs supported on image scan
#endif

    #define COLOR_EQUAL(col1, col2) ((col1.r == col2.r) && (col1.g == col2.g) && (col1.b == col2.b) && (col1.a == col2.a))

    auto font = GetFontDefault();

    auto charSpacing = 0;
    auto lineSpacing = 0;

    // Allocate a temporal arrays for glyphs data measures,
    // once the actual number of glyphs is obtained, copy data to a sized array
    i32 tempCharValues[MAX_GLYPHS_FROM_IMAGE] = { 0 };
    Rectangle tempCharRecs[MAX_GLYPHS_FROM_IMAGE] = { 0 };

    auto pixels = LoadImageColors(image);

    // Parse image data to get charSpacing and lineSpacing
    auto x = 0;
    auto y = 0;
    for (y = 0; y < image.height; y++)
    {
        for (x = 0; x < image.width; x++)
        {
            if (!COLOR_EQUAL(pixels[y*image.width + x], key)) break;
        }

        if (!COLOR_EQUAL(pixels[y*image.width + x], key)) break;
    }

    // Security check
    if ((x == 0) || (y == 0))
    {
        UnloadImageColors(pixels);
        return font;
    }

    charSpacing = x;
    lineSpacing = y;

    auto charHeight = 0;
    auto j = 0;

    while (((lineSpacing + j) < image.height) &&
        !COLOR_EQUAL(pixels[(lineSpacing + j)*image.width + charSpacing], key)) j++;

    charHeight = j;

    // Check array values to get characters: value, x, y, w, h
    auto index = 0;
    auto lineToRead = 0;
    i32 xPosToRead = charSpacing;

    // Parse image data to get rectangle sizes
    while ((lineSpacing + lineToRead*(charHeight + lineSpacing)) < image.height)
    {
        while ((xPosToRead < image.width) &&
              !COLOR_EQUAL((pixels[(lineSpacing + (charHeight+lineSpacing)*lineToRead)*image.width + xPosToRead]), key))
        {
            tempCharValues[index] = firstChar + index;

            tempCharRecs[index].x = static_cast<f32>(xPosToRead);
            tempCharRecs[index].y = static_cast<f32>(lineSpacing + lineToRead*(charHeight + lineSpacing));
            tempCharRecs[index].height = static_cast<f32>(charHeight);

            auto charWidth = 0;

            while (((xPosToRead + charWidth) < image.width) &&
                  !COLOR_EQUAL(pixels[(lineSpacing + (charHeight + lineSpacing)*lineToRead)*image.width + xPosToRead + charWidth], key)) charWidth++;

            tempCharRecs[index].width = static_cast<f32>(charWidth);

            index++;

            xPosToRead += (charWidth + charSpacing);
        }

        lineToRead++;
        xPosToRead = charSpacing;
    }

    // NOTE: Key color borders need to be removed from image to avoid weird
    // artifacts on texture scaling when using TEXTURE_FILTER_BILINEAR or TEXTURE_FILTER_TRILINEAR
    for (i32 i = 0; i < image.height*image.width; i++) if (COLOR_EQUAL(pixels[i], key)) pixels[i] = BLANK;

    // Create a new image with the processed color data (key color replaced by BLANK)
    const Image fontClear = {
        .data = pixels,
        .width = image.width,
        .height = image.height,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };

    // Set font with all data parsed from image
    font.texture = LoadTextureFromImage(fontClear); // Convert processed image to OpenGL texture
    font.glyphCount = index;
    font.glyphPadding = 0;

    // Populate tempCharValues and tempCharsRecs with glyphs data
    // Move temp data to sized charValues and charRecs arrays
    // NOTE: calloc(0) is implementation-defined (may return NULL), so allocate at least one glyph
    font.glyphs = static_cast<GlyphInfo *>(calloc((font.glyphCount > 0) ? font.glyphCount : 1, sizeof(GlyphInfo)));
    font.recs = static_cast<Rectangle *>(calloc((font.glyphCount > 0) ? font.glyphCount : 1, sizeof(Rectangle)));

    for (i32 i = 0; i < font.glyphCount; i++)
    {
        font.glyphs[i].value = tempCharValues[i];

        // Get character rectangle in the font atlas texture
        font.recs[i] = tempCharRecs[i];

        // NOTE: On image based fonts (XNA style), character offsets and xAdvance are not required (set to 0)
        font.glyphs[i].offsetX = 0;
        font.glyphs[i].offsetY = 0;
        font.glyphs[i].advanceX = 0;

        // Fill character image data from fontClear data
        font.glyphs[i].image = ImageFromImage(fontClear, tempCharRecs[i]);
    }

    UnloadImage(fontClear);     // Unload processed image once converted to texture

    font.baseSize = static_cast<i32>(font.recs[0].height);

    return font;
}

// Load font from memory buffer, fileType refers to extension: i.e. ".ttf"
Font LoadFontFromMemory(std::string_view fileTypeView, const u8 *fileData, const i32 dataSize, const i32 fontSize, const i32 *codepoints, const i32 codepointCount)
{
    const std::string fileTypeStr(fileTypeView);
    const char *fileType = fileTypeStr.c_str();

    auto font = Font{ 0 };

    char fileExtLower[16] = { 0 };
    snprintf(fileExtLower, 16, "%s", TextToLower(fileType).c_str());

    font.baseSize = fontSize;
    font.glyphPadding = 0;

#if SUPPORT_FILEFORMAT_TTF
    if (TextIsEqual(fileExtLower, ".ttf") ||
        TextIsEqual(fileExtLower, ".otf"))
    {
        font.glyphs = LoadFontData(fileData, dataSize, font.baseSize, codepoints, (codepointCount > 0)? codepointCount : 95, FONT_DEFAULT, &font.glyphCount);
    }
    else
#endif
#if SUPPORT_FILEFORMAT_BDF
    if (TextIsEqual(fileExtLower, ".bdf"))
    {
        font.glyphs = LoadFontDataBDF(fileData, dataSize, codepoints, (codepointCount > 0)? codepointCount : 95, &font.baseSize);
        font.glyphCount = (codepointCount > 0)? codepointCount : 95;
    }
    else
#endif
    {
        font.glyphs = nullptr;
    }

#if SUPPORT_FILEFORMAT_TTF || SUPPORT_FILEFORMAT_BDF
    if (font.glyphs != nullptr)
    {
        font.glyphPadding = FONT_TTF_DEFAULT_CHARS_PADDING;

        const auto atlas = GenImageFontAtlas(font.glyphs, &font.recs, font.glyphCount, font.baseSize, font.glyphPadding, 0);
        font.texture = LoadTextureFromImage(atlas);

        // Update glyphs[i].image to use alpha, required to be used on ImageDrawText()
        for (i32 i = 0; i < font.glyphCount; i++)
        {
            UnloadImage(font.glyphs[i].image);
            font.glyphs[i].image = ImageFromImage(atlas, font.recs[i]);
        }

        UnloadImage(atlas);

        log(LogLevel::info, "FONT: Data loaded successfully ({} pixel size | {} glyphs)", font.baseSize, font.glyphCount);
    }
    else
    {
        log(LogLevel::warn, "FONT: Font is not supported by LoadFontEx/LoadFontFromMemory or no glyphs found, reverted to default font");
        font = GetFontDefault();
    }
#else
    log(LogLevel::warn, "FONT: Font is not supported by LoadFontEx/LoadFontFromMemory or no glyphs found, reverted to default font");
    font = GetFontDefault();
#endif

    return font;
}

// Check if font is valid (font data loaded)
// WARNING: GPU texture not checked
bool IsFontValid(const Font font)
{
    return ((font.baseSize > 0) &&      // Validate font size
            (font.glyphCount > 0) &&    // Validate font contains some glyph
            (font.recs != nullptr) &&      // Validate font recs defining glyphs on texture atlas
            (font.glyphs != nullptr));     // Validate glyph data is loaded

    // NOTE: Further validations could be done to verify if recs and glyphs contain valid data (glyphs values, metrics...)
}

// Load font data for further use
// NOTE: Requires TTF font memory data and can generate SDF data
GlyphInfo *LoadFontData(const u8 *fileData, const i32 dataSize, const i32 fontSize, const i32 *codepoints, i32 codepointCount, const i32 type, i32 *glyphCount)
{
    // NOTE: Using some SDF generation default values,
    // trades off precision with ability to handle *smaller* sizes
#ifndef FONT_SDF_CHAR_PADDING
    #define FONT_SDF_CHAR_PADDING            4      // SDF font generation char padding
#endif
#ifndef FONT_SDF_ON_EDGE_VALUE
    #define FONT_SDF_ON_EDGE_VALUE         128      // SDF font generation on edge value
#endif
#ifndef FONT_SDF_PIXEL_DIST_SCALE
    #define FONT_SDF_PIXEL_DIST_SCALE     64.0f     // SDF font generation pixel distance scale
#endif
#ifndef FONT_BITMAP_ALPHA_THRESHOLD
    #define FONT_BITMAP_ALPHA_THRESHOLD     80      // Bitmap (B&W) font generation alpha threshold
#endif

    GlyphInfo *glyphs = nullptr;
    auto glyphCounter = 0;

#if SUPPORT_FILEFORMAT_TTF
    // Load font data (including pixel data) from TTF memory file
    // NOTE: Loaded information should be enough to generate font image atlas, using any packaging method
    if (fileData != nullptr)
    {
        auto genFontChars = false;
        auto fontInfo = stbtt_fontinfo{ nullptr };
        auto requiredCodepoints = const_cast<i32 *>(codepoints);

        if (stbtt_InitFont(&fontInfo, const_cast<u8 *>(fileData), 0)) // Initialize font for data reading
        {
            // Calculate font scale factor
            const f32 scaleFactor = stbtt_ScaleForPixelHeight(&fontInfo, static_cast<f32>(fontSize));

            // Calculate font basic metrics
            // NOTE: ascent is equivalent to font baseline
            auto ascent = 0;
            auto descent = 0;
            auto lineGap = 0;
            stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);

            // In case no chars count provided, default to 95
            codepointCount = (codepointCount > 0)? codepointCount : 95;

            // Fill fontChars in case not provided externally
            // NOTE: By default filling glyphCount consecutively, starting at 32 (Space)
            if (requiredCodepoints == nullptr)
            {
                requiredCodepoints = static_cast<i32 *>(calloc(codepointCount, sizeof(i32)));
                for (i32 i = 0; i < codepointCount; i++) requiredCodepoints[i] = i + 32;
                genFontChars = true;
            }

            // Check available glyphs on provided font before loading them
            for (i32 i = 0, index; i < codepointCount; i++)
            {
                index = stbtt_FindGlyphIndex(&fontInfo, requiredCodepoints[i]);
                if (index > 0) glyphCounter++;
            }

            // WARNING: Allocating space for maximum number of codepoints
            glyphs = static_cast<GlyphInfo *>(calloc(glyphCounter, sizeof(GlyphInfo)));
            glyphCounter = 0; // Reset to reuse

            auto k = 0;
            for (i32 i = 0; i < codepointCount; i++)
            {
                i32 cpWidth = 0, cpHeight = 0;         // Codepoint width and height (on generation)
                const i32 cp = requiredCodepoints[i];  // Codepoint value to get info for

                //  Render a unicode codepoint to a bitmap
                //      stbtt_GetCodepointBitmap()           -- allocates and returns a bitmap
                //      stbtt_GetCodepointBitmapBox()        -- how big the bitmap must be
                //      stbtt_MakeCodepointBitmap()          -- renders into a provided bitmap

                // Check if glyph is available in the font
                // WARNING: if (index == 0), glyph not found, it could fallback to default .notdef glyph (if defined in font)
                const i32 index = stbtt_FindGlyphIndex(&fontInfo, cp);

                if (index > 0)
                {
                    // NOTE: Only storing glyphs for codepoints found in the font
                    glyphs[k].value = cp;

                    switch (type)
                    {
                        case FONT_DEFAULT:
                        case FONT_BITMAP:
                        {
                            glyphs[k].image.data = stbtt_GetCodepointBitmap(&fontInfo, scaleFactor, scaleFactor, cp,
                                &cpWidth, &cpHeight, &glyphs[k].offsetX, &glyphs[k].offsetY);
                        } break;
                        case FONT_SDF:
                        {
                            if (cp != 32)
                            {
                                glyphs[k].image.data = stbtt_GetCodepointSDF(&fontInfo, scaleFactor, cp,
                                    FONT_SDF_CHAR_PADDING, FONT_SDF_ON_EDGE_VALUE, FONT_SDF_PIXEL_DIST_SCALE,
                                    &cpWidth, &cpHeight, &glyphs[k].offsetX, &glyphs[k].offsetY);
                            }
                        } break;
                        //case FONT_MSDF:
                        default: break;
                    }

                    if (glyphs[k].image.data != nullptr) // Glyph data has been found in the font
                    {
                        stbtt_GetCodepointHMetrics(&fontInfo, cp, &glyphs[k].advanceX, nullptr);
                        glyphs[k].advanceX = static_cast<i32>(static_cast<f32>(glyphs[k].advanceX)*scaleFactor);

                        // WARNING: If requested SDF font, sdf-glyph height is definitely bigger than fontSize due to FONT_SDF_CHAR_PADDING
                        if ((type != FONT_SDF) && (cpHeight > fontSize)) log(LogLevel::warn, "FONT: [0x{:04x}] Glyph height is bigger than requested font size: {} > {}", cp, cpHeight, static_cast<i32>(fontSize));

                        // Load glyph image
                        glyphs[k].image.width = cpWidth;
                        glyphs[k].image.height = cpHeight;
                        glyphs[k].image.mipmaps = 1;
                        glyphs[k].image.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;

                        glyphs[k].offsetY += static_cast<i32>(static_cast<f32>(ascent)*scaleFactor);
                    }
                    //else log(LogLevel::warn, "FONT: Glyph [0x{:08x}] has no image data available", cp); // Only reported for 0x20 and 0x3000

                    // Create an empty image for Unicode space characters, useful for sprite font generation
                    if ((cp == 0x20) ||   // ASCII space
                        (cp == 0xA0) ||   // No-break space
                        (cp == 0x1680) || // Ogham Space Mark
                        (cp == 0x2000) || // En Quad
                        (cp == 0x2001) || // Em Quad
                        (cp == 0x2002) || // En Space
                        (cp == 0x2003) || // Em Space
                        (cp == 0x2004) || // Three-per-em Space
                        (cp == 0x2005) || // Four-per-em Space
                        (cp == 0x2006) || // Six-per-em Space
                        (cp == 0x2007) || // Figure Space
                        (cp == 0x2008) || // Punctuation Space
                        (cp == 0x2009) || // Thin Space
                        (cp == 0x200A) || // Hair Space
                        (cp == 0x202F) || // Narrow No-break Space
                        (cp == 0x205F) || // Medium Mathematical Space
                        (cp == 0x3000))   // Ideographic Space
                    {
                        stbtt_GetCodepointHMetrics(&fontInfo, cp, &glyphs[k].advanceX, nullptr);
                        glyphs[k].advanceX = static_cast<i32>(static_cast<f32>(glyphs[k].advanceX)*scaleFactor);

                        Image imSpace = {
                            .data = nullptr,
                            .width = glyphs[k].advanceX,
                            .height = fontSize,
                            .mipmaps = 1,
                            .format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE
                        };

                        // Only allocate space image if required
                        if (glyphs[k].advanceX > 0) imSpace.data = calloc(glyphs[k].advanceX*fontSize, 1);
                        else glyphs[k].advanceX = 0;

                        glyphs[k].image = imSpace;
                    }

                    if (type == FONT_BITMAP)
                    {
                        // Aliased bitmap (black & white) font generation, avoiding anti-aliasing
                        // NOTE: For optimum results, bitmap font should be generated at base pixel size
                        for (i32 p = 0; p < cpWidth*cpHeight; p++)
                        {
                            if ((static_cast<u8 *>(glyphs[k].image.data))[p] < FONT_BITMAP_ALPHA_THRESHOLD)
                                (static_cast<u8 *>(glyphs[k].image.data))[p] = 0;
                            else (static_cast<u8 *>(glyphs[k].image.data))[p] = 255;
                        }
                    }

                    k++;
                    glyphCounter++;
                }
                else
                {
                    // WARNING: Glyph not found on font, optionally use a fallback glyph
                }
            }

            if (glyphCounter < codepointCount) log(LogLevel::warn, "FONT: Requested codepoints glyphs found: [{}/{}]", k, codepointCount);
        }
        else log(LogLevel::warn, "FONT: Failed to process TTF font data");

        if (genFontChars) free(requiredCodepoints);
    }
#endif

    *glyphCount = glyphCounter;
    return glyphs;
}

// Generate image font atlas using chars info
// NOTE: Packing method: 0-Default, 1-Skyline
#if SUPPORT_FILEFORMAT_TTF || SUPPORT_FILEFORMAT_BDF
Image GenImageFontAtlas(const GlyphInfo *glyphs, Rectangle **glyphRecs, i32 glyphCount, const i32 fontSize, const i32 padding, const i32 packMethod)
{
    auto atlas = Image{ nullptr };

    if (glyphs == nullptr)
    {
        log(LogLevel::warn, "FONT: Provided glyphs info not valid, returning empty image atlas");
        return atlas;
    }

    *glyphRecs = nullptr;

    // In case no chars count provided, suppose default of 95
    glyphCount = (glyphCount > 0)? glyphCount : 95;

    // NOTE: Rectangles memory is loaded here!
    auto recs = static_cast<Rectangle *>(calloc(glyphCount, sizeof(Rectangle)));

    // Calculate image size based on total glyph width and glyph row count
    auto totalWidth = 0;
    //int maxGlyphWidth = 0; // Not currently used

    for (i32 i = 0; i < glyphCount; i++)
    {
        //if (glyphs[i].image.width > maxGlyphWidth) maxGlyphWidth = glyphs[i].image.width;
        totalWidth += glyphs[i].image.width + 2*padding;
    }

    const i32 paddedFontSize = fontSize + 2*padding;

    // Estimate image atlas size from available data
    // NOTE: Multiplying total expected area by 1.2f scale factor but in case
    // some glyphs do not fit, the atlas height is scaled x2 to fit them
    const f32 totalArea = static_cast<f32>(totalWidth)*static_cast<f32>(paddedFontSize)*1.2f;
    const f32 imageMinSize = sqrtf(totalArea);
    const auto imageSize = static_cast<i32>(powf(2, ceilf(logf(imageMinSize)/logf(2))));

    if (totalArea < static_cast<f32>(((imageSize*imageSize)/2)))
    {
        atlas.width = imageSize;    // Atlas bitmap width
        atlas.height = imageSize/2; // Atlas bitmap height
    }
    else
    {
        atlas.width = imageSize;   // Atlas bitmap width
        atlas.height = imageSize;  // Atlas bitmap height
    }

    i32 atlasDataSize = atlas.width*atlas.height;                           // Save total size for bounds checking
    atlas.data = static_cast<u8 *>(calloc(atlasDataSize, 1)); // Create a bitmap to store characters (8 bpp)
    atlas.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;
    atlas.mipmaps = 1;

    // DEBUG: View padding in the generated image setting a gray background...
    //for (int i = 0; i < atlas.width*atlas.height; i++) ((unsigned char *)atlas.data)[i] = 100;

    if (packMethod == 0) // Use basic packing algorithm
    {
        i32 offsetX = padding;
        i32 offsetY = padding;

        // NOTE: Using simple packaging, one char after another
        for (i32 i = 0; i < glyphCount; i++)
        {
            // Check remaining space for glyph
            if (offsetX >= (atlas.width - glyphs[i].image.width - 2*padding))
            {
                offsetX = padding;

                // NOTE: Be careful on offsetY for SDF fonts, by default SDF
                // use an internal padding of 4 pixels, it means char rectangle
                // height is bigger than fontSize, it could be up to (fontSize + 8)
                offsetY += (fontSize + 2*padding);

                if (offsetY > (atlas.height - fontSize - padding))
                {
                    log(LogLevel::warn, "FONT: Updating atlas size to fit all characters");

                    // Update atlas size to fit all characters
                    const i32 updatedAtlasHeight = atlas.height*2;
                    const i32 updatedAtlasDataSize = atlas.width*updatedAtlasHeight;
                    auto updatedAtlasData = static_cast<u8 *>(calloc(updatedAtlasDataSize, 1));

                    memcpy(updatedAtlasData, atlas.data, atlasDataSize);
                    free(atlas.data);
                    atlas.data = updatedAtlasData;
                    atlas.height = updatedAtlasHeight;
                    atlasDataSize = updatedAtlasDataSize;
                }
            }

            // Copy pixel data from glyph image to atlas
            for (i32 y = 0; y < glyphs[i].image.height; y++)
            {
                for (i32 x = 0; x < glyphs[i].image.width; x++)
                {
                    const i32 destX = offsetX + x;
                    const i32 destY = offsetY + y;

                    // Security: check both lower and upper bounds
                    if ((destX >= 0) && (destX < atlas.width) && (destY >= 0) && (destY < atlas.height))
                    {
                        (static_cast<u8 *>(atlas.data))[destY*atlas.width + destX] =
                            (static_cast<u8 *>(glyphs[i].image.data))[y*glyphs[i].image.width + x];
                    }
                }
            }

            // Fill chars rectangles in atlas info
            recs[i].x = static_cast<f32>(offsetX);
            recs[i].y = static_cast<f32>(offsetY);
            recs[i].width = static_cast<f32>(glyphs[i].image.width);
            recs[i].height = static_cast<f32>(glyphs[i].image.height);

            // Move atlas position X for next character drawing
            offsetX += (glyphs[i].image.width + 2*padding);
        }
    }
    else if (packMethod == 1) // Use Skyline rect packing algorithm (stb_pack_rect)
    {
        auto context = static_cast<stbrp_context *>(calloc(1, sizeof(stbrp_context)));
        auto nodes = static_cast<stbrp_node *>(calloc(glyphCount, sizeof(stbrp_node)));

        stbrp_init_target(context, atlas.width, atlas.height, nodes, glyphCount);
        auto rects = static_cast<stbrp_rect *>(calloc(glyphCount, sizeof(stbrp_rect)));

        // Fill rectangles for packaging
        for (i32 i = 0; i < glyphCount; i++)
        {
            rects[i].id = i;
            rects[i].w = glyphs[i].image.width + 2*padding;
            rects[i].h = glyphs[i].image.height + 2*padding;
        }

        // Package rectangles into atlas
        stbrp_pack_rects(context, rects, glyphCount);

        for (i32 i = 0; i < glyphCount; i++)
        {
            // It returns char rectangles in atlas
            recs[i].x = static_cast<f32>(rects[i].x) + static_cast<f32>(padding);
            recs[i].y = static_cast<f32>(rects[i].y) + static_cast<f32>(padding);
            recs[i].width = static_cast<f32>(glyphs[i].image.width);
            recs[i].height = static_cast<f32>(glyphs[i].image.height);

            if (rects[i].was_packed)
            {
                // Copy pixel data from fc.data to atlas
                for (i32 y = 0; y < glyphs[i].image.height; y++)
                {
                    for (i32 x = 0; x < glyphs[i].image.width; x++)
                    {
                        const i32 destX = rects[i].x + padding + x;
                        const i32 destY = rects[i].y + padding + y;

                        // Security fix: check both lower and upper bounds
                        if (destX >= 0 && destX < atlas.width && destY >= 0 && destY < atlas.height)
                        {
                            (static_cast<u8 *>(atlas.data))[destY*atlas.width + destX] =
                                (static_cast<u8 *>(glyphs[i].image.data))[y*glyphs[i].image.width + x];
                        }
                    }
                }
            }
            else log(LogLevel::warn, "FONT: Failed to package glyph (0x{:02x})", glyphs[i].value);
        }

        free(rects);
        free(nodes);
        free(context);
    }

    // Add a 3x3 white rectangle at the bottom-right corner of the generated atlas,
    // useful to use as the white texture to draw shapes with raylib
    // Security: ensure the atlas is large enough to hold a 3x3 rectangle
    if ((FONT_ATLAS_CORNER_REC_SIZE > 0) && (atlas.width >= 3) && (atlas.height >= 3))
    {
        for (i32 i = 0, k = atlas.width*atlas.height - 1; i < FONT_ATLAS_CORNER_REC_SIZE; i++)
        {
            (static_cast<u8 *>(atlas.data))[k - 0] = 255;
            (static_cast<u8 *>(atlas.data))[k - 1] = 255;
            (static_cast<u8 *>(atlas.data))[k - 2] = 255;
            k -= atlas.width;
        }
    }

    // Convert image data from GRAYSCALE to GRAY_ALPHA
    auto dataGrayAlpha = static_cast<u8 *>(calloc(atlas.width*atlas.height*2, sizeof(u8))); // Two channels

    for (i32 i = 0, k = 0; i < atlas.width*atlas.height; i++, k += 2)
    {
        dataGrayAlpha[k] = 255;
        dataGrayAlpha[k + 1] = (static_cast<u8 *>(atlas.data))[i];
    }

    free(atlas.data);
    atlas.data = dataGrayAlpha;
    atlas.format = PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA;

    *glyphRecs = recs;

    return atlas;
}
#endif

// Unload font glyphs info data (RAM)
void UnloadFontData(GlyphInfo *glyphs, const i32 glyphCount)
{
    if (glyphs != nullptr)
    {
        for (i32 i = 0; i < glyphCount; i++) UnloadImage(glyphs[i].image);

        free(glyphs);
    }
}

// Unload Font from GPU memory (VRAM)
void UnloadFont(Font font)
{
    // NOTE: Make sure font is not default font (fallback)
    if (font.texture.id != GetFontDefault().texture.id)
    {
        UnloadFontData(font.glyphs, font.glyphCount);
        UnloadTexture(font.texture);
        free(font.recs);

        log(LogLevel::debug, "FONT: Unloaded font data from RAM and VRAM");
    }
}

// Export font as code file, returns true on success
bool ExportFontAsCode(const Font font, std::string_view fileNameView)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();

    auto result = false;

#ifndef TEXT_BYTES_PER_LINE
    #define TEXT_BYTES_PER_LINE     20
#endif

    // Get file name from path
    char fileNamePascal[256] = { 0 };
    snprintf(fileNamePascal, 256, "%s", TextToPascal(GetFileNameWithoutExt(fileName)).c_str());

    // Get font atlas image and size, required to estimate code file size
    // NOTE: This mechanism is highly coupled to raylib
    const auto image = LoadImageFromTexture(font.texture);
    if (image.format != PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA) log(LogLevel::warn, "Font export as code: Font image format is not GRAY+ALPHA!");
    const auto imageDataSize = GetPixelDataSize(image.width, image.height, image.format);

    // Image data is usually GRAYSCALE + ALPHA and can be reduced to GRAYSCALE
    //ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE);

    // Estimate text code size
    //  - Image data is stored as "0x%02x", so it requires at least 4 char per byte, let's use 6
    //  - font.recs[] data is stored as "{ %1.0f, %1.0f, %1.0f , %1.0f }", let's reserve 64 per rec
    //  - font.glyphs[] data is stored as "{ %i, %i, %i, %i, { 0 }},\n", let's reserve 64 per glyph
    //  - Comments and additional code, let's reserve 32KB
    const i32 txtDataSize = imageDataSize*6 + font.glyphCount*64 + font.glyphCount*64 + 32768;
    auto txtData = static_cast<char *>(calloc(txtDataSize, sizeof(char)));

    auto byteCount = 0;
    byteCount += sprintf(txtData + byteCount, "////////////////////////////////////////////////////////////////////////////////////////\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// FontAsCode exporter v1.0 - Font data exported as an array of bytes                 //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// more info and bugs-report:  github.com/raysan5/raylib                              //\n");
    byteCount += sprintf(txtData + byteCount, "// feedback and support:       ray[at]raylib.com                                      //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// Copyright (c) 2018-2026 Ramon Santamaria (@raysan5)                                //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// ---------------------------------------------------------------------------------- //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// TODO: Fill the information and license of the exported font here:                  //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// Font name:    ....                                                                 //\n");
    byteCount += sprintf(txtData + byteCount, "// Font creator: ....                                                                 //\n");
    byteCount += sprintf(txtData + byteCount, "// Font LICENSE: ....                                                                 //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "////////////////////////////////////////////////////////////////////////////////////////\n\n");

#define SUPPORT_COMPRESSED_FONT_ATLAS
#if defined(SUPPORT_COMPRESSED_FONT_ATLAS)
    // WARNING: Data is compressed using raylib CompressData() DEFLATE,
    // it requires to be decompressed with raylib DecompressData(), that requires
    // compiling raylib with SUPPORT_COMPRESSION_API config flag enabled

    // Compress font image data
    auto compDataSize = 0;
    auto compData = CompressData(static_cast<const u8 *>(image.data), imageDataSize, &compDataSize);

    // Save font image data (compressed)
    byteCount += sprintf(txtData + byteCount, "#define COMPRESSED_DATA_SIZE_FONT_%s %i\n\n", TextToUpper(fileNamePascal).c_str(), compDataSize);
    byteCount += sprintf(txtData + byteCount, "// Font image pixels data compressed (DEFLATE)\n");
    byteCount += sprintf(txtData + byteCount, "// NOTE: Original pixel data simplified to GRAYSCALE\n");
    byteCount += sprintf(txtData + byteCount, "static unsigned char fontData_%s[COMPRESSED_DATA_SIZE_FONT_%s] = { ", fileNamePascal, TextToUpper(fileNamePascal).c_str());
    for (i32 i = 0; i < compDataSize - 1; i++) byteCount += sprintf(txtData + byteCount, ((i%TEXT_BYTES_PER_LINE == 0)? "0x%02x,\n    " : "0x%02x, "), compData[i]);
    byteCount += sprintf(txtData + byteCount, "0x%02x };\n\n", compData[compDataSize - 1]);
    free(compData);
#else
    // Save font image data (uncompressed)
    byteCount += sprintf(txtData + byteCount, "// Font image pixels data\n");
    byteCount += sprintf(txtData + byteCount, "// NOTE: 2 bytes per pixel, GRAY + ALPHA channels\n");
    byteCount += sprintf(txtData + byteCount, "static unsigned char fontImageData_%s[%i] = { ", fileNamePascal, imageDataSize);
    for (i32 i = 0; i < imageDataSize - 1; i++) byteCount += sprintf(txtData + byteCount, ((i%TEXT_BYTES_PER_LINE == 0)? "0x%02x,\n    " : "0x%02x, "), (static_cast<u8 *>(imFont.data))[i]);
    byteCount += sprintf(txtData + byteCount, "0x%02x };\n\n", (static_cast<u8 *>(imFont.data))[imageDataSize - 1]);
#endif

    // Save font recs data
    byteCount += sprintf(txtData + byteCount, "// Font characters rectangles data\n");
    byteCount += sprintf(txtData + byteCount, "static Rectangle fontRecs_%s[%i] = {\n", fileNamePascal, font.glyphCount);
    for (i32 i = 0; i < font.glyphCount; i++)
    {
        byteCount += sprintf(txtData + byteCount, "    { %1.0f, %1.0f, %1.0f , %1.0f },\n", font.recs[i].x, font.recs[i].y, font.recs[i].width, font.recs[i].height);
    }
    byteCount += sprintf(txtData + byteCount, "};\n\n");

    // Save font glyphs data
    // NOTE: Glyphs image data not saved (grayscale pixels), it could be generated from image and recs
    byteCount += sprintf(txtData + byteCount, "// Font glyphs info data\n");
    byteCount += sprintf(txtData + byteCount, "// NOTE: No glyphs.image data provided\n");
    byteCount += sprintf(txtData + byteCount, "static GlyphInfo fontGlyphs_%s[%i] = {\n", fileNamePascal, font.glyphCount);
    for (i32 i = 0; i < font.glyphCount; i++)
    {
        byteCount += sprintf(txtData + byteCount, "    { %i, %i, %i, %i, { 0 }},\n", font.glyphs[i].value, font.glyphs[i].offsetX, font.glyphs[i].offsetY, font.glyphs[i].advanceX);
    }
    byteCount += sprintf(txtData + byteCount, "};\n\n");

    // Custom font loading function
    byteCount += sprintf(txtData + byteCount, "// Font loading function: %s\n", fileNamePascal);
    byteCount += sprintf(txtData + byteCount, "static Font LoadFont_%s(void)\n{\n", fileNamePascal);
    byteCount += sprintf(txtData + byteCount, "    Font font = { 0 };\n\n");
    byteCount += sprintf(txtData + byteCount, "    font.baseSize = %i;\n", font.baseSize);
    byteCount += sprintf(txtData + byteCount, "    font.glyphCount = %i;\n", font.glyphCount);
    byteCount += sprintf(txtData + byteCount, "    font.glyphPadding = %i;\n\n", font.glyphPadding);
    byteCount += sprintf(txtData + byteCount, "    // Custom font loading\n");
#if defined(SUPPORT_COMPRESSED_FONT_ATLAS)
    byteCount += sprintf(txtData + byteCount, "    // NOTE: Compressed font image data (DEFLATE), it requires DecompressData() function\n");
    byteCount += sprintf(txtData + byteCount, "    int fontDataSize_%s = 0;\n", fileNamePascal);
    byteCount += sprintf(txtData + byteCount, "    unsigned char *data = DecompressData(fontData_%s, COMPRESSED_DATA_SIZE_FONT_%s, &fontDataSize_%s);\n", fileNamePascal, TextToUpper(fileNamePascal).c_str(), fileNamePascal);
    byteCount += sprintf(txtData + byteCount, "    Image imFont = { data, %i, %i, 1, %i };\n\n", image.width, image.height, image.format);
#else
    byteCount += sprintf(txtData + byteCount, "    Image imFont = { fontImageData_%s, %i, %i, 1, %i };\n\n", styleName, image.width, image.height, image.format);
#endif
    byteCount += sprintf(txtData + byteCount, "    // Load texture from image\n");
    byteCount += sprintf(txtData + byteCount, "    font.texture = LoadTextureFromImage(imFont);\n");
#if defined(SUPPORT_COMPRESSED_FONT_ATLAS)
    byteCount += sprintf(txtData + byteCount, "    UnloadImage(imFont);  // Uncompressed data can be unloaded from memory\n\n");
#endif
    // There are two possible mechanisms to assign font.recs and font.glyphs data,
    // that data is already available as global arrays, two options to assign that data:
    //  - 1. Data copy. This option consumes more memory and Font MUST be unloaded by user, requiring additional code
    //  - 2. Data assignment. This option consumes less memory and Font MUST NOT be unloaded by user because data is on protected DATA segment
//#define SUPPORT_FONT_DATA_COPY
#if defined(SUPPORT_FONT_DATA_COPY)
    byteCount += sprintf(txtData + byteCount, "    // Copy glyph recs data from global fontRecs\n");
    byteCount += sprintf(txtData + byteCount, "    // NOTE: Required to avoid issues if trying to free font\n");
    byteCount += sprintf(txtData + byteCount, "    font.recs = (Rectangle *)malloc(font.glyphCount*sizeof(Rectangle));\n");
    byteCount += sprintf(txtData + byteCount, "    memcpy(font.recs, fontRecs_%s, font.glyphCount*sizeof(Rectangle));\n\n", fileNamePascal);

    byteCount += sprintf(txtData + byteCount, "    // Copy font glyph info data from global fontChars\n");
    byteCount += sprintf(txtData + byteCount, "    // NOTE: Required to avoid issues if trying to free font\n");
    byteCount += sprintf(txtData + byteCount, "    font.glyphs = (GlyphInfo *)malloc(font.glyphCount*sizeof(GlyphInfo));\n");
    byteCount += sprintf(txtData + byteCount, "    memcpy(font.glyphs, fontGlyphs_%s, font.glyphCount*sizeof(GlyphInfo));\n\n", fileNamePascal);
#else
    byteCount += sprintf(txtData + byteCount, "    // Assign glyph recs and info data directly\n");
    byteCount += sprintf(txtData + byteCount, "    // WARNING: This font data must not be unloaded\n");
    byteCount += sprintf(txtData + byteCount, "    font.recs = fontRecs_%s;\n", fileNamePascal);
    byteCount += sprintf(txtData + byteCount, "    font.glyphs = fontGlyphs_%s;\n\n", fileNamePascal);
#endif
    byteCount += sprintf(txtData + byteCount, "    return font;\n");
    byteCount += sprintf(txtData + byteCount, "}\n");

    UnloadImage(image);

    // NOTE: Text data size exported is determined by '\0' (NULL) character
    result = SaveFileText(fileName, txtData);

    free(txtData);

    if (result != 0) log(LogLevel::info, "FILEIO: [{}] Font as code exported successfully", fileName);
    else log(LogLevel::warn, "FILEIO: [{}] Failed to export font as code", fileName);

    return result;
}

// Draw current FPS
// NOTE: Uses default font
void DrawFPS(const i32 posX, const i32 posY)
{
    Color color = LIME;                         // Good FPS
    const auto fps = GetFPS();

    if ((fps < 30) && (fps >= 15)) color = ORANGE;  // Warning FPS
    else if (fps < 15) color = RED;                 // Low FPS

    DrawText(TextFormat("%2i FPS", fps), posX, posY, 20, color);
}

// Draw text (using default font)
// NOTE: fontSize work like in any drawing program but if fontSize is lower than font-base-size, then font-base-size is used
// NOTE: chars spacing is proportional to fontSize
void DrawText(std::string_view textView, const i32 posX, const i32 posY, i32 fontSize, const Color color)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();

    // Check if default font has been loaded
    if (GetFontDefault().texture.id != 0)
    {
        const auto position = vec2{ static_cast<f32>(posX), static_cast<f32>(posY) };

        constexpr auto defaultFontSize = 10;   // Default Font chars height in pixel
        if (fontSize < defaultFontSize) fontSize = defaultFontSize;
        const i32 spacing = fontSize/defaultFontSize;

        DrawTextEx(GetFontDefault(), text, position, static_cast<f32>(fontSize), static_cast<f32>(spacing), color);
    }
}

// Draw text using Font
// NOTE: chars spacing is NOT proportional to fontSize
void DrawTextEx(Font font, std::string_view textView, const vec2 position, const f32 fontSize, const f32 spacing, const Color tint)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();

    if (font.texture.id == 0) font = GetFontDefault();  // Security check in case of not valid font

    const i32 size = static_cast<i32>(TextLength(text));    // Total size in bytes of the text, scanned by codepoints in loop

    f32 textOffsetY = 0;          // Offset between lines (on linebreak '\n')
    auto textOffsetX = 0.0f;       // Offset X to next character to draw

    const f32 scaleFactor = fontSize/static_cast<f32>(font.baseSize);         // Character quad scaling factor

    for (i32 i = 0; i < size;)
    {
        // Get next codepoint from byte string and glyph index in font
        auto codepointByteCount = 0;
        const auto codepoint = GetCodepointNext(&text[i], &codepointByteCount);
        const auto index = GetGlyphIndex(font, codepoint);

        if (codepoint == '\n')
        {
            // NOTE: Line spacing is a global variable, use SetTextLineSpacing() to setup
            textOffsetY += (fontSize + static_cast<f32>(textLineSpacing));
            textOffsetX = 0.0f;
        }
        else
        {
            if ((codepoint != ' ') && (codepoint != '\t'))
            {
                DrawTextCodepoint(font, codepoint, vec2{ position.x + textOffsetX, position.y + textOffsetY }, fontSize, tint);
            }

            if (font.glyphs[index].advanceX == 0) textOffsetX += (font.recs[index].width*scaleFactor + spacing);
            else textOffsetX += (static_cast<f32>(font.glyphs[index].advanceX)*scaleFactor + spacing);
        }

        i += codepointByteCount;   // Move text bytes counter to next codepoint
    }
}

// Draw text using Font and pro parameters (rotation)
void DrawTextPro(const Font font, std::string_view textView, const vec2 position, const vec2 origin, const f32 rotation, const f32 fontSize, const f32 spacing, const Color tint)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();

    rlPushMatrix();

        rlTranslatef(position.x, position.y, 0.0f);
        rlRotatef(rotation, 0.0f, 0.0f, 1.0f);
        rlTranslatef(-origin.x, -origin.y, 0.0f);

        DrawTextEx(font, text, vec2{ 0.0f, 0.0f }, fontSize, spacing, tint);

    rlPopMatrix();
}

// Draw one character (codepoint)
void DrawTextCodepoint(const Font font, const i32 codepoint, const vec2 position, const f32 fontSize, const Color tint)
{
    // Character index position in sprite font
    // NOTE: In case a codepoint is not available in the font, index returned points to '?'
    const auto index = GetGlyphIndex(font, codepoint);
    const f32 scaleFactor = fontSize/static_cast<f32>(font.baseSize);     // Character quad scaling factor

    // Character destination rectangle on screen
    // NOTE: Considering glyph padding on drawing
    const Rectangle dstRec = { position.x + static_cast<f32>(font.glyphs[index].offsetX)*scaleFactor - static_cast<f32>(font.glyphPadding)*scaleFactor,
                      position.y + static_cast<f32>(font.glyphs[index].offsetY)*scaleFactor - static_cast<f32>(font.glyphPadding)*scaleFactor,
                      (font.recs[index].width + 2.0f*static_cast<f32>(font.glyphPadding))*scaleFactor,
                      (font.recs[index].height + 2.0f*static_cast<f32>(font.glyphPadding))*scaleFactor };

    // Character source rectangle from font texture atlas
    // NOTE: Considering glyphs padding when drawing, it could be required for outline/glow shader effects
    const Rectangle srcRec = { font.recs[index].x - static_cast<f32>(font.glyphPadding), font.recs[index].y - static_cast<f32>(font.glyphPadding),
                         font.recs[index].width + 2.0f*static_cast<f32>(font.glyphPadding), font.recs[index].height + 2.0f*static_cast<f32>(font.glyphPadding) };

    // Draw the character texture on the screen
    DrawTexturePro(font.texture, srcRec, dstRec, vec2{ 0, 0 }, 0.0f, tint);
}

// Draw multiple characters (codepoints)
void DrawTextCodepoints(const Font font, const i32 *codepoints, const i32 codepointCount, const vec2 position, const f32 fontSize, const f32 spacing, const Color tint)
{
    f32 textOffsetY = 0;          // Offset between lines (on linebreak '\n')
    auto textOffsetX = 0.0f;       // Offset X to next character to draw

    const f32 scaleFactor = fontSize/static_cast<f32>(font.baseSize);         // Character quad scaling factor

    for (i32 i = 0; i < codepointCount; i++)
    {
        const auto index = GetGlyphIndex(font, codepoints[i]);

        if (codepoints[i] == '\n')
        {
            // NOTE: Line spacing is a global variable, use SetTextLineSpacing() to setup
            textOffsetY += (fontSize + static_cast<f32>(textLineSpacing));
            textOffsetX = 0.0f;
        }
        else
        {
            if ((codepoints[i] != ' ') && (codepoints[i] != '\t'))
            {
                DrawTextCodepoint(font, codepoints[i], vec2{ position.x + textOffsetX, position.y + textOffsetY }, fontSize, tint);
            }

            if (font.glyphs[index].advanceX == 0) textOffsetX += (font.recs[index].width*scaleFactor + spacing);
            else textOffsetX += (static_cast<f32>(font.glyphs[index].advanceX)*scaleFactor + spacing);
        }
    }
}

// Set vertical line spacing when drawing with line-breaks
void SetTextLineSpacing(const i32 spacing)
{
    textLineSpacing = spacing;
}

// Measure string width for default font
i32 MeasureText(std::string_view textView, i32 fontSize)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();

    auto textSize = vec2{ 0.0f, 0.0f };

    // Check if default font has been loaded
    if (GetFontDefault().texture.id != 0)
    {
        constexpr auto defaultFontSize = 10;   // Default Font glyphs height in pixel
        if (fontSize < defaultFontSize) fontSize = defaultFontSize;
        const i32 spacing = fontSize/defaultFontSize;

        textSize = MeasureTextEx(GetFontDefault(), text, static_cast<f32>(fontSize), static_cast<f32>(spacing));
    }

    return static_cast<i32>(textSize.x);
}

// Measure string size for Font
vec2 MeasureTextEx(const Font font, std::string_view textView, const f32 fontSize, const f32 spacing)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();

    vec2 textSize{};

    if ((font.texture.id == 0) || (text == nullptr) || (text[0] == '\0')) return textSize; // Security check

    const i32 size = static_cast<i32>(TextLength(text));    // Get size in bytes of text
    auto tempByteCounter = 0;              // Used to count longer text line num chars
    auto byteCounter = 0;

    auto textWidth = 0.0f;
    auto tempTextWidth = 0.0f;     // Used to count longer text line width

    f32 textHeight = fontSize;
    const f32 scaleFactor = fontSize/static_cast<f32>(font.baseSize);

    auto letter = 0;                 // Current character
    auto index = 0;                  // Index position in sprite font

    for (i32 i = 0; i < size;)
    {
        byteCounter++;

        auto codepointByteCount = 0;
        letter = GetCodepointNext(&text[i], &codepointByteCount);
        index = GetGlyphIndex(font, letter);

        i += codepointByteCount;

        if (letter != '\n')
        {
            if (font.glyphs[index].advanceX > 0) textWidth += static_cast<f32>(font.glyphs[index].advanceX);
            else textWidth += (font.recs[index].width + static_cast<f32>(font.glyphs[index].offsetX));
        }
        else
        {
            if (tempTextWidth < textWidth) tempTextWidth = textWidth;
            byteCounter = 0;
            textWidth = 0;

            // NOTE: Line spacing is a global variable, use SetTextLineSpacing() to setup
            textHeight += (fontSize + static_cast<f32>(textLineSpacing));
        }

        if (tempByteCounter < byteCounter) tempByteCounter = byteCounter;
    }

    if (tempTextWidth < textWidth) tempTextWidth = textWidth;

    textSize.x = tempTextWidth*scaleFactor + (static_cast<f32>((tempByteCounter - 1))*spacing);
    textSize.y = textHeight;

    return textSize;
}

// Measure string size for an existing array of codepoints for Font
vec2 MeasureTextCodepoints(const Font font, const i32 *codepoints, const i32 length, const f32 fontSize, const f32 spacing)
{
    vec2 textSize{};

    if ((font.texture.id == 0) || (codepoints == nullptr) || (length == 0)) return textSize; // Security check

    auto textWidth = 0.0f;
    // Used to count longer text line width
    auto tempTextWidth = 0.0f;

    // Used to count longer text line num chars
    auto tempGlyphCounter = 0;
    auto glyphCounter = 0;

    f32 textHeight = fontSize;
    const f32 scaleFactor = fontSize/static_cast<f32>(font.baseSize);

    // Current character
    auto letter = 0;
    // Index position in sprite font
    auto index = 0;

    for (i32 i = 0; i < length; i++)
    {
        letter = codepoints[i];
        index = GetGlyphIndex(font, letter);

        if (letter != '\n')
        {
            glyphCounter++;

            if (font.glyphs[index].advanceX > 0) textWidth += static_cast<f32>(font.glyphs[index].advanceX);
            else textWidth += (font.recs[index].width + static_cast<f32>(font.glyphs[index].offsetX));
        }
        else
        {
            if (tempTextWidth < textWidth) tempTextWidth = textWidth;

            textWidth = 0;
            glyphCounter = 0;

            // NOTE: Line spacing is a global variable, use SetTextLineSpacing() to setup
            textHeight += (fontSize + static_cast<f32>(textLineSpacing));
        }

        if (tempGlyphCounter < glyphCounter) tempGlyphCounter = glyphCounter;
    }

    if (tempTextWidth < textWidth) tempTextWidth = textWidth;

    textSize.x = tempTextWidth*scaleFactor + (static_cast<f32>((tempGlyphCounter - 1))*spacing);
    textSize.y = textHeight;

    return textSize;
}

// Get index position for a unicode character on font
// NOTE: If codepoint is not found in the font it fallbacks to '?'
i32 GetGlyphIndex(const Font font, const i32 codepoint)
{
    auto index = 0;
    if (!IsFontValid(font)) return index;

#define SUPPORT_UNORDERED_CHARSET
#if defined(SUPPORT_UNORDERED_CHARSET)
    auto fallbackIndex = 0;      // Get index of fallback glyph '?'

    // Look for character index in the unordered charset
    for (i32 i = 0; i < font.glyphCount; i++)
    {
        if (font.glyphs[i].value == 63) fallbackIndex = i;

        if (font.glyphs[i].value == codepoint)
        {
            index = i;
            break;
        }
    }

    if ((index == 0) && (font.glyphs[0].value != codepoint)) index = fallbackIndex;
#else
    index = codepoint - 32;
#endif

    return index;
}

// Get glyph font info data for a codepoint (unicode character)
// NOTE: If codepoint is not found in the font it fallbacks to '?'
GlyphInfo GetGlyphInfo(const Font font, const i32 codepoint)
{
    auto info = GlyphInfo{ 0 };

    info = font.glyphs[GetGlyphIndex(font, codepoint)];

    return info;
}

// Get glyph rectangle in font atlas for a codepoint (unicode character)
// NOTE: If codepoint is not found in the font it fallbacks to '?'
Rectangle GetGlyphAtlasRec(const Font font, const i32 codepoint)
{
    auto rec = Rectangle{ 0 };

    rec = font.recs[GetGlyphIndex(font, codepoint)];

    return rec;
}

//----------------------------------------------------------------------------------
// Text strings management functions
//----------------------------------------------------------------------------------
// Load text as separate lines ('\n')
// NOTE: Returned lines end with null terminator '\0'
std::vector<std::string> LoadTextLines(const std::string_view textView)
{
    std::vector<std::string> lines;

    size_t start = 0;
    while (start <= textView.size())
    {
        size_t end = textView.find('\n', start);
        if (end == std::string_view::npos) end = textView.size();

        lines.emplace_back(textView.substr(start, end - start));
        start = end + 1;
    }

    return lines;
}

// Unload text lines


// Get text length in bytes, check for \0 character
// NOTE: Alternative: use strlen(text)
u32 TextLength(std::string_view textView)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();

    u32 length = 0;

    if (text != nullptr)
    {
        while (text[length] != '\0') length++;
    }

    return length;
}

// Formatting of text with variables to 'embed'
// WARNING: String returned will expire after this function is called MAX_TEXTFORMAT_BUFFERS times
std::string TextFormat(std::string_view textView, ...)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();

#ifndef MAX_TEXTFORMAT_BUFFERS
    #define MAX_TEXTFORMAT_BUFFERS 4        // Maximum number of static buffers for text formatting
#endif

    // Create an array of buffers so strings don't expire until MAX_TEXTFORMAT_BUFFERS invocations
    static char buffers[MAX_TEXTFORMAT_BUFFERS][MAX_TEXT_BUFFER_LENGTH] = { 0 };
    static auto index = 0;

    char *currentBuffer = buffers[index];
    memset(currentBuffer, 0, MAX_TEXT_BUFFER_LENGTH); // Clear buffer before using

    if (text != nullptr)
    {
        va_list args;
        va_start(args, textView);
        const i32 requiredByteCount = vsnprintf(currentBuffer, MAX_TEXT_BUFFER_LENGTH, text, args);
        va_end(args);

        // If requiredByteCount is larger than the MAX_TEXT_BUFFER_LENGTH, then overflow occurred
        if (requiredByteCount >= MAX_TEXT_BUFFER_LENGTH)
        {
            // Inserting "..." at the end of the string to mark as truncated
            char *truncBuffer = buffers[index] + MAX_TEXT_BUFFER_LENGTH - 4; // Adding 4 bytes = "...\0"
            snprintf(truncBuffer, 4, "...");
        }

        index += 1;     // Move to next buffer for next function call
        if (index >= MAX_TEXTFORMAT_BUFFERS) index = 0;
    }

    return currentBuffer;
}

// Get integer value from text
// NOTE: This function replaces atoi() [stdlib.h]
i32 TextToInteger(std::string_view textView)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();

    auto value = 0;
    auto sign = 1;

    if (text != nullptr)
    {
        if ((text[0] == '+') || (text[0] == '-'))
        {
            if (text[0] == '-') sign = -1;
            text++;
        }

        for (i32 i = 0; ((text[i] >= '0') && (text[i] <= '9')); i++) value = value*10 + (text[i] - '0');
    }

    return value*sign;
}

// Get float value from text
// NOTE: This function replaces atof() [stdlib.h]
// WARNING: Only '.' character is understood as decimal point
f32 TextToFloat(std::string_view textView)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();

    auto value = 0.0f;
    auto sign = 1.0f;

    if (text != nullptr)
    {
        if ((text[0] == '+') || (text[0] == '-'))
        {
            if (text[0] == '-') sign = -1.0f;
            text++;
        }

        auto i = 0;
        for (; ((text[i] >= '0') && (text[i] <= '9')); i++) value = value*10.0f + static_cast<f32>(text[i] - '0');

        if (text[i++] == '.')
        {
            auto divisor = 10.0f;
            for (; ((text[i] >= '0') && (text[i] <= '9')); i++)
            {
                value += (static_cast<f32>(text[i] - '0'))/divisor;
                divisor = divisor*10.0f;
            }
        }
    }

    return value*sign;
}

// Copy one string to another, returns bytes copied
// NOTE: Alternative implementation to strcpy(dst, src) from C standard library


// Check if two text strings are equal
// REQUIRES: strcmp()
bool TextIsEqual(std::string_view text1View, std::string_view text2View)
{
    const std::string text1Str(text1View);
    const char *text1 = text1Str.c_str();
    const std::string text2Str(text2View);
    const char *text2 = text2Str.c_str();

    auto result = false;

    if ((text1 != nullptr) && (text2 != nullptr))
    {
        if (strcmp(text1, text2) == 0) result = true;
    }

    return result;
}

// Get a piece of a text string
std::string TextSubtext(std::string_view textView, const i32 position, i32 length)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();

    static char buffer[MAX_TEXT_BUFFER_LENGTH] = { 0 };
    memset(buffer, 0, MAX_TEXT_BUFFER_LENGTH);

    if ((text != nullptr) && (position >= 0) && (length > 0))
    {
        const i32 textLength = static_cast<i32>(TextLength(text));

        if (position < textLength)
        {
            const i32 maxLength = textLength - position;
            if (length > maxLength) length = maxLength;
            if (length >= MAX_TEXT_BUFFER_LENGTH) length = MAX_TEXT_BUFFER_LENGTH - 1;

            // NOTE: Alternative: memcpy(buffer, text + position, length)

            for (i32 c = 0; c < length; c++) buffer[c] = text[position + c];

            buffer[length] = '\0';
        }
    }

    return buffer;
}

// Remove text spaces, concat words
std::string TextRemoveSpaces(std::string_view textView)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();

    static char buffer[MAX_TEXT_BUFFER_LENGTH] = { 0 };
    memset(buffer, 0, MAX_TEXT_BUFFER_LENGTH);

    if (text != nullptr)
    {
        // Avoid copying the ' ' characters
        for (i32 i = 0, j = 0; (i < MAX_TEXT_BUFFER_LENGTH - 1) && (text[i] != '\0'); i++)
        {
            if (text[i] != ' ') { buffer[j] = text[i]; j++; }
        }
    }

    return buffer;
}

// Get text between two strings
std::string GetTextBetween(std::string_view textView, std::string_view beginView, std::string_view endView)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();
    const std::string beginStr(beginView);
    const char *begin = beginStr.c_str();
    const std::string endStr(endView);
    const char *end = endStr.c_str();

    static char buffer[MAX_TEXT_BUFFER_LENGTH] = { 0 };
    memset(buffer, 0, MAX_TEXT_BUFFER_LENGTH);

    const auto beginIndex = TextFindIndex(text, begin);

    if (beginIndex > -1)
    {
        const i32 beginLen = static_cast<i32>(TextLength(begin));
        auto endIndex = TextFindIndex(text + beginIndex + beginLen, end);

        if (endIndex > -1)
        {
            endIndex += (beginIndex + beginLen);
            const i32 len = (endIndex - beginIndex - beginLen);
            if (len < (MAX_TEXT_BUFFER_LENGTH - 1)) memcpy(buffer, text + beginIndex + beginLen, len);
            else snprintf(buffer, MAX_TEXT_BUFFER_LENGTH, "%s", text + beginIndex + beginLen);
        }
    }

    return buffer;
}

// Replace text string
// REQUIRES: strstr(), memcpy()
// NOTE: Limited text replace functionality, using static string
std::string TextReplace(std::string_view textView, std::string_view searchView, std::string_view replacementView)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();
    const std::string searchStr(searchView);
    const char *search = searchStr.c_str();
    const std::string replacementStr(replacementView);
    const char *replacement = replacementStr.c_str();

    static char buffer[MAX_TEXT_BUFFER_LENGTH] = { 0 };
    memset(buffer, 0, MAX_TEXT_BUFFER_LENGTH);

    if (!textStr.empty() && !searchStr.empty())
    {

        const char *insertPoint = nullptr;    // Next insert point
        char *tempPtr = nullptr;              // Temp pointer
        auto textLen = 0;                      // Text string length
        auto searchLen = 0;                    // Search string length of (the string to remove)
        auto replaceLen = 0;                   // Replacement length (the string to replace by)
        auto lastReplacePos = 0;               // Distance between next search and end of last replace
        auto count = 0;                        // Number of replacements

        textLen = static_cast<i32>(TextLength(text));
        searchLen = static_cast<i32>(TextLength(search));
        replaceLen = static_cast<i32>(TextLength(replacement));

        // Count the number of replacements needed
        insertPoint = text;
        for (count = 0; (insertPoint = strstr(insertPoint, search)); count++) insertPoint += searchLen;

        if ((textLen + count*(replaceLen - searchLen)) < (MAX_TEXT_BUFFER_LENGTH - 1))
        {
            tempPtr = buffer; // Point to result start

            // First time through the loop, all the variable are set correctly from here on,
            //  - 'temp' points to the end of the result string
            //  - 'insertPoint' points to the next occurrence of replace in text
            //  - 'text' points to the remainder of text after "end of replace"
            while (count > 0)
            {
                insertPoint = strstr(text, search);
                lastReplacePos = static_cast<i32>(insertPoint - text);

                memcpy(tempPtr, text, lastReplacePos);
                tempPtr += lastReplacePos;

                if (replaceLen > 0)
                {
                    memcpy(tempPtr, replacement, replaceLen);
                    tempPtr += replaceLen;
                }

                text += (lastReplacePos + searchLen); // Move to next "end of replace"
                count--;
            }

            // Copy remaind text part after replacement to result (pointed by moving temp)
            // NOTE: Text pointer internal copy has been updated along the process
            memcpy(tempPtr, text, TextLength(text));
        }
        else log(LogLevel::warn, "Text with replacement is longer than internal buffer");
    }

    return buffer;
}

// Replace text string
// REQUIRES: strstr(), memcpy()
// WARNING: Allocated memory must be manually freed


// Replace text between two specific strings
// REQUIRES: strncpy()
// NOTE: If (replacement == NULL) removes "begin"[ ]"end" text
std::string TextReplaceBetween(std::string_view textView, std::string_view beginView, std::string_view endView, std::string_view replacementView)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();
    const std::string beginStr(beginView);
    const char *begin = beginStr.c_str();
    const std::string endStr(endView);
    const char *end = endStr.c_str();
    const std::string replacementStr(replacementView);
    const char *replacement = replacementStr.c_str();

    static char buffer[MAX_TEXT_BUFFER_LENGTH] = { 0 };
    memset(buffer, 0, MAX_TEXT_BUFFER_LENGTH);

    if (!textStr.empty() && !beginStr.empty() && !endStr.empty())
    {
        const auto beginIndex = TextFindIndex(text, begin);

        if (beginIndex > -1)
        {
            const i32 beginLen = static_cast<i32>(TextLength(begin));
            auto endIndex = TextFindIndex(text + beginIndex + beginLen, end);

            if (endIndex > -1)
            {
                endIndex += (beginIndex + beginLen);

                const i32 textLen = static_cast<i32>(TextLength(text));
                const auto replaceLen = static_cast<i32>(replacementStr.size());
                //int toreplaceLen = endIndex - beginIndex - beginLen;

                if ((beginIndex + beginLen + replaceLen + (textLen - endIndex)) < (MAX_TEXT_BUFFER_LENGTH - 1))
                {
                    strncpy(buffer, text, beginIndex + beginLen);                                              // Copy first text part
                    strncpy(buffer + beginIndex + beginLen, replacement, replaceLen);                          // Copy replacement (if provided)
                    strncpy(buffer + beginIndex + beginLen + replaceLen, text + endIndex, textLen - endIndex); // Copy end text part
                }
                else log(LogLevel::warn, "TEXT: Text with replaced string is longer than internal buffer (MAX_TEXT_BUFFER_LENGTH)");
            }
        }
    }

    return buffer;
}

// Replace text between two specific strings
// REQUIRES: strncpy()
// NOTE: If (replacement == NULL) remove "begin"[ ]"end" text
// WARNING: Returned string must be freed by user


// Insert text in a specific position, moves all text forward
// WARNING: Allocated memory must be manually freed
std::string TextInsert(std::string_view textView, std::string_view insertView, i32 position)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();
    const std::string insertStr(insertView);
    const char *insert = insertStr.c_str();

    static char buffer[MAX_TEXT_BUFFER_LENGTH] = { 0 };
    memset(buffer, 0, MAX_TEXT_BUFFER_LENGTH);
    const i32 textLen = static_cast<i32>(TextLength(text));

    if (!textStr.empty() && !insertStr.empty() && (position >= 0))
    {
        if (position > textLen) position = textLen; // End of text string
        const i32 insertLen = static_cast<i32>(TextLength(insert));

        if ((textLen + insertLen) < (MAX_TEXT_BUFFER_LENGTH - 1))
        {
            for (i32 i = 0; i < position; i++) buffer[i] = text[i];
            for (i32 i = 0; i < insertLen; i++) buffer[i+position] = insert[i];
            for (i32 i = position; i < textLen; i++) buffer[i+insertLen] = text[i];

            buffer[textLen + insertLen] = '\0'; // Add EOL
        }
        else log(LogLevel::warn, "Text with inserted string is longer than internal buffer, use TextInserExt()");
    }

    return buffer;
}

// Insert text in a specific position, moves all text forward
// WARNING: Allocated memory must be manually freed


// Join text strings with delimiter
// REQUIRES: memset(), memcpy()
std::string TextJoin(const std::vector<std::string> &textList, const std::string_view delimiter)
{
    std::string result;

    for (size_t i = 0; i < textList.size(); i++)
    {
        if (i > 0) result += delimiter;
        result += textList[i];
    }

    return result;
}

// Split string into multiple strings
// REQUIRES: memset()
std::vector<std::string> TextSplit(const std::string_view text, const char delimiter)
{
    std::vector<std::string> result;

    size_t start = 0;
    while (true)
    {
        const size_t end = text.find(delimiter, start);
        if (end == std::string_view::npos)
        {
            result.emplace_back(text.substr(start));
            break;
        }

        result.emplace_back(text.substr(start, end - start));
        start = end + 1;
    }

    return result;
}

// Append text at specific position and move cursor
// WARNING: It's up to the user to make sure appended text does not overflow the buffer!


// Find first text occurrence within a string
// REQUIRES: strstr()
i32 TextFindIndex(std::string_view textView, std::string_view searchView)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();
    const std::string searchStr(searchView);
    const char *search = searchStr.c_str();

    auto position = -1;

    if (text != nullptr)
    {
        const char *ptr = strstr(text, search);

        if (ptr != nullptr) position = static_cast<i32>(ptr - text);
    }

    return position;
}

// Get upper case version of provided string
// WARNING: Limited functionality, only basic characters set
// TODO: Support UTF-8 diacritics (á, ñ, ü...) to upper-case, check codepoints
std::string TextToUpper(std::string_view textView)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();

    static char buffer[MAX_TEXT_BUFFER_LENGTH] = { 0 };
    memset(buffer, 0, MAX_TEXT_BUFFER_LENGTH);

    if (text != nullptr)
    {
        for (i32 i = 0; (i < MAX_TEXT_BUFFER_LENGTH - 1) && (text[i] != '\0'); i++)
        {
            if ((text[i] >= 'a') && (text[i] <= 'z')) buffer[i] = static_cast<char>(text[i] - 32);
            else buffer[i] = text[i];
        }
    }

    return buffer;
}

// Get lower case version of provided string
// WARNING: Limited functionality, only basic characters set
std::string TextToLower(std::string_view textView)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();

    static char buffer[MAX_TEXT_BUFFER_LENGTH] = { 0 };
    memset(buffer, 0, MAX_TEXT_BUFFER_LENGTH);

    if (text != nullptr)
    {
        for (i32 i = 0; (i < MAX_TEXT_BUFFER_LENGTH - 1) && (text[i] != '\0'); i++)
        {
            if ((text[i] >= 'A') && (text[i] <= 'Z')) buffer[i] = static_cast<char>(text[i] + 32);
            else buffer[i] = text[i];
        }
    }

    return buffer;
}

// Get Pascal case notation version of provided string
// WARNING: Limited functionality, only basic characters set
std::string TextToPascal(std::string_view textView)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();

    static char buffer[MAX_TEXT_BUFFER_LENGTH] = { 0 };
    memset(buffer, 0, MAX_TEXT_BUFFER_LENGTH);

    if ((text != nullptr) && (text[0] != '\0'))
    {
        // Upper case first character
        if ((text[0] >= 'a') && (text[0] <= 'z')) buffer[0] = static_cast<char>(text[0] - 32);
        else buffer[0] = text[0];

        // Check for next separator to upper case another character
        for (i32 i = 1, j = 1; (i < MAX_TEXT_BUFFER_LENGTH - 1) && (text[j] != '\0'); i++, j++)
        {
            if (text[j] != '_') buffer[i] = text[j];
            else
            {
                while (text[j] == '_') j++;     // Skip one or more separators
                if (text[j] == '\0') break;     // Text ends on a separator, nothing left to copy

                if ((text[j] >= 'a') && (text[j] <= 'z')) buffer[i] = static_cast<char>(text[j] - 32);
                else buffer[i] = text[j];       // Character can not be upper-cased, copy it as is
            }
        }
    }

    return buffer;
}

// Get snake case notation version of provided string
// WARNING: Limited functionality, only basic characters set
std::string TextToSnake(std::string_view textView)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();

    static char buffer[MAX_TEXT_BUFFER_LENGTH] = { 0 };
    memset(buffer, 0, MAX_TEXT_BUFFER_LENGTH);

    if (text != nullptr)
    {
        // Check for next separator to upper case another character
        for (i32 i = 0, j = 0; (i < MAX_TEXT_BUFFER_LENGTH - 1) && (text[j] != '\0'); j++)
        {
            if (text[j] == ' ')
            {
                if ((i > 0) && (buffer[i - 1] != '_'))
                {
                    buffer[i] = '_';
                    i++;
                }
            }
            else if ((text[j] >= 'A') && (text[j] <= 'Z'))
            {
                if ((i > 0) && (buffer[i - 1] != '_'))
                {
                    const char prev = text[j - 1];
                    const char next = text[j + 1];

                    // Considering multiple cap leters to be on single word (HTTPRequest --> http_request)
                    if (((prev >= 'a') && (prev <= 'z')) ||
                        (((prev >= 'A') && (prev <= 'Z')) && ((next >= 'a') && (next <= 'z'))))
                    {
                        if (i < MAX_TEXT_BUFFER_LENGTH - 2)
                        {
                            buffer[i] = '_';
                            i++;
                        }
                    }
                }

                buffer[i] = static_cast<char>(text[j] + 32);
                i++;
            }
            else
            {
                buffer[i] = text[j];
                i++;
            }
        }
    }

    return buffer;
}

// Get Camel case notation version of provided string
// WARNING: Limited functionality, only basic characters set
std::string TextToCamel(std::string_view textView)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();

    static char buffer[MAX_TEXT_BUFFER_LENGTH] = { 0 };
    memset(buffer, 0, MAX_TEXT_BUFFER_LENGTH);

    if ((text != nullptr) && (text[0] != '\0'))
    {
        // Lower case first character
        if ((text[0] >= 'A') && (text[0] <= 'Z')) buffer[0] = static_cast<char>(text[0] + 32);
        else buffer[0] = text[0];

        // Check for next separator to upper case another character
        for (i32 i = 1, j = 1; (i < MAX_TEXT_BUFFER_LENGTH - 1) && (text[j] != '\0'); i++, j++)
        {
            if (text[j] != '_') buffer[i] = text[j];
            else
            {
                while (text[j] == '_') j++;     // Skip one or more separators
                if (text[j] == '\0') break;     // Text ends on a separator, nothing left to copy

                if ((text[j] >= 'a') && (text[j] <= 'z')) buffer[i] = static_cast<char>(text[j] - 32);
                else buffer[i] = text[j];       // Character can not be upper-cased, copy it as is
            }
        }
    }

    return buffer;
}

// Encode text codepoint into UTF-8 text
// REQUIRES: memcpy()
// WARNING: Allocated memory must be manually freed
std::string LoadUTF8(const i32 *codepoints, const i32 length)
{
    std::string text;
    text.reserve(static_cast<size_t>(length)*4);

    for (i32 i = 0; i < length; i++) text += CodepointToUTF8(codepoints[i]);

    return text;
}

// Unload UTF-8 text encoded from codepoints array


// Load all codepoints from a UTF-8 text string, codepoints count returned by parameter
i32 * LoadCodepoints(std::string_view textView, i32 *count)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();

    i32 *codepoints = nullptr;
    auto codepointCount = 0;

    if (text != nullptr)
    {
        const i32 textLength = static_cast<i32>(TextLength(text));

        // Allocate a big enough buffer to store as many codepoints as text bytes
        codepoints = static_cast<i32 *>(calloc((textLength > 0) ? textLength : 1, sizeof(i32)));

        auto codepointSize = 0;
        for (i32 i = 0; i < textLength; codepointCount++)
        {
            codepoints[codepointCount] = GetCodepointNext(text + i, &codepointSize);
            i += codepointSize;
        }

        // Create second buffer and copy data manually to it
        auto temp = static_cast<i32 *>(calloc((codepointCount > 0) ? codepointCount : 1, sizeof(i32)));
        for (i32 i = 0; i < codepointCount; i++) temp[i] = codepoints[i];
        free(codepoints);
        codepoints = temp;
    }

    *count = codepointCount;
    return codepoints;
}

// Unload codepoints data from memory
void UnloadCodepoints(i32 *codepoints)
{
    free(codepoints);
}

// Get total number of characters(codepoints) in a UTF-8 encoded text, until '\0' is found
// NOTE: If an invalid UTF-8 sequence is encountered a '?'(0x3f) codepoint is counted instead
i32 GetCodepointCount(std::string_view textView)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();

    u32 length = 0;
    const char *ptr = text;

    if (ptr != nullptr)
    {
        while (*ptr != '\0')
        {
            auto next = 0;
            GetCodepointNext(ptr, &next);
            ptr += next;
            length++;
        }
    }

    return static_cast<i32>(length);
}

// Encode codepoint into utf8 text (char array length returned as parameter)
// NOTE: It uses a static array to store UTF-8 bytes
std::string CodepointToUTF8(const i32 codepoint)
{
    char utf8[5] = { 0 };
    auto size = 0;

    if (codepoint < 0x80)
    {
        utf8[0] = static_cast<char>(codepoint);
        size = 1;
    }
    else if (codepoint < 0x800)
    {
        utf8[0] = static_cast<char>(((codepoint >> 6) & 0x1f) | 0xc0);
        utf8[1] = static_cast<char>((codepoint & 0x3f) | 0x80);
        size = 2;
    }
    else if (codepoint <= 0xffff)
    {
        utf8[0] = static_cast<char>(((codepoint >> 12) & 0x0f) | 0xe0);
        utf8[1] = static_cast<char>(((codepoint >>  6) & 0x3f) | 0x80);
        utf8[2] = static_cast<char>((codepoint & 0x3f) | 0x80);
        size = 3;
    }
    else if (codepoint <= 0x10ffff)
    {
        utf8[0] = static_cast<char>(((codepoint >> 18) & 0x07) | 0xf0);
        utf8[1] = static_cast<char>(((codepoint >> 12) & 0x3f) | 0x80);
        utf8[2] = static_cast<char>(((codepoint >>  6) & 0x3f) | 0x80);
        utf8[3] = static_cast<char>((codepoint & 0x3f) | 0x80);
        size = 4;
    }

    return std::string(utf8, size);
}

// Get next codepoint in a UTF-8 encoded text, scanning until '\0' is found
// When an invalid UTF-8 byte is encountered, exit as soon as possible and a '?'(0x3f) codepoint is returned
// Total number of bytes processed are returned as a parameter
// NOTE: The standard says U+FFFD should be returned in case of errors
// but that character is not supported by the default font in raylib
i32 GetCodepoint(std::string_view textView, i32 *codepointSize)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();

/*
    UTF-8 specs from https://www.ietf.org/rfc/rfc3629.txt

    Char. number range  |        UTF-8 octet sequence
      (hexadecimal)    |              (binary)
    --------------------+---------------------------------------------
    0000 0000-0000 007F | 0xxxxxxx
    0000 0080-0000 07FF | 110xxxxx 10xxxxxx
    0000 0800-0000 FFFF | 1110xxxx 10xxxxxx 10xxxxxx
    0001 0000-0010 FFFF | 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
*/

    i32 codepoint = 0x3f;   // Codepoint (defaults to '?')
    *codepointSize = 1;
    if (text == nullptr) return codepoint;

    // NOTE: On decoding errors, return as soon as possible
    const i32 octet = static_cast<u8>(text[0]); // The first UTF8 octet

    if (octet <= 0x7f)
    {
        // Only one octet (ASCII range x00-7F)
        codepoint = text[0];
    }
    else if ((octet & 0xe0) == 0xc0)
    {
        // Two octets

        // [0]xC2-DF    [1]UTF8-tail(x80-BF)
        const u8 octet1 = text[1];

        if ((octet1 == '\0') || ((octet1 >> 6) != 2)) return codepoint; // Unexpected sequence

        if ((octet >= 0xc2) && (octet <= 0xdf))
        {
            codepoint = ((octet & 0x1f) << 6) | (octet1 & 0x3f);
            *codepointSize = 2;
        }
    }
    else if ((octet & 0xf0) == 0xe0)
    {
        // Three octets
        const u8 octet1 = text[1];
        u8 octet2 = '\0';

        if ((octet1 == '\0') || ((octet1 >> 6) != 2)) return codepoint; // Unexpected sequence

        octet2 = text[2];

        if ((octet2 == '\0') || ((octet2 >> 6) != 2)) { *codepointSize = 2; return codepoint; } // Unexpected sequence

        // [0]xE0    [1]xA0-BF       [2]UTF8-tail(x80-BF)
        // [0]xE1-EC [1]UTF8-tail    [2]UTF8-tail(x80-BF)
        // [0]xED    [1]x80-9F       [2]UTF8-tail(x80-BF)
        // [0]xEE-EF [1]UTF8-tail    [2]UTF8-tail(x80-BF)

        if (((octet == 0xe0) && !((octet1 >= 0xa0) && (octet1 <= 0xbf))) ||
            ((octet == 0xed) && !((octet1 >= 0x80) && (octet1 <= 0x9f)))) return codepoint;

        if ((octet >= 0xe0) && (octet <= 0xef))
        {
            codepoint = ((octet & 0xf) << 12) | ((octet1 & 0x3f) << 6) | (octet2 & 0x3f);
            *codepointSize = 3;
        }
    }
    else if ((octet & 0xf8) == 0xf0)
    {
        // Four octets
        if (octet > 0xf4) return codepoint;

        const u8 octet1 = text[1];
        u8 octet2 = '\0';
        u8 octet3 = '\0';

        if ((octet1 == '\0') || ((octet1 >> 6) != 2)) return codepoint;  // Unexpected sequence

        octet2 = text[2];

        if ((octet2 == '\0') || ((octet2 >> 6) != 2)) { *codepointSize = 2; return codepoint; }  // Unexpected sequence

        octet3 = text[3];

        if ((octet3 == '\0') || ((octet3 >> 6) != 2)) { *codepointSize = 3; return codepoint; }  // Unexpected sequence

        // [0]xF0       [1]x90-BF       [2]UTF8-tail  [3]UTF8-tail
        // [0]xF1-F3    [1]UTF8-tail    [2]UTF8-tail  [3]UTF8-tail
        // [0]xF4       [1]x80-8F       [2]UTF8-tail  [3]UTF8-tail

        if (((octet == 0xf0) && !((octet1 >= 0x90) && (octet1 <= 0xbf))) ||
            ((octet == 0xf4) && !((octet1 >= 0x80) && (octet1 <= 0x8f)))) return codepoint; // Unexpected sequence

        if (octet >= 0xf0)
        {
            codepoint = ((octet & 0x7) << 18) | ((octet1 & 0x3f) << 12) | ((octet2 & 0x3f) << 6) | (octet3 & 0x3f);
            *codepointSize = 4;
        }
    }

    if (codepoint > 0x10ffff) codepoint = 0x3f;     // Codepoints after U+10ffff are invalid

    return codepoint;
}

// Get next codepoint in a byte sequence and bytes processed
i32 GetCodepointNext(std::string_view textView, i32 *codepointSize)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();

    const char *ptr = text;
    i32 codepoint = 0x3f;       // Codepoint (defaults to '?')
    *codepointSize = 1;
    if (text == nullptr) return codepoint;

    // Get current codepoint and bytes processed
    if (0xf0 == (0xf8 & ptr[0]))
    {
        // 4 byte UTF-8 codepoint
        if (((ptr[1] & 0xC0) ^ 0x80) || ((ptr[2] & 0xC0) ^ 0x80) || ((ptr[3] & 0xC0) ^ 0x80)) { return codepoint; } // 10xxxxxx checks
        codepoint = ((0x07 & ptr[0]) << 18) | ((0x3f & ptr[1]) << 12) | ((0x3f & ptr[2]) << 6) | (0x3f & ptr[3]);
        *codepointSize = 4;
    }
    else if (0xe0 == (0xf0 & ptr[0]))
    {
        // 3 byte UTF-8 codepoint */
        if (((ptr[1] & 0xC0) ^ 0x80) || ((ptr[2] & 0xC0) ^ 0x80)) { return codepoint; } // 10xxxxxx checks
        codepoint = ((0x0f & ptr[0]) << 12) | ((0x3f & ptr[1]) << 6) | (0x3f & ptr[2]);
        *codepointSize = 3;
    }
    else if (0xc0 == (0xe0 & ptr[0]))
    {
        // 2 byte UTF-8 codepoint
        if ((ptr[1] & 0xC0) ^ 0x80) { return codepoint; } // 10xxxxxx checks
        codepoint = ((0x1f & ptr[0]) << 6) | (0x3f & ptr[1]);
        *codepointSize = 2;
    }
    else if (0x00 == (0x80 & ptr[0]))
    {
        // 1 byte UTF-8 codepoint
        codepoint = ptr[0];
        *codepointSize = 1;
    }

    return codepoint;
}

// Get previous codepoint in a byte sequence and bytes processed
i32 GetCodepointPrevious(std::string_view textView, i32 *codepointSize)
{
    const std::string textStr(textView);
    const char *text = textStr.c_str();

    const char *ptr = text;
    i32 codepoint = 0x3f;       // Codepoint (defaults to '?')
    *codepointSize = 1;
    if (text == nullptr) return codepoint;

    // Move to previous codepoint
    do ptr--;
    while (((0x80 & ptr[0]) != 0) && ((0xc0 & ptr[0]) ==  0x80));

    auto cpSize = 0;
    codepoint = GetCodepointNext(ptr, &cpSize);
    if (codepoint != 0) *codepointSize = cpSize;

    return codepoint;
}

//----------------------------------------------------------------------------------
// Module Internal Functions Definition
//----------------------------------------------------------------------------------
#if SUPPORT_FILEFORMAT_FNT || SUPPORT_FILEFORMAT_BDF
// Read a line from memory
// REQUIRES: memcpy()
// NOTE: Returns the number of bytes read
static i32 GetLine(const char *origin, char *buffer, const i32 maxLength)
{
    auto count = 0;
    for (; count < maxLength - 1; count++) if (origin[count] == '\n') break;
    memcpy(buffer, origin, count);
    buffer[count] = '\0';
    return count;
}
#endif

#if SUPPORT_FILEFORMAT_FNT
// Load a BMFont file (AngelCode font file)
// REQUIRES: strstr(), sscanf(), strrchr(), memcpy()
static Font LoadBMFont(const char *fileName)
{
    #define MAX_BUFFER_SIZE       256
    #define MAX_FONT_IMAGE_PAGES    8

    auto font = Font{ 0 };

    char buffer[MAX_BUFFER_SIZE] = { 0 };
    char *searchPoint = nullptr;

    auto fontSize = 0;
    auto glyphCount = 0;

    auto imWidth = 0;
    auto imHeight = 0;
    auto pageCount = 1;
    char imFileName[MAX_FONT_IMAGE_PAGES][129] = { 0 };

    auto base = 0;       // Useless data
    auto readBytes = 0;  // Data bytes read
    auto readVars = 0;   // Variables filled by sscanf()

    auto fileText = LoadFileText(fileName);

    if (fileText.empty()) return font;

    char *fileTextPtr = fileText.data();

    // NOTE: Skip first line, it contains no useful information
    readBytes = GetLine(fileTextPtr, buffer, MAX_BUFFER_SIZE);
    fileTextPtr += (readBytes + 1);

    // Read line data
    readBytes = GetLine(fileTextPtr, buffer, MAX_BUFFER_SIZE);
    searchPoint = strstr(buffer, "lineHeight");
    readVars = sscanf(searchPoint, "lineHeight=%i base=%i scaleW=%i scaleH=%i pages=%i", &fontSize, &base, &imWidth, &imHeight, &pageCount);
    fileTextPtr += (readBytes + 1);

    if (readVars < 4) return font; // Some data not available, file malformed

    if (pageCount > MAX_FONT_IMAGE_PAGES)
    {
        log(LogLevel::warn, "FONT: [{}] Font defines more pages than supported: {}/{}", fileName, pageCount, MAX_FONT_IMAGE_PAGES);
        pageCount = MAX_FONT_IMAGE_PAGES;
    }

    for (i32 i = 0; i < pageCount; i++)
    {
        readBytes = GetLine(fileTextPtr, buffer, MAX_BUFFER_SIZE);
        searchPoint = strstr(buffer, "file");
        readVars = sscanf(searchPoint, "file=\"%128[^\"]\"", imFileName[i]);
        fileTextPtr += (readBytes + 1);

        if (readVars < 1) return font; // No fileName read
    }

    readBytes = GetLine(fileTextPtr, buffer, MAX_BUFFER_SIZE);
    searchPoint = strstr(buffer, "count");
    readVars = sscanf(searchPoint, "count=%i", &glyphCount);
    fileTextPtr += (readBytes + 1);

    if (readVars < 1) return font; // No glyphCount read

    // Load all required images for further compose
    auto imFonts = static_cast<Image *>(calloc(pageCount, sizeof(Image))); // Font atlases, multiple images

    for (i32 i = 0; i < pageCount; i++)
    {
        imFonts[i] = LoadImage(TextFormat("%s/%s", GetDirectoryPath(fileName).c_str(), imFileName[i]));

        if (imFonts[i].format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE)
        {
            // Convert image to GRAYSCALE + ALPHA, using the mask as the alpha channel
            const Image imFontAlpha = {
                .data = calloc(imFonts[i].width*imFonts[i].height, 2),
                .width = imFonts[i].width,
                .height = imFonts[i].height,
                .mipmaps = 1,
                .format = PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA
            };

            for (i32 p = 0, pi = 0; p < (imFonts[i].width*imFonts[i].height*2); p += 2, pi++)
            {
                (static_cast<u8 *>(imFontAlpha.data))[p] = 0xff;
                (static_cast<u8 *>(imFontAlpha.data))[p + 1] = (static_cast<u8 *>(imFonts[i].data))[pi];
            }

            UnloadImage(imFonts[i]);
            imFonts[i] = imFontAlpha;
        }
    }

    Image fullFont = imFonts[0];
    for (i32 i = 1; i < pageCount; i++) UnloadImage(imFonts[i]);

    // If multiple atlas, then merge atlas
    // NOTE: WARNING: This process could be really slow!
    if (pageCount > 1)
    {
        // Resize font atlas to draw additional images
        ImageResizeCanvas(&fullFont, imWidth, imHeight*pageCount, 0, 0, BLACK);

        for (i32 i = 1; i < pageCount; i++)
        {
            const auto srcRec = Rectangle{ 0.0f, 0.0f, static_cast<f32>(imWidth), static_cast<f32>(imHeight) };
            const auto dstRec = Rectangle{ 0.0f, static_cast<f32>(imHeight)*static_cast<f32>(i), static_cast<f32>(imWidth), static_cast<f32>(imHeight) };
            ImageDrawImagePro(&fullFont, imFonts[i], srcRec, dstRec, vec2{ 0 }, 0.0f, WHITE);
        }
    }

    free(imFonts);

    font.texture = LoadTextureFromImage(fullFont);

    // Fill font characters info data
    font.baseSize = fontSize;
    font.glyphCount = glyphCount;
    font.glyphPadding = 0;
    font.glyphs = static_cast<GlyphInfo *>(calloc(glyphCount, sizeof(GlyphInfo)));
    font.recs = static_cast<Rectangle *>(calloc(glyphCount, sizeof(Rectangle)));

    auto charId = 0;
    auto charX = 0;
    auto charY = 0;
    auto charWidth = 0;
    auto charHeight = 0;
    auto charOffsetX = 0;
    auto charOffsetY = 0;
    auto charAdvanceX = 0;
    auto pageID = 0;

    for (i32 i = 0; i < glyphCount; i++)
    {
        readBytes = GetLine(fileTextPtr, buffer, MAX_BUFFER_SIZE);
        readVars = sscanf(buffer, "char id=%i x=%i y=%i width=%i height=%i xoffset=%i yoffset=%i xadvance=%i page=%i",
                       &charId, &charX, &charY, &charWidth, &charHeight, &charOffsetX, &charOffsetY, &charAdvanceX, &pageID);
        fileTextPtr += (readBytes + 1);

        if (readVars == 9) // Make sure all char data has been properly read
        {
            // Get character rectangle in the font atlas texture
            font.recs[i] = Rectangle{ static_cast<f32>(charX), static_cast<f32>(charY) + static_cast<f32>(imHeight)*static_cast<f32>(pageID), static_cast<f32>(charWidth), static_cast<f32>(charHeight) };

            // Save data properly in sprite font
            font.glyphs[i].value = charId;
            font.glyphs[i].offsetX = charOffsetX;
            font.glyphs[i].offsetY = charOffsetY;
            font.glyphs[i].advanceX = charAdvanceX;

            // Fill character image data from full font data
            font.glyphs[i].image = ImageFromImage(fullFont, font.recs[i]);
        }
        else
        {
            font.glyphs[i].image = GenImageColor(static_cast<i32>(font.recs[i].width), static_cast<i32>(font.recs[i].height), BLACK);
            log(LogLevel::warn, "FONT: [{}] Some characters data not correctly provided", fileName);
        }
    }

    UnloadImage(fullFont);

    if (font.texture.id == 0)
    {
        UnloadFont(font);
        font = GetFontDefault();
        log(LogLevel::warn, "FONT: [{}] Failed to load texture, reverted to default font", fileName);
    }
    else log(LogLevel::info, "FONT: [{}] Font loaded successfully ({} glyphs)", fileName, font.glyphCount);

    return font;
}
#endif

#if SUPPORT_FILEFORMAT_BDF
// Convert hexadecimal to decimal (single digit)
static u8 HexToInt(const char hex)
{
    if ((hex >= '0') && (hex <= '9')) return hex - '0';
    else if ((hex >= 'a') && (hex <= 'f')) return hex - 'a' + 10;
    else if ((hex >= 'A') && (hex <= 'F')) return hex - 'A' + 10;
    else return 0;
}

// Load font data for further use
// NOTE: Requires BDF font memory data
static GlyphInfo *LoadFontDataBDF(const u8 *fileData, const i32 dataSize, const i32 *codepoints, i32 codepointCount, const i32 *outFontSize)
{
    #define MAX_BUFFER_SIZE 256

    char buffer[MAX_BUFFER_SIZE] = { 0 };

    GlyphInfo *glyphs = nullptr;
    GlyphInfo *outGlyphPtr = nullptr;  // Pointer to output glyph info (NULL if not set)
    auto internalCodepoints = false;

    auto totalReadBytes = 0;         // Data bytes read (total)
    auto readBytes = 0;              // Data bytes read (line)
    auto readVars = 0;               // Variables filled by sscanf()

    auto fileText = static_cast<const char *>(fileData);
    const char *fileTextPtr = fileText;

    auto fontMalformed = false;     // Is the font malformed
    auto fontStarted = false;       // Has font started (STARTFONT)
    auto fontBBw = 0;                // Font base character bounding box width
    auto fontBBh = 0;                // Font base character bounding box height
    auto fontBBxoff0 = 0;            // Font base character bounding box X0 offset
    auto fontBByoff0 = 0;            // Font base character bounding box Y0 offset
    auto fontAscent = 0;             // Font ascent

    auto charStarted = false;       // Has character started (STARTCHAR)
    auto charBitmapStarted = false; // Has bitmap data started (BITMAP)
    auto charBitmapNextRow = 0;      // Y position for the next row of bitmap data
    auto charEncoding = -1;          // The unicode value of the character (-1 if not set)
    auto charBBw = 0;                // Character bounding box width
    auto charBBh = 0;                // Character bounding box height
    auto charBBxoff0 = 0;            // Character bounding box X0 offset
    auto charBByoff0 = 0;            // Character bounding box Y0 offset
    auto charDWidthX = 0;            // Character advance X
    auto charDWidthY = 0;            // Character advance Y (unused)

    if (fileData == nullptr) return glyphs;

    // In case no chars count provided, default to 95
    codepointCount = (codepointCount > 0)? codepointCount : 95;
    auto requiredCodepoints = static_cast<i32 *>(calloc(codepointCount, sizeof(i32)));

    if (codepoints == nullptr)
    {
        // Fill internal codepoints array in case not provided externally
        // NOTE: By default, filling glyph count consecutively, starting at 32 (Space)
        for (i32 i = 0; i < codepointCount; i++) requiredCodepoints[i] = i + 32;
        internalCodepoints = true;
    }
    else
    {
        for (i32 i = 0; i < codepointCount; i++) requiredCodepoints[i] = codepoints[i];
    }

    glyphs = static_cast<GlyphInfo *>(calloc(codepointCount, sizeof(GlyphInfo)));

    while (totalReadBytes <= dataSize)
    {
        readBytes = GetLine(fileTextPtr, buffer, MAX_BUFFER_SIZE);
        totalReadBytes += (readBytes + 1);
        fileTextPtr += (readBytes + 1);

        // Line: COMMENT
        if (strstr(buffer, "COMMENT") != nullptr) continue; // Ignore line

        if (charStarted)
        {
            // Line: ENDCHAR
            if (strstr(buffer, "ENDCHAR") != nullptr)
            {
                charStarted = false;
                continue;
            }

            if (charBitmapStarted)
            {
                if (outGlyphPtr != nullptr)
                {
                    i32 pixelY = charBitmapNextRow++;

                    if (pixelY >= outGlyphPtr->image.height) break;

                    for (i32 x = 0; x < readBytes; x++)
                    {
                        auto byte = HexToInt(buffer[x]);

                        for (i32 bitX = 0; bitX < 4; bitX++)
                        {
                            i32 pixelX = ((x*4) + bitX);

                            if (pixelX >= outGlyphPtr->image.width) break;

                            if ((byte & (8 >> bitX)) > 0) (static_cast<u8 *>(outGlyphPtr->image.data))[(pixelY*outGlyphPtr->image.width) + pixelX] = 255;
                        }
                    }
                }
                continue;
            }

            // Line: ENCODING
            if (strstr(buffer, "ENCODING") != nullptr)
            {
                readVars = sscanf(buffer, "ENCODING %i", &charEncoding);
                continue;
            }

            // Line: BBX
            if (strstr(buffer, "BBX") != nullptr)
            {
                readVars = sscanf(buffer, "BBX %i %i %i %i", &charBBw, &charBBh, &charBBxoff0, &charBByoff0);
                continue;
            }

            // Line: DWIDTH
            if (strstr(buffer, "DWIDTH") != nullptr)
            {
                readVars = sscanf(buffer, "DWIDTH %i %i", &charDWidthX, &charDWidthY);
                continue;
            }

            // Line: BITMAP
            if (strstr(buffer, "BITMAP") != nullptr)
            {
                // Search for glyph index in codepoints
                outGlyphPtr = nullptr;

                for (i32 index = 0; index < codepointCount; index++)
                {
                    if (requiredCodepoints[index] == charEncoding)
                    {
                        outGlyphPtr = &glyphs[index];
                        break;
                    }
                }

                // Init glyph info
                if (outGlyphPtr != nullptr)
                {
                    outGlyphPtr->value = charEncoding;
                    // BBX offsets place the glyph bitmap relative to the pen position on the baseline,
                    // raylib offsets are measured from the top of the line, fontAscent above the baseline
                    outGlyphPtr->offsetX = charBBxoff0;
                    outGlyphPtr->offsetY = fontAscent - (charBBh + charBByoff0);
                    outGlyphPtr->advanceX = charDWidthX;

                    outGlyphPtr->image.data = calloc(charBBw*charBBh, 1);
                    outGlyphPtr->image.width = charBBw;
                    outGlyphPtr->image.height = charBBh;
                    outGlyphPtr->image.mipmaps = 1;
                    outGlyphPtr->image.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;
                }

                charBitmapStarted = true;
                charBitmapNextRow = 0;

                continue;
            }
        }
        else if (fontStarted)
        {
            // Line: ENDFONT
            if (strstr(buffer, "ENDFONT") != nullptr)
            {
                fontStarted = false;
                break;
            }

            // Line: SIZE
            if (strstr(buffer, "SIZE") != nullptr)
            {
                if (outFontSize != nullptr) readVars = sscanf(buffer, "SIZE %i", outFontSize);
                continue;
            }

            // PIXEL_SIZE
            if (strstr(buffer, "PIXEL_SIZE") != nullptr)
            {
                if (outFontSize != nullptr) readVars = sscanf(buffer, "PIXEL_SIZE %i", outFontSize);
                continue;
            }

            // FONTBOUNDINGBOX
            if (strstr(buffer, "FONTBOUNDINGBOX") != nullptr)
            {
                readVars = sscanf(buffer, "FONTBOUNDINGBOX %i %i %i %i", &fontBBw, &fontBBh, &fontBBxoff0, &fontBByoff0);
                fontAscent = fontBBh + fontBByoff0; // Default if FONT_ASCENT property is not provided
                continue;
            }

            // FONT_ASCENT
            if (strstr(buffer, "FONT_ASCENT") != nullptr)
            {
                readVars = sscanf(buffer, "FONT_ASCENT %i", &fontAscent);
                continue;
            }

            // STARTCHAR
            if (strstr(buffer, "STARTCHAR") != nullptr)
            {
                charStarted = true;
                charEncoding = -1;
                outGlyphPtr = nullptr;
                charBBw = 0;
                charBBh = 0;
                charBBxoff0 = 0;
                charBByoff0 = 0;
                charDWidthX = 0;
                charDWidthY = 0;
                outGlyphPtr = nullptr;
                charBitmapStarted = false;
                charBitmapNextRow = 0;
                continue;
            }
        }
        else
        {
            // STARTFONT
            if (strstr(buffer, "STARTFONT") != nullptr)
            {
                if (fontStarted)
                {
                    fontMalformed = true;
                    break;
                }
                else
                {
                    fontStarted = true;
                    continue;
                }
            }
        }
    }

    free(requiredCodepoints);

    if (fontMalformed)
    {
        free(glyphs);
        glyphs = nullptr;
    }

    return glyphs;
}
#endif      // SUPPORT_FILEFORMAT_BDF

#endif      // SUPPORT_MODULE_RTEXT
