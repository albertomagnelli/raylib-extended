/*******************************************************************************************
*
*   raylib [shapes] example - ellipse collision
*
*   Example complexity rating: [★★☆☆] 2/4
*
*   Example originally created with raylib 5.5, last time updated with raylib 5.5
*
*   Example contributed by Ziya (@Monjaris)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2025 Ziya (@Monjaris)
*
********************************************************************************************/

#include "raylib.h"
#include <math.h>

// Check if point is inside ellipse
static bool CheckCollisionPointEllipse(vec2 point, vec2 center, f32 rx, f32 ry)
{
    const f32 dx = (point.x - center.x)/rx;
    const f32 dy = (point.y - center.y)/ry;
    return (dx*dx + dy*dy) <= 1.0f;
}

// Check if two ellipses collide
// Uses radial boundary distance in the direction between centers — scales correctly with radii
static bool CheckCollisionEllipses(vec2 c1, f32 rx1, f32 ry1, vec2 c2, f32 rx2, f32 ry2)
{
    const f32 dx = c2.x - c1.x;
    const f32 dy = c2.y - c1.y;
    const f32 dist = sqrtf(dx*dx + dy*dy);

    // Ellipses are on top of each other
    if (dist == 0.0f) return true;

    const f32 theta = atan2f(dy, dx);
    const f32 cosT = cosf(theta);
    const f32 sinT = sinf(theta);

    // Radial distance from center to ellipse boundary in direction theta
    // r(theta) = (rx * ry) / sqrt((ry*cos)^2 + (rx*sin)^2)
    const f32 r1 = (rx1*ry1)/sqrtf((ry1*cosT)*(ry1*cosT) + (rx1*sinT)*(rx1*sinT));
    const f32 r2 = (rx2*ry2)/sqrtf((ry2*cosT)*(ry2*cosT) + (rx2*sinT)*(rx2*sinT));

    return dist <= (r1 + r2);
}

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr i32 screenWidth = 800;
    constexpr i32 screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [shapes] example - ellipse collision");
    SetTargetFPS(60);

    vec2 ellipseACenter = { static_cast<f32>(screenWidth)/4, static_cast<f32>(screenHeight)/2 };
    constexpr f32 ellipseARx = 120.0f;
    constexpr f32 ellipseARy = 70.0f;

    vec2 ellipseBCenter = { static_cast<f32>(screenWidth)*3/4, static_cast<f32>(screenHeight)/2 };
    constexpr f32 ellipseBRx = 90.0f;
    constexpr f32 ellipseBRy = 140.0f;

    // 0 = controlling A, 1 = controlling B
    i32 controlled = 0;

    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        if (IsKeyPressed(KEY_A)) controlled = 0;
        if (IsKeyPressed(KEY_B)) controlled = 1;

        if (controlled == 0) ellipseACenter = GetMousePosition();
        else ellipseBCenter = GetMousePosition();

        const bool ellipsesCollide = CheckCollisionEllipses(
            ellipseACenter, ellipseARx, ellipseARy,
            ellipseBCenter, ellipseBRx, ellipseBRy
        );

        const bool mouseInA = CheckCollisionPointEllipse(GetMousePosition(), ellipseACenter, ellipseARx, ellipseARy);
        const bool mouseInB = CheckCollisionPointEllipse(GetMousePosition(), ellipseBCenter, ellipseBRx, ellipseBRy);
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawEllipse(static_cast<i32>(ellipseACenter.x), static_cast<i32>(ellipseACenter.y), ellipseARx, ellipseARy, ellipsesCollide ? RED : BLUE);

            DrawEllipse(static_cast<i32>(ellipseBCenter.x), static_cast<i32>(ellipseBCenter.y), ellipseBRx, ellipseBRy, ellipsesCollide ? RED : GREEN);

            DrawEllipseLines(static_cast<i32>(ellipseACenter.x), static_cast<i32>(ellipseACenter.y), ellipseARx, ellipseARy, WHITE);

            DrawEllipseLines(static_cast<i32>(ellipseBCenter.x), static_cast<i32>(ellipseBCenter.y), ellipseBRx, ellipseBRy, WHITE);

            DrawCircleV(ellipseACenter, 4, WHITE);
            DrawCircleV(ellipseBCenter, 4, WHITE);

            if (ellipsesCollide) DrawText("ELLIPSES COLLIDE", screenWidth/2 - 120, 40, 28, RED);
            else DrawText("NO COLLISION", screenWidth/2 - 80, 40, 28, DARKGRAY);

            DrawText(controlled == 0 ? "Controlling: A" : "Controlling: B", 20, screenHeight - 40, 20, YELLOW);

            if (mouseInA && controlled != 0) DrawText("Mouse inside ellipse A", 20, screenHeight - 70, 20, BLUE);
            if (mouseInB && controlled != 1) DrawText("Mouse inside ellipse B", 20, screenHeight - 70, 20, GREEN);

            DrawText("Press [A] or [B] to switch control", 20, 20, 20, GRAY);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    CloseWindow();

    return 0;
}
