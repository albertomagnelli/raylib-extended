/*******************************************************************************************
*
*   raylib [core] example - random sequence
*
*   Example complexity rating: [★☆☆☆] 1/4
*
*   Example originally created with raylib 5.0, last time updated with raylib 5.0
*
*   Example contributed by Dalton Overmyer (@REDl3east) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2023-2025 Dalton Overmyer (@REDl3east)
*
********************************************************************************************/

#include "raylib.h"

#include "raymath.h"

#include <stdlib.h>     // Required for: malloc(), free()

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct ColorRect {
    Color color;
    Rectangle rect;
} ColorRect;

//------------------------------------------------------------------------------------
// Module Functions Declaration
//------------------------------------------------------------------------------------
static Color GenerateRandomColor(void);
static ColorRect *GenerateRandomColorRectSequence(f32 rectCount, f32 rectWidth, f32 screenWidth, f32 screenHeight);
static void ShuffleColorRectSequence(ColorRect *rectangles, i32 rectCount);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr i32 screenWidth = 800;
    constexpr i32 screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [core] example - random sequence");

    i32 rectCount = 20;
    f32 rectSize = static_cast<f32>(screenWidth)/rectCount;
    ColorRect *rectangles = GenerateRandomColorRectSequence(static_cast<f32>(rectCount), rectSize, static_cast<f32>(screenWidth), 0.75f*screenHeight);

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose()) // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        if (IsKeyPressed(KEY_SPACE)) ShuffleColorRectSequence(rectangles, rectCount);

        if (IsKeyPressed(KEY_UP))
        {
            rectCount++;
            rectSize = static_cast<f32>(screenWidth)/rectCount;
            free(rectangles);

            // Re-generate random sequence with new count
            rectangles = GenerateRandomColorRectSequence(static_cast<f32>(rectCount), rectSize, static_cast<f32>(screenWidth), 0.75f*screenHeight);
        }

        if (IsKeyPressed(KEY_DOWN))
        {
            if (rectCount >= 4)
            {
                rectCount--;
                rectSize = static_cast<f32>(screenWidth)/rectCount;
                free(rectangles);

                // Re-generate random sequence with new count
                rectangles = GenerateRandomColorRectSequence(static_cast<f32>(rectCount), rectSize, static_cast<f32>(screenWidth), 0.75f*screenHeight);
            }
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            for (i32 i = 0; i < rectCount; i++)
            {
                DrawRectangleRec(rectangles[i].rect, rectangles[i].color);

                DrawText("Press SPACE to shuffle the current sequence", 10, screenHeight - 96, 20, BLACK);
                DrawText("Press UP to add a rectangle and generate a new sequence", 10, screenHeight - 64, 20, BLACK);
                DrawText("Press DOWN to remove a rectangle and generate a new sequence", 10, screenHeight - 32, 20, BLACK);
            }

            DrawText(TextFormat("Count: %d rectangles", rectCount), 10, 10, 20, MAROON);

            DrawFPS(screenWidth - 80, 10);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    free(rectangles);
    CloseWindow(); // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//------------------------------------------------------------------------------------
// Module Functions Definition
//------------------------------------------------------------------------------------
static Color GenerateRandomColor(void)
{
    Color color = {
        static_cast<u8>(GetRandomValue(0, 255)),
        static_cast<u8>(GetRandomValue(0, 255)),
        static_cast<u8>(GetRandomValue(0, 255)),
        255
    };

    return color;
}

static ColorRect *GenerateRandomColorRectSequence(f32 rectCount, f32 rectWidth, f32 screenWidth, f32 screenHeight)
{
    ColorRect *rectangles = static_cast<ColorRect *>(calloc(static_cast<i32>(rectCount), sizeof(ColorRect)));

    i32 *seq = LoadRandomSequence(static_cast<u32>(rectCount), 0, static_cast<u32>(rectCount) - 1);
    const f32 rectSeqWidth = rectCount*rectWidth;
    const f32 startX = (screenWidth - rectSeqWidth)*0.5f;

    for (i32 i = 0; i < rectCount; i++)
    {
        const i32 rectHeight = static_cast<i32>(Remap(static_cast<f32>(seq[i]), 0, rectCount - 1, 0, screenHeight));

        rectangles[i].color = GenerateRandomColor();
        rectangles[i].rect = CLITERAL(Rectangle){ startX + i*rectWidth, screenHeight - rectHeight, rectWidth, static_cast<f32>(rectHeight) };
    }

    UnloadRandomSequence(seq);

    return rectangles;
}

static void ShuffleColorRectSequence(ColorRect *rectangles, i32 rectCount)
{
    i32 *seq = LoadRandomSequence(rectCount, 0, rectCount -  1);

    for (i32 i1 = 0; i1 < rectCount; i1++)
    {
        ColorRect *r1 = &rectangles[i1];
        ColorRect *r2 = &rectangles[seq[i1]];

        // Swap only the color and height
        const ColorRect tmp = *r1;
        r1->color = r2->color;
        r1->rect.height = r2->rect.height;
        r1->rect.y = r2->rect.y;
        r2->color = tmp.color;
        r2->rect.height = tmp.rect.height;
        r2->rect.y = tmp.rect.y;
    }

    UnloadRandomSequence(seq);
}
