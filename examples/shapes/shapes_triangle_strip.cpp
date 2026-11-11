/*******************************************************************************************
*
*   raylib [shapes] example - triangle strip
*
*   Example complexity rating: [★★☆☆] 2/4
*
*   Example originally created with raylib 6.0, last time updated with raylib 6.0
*
*   Example contributed by Jopestpe (@jopestpe)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2025 Jopestpe (@jopestpe)
*
********************************************************************************************/

#include "raylib.h"
#include <math.h>

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

    InitWindow(screenWidth, screenHeight, "raylib [shapes] example - triangle strip");

    vec2 points[122] = {};
    const auto center = vec2{ (screenWidth/2.0f) - 125.0f, screenHeight/2.0f };
    auto segments = 6.0f;
    constexpr auto insideRadius = 100.0f;
    constexpr auto outsideRadius = 150.0f;
    auto outline = true;

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        const auto pointCount = static_cast<i32>(segments);
        const f32 angleStep = (360.0f/pointCount)*DEG2RAD;

        for (i32 i = 0, i2 = 0; i < pointCount; i++, i2 += 2)
        {
            const f32 angle1 = i*angleStep;
            points[i2] = vec2{ center.x + cosf(angle1)*insideRadius, center.y + sinf(angle1)*insideRadius };
            const f32 angle2 = angle1 + angleStep/2.0f;
            points[i2 + 1] = vec2{ center.x + cosf(angle2)*outsideRadius, center.y + sinf(angle2)*outsideRadius };
        }

        points[pointCount*2] = points[0];
        points[pointCount*2 + 1] = points[1];
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            for (i32 i = 0; i < pointCount; i++)
            {
                const vec2 a = points[i*2];
                const vec2 b = points[i*2 + 1];
                const vec2 c = points[i*2 + 2];
                const vec2 d = points[i*2 + 3];

                const f32 angle1 = i*angleStep;
                DrawTriangle(c, b, a, ColorFromHSV(angle1*RAD2DEG, 1.0f, 1.0f));
                DrawTriangle(d, b, c, ColorFromHSV((angle1 + angleStep/2)*RAD2DEG, 1.0f, 1.0f));

                if (outline)
                {
                    DrawTriangleLines(a, b, c, BLACK);
                    DrawTriangleLines(c, b, d, BLACK);
                }
            }

            DrawLine(580, 0, 580, GetScreenHeight(), Color{ 218, 218, 218, 255 });
            DrawRectangle(580, 0, GetScreenWidth(), GetScreenHeight(), Color{ 232, 232, 232, 255 });

            // Draw GUI controls
            //------------------------------------------------------------------------------
            GuiSliderBar(Rectangle{ 640, 40, 120, 20}, "Segments", TextFormat("%.0f", segments).c_str(), &segments, 6.0f, 60.0f);
            GuiCheckBox(Rectangle{ 640, 70, 20, 20 }, "Outline", &outline);
            //------------------------------------------------------------------------------

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