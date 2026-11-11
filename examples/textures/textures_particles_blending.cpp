/*******************************************************************************************
*
*   raylib [textures] example - particles blending
*
*   Example complexity rating: [★☆☆☆] 1/4
*
*   Example originally created with raylib 1.7, last time updated with raylib 3.5
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2017-2025 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"

#define MAX_PARTICLES 200

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// Particle structure
typedef struct {
    vec2 position;
    Color color;
    f32 alpha;
    f32 size;
    f32 rotation;
    bool active;        // NOTE: Use it to activate/deactive particle
} Particle;

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr i32 screenWidth = 800;
    constexpr i32 screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [textures] example - particles blending");

    // Particles pool, reuse them!
    Particle mouseTail[MAX_PARTICLES] = {};

    // Initialize particles
    for (i32 i = 0; i < MAX_PARTICLES; i++)
    {
        mouseTail[i].position = vec2{ 0, 0 };
        mouseTail[i].color = Color{ static_cast<u8>(GetRandomValue(0, 255)), static_cast<u8>(GetRandomValue(0, 255)), static_cast<u8>(GetRandomValue(0, 255)), 255 };
        mouseTail[i].alpha = 1.0f;
        mouseTail[i].size = static_cast<f32>(GetRandomValue(1, 30))/20.0f;
        mouseTail[i].rotation = static_cast<f32>(GetRandomValue(0, 360));
        mouseTail[i].active = false;
    }

    constexpr f32 gravity = 3.0f;

    const Texture2D smoke = LoadTexture("resources/spark_flame.png");

    i32 blending = BLEND_ALPHA;

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------

        // Activate one particle every frame and Update active particles
        // NOTE: Particles initial position should be mouse position when activated
        // NOTE: Particles fall down with gravity and rotation... and disappear after 2 seconds (alpha = 0)
        // NOTE: When a particle disappears, active = false and it can be reused
        for (i32 i = 0; i < MAX_PARTICLES; i++)
        {
            if (!mouseTail[i].active)
            {
                mouseTail[i].active = true;
                mouseTail[i].alpha = 1.0f;
                mouseTail[i].position = GetMousePosition();
                i = MAX_PARTICLES;
            }
        }

        for (i32 i = 0; i < MAX_PARTICLES; i++)
        {
            if (mouseTail[i].active)
            {
                mouseTail[i].position.y += gravity/2;
                mouseTail[i].alpha -= 0.005f;

                if (mouseTail[i].alpha <= 0.0f) mouseTail[i].active = false;

                mouseTail[i].rotation += 2.0f;
            }
        }

        if (IsKeyPressed(KEY_SPACE))
        {
            if (blending == BLEND_ALPHA) blending = BLEND_ADDITIVE;
            else blending = BLEND_ALPHA;
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(DARKGRAY);

            BeginBlendMode(blending);

                // Draw active particles
                for (i32 i = 0; i < MAX_PARTICLES; i++)
                {
                    if (mouseTail[i].active) DrawTexturePro(smoke, Rectangle{ 0.0f, 0.0f, static_cast<f32>(smoke.width), static_cast<f32>(smoke.height) },
                                                           Rectangle{ mouseTail[i].position.x, mouseTail[i].position.y, smoke.width*mouseTail[i].size, smoke.height*mouseTail[i].size },
                                                           vec2{ (smoke.width*mouseTail[i].size/2.0f), (smoke.height*mouseTail[i].size/2.0f) }, mouseTail[i].rotation,
                                                           Fade(mouseTail[i].color, mouseTail[i].alpha));
                }

            EndBlendMode();

            DrawText("PRESS SPACE to CHANGE BLENDING MODE", 180, 20, 20, BLACK);

            if (blending == BLEND_ALPHA) DrawText("ALPHA BLENDING", 290, screenHeight - 40, 20, BLACK);
            else DrawText("ADDITIVE BLENDING", 280, screenHeight - 40, 20, RAYWHITE);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadTexture(smoke);

    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}