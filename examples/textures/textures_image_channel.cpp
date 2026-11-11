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
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr auto screenWidth = 800;
    constexpr auto screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [textures] example - image channel");

    const Image fudesumiImage = LoadImage("resources/fudesumi.png");

    auto imageAlpha = ImageFromChannel(fudesumiImage, 3);
    ImageAlphaMask(&imageAlpha, imageAlpha);

    auto imageRed = ImageFromChannel(fudesumiImage, 0);
    ImageAlphaMask(&imageRed, imageAlpha);

    auto imageGreen = ImageFromChannel(fudesumiImage, 1);
    ImageAlphaMask(&imageGreen, imageAlpha);

    auto imageBlue = ImageFromChannel(fudesumiImage, 2);
    ImageAlphaMask(&imageBlue, imageAlpha);

    const auto backgroundImage = GenImageChecked(screenWidth, screenHeight, screenWidth/20, screenHeight/20, ORANGE, YELLOW);

    const auto fudesumiTexture = LoadTextureFromImage(fudesumiImage);
    const auto textureAlpha = LoadTextureFromImage(imageAlpha);
    const auto textureRed = LoadTextureFromImage(imageRed);
    const auto textureGreen = LoadTextureFromImage(imageGreen);
    const auto textureBlue = LoadTextureFromImage(imageBlue);
    const auto backgroundTexture = LoadTextureFromImage(backgroundImage);

    UnloadImage(fudesumiImage);
    UnloadImage(imageAlpha);
    UnloadImage(imageRed);
    UnloadImage(imageGreen);
    UnloadImage(imageBlue);
    UnloadImage(backgroundImage);

    const auto fudesumiRec = Rectangle{0, 0, static_cast<f32>(fudesumiImage.width), static_cast<f32>(fudesumiImage.height)};

    const auto fudesumiPos = Rectangle{50, 10, fudesumiImage.width*0.8f, fudesumiImage.height*0.8f};
    const auto redPos = Rectangle{ 410, 10, fudesumiPos.width/2.0f, fudesumiPos.height/2.0f };
    const auto greenPos = Rectangle{ 600, 10, fudesumiPos.width/2.0f, fudesumiPos.height/2.0f };
    const auto bluePos = Rectangle{ 410, 230, fudesumiPos.width/2.0f, fudesumiPos.height/2.0f };
    const auto alphaPos = Rectangle{ 600, 230, fudesumiPos.width/2.0f, fudesumiPos.height/2.0f };

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
