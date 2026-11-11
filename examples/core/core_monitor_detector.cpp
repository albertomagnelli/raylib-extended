/*******************************************************************************************
*
*   raylib [core] example - monitor detector
*
*   Example complexity rating: [★☆☆☆] 1/4
*
*   Example originally created with raylib 5.5, last time updated with raylib 5.6
*
*   Example contributed by Maicon Santana (@maiconpintoabreu) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2025 Maicon Santana (@maiconpintoabreu)
*
********************************************************************************************/

#include "raylib.h"

#define MAX_MONITORS 10

// Monitor info
typedef struct MonitorInfo {
    vec2 position;
    std::string name;
    i32 width;
    i32 height;
    i32 physicalWidth;
    i32 physicalHeight;
    i32 refreshRate;
} MonitorInfo;

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr auto screenWidth = 800;
    constexpr auto screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [core] example - monitor detector");

    MonitorInfo monitors[MAX_MONITORS] = {};
    auto currentMonitorIndex = GetCurrentMonitor();
    auto monitorCount = 0;

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        // Variables to find the max x and Y to calculate the scale
        auto maxWidth = 1;
        auto maxHeight = 1;

        // Monitor offset is to fix when monitor position x is negative
        auto monitorOffsetX = 0;

        // Rebuild monitors array every frame
        monitorCount = GetMonitorCount();
        for (i32 i = 0; i < monitorCount; i++)
        {
            monitors[i] = MonitorInfo{
                GetMonitorPosition(i),
                GetMonitorName(i),
                GetMonitorWidth(i),
                GetMonitorHeight(i),
                GetMonitorPhysicalWidth(i),
                GetMonitorPhysicalHeight(i),
                GetMonitorRefreshRate(i)
            };

            if (monitors[i].position.x < monitorOffsetX) monitorOffsetX = -static_cast<i32>(monitors[i].position.x);

            const i32 width = static_cast<i32>(monitors[i].position.x) + monitors[i].width;
            const i32 height = static_cast<i32>(monitors[i].position.y) + monitors[i].height;

            if (maxWidth < width) maxWidth = width;
            if (maxHeight < height) maxHeight = height;
        }

        if (IsKeyPressed(KEY_ENTER) && (monitorCount > 1))
        {
            currentMonitorIndex += 1;

            // Set index to 0 if the last one
            if (currentMonitorIndex == monitorCount) currentMonitorIndex = 0;

            SetWindowMonitor(currentMonitorIndex); // Move window to currentMonitorIndex
        }
        else currentMonitorIndex = GetCurrentMonitor(); // Get currentMonitorIndex if manually moved

        auto monitorScale = 0.6f;

        if (maxHeight > (maxWidth + monitorOffsetX)) monitorScale *= (static_cast<f32>(screenHeight)/static_cast<f32>(maxHeight));
        else monitorScale *= (static_cast<f32>(screenWidth)/static_cast<f32>(maxWidth + monitorOffsetX));
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawText("Press [Enter] to move window to next monitor available", 20, 20, 20, DARKGRAY);

            DrawRectangleLines(20, 60, screenWidth - 40, screenHeight - 100, DARKGRAY);

            // Draw Monitor Rectangles with information inside
            for (i32 i = 0; i < monitorCount; i++)
            {
                // Calculate retangle position and size using monitorScale
                const Rectangle rec = Rectangle{
                    (monitors[i].position.x + monitorOffsetX)*monitorScale + 140,
                    monitors[i].position.y*monitorScale + 80,
                    monitors[i].width*monitorScale,
                    monitors[i].height*monitorScale
                };

                // Draw monitor name and information inside the rectangle
                DrawText(TextFormat("[%i] %s", i, monitors[i].name.c_str()), static_cast<i32>(rec.x) + 10, static_cast<i32>(rec.y) + static_cast<i32>(100*monitorScale), static_cast<i32>(120*monitorScale), BLUE);
                DrawText(
                    TextFormat("Resolution: [%ipx x %ipx]\nRefreshRate: [%ihz]\nPhysical Size: [%imm x %imm]\nPosition: %3.0f x %3.0f",
                        monitors[i].width,
                        monitors[i].height,
                        monitors[i].refreshRate,
                        monitors[i].physicalWidth,
                        monitors[i].physicalHeight,
                        monitors[i].position.x,
                        monitors[i].position.y
                    ), static_cast<i32>(rec.x) + 10, static_cast<i32>(rec.y) + static_cast<i32>(200*monitorScale), static_cast<i32>(120*monitorScale), DARKGRAY);

                // Highlight current monitor
                if (i == currentMonitorIndex)
                {
                    DrawRectangleLinesEx(rec, 5, RED);
                    const auto windowPosition = vec2{ (GetWindowPosition().x + monitorOffsetX)*monitorScale  + 140, GetWindowPosition().y*monitorScale + 80 };

                    // Draw window position based on monitors
                    DrawRectangleV(windowPosition, vec2{screenWidth*monitorScale, screenHeight*monitorScale}, Fade(GREEN, 0.5));
                }
                else DrawRectangleLinesEx(rec, 5, GRAY);
            }

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}