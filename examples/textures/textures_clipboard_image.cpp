/*******************************************************************************************
*
*   raylib [textures] example - clipboard image
*
*   Example complexity rating: [★☆☆☆] 1/4
*
*   Example originally created with raylib 6.0, last time updated with raylib 6.0
*
*   Example contributed by Maicon Santana (@maiconpintoabreu) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2026 Maicon Santana (@maiconpintoabreu)
*
********************************************************************************************/

#include "raylib.h"

#define MAX_TEXTURE_COLLECTION  20

typedef struct TextureCollection {
    Texture2D texture;
    vec2 position;
} TextureCollection;

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr i32 screenWidth = 800;
    constexpr i32 screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [textures] example - clipboard image");

    TextureCollection collection[MAX_TEXTURE_COLLECTION] = {};
    i32 currentCollectionIndex = 0;

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        if (IsKeyPressed(KEY_R))    // Reset image collection
        {
            // Unload textures to avoid memory leaks
            for (i32 i = 0; i < currentCollectionIndex; i++) UnloadTexture(collection[i].texture);

            currentCollectionIndex = 0;
        }

        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_V) &&
            (currentCollectionIndex < MAX_TEXTURE_COLLECTION))
        {
            const Image image = GetClipboardImage();

            if (IsImageValid(image))
            {
                collection[currentCollectionIndex].texture = LoadTextureFromImage(image);
                collection[currentCollectionIndex].position = GetMousePosition();
                currentCollectionIndex++;
                UnloadImage(image);
            }
            else TraceLog(LOG_INFO, "IMAGE: Could not retrieve image from clipboard");
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            for (i32 i = 0; i < currentCollectionIndex; i++)
            {
                if (IsTextureValid(collection[i].texture))
                {
                    DrawTexturePro(collection[i].texture,
                        Rectangle{0,0,static_cast<f32>(collection[i].texture.width), static_cast<f32>(collection[i].texture.height)},
                        Rectangle{collection[i].position.x,collection[i].position.y, static_cast<f32>(collection[i].texture.width), static_cast<f32>(collection[i].texture.height)},
                        vec2{collection[i].texture.width*0.5f, collection[i].texture.height*0.5f},
                        0.0f, WHITE);
                }
            }

            DrawRectangle(0, 0, screenWidth, 40, BLACK);
            DrawText("Clipboard Image - Ctrl+V to Paste and R to Reset ", 120, 10, 20, LIGHTGRAY);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    for (i32 i = 0; i < currentCollectionIndex; i++)
    {
        UnloadTexture(collection[i].texture);
    }

    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
