/*******************************************************************************************
*
*   raylib [textures] example - image generation
*
*   Example complexity rating: [★★☆☆] 2/4
*
*   Example originally created with raylib 1.8, last time updated with raylib 1.8
*
*   Example contributed by Wilhem Barbier (@nounoursheureux) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2017-2025 Wilhem Barbier (@nounoursheureux) and Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"

#define NUM_TEXTURES  9      // Currently we have 8 generation algorithms but some have multiple purposes (Linear and Square Gradients)

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr auto screenWidth = 800;
    constexpr auto screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [textures] example - image generation");

    const auto verticalGradient = GenImageGradientLinear(screenWidth, screenHeight, 0, RED, BLUE);
    const auto horizontalGradient = GenImageGradientLinear(screenWidth, screenHeight, 90, RED, BLUE);
    const auto diagonalGradient = GenImageGradientLinear(screenWidth, screenHeight, 45, RED, BLUE);
    const auto radialGradient = GenImageGradientRadial(screenWidth, screenHeight, 0.0f, WHITE, BLACK);
    const auto squareGradient = GenImageGradientSquare(screenWidth, screenHeight, 0.0f, WHITE, BLACK);
    const auto checked = GenImageChecked(screenWidth, screenHeight, 32, 32, RED, BLUE);
    const auto whiteNoise = GenImageWhiteNoise(screenWidth, screenHeight, 0.5f);
    const auto perlinNoise = GenImagePerlinNoise(screenWidth, screenHeight, 50, 50, 4.0f);
    const auto cellular = GenImageCellular(screenWidth, screenHeight, 32);

    Texture2D textures[NUM_TEXTURES] = {};

    textures[0] = LoadTextureFromImage(verticalGradient);
    textures[1] = LoadTextureFromImage(horizontalGradient);
    textures[2] = LoadTextureFromImage(diagonalGradient);
    textures[3] = LoadTextureFromImage(radialGradient);
    textures[4] = LoadTextureFromImage(squareGradient);
    textures[5] = LoadTextureFromImage(checked);
    textures[6] = LoadTextureFromImage(whiteNoise);
    textures[7] = LoadTextureFromImage(perlinNoise);
    textures[8] = LoadTextureFromImage(cellular);

    // Unload image data (CPU RAM)
    UnloadImage(verticalGradient);
    UnloadImage(horizontalGradient);
    UnloadImage(diagonalGradient);
    UnloadImage(radialGradient);
    UnloadImage(squareGradient);
    UnloadImage(checked);
    UnloadImage(whiteNoise);
    UnloadImage(perlinNoise);
    UnloadImage(cellular);

    auto currentTexture = 0;

    SetTargetFPS(60);
    //---------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())
    {
        // Update
        //----------------------------------------------------------------------------------
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsKeyPressed(KEY_RIGHT))
        {
            currentTexture = (currentTexture + 1)%NUM_TEXTURES; // Cycle between the textures
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawTexture(textures[currentTexture], 0, 0, WHITE);

            DrawRectangle(30, 400, 325, 30, Fade(SKYBLUE, 0.5f));
            DrawRectangleLines(30, 400, 325, 30, Fade(WHITE, 0.5f));
            DrawText("MOUSE LEFT BUTTON to CYCLE PROCEDURAL TEXTURES", 40, 410, 10, WHITE);

            switch (currentTexture)
            {
                case 0: DrawText("VERTICAL GRADIENT", 560, 10, 20, RAYWHITE); break;
                case 1: DrawText("HORIZONTAL GRADIENT", 540, 10, 20, RAYWHITE); break;
                case 2: DrawText("DIAGONAL GRADIENT", 540, 10, 20, RAYWHITE); break;
                case 3: DrawText("RADIAL GRADIENT", 580, 10, 20, LIGHTGRAY); break;
                case 4: DrawText("SQUARE GRADIENT", 580, 10, 20, LIGHTGRAY); break;
                case 5: DrawText("CHECKED", 680, 10, 20, RAYWHITE); break;
                case 6: DrawText("WHITE NOISE", 640, 10, 20, RED); break;
                case 7: DrawText("PERLIN NOISE", 640, 10, 20, RED); break;
                case 8: DrawText("CELLULAR", 670, 10, 20, RAYWHITE); break;
                default: break;
            }

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------

    // Unload textures data (GPU VRAM)
    for (i32 i = 0; i < NUM_TEXTURES; i++) UnloadTexture(textures[i]);

    CloseWindow();                // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
