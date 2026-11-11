/*******************************************************************************************
*
*   raylib [shapes] example - rounded rectangle drawing
*
*   Example complexity rating: [★★★☆] 3/4
*
*   Example originally created with raylib 2.5, last time updated with raylib 2.5
*
*   Example contributed by Vlad Adrian (@demizdor) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2018-2025 Vlad Adrian (@demizdor) and Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"                 // Required for GUI controls

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr auto screenWidth = 800;
    constexpr auto screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [shapes] example - rounded rectangle drawing");

    auto roundness = 0.2f;
    auto width = 200.0f;
    auto height = 100.0f;
    auto segments = 0.0f;
    auto lineThick = 1.0f;

    auto drawRect = false;
    auto drawRoundedRect = true;
    auto drawRoundedLines = false;

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        const auto rec = Rectangle{ (static_cast<f32>(GetScreenWidth()) - width - 250)/2, (GetScreenHeight() - height)/2.0f, width, height };
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawLine(560, 0, 560, GetScreenHeight(), Fade(LIGHTGRAY, 0.6f));
            DrawRectangle(560, 0, GetScreenWidth() - 500, GetScreenHeight(), Fade(LIGHTGRAY, 0.3f));

            if (drawRect) DrawRectangleRec(rec, Fade(GOLD, 0.6f));
            if (drawRoundedRect) DrawRectangleRounded(rec, roundness, static_cast<i32>(segments), Fade(MAROON, 0.2f));
            if (drawRoundedLines) DrawRectangleRoundedLinesEx(rec, roundness, static_cast<i32>(segments), lineThick, Fade(MAROON, 0.4f));

            // Draw GUI controls
            //------------------------------------------------------------------------------
            GuiSliderBar(Rectangle{ 640, 40, 105, 20 }, "Width", TextFormat("%.2f", width).c_str(), &width, 0, static_cast<f32>(GetScreenWidth()) - 300);
            GuiSliderBar(Rectangle{ 640, 70, 105, 20 }, "Height", TextFormat("%.2f", height).c_str(), &height, 0, static_cast<f32>(GetScreenHeight()) - 50);
            GuiSliderBar(Rectangle{ 640, 140, 105, 20 }, "Roundness", TextFormat("%.2f", roundness).c_str(), &roundness, 0.0f, 1.0f);
            GuiSliderBar(Rectangle{ 640, 170, 105, 20 }, "Thickness", TextFormat("%.2f", lineThick).c_str(), &lineThick, 0, 20);
            GuiSliderBar(Rectangle{ 640, 240, 105, 20}, "Segments", TextFormat("%.2f", segments).c_str(), &segments, 0, 60);

            GuiCheckBox(Rectangle{ 640, 320, 20, 20 }, "DrawRoundedRect", &drawRoundedRect);
            GuiCheckBox(Rectangle{ 640, 350, 20, 20 }, "DrawRoundedLines", &drawRoundedLines);
            GuiCheckBox(Rectangle{ 640, 380, 20, 20}, "DrawRect", &drawRect);
            //------------------------------------------------------------------------------

            DrawText(TextFormat("MODE: %s", (segments >= 4)? "MANUAL" : "AUTO"), 640, 280, 10, (segments >= 4)? MAROON : DARKGRAY);

            DrawFPS(10, 10);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
