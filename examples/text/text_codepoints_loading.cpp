/*******************************************************************************************
*
*   raylib [text] example - codepoints loading
*
*   Example complexity rating: [★★★☆] 3/4
*
*   Example originally created with raylib 4.2, last time updated with raylib 4.2
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2022-2025 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"

#include <stdlib.h>         // Required for: calloc(), realloc(), free()
#include <string.h>         // Required for: memcpy()

// Text to be displayed, must be UTF-8 (save this code file as UTF-8)
// NOTE: It can contain all the required text for the game,
// this text will be scanned to get all the required codepoints
static const char *text = "いろはにほへと　ちりぬるを\nわかよたれそ　つねならむ\nうゐのおくやま　けふこえて\nあさきゆめみし　ゑひもせす";

//------------------------------------------------------------------------------------
// Module Functions Declaration
//------------------------------------------------------------------------------------
// Remove codepoint duplicates if requested
static i32 *CodepointRemoveDuplicates(i32 *codepoints, i32 codepointCount, i32 *codepointResultCount);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr auto screenWidth = 800;
    constexpr auto screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [text] example - codepoints loading");

    // Convert each utf-8 character into its
    // corresponding codepoint in the font file
    auto codepointCount = 0;
    auto codepoints = LoadCodepoints(text, &codepointCount);

    // Removed duplicate codepoints to generate smaller font atlas
    auto codepointsNoDupsCount = 0;
    auto codepointsNoDups = CodepointRemoveDuplicates(codepoints, codepointCount, &codepointsNoDupsCount);
    UnloadCodepoints(codepoints);

    // Load font containing all the provided codepoint glyphs
    // A texture font atlas is automatically generated
    const Font font = LoadFontEx("resources/DotGothic16-Regular.ttf", 36, codepointsNoDups, codepointsNoDupsCount);

    // Set bilinear scale filter for better font scaling
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);

    SetTextLineSpacing(20);         // Set line spacing for multiline text (when line breaks are included '\n')

    // Free codepoints, atlas has already been generated
    free(codepointsNoDups);

    auto showFontAtlas = false;

    auto codepointSize = 0;
    const char *ptr = text;

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        if (IsKeyPressed(KEY_SPACE)) showFontAtlas = !showFontAtlas;

        // Testing code: getting next and previous codepoints on provided text
        if (IsKeyPressed(KEY_RIGHT))
        {
            // Get next codepoint in string and move pointer
            GetCodepointNext(ptr, &codepointSize);
            ptr += codepointSize;
        }
        else if (IsKeyPressed(KEY_LEFT))
        {
            // Get previous codepoint in string and move pointer
            GetCodepointPrevious(ptr, &codepointSize);
            ptr -= codepointSize;
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawRectangle(0, 0, GetScreenWidth(), 70, BLACK);
            DrawText(TextFormat("Total codepoints contained in provided text: %i", codepointCount), 10, 10, 20, GREEN);
            DrawText(TextFormat("Total codepoints required for font atlas (duplicates excluded): %i", codepointsNoDupsCount), 10, 40, 20, GREEN);

            if (showFontAtlas)
            {
                // Draw generated font texture atlas containing provided codepoints
                DrawTexture(font.texture, 150, 100, BLACK);
                DrawRectangleLines(150, 100, font.texture.width, font.texture.height, BLACK);
            }
            else
            {
                // Draw provided text with loaded font, containing all required codepoint glyphs
                DrawTextEx(font, text, vec2{ 160, 110 }, 48, 5, BLACK);
            }

            DrawText("Press SPACE to toggle font atlas view!", 10, GetScreenHeight() - 30, 20, GRAY);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadFont(font);     // Unload font

    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//------------------------------------------------------------------------------------
// Module Functions Definition
//------------------------------------------------------------------------------------
// Remove codepoint duplicates if requested
// WARNING: This process could be a bit slow if the text to process is very long
static i32 *CodepointRemoveDuplicates(i32 *codepoints, i32 codepointCount, i32 *codepointsResultCount)
{
    i32 codepointsNoDupsCount = codepointCount;
    auto codepointsNoDups = static_cast<i32 *>(calloc(codepointCount, sizeof(i32)));
    memcpy(codepointsNoDups, codepoints, codepointCount*sizeof(i32));

    // Remove duplicates
    for (i32 i = 0; i < codepointsNoDupsCount; i++)
    {
        for (i32 j = i + 1; j < codepointsNoDupsCount; j++)
        {
            if (codepointsNoDups[i] == codepointsNoDups[j])
            {
                for (i32 k = j; k < codepointsNoDupsCount - 1; k++) codepointsNoDups[k] = codepointsNoDups[k + 1];

                codepointsNoDupsCount--;
                j--;
            }
        }
    }

    // NOTE: The size of codepointsNoDups is the same as original array but
    // only required positions are filled (codepointsNoDupsCount)

    *codepointsResultCount = codepointsNoDupsCount;
    return codepointsNoDups;
}
