/*******************************************************************************************
*
*   raylib [shapes] example - math sine cosine
*
*   Example complexity rating: [★★☆☆] 2/4
*
*   Example originally created with raylib 6.0, last time updated with raylib 6.0
*
*   Example contributed by Jopestpe (@jopestpe) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2025 Jopestpe (@jopestpe)
*
********************************************************************************************/

#include "raylib.h"
#include <math.h>
#include "raymath.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"                 // Required for GUI controls

// Wave points for sine/cosine visualization
#define WAVE_POINTS 36

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr i32 screenWidth = 800;
    constexpr i32 screenHeight = 450;

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "raylib [shapes] example - math sine cosine");

    vec2 sinePoints[WAVE_POINTS];
    vec2 cosPoints[WAVE_POINTS];
    const vec2 center = { (screenWidth/2.0f) - 30.0f, screenHeight/2.0f };
    const Rectangle start = { 20.0f, screenHeight - 120.f , 200.0f, 100.0f};
    constexpr f32 radius = 130.0f;
    f32 angle = 0.0f;
    bool pause = false;

    for (i32 i = 0; i < WAVE_POINTS; i++)
    {
        const f32 t = i/static_cast<f32>(WAVE_POINTS - 1);
        const f32 currentAngle = t*360.0f*DEG2RAD;
        sinePoints[i] = vec2{ start.x + t*start.width, start.y + start.height/2.0f - sinf(currentAngle)*(start.height/2.0f) };
        cosPoints[i] = vec2{ start.x + t*start.width, start.y + start.height/2.0f - cosf(currentAngle)*(start.height/2.0f) };
    }

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        const f32 angleRad = angle*DEG2RAD;
        const f32 cosRad = cosf(angleRad);
        const f32 sinRad = sinf(angleRad);

        const vec2 point = { center.x + cosRad*radius, center.y - sinRad*radius };
        const vec2 limitMin = { center.x - radius, center.y - radius };
        const vec2 limitMax = { center.x + radius, center.y + radius };

        const f32 complementary = 90.0f - angle;
        const f32 supplementary = 180.0f - angle;
        const f32 explementary = 360.0f - angle;

        const f32 tangent = Clamp(tanf(angleRad), -10.0f, 10.0f);
        const f32 cotangent = (fabsf(tangent) > 0.001f) ? Clamp(1.0f/tangent, -radius, radius) : 0.0f;
        const vec2 tangentPoint = { center.x + radius, center.y - tangent*radius };
        const vec2 cotangentPoint = { center.x + cotangent*radius, center.y - radius };

        angle = Wrap(angle + (!pause ? 1.0f : 0.0f), 0.0f, 360.0f);
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();
            ClearBackground(RAYWHITE);

            // Cotangent (orange)
            DrawLineEx(vec2{ center.x , limitMin.y }, vec2{ cotangentPoint.x, limitMin.y }, 2.0f, ORANGE);
            DrawLineDashed(center, cotangentPoint, 10, 4, ORANGE);

            // Side background
            DrawLine(580, 0, 580, GetScreenHeight(), Color{ 218, 218, 218, 255 });
            DrawRectangle(580, 0, GetScreenWidth(), GetScreenHeight(), Color{ 232, 232, 232, 255 });

            // Base circle and axes
            DrawCircleLinesV(center, radius, GRAY);
            DrawLineEx(vec2{ center.x, limitMin.y }, vec2{ center.x, limitMax.y }, 1.0f, GRAY);
            DrawLineEx(vec2{ limitMin.x, center.y }, vec2{ limitMax.x, center.y }, 1.0f, GRAY);

            // Wave graph axes
            DrawLineEx(vec2{ start.x , start.y }, vec2{ start.x , start.y + start.height }, 2.0f, GRAY);
            DrawLineEx(vec2{ start.x + start.width, start.y }, vec2{ start.x + start.width, start.y + start.height }, 2.0f, GRAY);
            DrawLineEx(vec2{ start.x, start.y + start.height/2 }, vec2{ start.x + start.width, start.y + start.height/2 }, 2.0f, GRAY);

            // Wave graph axis labels
            DrawText("1", static_cast<i32>(start.x) - 8, static_cast<i32>(start.y), 6, GRAY);
            DrawText("0", static_cast<i32>(start.x) - 8, static_cast<i32>(start.y) + static_cast<i32>(start.height)/2 - 6, 6, GRAY);
            DrawText("-1", static_cast<i32>(start.x) - 12, static_cast<i32>(start.y) + static_cast<i32>(start.height) - 8, 6, GRAY);
            DrawText("0", static_cast<i32>(start.x) - 2, static_cast<i32>(start.y) + static_cast<i32>(start.height) + 4, 6, GRAY);
            DrawText("360", static_cast<i32>(start.x) + static_cast<i32>(start.width) - 8, static_cast<i32>(start.y) + static_cast<i32>(start.height) + 4, 6, GRAY);

            // Sine (red - vertical)
            DrawLineEx(vec2{ center.x, center.y }, vec2{ center.x, point.y }, 2.0f, RED);
            DrawLineDashed(vec2{ point.x, center.y }, vec2{ point.x, point.y }, 10, 4, RED);
            DrawText(TextFormat("Sine %.2f", sinRad), 640, 190, 6, RED);
            DrawCircleV(vec2{ start.x + (angle/360.0f)*start.width, start.y + ((-sinRad + 1)*start.height/2.0f) }, 4.0f, RED);
            DrawSplineLinear(sinePoints, WAVE_POINTS, 1.0f, RED);

            // Cosine (blue - horizontal)
            DrawLineEx(vec2{ center.x, center.y }, vec2{ point.x, center.y }, 2.0f, BLUE);
            DrawLineDashed(vec2{ center.x , point.y }, vec2{ point.x, point.y }, 10, 4, BLUE);
            DrawText(TextFormat("Cosine %.2f", cosRad), 640, 210, 6, BLUE);
            DrawCircleV(vec2{ start.x + (angle/360.0f)*start.width, start.y + ((-cosRad + 1)*start.height/2.0f) }, 4.0f, BLUE);
            DrawSplineLinear(cosPoints, WAVE_POINTS, 1.0f, BLUE);

            // Tangent (purple)
            DrawLineEx(vec2{ limitMax.x , center.y }, vec2{ limitMax.x, tangentPoint.y }, 2.0f, PURPLE);
            DrawLineDashed(center, tangentPoint, 10, 4, PURPLE);
            DrawText(TextFormat("Tangent %.2f", tangent), 640, 230, 6, PURPLE);

            // Cotangent (orange)
            DrawText(TextFormat("Cotangent %.2f", cotangent), 640, 250, 6, ORANGE);

            // Complementary angle (beige)
            DrawCircleSectorLines(center, radius*0.6f , -angle, -90.0f , 36, BEIGE);
            DrawText(TextFormat("Complementary  %0.f°",complementary), 640, 150, 6, BEIGE);

            // Supplementary angle (darkblue)
            DrawCircleSectorLines(center, radius*0.5f , -angle, -180.0f , 36, DARKBLUE);
            DrawText(TextFormat("Supplementary  %0.f°",supplementary), 640, 130, 6, DARKBLUE);

            // Explementary angle (pink)
            DrawCircleSectorLines(center, radius*0.4f , -angle, -360.0f , 36, PINK);
            DrawText(TextFormat("Explementary  %0.f°",explementary), 640, 170, 6, PINK);

            // Current angle - arc (lime), radius (black), endpoint (black)
            DrawCircleSectorLines(center, radius*0.7f , -angle, 0.0f, 36, LIME);
            DrawLineEx(vec2{ center.x , center.y }, point, 2.0f, BLACK);
            DrawCircleV(point, 4.0f, BLACK);

            // Draw GUI controls
            //------------------------------------------------------------------------------
            GuiSetStyle(LABEL, TEXT_COLOR_NORMAL, ColorToInt(GRAY));
            GuiToggle(Rectangle{ 640, 70, 120, 20}, TextFormat("Pause").c_str(), &pause);
            GuiSetStyle(LABEL, TEXT_COLOR_NORMAL, ColorToInt(LIME));
            GuiSliderBar(Rectangle{ 640, 40, 120, 20}, "Angle", TextFormat("%.0f°", angle).c_str(), &angle, 0.0f, 360.0f);

            // Angle values panel
            GuiGroupBox(Rectangle{ 620, 110, 140, 170}, "Angle Values");
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