/*******************************************************************************************
*
*   raylib [shapes] example - hilbert curve
*
*   Example complexity rating: [★★★☆] 3/4
*
*   Example originally created with raylib 5.6, last time updated with raylib 5.6
*
*   Example contributed by Hamza RAHAL (@hmz-rhl) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2025 Hamza RAHAL (@hmz-rhl)
*
********************************************************************************************/


#include "raylib.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#include <stdlib.h>     // Required for: calloc(), free()

//------------------------------------------------------------------------------------
// Module Functions Declaration
//------------------------------------------------------------------------------------
static vec2 *LoadHilbertPath(i32 order, f32 size, i32 *strokeCount);
static void UnloadHilbertPath(vec2 *hilbertPath);
static vec2 ComputeHilbertStep(i32 order, i32 index);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr i32 screenWidth = 800;
    constexpr i32 screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [shapes] example - hilbert curve");

    i32 order = 2;
    f32 size = static_cast<f32>(GetScreenHeight());
    i32 strokeCount = 0;
    vec2 *hilbertPath = LoadHilbertPath(order, size, &strokeCount);

    i32 prevOrder = order;
    i32 prevSize = static_cast<i32>(size);       // NOTE: Size from slider is float but for comparison we use int
    i32 counter = 0;
    f32 thick = 2.0f;
    bool animate = true;

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    //--------------------------------------------------------------------------------------
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        // Check if order or size have changed to regenerate
        // NOTE: Size from slider is float but for comparison we use int
        if ((prevOrder != order) || (prevSize != static_cast<i32>(size)))
        {
            UnloadHilbertPath(hilbertPath);
            hilbertPath = LoadHilbertPath(order, size, &strokeCount);

            if (animate) counter = 0;
            else counter = strokeCount;

            prevOrder = order;
            prevSize = static_cast<i32>(size);
        }
        //----------------------------------------------------------------------------------

        // Draw
        //--------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            if (counter < strokeCount)
            {
                // Draw Hilbert path animation, one stroke every frame
                for (i32 i = 1; i <= counter; i++)
                {
                    DrawLineEx(hilbertPath[i], hilbertPath[i - 1], thick, ColorFromHSV((static_cast<f32>(i)/strokeCount)*360.0f, 1.0f, 1.0f));
                }

                counter += 1;
            }
            else
            {
                // Draw full Hilbert path
                for (i32 i = 1; i < strokeCount; i++)
                {
                    DrawLineEx(hilbertPath[i], hilbertPath[i - 1], thick, ColorFromHSV((static_cast<f32>(i)/strokeCount)*360.0f, 1.0f, 1.0f));
                }
            }

            // Draw UI using raygui
            GuiCheckBox(Rectangle{ 450, 50, 20, 20 }, "ANIMATE GENERATION ON CHANGE", &animate);
            GuiSpinner(Rectangle{ 585, 100, 180, 30 }, "HILBERT CURVE ORDER:  ", &order, 2, 8, false);
            GuiSlider(Rectangle{ 524, 150, 240, 24 }, "THICKNESS:  ", nullptr, &thick, 1.0f, 10.0f);
            GuiSlider(Rectangle{ 524, 190, 240, 24 }, "TOTAL SIZE: ", nullptr, &size, 10.0f, GetScreenHeight()*1.5f);

        EndDrawing();
        //--------------------------------------------------------------------------
    }
    //--------------------------------------------------------------------------------------

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadHilbertPath(hilbertPath);

    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------
    return 0;
}

//------------------------------------------------------------------------------------
// Module Functions Definition
//------------------------------------------------------------------------------------
// Load the whole Hilbert Path (including each U and their link)
static vec2 *LoadHilbertPath(i32 order, f32 size, i32 *strokeCount)
{
    const i32 N = 1 << order;
    const f32 len = size/N;
    *strokeCount = N*N;

    vec2 *hilbertPath = static_cast<vec2 *>(calloc(*strokeCount, sizeof(vec2)));

    for (i32 i = 0; i < *strokeCount; i++)
    {
        hilbertPath[i] = ComputeHilbertStep(order, i);
        hilbertPath[i].x = hilbertPath[i].x*len + len/2.0f;
        hilbertPath[i].y = hilbertPath[i].y*len + len/2.0f;
    }

    return hilbertPath;
}

// Unload Hilbert path data
static void UnloadHilbertPath(vec2 *hilbertPath)
{
    free(hilbertPath);
}

// Compute Hilbert path U positions
static vec2 ComputeHilbertStep(i32 order, i32 index)
{
    // Hilbert points base pattern
    static const vec2 hilbertPoints[4] = {
        { 0, 0 },
        { 0, 1 },
        { 1, 1 },
        { 1, 0 },
    };

    i32 hilbertIndex = index&3;
    vec2 vect = hilbertPoints[hilbertIndex];
    f32 temp = 0.0f;
    i32 len = 0;

    for (i32 j = 1; j < order; j++)
    {
        index = index >> 2;
        hilbertIndex = index&3;
        len = 1 << j;

        switch (hilbertIndex)
        {
            case 0:
            {
                temp = vect.x;
                vect.x = vect.y;
                vect.y = temp;
            } break;
            case 2: vect.x += len;
            case 1: vect.y += len; break;
            case 3:
            {
                temp = len - 1 - vect.x;
                vect.x = 2*len - 1 - vect.y;
                vect.y = temp;
            } break;
            default: break;
        }
    }

    return vect;
}
