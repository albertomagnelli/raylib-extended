/*******************************************************************************************
*
*   raylib [textures] example - image text
*
*   Example complexity rating: [★★☆☆] 2/4
*
*   Example originally created with raylib 1.8, last time updated with raylib 4.0
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2017-2025 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr int screenWidth = 800;
    constexpr int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [textures] example - image text");

    Image parrots = LoadImage("resources/parrots.png"); // Load image in CPU memory (RAM)

    // When no codepoints are provided, LoadFontEx() loads a default set of codepoints
    // This set includes '@', which "KAISG.ttf" doesn't have a glyph for, causing a warning to be logged
    // We avoid this by just excluding it from the list of codepoints we load
    int codepointCount = 0;
    int *codepoints = LoadCodepoints(" !\"#$%&'()*+,-./0123456789:;<=>?ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~", &codepointCount);
    // TTF Font loading with custom generation parameters
    const Font font = LoadFontEx("resources/KAISG.ttf", 64, codepoints, codepointCount);
    UnloadCodepoints(codepoints);

    // Draw over image using custom font
    ImageDrawTextEx(&parrots, font, "[Parrots font drawing]", glm::vec2{ 20.0f, 20.0f }, static_cast<float>(font.baseSize), 0.0f, RED);

    const Texture2D texture = LoadTextureFromImage(parrots);  // Image converted to texture, uploaded to GPU memory (VRAM)
    UnloadImage(parrots);   // Once image has been converted to texture and uploaded to VRAM, it can be unloaded from RAM

    const glm::vec2 position = { static_cast<float>(screenWidth)/2 - static_cast<float>(texture.width)/2, static_cast<float>(screenHeight)/2 - static_cast<float>(texture.height)/2 - 20 };

    bool showFont = false;

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        if (IsKeyDown(KEY_SPACE)) showFont = true;
        else showFont = false;
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            if (!showFont)
            {
                // Draw texture with text already drawn inside
                DrawTextureV(texture, position, WHITE);

                // Draw text directly using sprite font
                DrawTextEx(font, "[Parrots font drawing]", glm::vec2{ position.x + 20,
                           position.y + 20 + 280 }, static_cast<float>(font.baseSize), 0.0f, WHITE);
            }
            else
            {
                // Make the font atlas texture fit the screen, so we can see the whole thing
                float scale = 1.0f;

                const float atlasRatio = static_cast<float>(font.texture.width)/static_cast<float>(font.texture.height);
                const float screenRatio = static_cast<float>(screenWidth)/static_cast<float>(screenHeight);
                if (atlasRatio >= screenRatio) scale = static_cast<float>(screenWidth)/static_cast<float>(font.texture.width);
                else scale = static_cast<float>(screenHeight)/static_cast<float>(font.texture.height);

                const float width = static_cast<float>(font.texture.width) * scale;
                const float height = static_cast<float>(font.texture.height) * scale;

                DrawTextureEx(font.texture, glm::vec2{ (static_cast<float>(screenWidth) - width)/2, (static_cast<float>(screenHeight) - height)/2 }, 0, scale, BLACK);
            }

            DrawText("PRESS SPACE to SHOW FONT ATLAS USED", 290, 420, 10, DARKGRAY);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadTexture(texture);     // Texture unloading

    UnloadFont(font);           // Unload custom font

    CloseWindow();              // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}