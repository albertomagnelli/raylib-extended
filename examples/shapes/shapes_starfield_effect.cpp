/*******************************************************************************************
*
*   raylib [shapes] example - starfield effect
*
*   Example complexity rating: [★★☆☆] 2/4
*
*   Example originally created with raylib 5.5, last time updated with raylib 6.0
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

#define STAR_COUNT 420

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr i32 screenWidth = 800;
    constexpr i32 screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [shapes] example - starfield effect");

    const Color bgColor = ColorLerp(DARKBLUE, BLACK, 0.69f);

    // Speed at which we fly forward
    f32 speed = 10.0f/9.0f;

    // We're either drawing lines or circles
    bool drawLines = true;

    vec3 stars[STAR_COUNT] = {};
    vec2 starsScreenPos[STAR_COUNT] = {};

    // Setup the stars with a random position
    for (i32 i = 0; i < STAR_COUNT; i++)
    {
        stars[i].x = static_cast<f32>(GetRandomValue(-screenWidth / 2, screenWidth / 2));
        stars[i].y = static_cast<f32>(GetRandomValue(-screenHeight / 2, screenHeight / 2));
        stars[i].z = 1.0f;
    }

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        // Change speed based on mouse
        const f32 mouseMove = GetMouseWheelMove();
        if (static_cast<i32>(mouseMove) != 0) speed += 2.0f*mouseMove/9.0f;
        if (speed < 0.0f) speed = 0.1f;
        else if (speed > 2.0f) speed = 2.0f;

        // Toggle lines / points with space bar
        if (IsKeyPressed(KEY_SPACE)) drawLines = !drawLines;

        const f32 dt = GetFrameTime();
        for (i32 i = 0; i < STAR_COUNT; i++)
        {
            // Update star's timer
            stars[i].z -= dt*speed;

            // Calculate the screen position
            starsScreenPos[i] = vec2{
                screenWidth*0.5f + stars[i].x/stars[i].z,
                screenHeight*0.5f + stars[i].y/stars[i].z,
            };

            // If the star is too old, or offscreen, it dies and we make a new random one
            if ((stars[i].z < 0.0f) || (starsScreenPos[i].x < 0) || (starsScreenPos[i].y < 0.0f) ||
                (starsScreenPos[i].x > screenWidth) || (starsScreenPos[i].y > screenHeight))
            {
                stars[i].x = static_cast<f32>(GetRandomValue(-screenWidth / 2, screenWidth / 2));
                stars[i].y = static_cast<f32>(GetRandomValue(-screenHeight / 2, screenHeight / 2));
                stars[i].z = 1.0f;
            }
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(bgColor);

            for (i32 i = 0; i < STAR_COUNT; i++)
            {
                if (drawLines)
                {
                    // Get the time a little while ago for this star, but clamp it
                    const f32 t = Clamp(stars[i].z + 1.0f/32.0f, 0.0f, 1.0f);

                    // If it's different enough from the current time, we proceed
                    if ((t - stars[i].z) > 1e-3)
                    {
                        // Calculate the screen position of the old point
                        const vec2 startPos = vec2{
                            screenWidth*0.5f + stars[i].x/t,
                            screenHeight*0.5f + stars[i].y/t,
                        };

                        // Draw a line connecting the old point to the current point
                        DrawLineV(startPos, starsScreenPos[i], RAYWHITE);
                    }
                }
                else
                {
                    // Make the radius grow as the star ages
                    const f32 radius = Lerp(stars[i].z, 1.0f, 5.0f);

                    // Draw the circle
                    DrawCircleV(starsScreenPos[i], radius, RAYWHITE);
                }
            }

            DrawText(TextFormat("[MOUSE WHEEL] Current Speed: %.0f", 9.0f*speed/2.0f), 10, 40, 20, RAYWHITE);
            DrawText(TextFormat("[SPACE] Current draw mode: %s", drawLines ? "Lines" : "Circles"), 10, 70, 20, RAYWHITE);

            DrawFPS(10, 10);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();          // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}