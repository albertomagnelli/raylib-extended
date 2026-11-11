/*******************************************************************************************
*
*   raylib [textures] example - screen buffer
*
*   Example complexity rating: [★★☆☆] 2/4
*
*   Example originally created with raylib 5.5, last time updated with raylib 5.5
*
*   Example contributed by Agnis Aldiņš (@nezvers) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2025 Agnis Aldiņš (@nezvers)
*
********************************************************************************************/

#include "raylib.h"

#include <stdlib.h>     // Required for: calloc(), free()

#define MAX_COLORS      256
#define SCALE_FACTOR      2

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr i32 screenWidth = 800;
    constexpr i32 screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [textures] example - screen buffer");

    const i32 imageWidth = screenWidth/SCALE_FACTOR;
    const i32 imageHeight = screenHeight/SCALE_FACTOR;
    const i32 flameWidth = screenWidth/SCALE_FACTOR;

    Color palette[MAX_COLORS] = {};
    u8 *indexBuffer = static_cast<u8 *>(calloc(imageWidth*imageWidth, sizeof(u8)));
    u8 *flameRootBuffer = static_cast<u8 *>(calloc(flameWidth, sizeof(u8)));

    Image screenImage = GenImageColor(imageWidth, imageHeight, BLACK);
    const Texture screenTexture = LoadTextureFromImage(screenImage);

    // Generate flame color palette
    for (i32 i = 0; i < MAX_COLORS; i++)
    {
        const f32 t = static_cast<f32>(i)/static_cast<f32>(MAX_COLORS - 1);
        const f32 hue = t*t;
        const f32 saturation = t;
        const f32 value = t;

        palette[i] = ColorFromHSV(250.0f + 150.0f*hue, saturation, value);
    }

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        // Grow flameRoot
        for (i32 x = 2; x < flameWidth; x++)
        {
            i32 flame = static_cast<i32>(flameRootBuffer[x]);
            flame += GetRandomValue(0, 2);
            flameRootBuffer[x] = (flame > 255)? 255: static_cast<u8>(flame);
        }

        // Transfer flameRoot to indexBuffer
        for (i32 x = 0; x < flameWidth; x++)
        {
            const i32 i = x + (imageHeight - 1)*imageWidth;
            indexBuffer[i] = flameRootBuffer[x];
        }

        // Clear top row, because it can't move any higher
        for (i32 x = 0; x < imageWidth; x++)
        {
            if (indexBuffer[x] != 0) indexBuffer[x] = 0;
        }

        // Skip top row, it is already cleared
        for (i32 y = 1; y < imageHeight; y++)
        {
            for (i32 x = 0; x < imageWidth; x++)
            {
                const u32 i = x + y*imageWidth;
                u8 colorIndex = indexBuffer[i];

                if (colorIndex != 0)
                {
                    // Move pixel a row above
                    indexBuffer[i] = 0;
                    const i32 moveX = GetRandomValue(0, 2) - 1;
                    const i32 newX = x + moveX;

                    if ((newX > 0) && (newX < imageWidth))
                    {
                        const u32 iabove = i - imageWidth + moveX;
                        const i32 decay = GetRandomValue(0, 3);
                        colorIndex -= (decay < colorIndex)? decay : colorIndex;
                        indexBuffer[iabove] = colorIndex;
                    }
                }
            }
        }

        // Update screenImage with palette colors
        for (i32 y = 1; y < imageHeight; y++)
        {
            for (i32 x = 0; x < imageWidth; x++)
            {
                const u32 i = x + y*imageWidth;
                const u8 colorIndex = indexBuffer[i];
                const Color col = palette[colorIndex];

                ImageDrawPixel(&screenImage, x, y, col);
            }
        }

        UpdateTexture(screenTexture, screenImage.data);
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawTextureEx(screenTexture, vec2{ 0, 0 }, 0.0f, 2.0f, WHITE);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    free(indexBuffer);
    free(flameRootBuffer);
    UnloadTexture(screenTexture);
    UnloadImage(screenImage);

    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
