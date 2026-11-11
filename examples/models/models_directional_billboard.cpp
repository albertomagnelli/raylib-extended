/*******************************************************************************************
*
*   raylib [models] example - directional billboard
*
*   Example complexity rating: [★★☆☆] 2/4
*
*   Example originally created with raylib 6.0, last time updated with raylib 6.0
*
*   Example contributed by Robin (@RobinsAviary) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2025 Robin (@RobinsAviary)
*   Killbot art by patvanmackelberg https://opengameart.org/content/killbot-8-directional under CC0
*
********************************************************************************************/

#include "raylib.h"

#include "raymath.h"

#include <stdlib.h>

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr auto screenWidth = 800;
    constexpr auto screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [models] example - directional billboard");

    // Set up the camera
    auto camera = Camera{};
    camera.position = vec3{ 2.0f, 1.0f, 2.0f }; // Starting position
    camera.target = vec3{ 0.0f, 0.5f, 0.0f };   // Target position
    camera.up = vec3{ 0.0f, 1.0f, 0.0f };       // Up vector
    camera.fovy = 45.0f;                        // FOV
    camera.projection = CAMERA_PERSPECTIVE;     // Projection type (Standard 3D perspective)

    // Load billboard texture
    const Texture skillbot = LoadTexture("resources/skillbot.png");

    // Timer to update animation
    auto anim_timer = 0.0f;
    // Animation frame
    u32 anim = 0;

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        UpdateCamera(&camera, CAMERA_ORBITAL);

        // Update timer with delta time
        anim_timer += GetFrameTime();

        // Update frame index after a certain amount of time (half a second)
        if (anim_timer > 0.5f)
        {
            anim_timer = 0.0f;
            anim += 1;
        }

        // Reset frame index to zero on overflow
        if (anim >= 4) anim = 0;

        // Find the current direction frame based on the camera position to the billboard object
        auto dir = static_cast<f32>(floor(((Vector2Angle(vec2{ 2.0f, 0.0f }, vec2{ camera.position.x, camera.position.z })/PI)*4.0f) + 0.25f));

        // Correct frame index if angle is negative
        if (dir < 0.0f)
        {
            dir = 8.0f - static_cast<f32>(abs(static_cast<i32>(dir)));
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

        ClearBackground(RAYWHITE);

        BeginMode3D(camera);

            DrawGrid(10, 1.0f);

            // Draw billboard pointing straight up to the sky, rotated relative to the camera and offset from the bottom
            DrawBillboardPro(camera, skillbot, Rectangle{ 0.0f + (anim*24.0f), 0.0f + (dir*24.0f), 24.0f, 24.0f }, vec3{}, vec3{ 0.0f, 1.0f, 0.0f }, vec2(1.0f), vec2{ 0.5f, 0.0f }, 0, WHITE);

        EndMode3D();

        // Render various variables for reference
        DrawText(TextFormat("animation: %d", anim), 10, 10, 20, DARKGRAY);
        DrawText(TextFormat("direction frame: %.0f", dir), 10, 40, 20, DARKGRAY);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    // Unload billboard texture
    UnloadTexture(skillbot);

    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}