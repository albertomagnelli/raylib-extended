/*******************************************************************************************
*
*   raylib [core] example - highdpi demo
*
*   Example complexity rating: [★★☆☆] 2/4
*
*   Example originally created with raylib 5.0, last time updated with raylib 5.5
*
*   Example contributed by Jonathan Marler (@marler8997) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2025 Jonathan Marler (@marler8997)
*
********************************************************************************************/

#include "raylib.h"

//------------------------------------------------------------------------------------
// Module Functions Declaration
//------------------------------------------------------------------------------------
static void DrawTextCenter(std::string_view text, i32 x, i32 y, i32 fontSize, Color color);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr i32 screenWidth = 800;
    constexpr i32 screenHeight = 450;

    SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, "raylib [core] example - highdpi demo");
    SetWindowMinSize(450, 450);

    constexpr i32 logicalGridDescY = 120;
    const i32 logicalGridLabelY = logicalGridDescY + 30;
    const i32 logicalGridTop = logicalGridLabelY + 30;
    const i32 logicalGridBottom = logicalGridTop + 80;
    const i32 pixelGridTop = logicalGridBottom - 20;
    const i32 pixelGridBottom = pixelGridTop + 80;
    const i32 pixelGridLabelY = pixelGridBottom + 30;
    const i32 pixelGridDescY = pixelGridLabelY + 30;
    constexpr i32 cellSize = 50;
    f32 cellSizePx = static_cast<f32>(cellSize);

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        const i32 monitorCount = GetMonitorCount();

        if ((monitorCount > 1) && IsKeyPressed(KEY_N))
        {
            SetWindowMonitor((GetCurrentMonitor() + 1)%monitorCount);
        }

        const i32 currentMonitor = GetCurrentMonitor();
        const vec2 dpiScale = GetWindowScaleDPI();
        cellSizePx = (static_cast<f32>(cellSize))/dpiScale.x;
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            const i32 windowCenter = GetScreenWidth()/2;
            DrawTextCenter(TextFormat("Dpi Scale: %f", dpiScale.x), windowCenter, 30, 40, DARKGRAY);
            DrawTextCenter(TextFormat("Monitor: %d/%d ([N] next monitor)", currentMonitor+1, monitorCount), windowCenter, 70, 20, LIGHTGRAY);
            DrawTextCenter(TextFormat("Window is %d \"logical points\" wide", GetScreenWidth()), windowCenter, logicalGridDescY, 20, ORANGE);

            bool odd = true;
            for (i32 i = cellSize; i < GetScreenWidth(); i += cellSize, odd = !odd)
            {
                if (odd) DrawRectangle(i, logicalGridTop, cellSize, logicalGridBottom-logicalGridTop, ORANGE);

                DrawTextCenter(TextFormat("%d", i), i, logicalGridLabelY, 10, LIGHTGRAY);
                DrawLine(i, logicalGridLabelY + 10, i, logicalGridBottom, GRAY);
            }

            odd = true;
            constexpr i32 minTextSpace = 30;
            i32 lastTextX = -minTextSpace;
            for (i32 i = cellSize; i < GetRenderWidth(); i += cellSize, odd = !odd)
            {
                const i32 x = static_cast<i32>((static_cast<f32>(i))/dpiScale.x);
                if (odd) DrawRectangle(x, pixelGridTop, static_cast<i32>(cellSizePx), pixelGridBottom - pixelGridTop, CLITERAL(Color){ 0, 121, 241, 100 });

                DrawLine(x, pixelGridTop, static_cast<i32>((static_cast<f32>(i))/dpiScale.x), pixelGridLabelY - 10, GRAY);

                if ((x - lastTextX) >= minTextSpace)
                {
                    DrawTextCenter(TextFormat("%d", i), x, pixelGridLabelY, 10, LIGHTGRAY);
                    lastTextX = x;
                }
            }

            DrawTextCenter(TextFormat("Window is %d \"physical pixels\" wide", GetRenderWidth()), windowCenter, pixelGridDescY, 20, BLUE);

            constexpr const char *text = "Can you see this?";
            const vec2 size = MeasureTextEx(GetFontDefault(), text, 20, 3);
            const vec2 pos = vec2{ GetScreenWidth() - size.x - 5, GetScreenHeight() - size.y - 5 };
            DrawTextEx(GetFontDefault(), text, pos, 20, 3, LIGHTGRAY);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//------------------------------------------------------------------------------------
// Module Functions Definition
//------------------------------------------------------------------------------------
static void DrawTextCenter(std::string_view text, i32 x, i32 y, i32 fontSize, Color color)
{
    const vec2 size = MeasureTextEx(GetFontDefault(), text, static_cast<f32>(fontSize), 3);
    const vec2 pos = vec2{ x - size.x/2, y - size.y/2 };
    DrawTextEx(GetFontDefault(), text, pos, static_cast<f32>(fontSize), 3, color);
}
