/*******************************************************************************************
*
*   raylib [textures] example - sprite explosion
*
*   Example complexity rating: [★★☆☆] 2/4
*
*   Example originally created with raylib 2.5, last time updated with raylib 3.5
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2019-2025 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"

#define NUM_FRAMES_PER_LINE     5
#define NUM_LINES               5

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr auto screenWidth = 800;
    constexpr auto screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [textures] example - sprite explosion");

    InitAudioDevice();

    // Load explosion sound
    const Sound fxBoom = LoadSound("resources/boom.wav");

    // Load explosion texture
    const Texture2D explosion = LoadTexture("resources/explosion.png");

    // Init variables for animation
    const f32 frameWidth = static_cast<f32>(explosion.width)/NUM_FRAMES_PER_LINE;   // Sprite one frame rectangle width
    const f32 frameHeight = static_cast<f32>(explosion.height)/NUM_LINES;           // Sprite one frame rectangle height
    auto currentFrame = 0;
    auto currentLine = 0;

    auto frameRec = Rectangle{ 0, 0, frameWidth, frameHeight };
    auto position = vec2{ 0.0f, 0.0f };

    auto active = false;
    auto framesCounter = 0;

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //---------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------

        // Check for mouse button pressed and activate explosion (if not active)
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !active)
        {
            position = GetMousePosition();
            active = true;

            position.x -= frameWidth/2.0f;
            position.y -= frameHeight/2.0f;

            PlaySound(fxBoom);
        }

        // Compute explosion animation frames
        if (active)
        {
            framesCounter++;

            if (framesCounter > 2)
            {
                currentFrame++;

                if (currentFrame >= NUM_FRAMES_PER_LINE)
                {
                    currentFrame = 0;
                    currentLine++;

                    if (currentLine >= NUM_LINES)
                    {
                        currentLine = 0;
                        active = false;
                    }
                }

                framesCounter = 0;
            }
        }

        frameRec.x = frameWidth*currentFrame;
        frameRec.y = frameHeight*currentLine;
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            // Draw explosion required frame rectangle
            if (active) DrawTextureRec(explosion, frameRec, position, WHITE);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadTexture(explosion);   // Unload texture
    UnloadSound(fxBoom);        // Unload sound

    CloseAudioDevice();

    CloseWindow();              // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}