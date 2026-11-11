/*******************************************************************************************
*
*   raylib [textures] example - image channel
*
*   NOTE: Images are loaded in CPU memory (RAM); textures are loaded in GPU memory (VRAM)
*
*   Example complexity rating: [★★☆☆] 2/4
*
*   Example originally created with raylib 5.5, last time updated with raylib 5.5
*
*   Example contributed by Bruno Cabral (@brccabral) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2024-2025 Bruno Cabral (@brccabral) and Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr int screenWidth = 800;
    constexpr int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [textures] example - image channel");

    const Image fudesumiImage = LoadImage("resources/fudesumi.png");

    Image imageAlpha = ImageFromChannel(fudesumiImage, 3);
    ImageAlphaMask(&imageAlpha, imageAlpha);

    Image imageRed = ImageFromChannel(fudesumiImage, 0);
    ImageAlphaMask(&imageRed, imageAlpha);

    Image imageGreen = ImageFromChannel(fudesumiImage, 1);
    ImageAlphaMask(&imageGreen, imageAlpha);

    Image imageBlue = ImageFromChannel(fudesumiImage, 2);
    ImageAlphaMask(&imageBlue, imageAlpha);

    const Image backgroundImage = GenImageChecked(screenWidth, screenHeight, screenWidth/20, screenHeight/20, ORANGE, YELLOW);

    const Texture2D fudesumiTexture = LoadTextureFromImage(fudesumiImage);
    const Texture2D textureAlpha = LoadTextureFromImage(imageAlpha);
    const Texture2D textureRed = LoadTextureFromImage(imageRed);
    const Texture2D textureGreen = LoadTextureFromImage(imageGreen);
    const Texture2D textureBlue = LoadTextureFromImage(imageBlue);
    const Texture2D backgroundTexture = LoadTextureFromImage(backgroundImage);

    UnloadImage(fudesumiImage);
    UnloadImage(imageAlpha);
    UnloadImage(imageRed);
    UnloadImage(imageGreen);
    UnloadImage(imageBlue);
    UnloadImage(backgroundImage);

    const Rectangle fudesumiRec = {0, 0, static_cast<float>(fudesumiImage.width), static_cast<float>(fudesumiImage.height)};

    const Rectangle fudesumiPos = {50, 10, fudesumiImage.width*0.8f, fudesumiImage.height*0.8f};
    const Rectangle redPos = { 410, 10, fudesumiPos.width/2.0f, fudesumiPos.height/2.0f };
    const Rectangle greenPos = { 600, 10, fudesumiPos.width/2.0f, fudesumiPos.height/2.0f };
    const Rectangle bluePos = { 410, 230, fudesumiPos.width/2.0f, fudesumiPos.height/2.0f };
    const Rectangle alphaPos = { 600, 230, fudesumiPos.width/2.0f, fudesumiPos.height/2.0f };

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        // Nothing to update...
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            DrawTexture(backgroundTexture, 0, 0, WHITE);
            DrawTexturePro(fudesumiTexture, fudesumiRec, fudesumiPos, vec2{0, 0}, 0, WHITE);

            DrawTexturePro(textureRed, fudesumiRec, redPos, vec2{0, 0}, 0, RED);
            DrawTexturePro(textureGreen, fudesumiRec, greenPos, vec2{0, 0}, 0, GREEN);
            DrawTexturePro(textureBlue, fudesumiRec, bluePos, vec2{0, 0}, 0, BLUE);
            DrawTexturePro(textureAlpha, fudesumiRec, alphaPos, vec2{0, 0}, 0, WHITE);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadTexture(backgroundTexture);
    UnloadTexture(fudesumiTexture);
    UnloadTexture(textureRed);
    UnloadTexture(textureGreen);
    UnloadTexture(textureBlue);
    UnloadTexture(textureAlpha);

    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
