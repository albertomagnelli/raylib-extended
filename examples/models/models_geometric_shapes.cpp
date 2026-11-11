/*******************************************************************************************
*
*   raylib [models] example - geometric shapes
*
*   Example complexity rating: [★☆☆☆] 1/4
*
*   Example originally created with raylib 1.0, last time updated with raylib 3.5
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2014-2025 Ramon Santamaria (@raysan5)
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

    InitWindow(screenWidth, screenHeight, "raylib [models] example - geometric shapes");

    // Define the camera to look into our 3d world
    auto camera = Camera{};
    camera.position = vec3{ 0.0f, 10.0f, 10.0f };
    camera.target = vec3{ 0.0f, 0.0f, 0.0f };
    camera.up = vec3{ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        // TODO: Update your variables here
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            BeginMode3D(camera);

                DrawCube(vec3{-4.0f, 0.0f, 2.0f}, 2.0f, 5.0f, 2.0f, RED);
                DrawCubeWires(vec3{-4.0f, 0.0f, 2.0f}, 2.0f, 5.0f, 2.0f, GOLD);
                DrawCubeWires(vec3{-4.0f, 0.0f, -2.0f}, 3.0f, 6.0f, 2.0f, MAROON);

                DrawSphere(vec3{-1.0f, 0.0f, -2.0f}, 1.0f, GREEN);
                DrawSphereWires(vec3{1.0f, 0.0f, 2.0f}, 2.0f, 16, 16, LIME);

                DrawCylinder(vec3{4.0f, 0.0f, -2.0f}, 1.0f, 2.0f, 3.0f, 4, SKYBLUE);
                DrawCylinderWires(vec3{4.0f, 0.0f, -2.0f}, 1.0f, 2.0f, 3.0f, 4, DARKBLUE);
                DrawCylinderWires(vec3{4.5f, -1.0f, 2.0f}, 1.0f, 1.0f, 2.0f, 6, BROWN);

                DrawCylinder(vec3{1.0f, 0.0f, -4.0f}, 0.0f, 1.5f, 3.0f, 8, GOLD);
                DrawCylinderWires(vec3{1.0f, 0.0f, -4.0f}, 0.0f, 1.5f, 3.0f, 8, PINK);

                DrawCapsule     (vec3{-3.0f, 1.5f, -4.0f}, vec3{-4.0f, -1.0f, -4.0f}, 1.2f, 8, 8, VIOLET);
                DrawCapsuleWires(vec3{-3.0f, 1.5f, -4.0f}, vec3{-4.0f, -1.0f, -4.0f}, 1.2f, 8, 8, PURPLE);

                DrawGrid(10, 1.0f);        // Draw a grid

            EndMode3D();

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