/*******************************************************************************************
*
*   raylib [core] example - world screen
*
*   Example complexity rating: [★★☆☆] 2/4
*
*   Example originally created with raylib 1.3, last time updated with raylib 1.4
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2015-2025 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr auto screenWidth = 800;
    constexpr auto screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [core] example - world screen");

    // Define the camera to look into our 3d world
    auto camera = Camera{};
    camera.position = vec3{ 10.0f, 10.0f, 10.0f };      // Camera position
    camera.target = vec3{ 0.0f, 0.0f, 0.0f };           // Camera looking at point
    camera.up = vec3{ 0.0f, 1.0f, 0.0f };               // Camera up vector (rotation towards target)
    camera.fovy = 45.0f;                                // Camera field-of-view Y
    camera.projection = CAMERA_PERSPECTIVE;             // Camera projection type

    const auto cubePosition = vec3{ 0.0f, 0.0f, 0.0f };
    auto cubeScreenPosition = vec2{ 0.0f, 0.0f };

    DisableCursor();                    // Limit cursor to relative movement inside the window

    SetTargetFPS(60);                   // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())        // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        UpdateCamera(&camera, CAMERA_THIRD_PERSON);

        // Calculate cube screen space position (with a little offset to be in top)
        cubeScreenPosition = GetWorldToScreen(vec3{cubePosition.x, cubePosition.y + 2.5f, cubePosition.z}, camera);
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            BeginMode3D(camera);

                DrawCube(cubePosition, 2.0f, 2.0f, 2.0f, RED);
                DrawCubeWires(cubePosition, 2.0f, 2.0f, 2.0f, MAROON);

                DrawGrid(10, 1.0f);

            EndMode3D();

            DrawText("Enemy: 100/100", static_cast<i32>(cubeScreenPosition.x) - MeasureText("Enemy: 100/100", 20)/2, static_cast<i32>(cubeScreenPosition.y), 20, BLACK);

            DrawText(TextFormat("Cube position in screen space coordinates: [%i, %i]", static_cast<i32>(cubeScreenPosition.x), static_cast<i32>(cubeScreenPosition.y)), 10, 10, 20, LIME);
            DrawText("Text 2d should be always on top of the cube", 10, 40, 20, GRAY);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
