/*******************************************************************************************
*
*   raylib [text] example - words alignment
*
*   Example complexity rating: [★☆☆☆] 1/4
*
*   Example originally created with raylib 6.0, last time updated with raylib 6.0
*
*   Example contributed by JP Mortiboys (@themushroompirates) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2025 JP Mortiboys (@themushroompirates)
*
********************************************************************************************/

#include "raylib.h"

#include "raymath.h"    // Required for: Lerp()

typedef enum TextAlignment {
    TEXT_ALIGN_LEFT   = 0,
    TEXT_ALIGN_TOP    = 0,
    TEXT_ALIGN_CENTRE = 1,
    TEXT_ALIGN_MIDDLE = 1,
    TEXT_ALIGN_RIGHT  = 2,
    TEXT_ALIGN_BOTTOM = 2
} TextAlignment;

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr auto screenWidth = 800;
    constexpr auto screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [text] example - words alignment");

    // Define the rectangle we will draw the text in
    const auto textContainerRect = Rectangle{ static_cast<f32>(screenWidth)/2-static_cast<f32>(screenWidth)/4, static_cast<f32>(screenHeight)/2-static_cast<f32>(screenHeight)/3, static_cast<f32>(screenWidth)/2, static_cast<f32>(screenHeight)*2/3 };

    // Some text to display the current alignment
    const char *textAlignNameH[] = { "Left", "Centre", "Right" };
    const char *textAlignNameV[] = { "Top", "Middle", "Bottom" };

    // Define the text we're going to draw in the rectangle
    auto wordIndex = 0;
    std::vector<std::string> words = TextSplit("raylib is a simple and easy-to-use library to enjoy videogames programming", ' ');
    const auto wordCount = static_cast<i32>(words.size());

    // Initialize the font size we're going to use
    constexpr auto fontSize = 40;

    // And of course the font...
    const auto font = GetFontDefault();

    // Initialize the alignment variables
    i32 hAlign = TEXT_ALIGN_CENTRE;
    i32 vAlign = TEXT_ALIGN_MIDDLE;

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        if (IsKeyPressed(KEY_LEFT))
        {
            if (hAlign > 0) hAlign = hAlign - 1;
        }

        if (IsKeyPressed(KEY_RIGHT))
        {
            hAlign = hAlign + 1;
            if (hAlign > 2) hAlign = 2;
        }

        if (IsKeyPressed(KEY_UP))
        {
            if (vAlign > 0) vAlign = vAlign - 1;
        }

        if (IsKeyPressed(KEY_DOWN))
        {
            vAlign = vAlign + 1;
            if (vAlign > 2) vAlign = 2;
        }

        // One word per second
        if (wordCount > 0)  wordIndex = static_cast<i32>(GetTime())%wordCount;
        else  wordIndex = 0;
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(DARKBLUE);

            DrawText("Use Arrow Keys to change the text alignment", 20, 20, 20, LIGHTGRAY);
            DrawText(TextFormat("Alignment: Horizontal = %s, Vertical = %s", textAlignNameH[hAlign], textAlignNameV[vAlign]), 20, 40, 20, LIGHTGRAY);

            DrawRectangleRec(textContainerRect, BLUE);

            // Get the size of the text to draw
            const auto textSize = MeasureTextEx(font, words[wordIndex], static_cast<f32>(fontSize), fontSize*.1f);

            // Calculate the top-left text position based on the rectangle and alignment
            const vec2 textPos = vec2{
                textContainerRect.x + Lerp(0.0f, textContainerRect.width  - textSize.x, (static_cast<f32>(hAlign))*0.5f),
                textContainerRect.y + Lerp(0.0f, textContainerRect.height - textSize.y, (static_cast<f32>(vAlign))*0.5f)
            };

            // Draw the text
            DrawTextEx(font, words[wordIndex], textPos, static_cast<f32>(fontSize), fontSize*.1f, RAYWHITE);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------

    CloseWindow();          // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
