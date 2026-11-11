/*******************************************************************************************
*
*   raylib [shapes] example - easings testbed
*
*   Example complexity rating: [★★★☆] 3/4
*
*   Example originally created with raylib 2.5, last time updated with raylib 2.5
*
*   Example contributed by Juan Miguel López (@flashback-fx) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2019-2025 Juan Miguel López (@flashback-fx) and Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"

#include "reasings.h"       // Required for: easing functions

#define FONT_SIZE         20

#define D_STEP         20.0f
#define D_STEP_FINE     2.0f
#define D_MIN           1.0f
#define D_MAX       10000.0f

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// Easing types
enum EasingTypes {
    EASE_LINEAR_NONE = 0,
    EASE_LINEAR_IN,
    EASE_LINEAR_OUT,
    EASE_LINEAR_IN_OUT,
    EASE_SINE_IN,
    EASE_SINE_OUT,
    EASE_SINE_IN_OUT,
    EASE_CIRC_IN,
    EASE_CIRC_OUT,
    EASE_CIRC_IN_OUT,
    EASE_CUBIC_IN,
    EASE_CUBIC_OUT,
    EASE_CUBIC_IN_OUT,
    EASE_QUAD_IN,
    EASE_QUAD_OUT,
    EASE_QUAD_IN_OUT,
    EASE_EXPO_IN,
    EASE_EXPO_OUT,
    EASE_EXPO_IN_OUT,
    EASE_BACK_IN,
    EASE_BACK_OUT,
    EASE_BACK_IN_OUT,
    EASE_BOUNCE_OUT,
    EASE_BOUNCE_IN,
    EASE_BOUNCE_IN_OUT,
    EASE_ELASTIC_IN,
    EASE_ELASTIC_OUT,
    EASE_ELASTIC_IN_OUT,
    NUM_EASING_TYPES,
    EASING_NONE = NUM_EASING_TYPES
};

typedef struct EasingFuncs {
    const char *name;
    f32 (*func)(f32, f32, f32, f32);
} EasingFuncs;

//------------------------------------------------------------------------------------
// Module Functions Declaration
//------------------------------------------------------------------------------------
// Function used when "no easing" is selected for any axis
static f32 NoEase(f32 t, f32 b, f32 c, f32 d);

//------------------------------------------------------------------------------------
// Global Variables Definition
//------------------------------------------------------------------------------------
// Easing functions reference data
static const EasingFuncs easings[] = {
    { "EaseLinearNone", EaseLinearNone },
    { "EaseLinearIn", EaseLinearIn },
    { "EaseLinearOut", EaseLinearOut },
    { "EaseLinearInOut", EaseLinearInOut },
    { "EaseSineIn", EaseSineIn },
    { "EaseSineOut", EaseSineOut },
    { "EaseSineInOut", EaseSineInOut },
    { "EaseCircIn", EaseCircIn },
    { "EaseCircOut", EaseCircOut },
    { "EaseCircInOut", EaseCircInOut },
    { "EaseCubicIn", EaseCubicIn },
    { "EaseCubicOut", EaseCubicOut },
    { "EaseCubicInOut", EaseCubicInOut },
    { "EaseQuadIn", EaseQuadIn },
    { "EaseQuadOut", EaseQuadOut },
    { "EaseQuadInOut", EaseQuadInOut },
    { "EaseExpoIn", EaseExpoIn },
    { "EaseExpoOut", EaseExpoOut },
    { "EaseExpoInOut", EaseExpoInOut },
    { "EaseBackIn", EaseBackIn },
    { "EaseBackOut", EaseBackOut },
    { "EaseBackInOut", EaseBackInOut },
    { "EaseBounceOut", EaseBounceOut },
    { "EaseBounceIn", EaseBounceIn },
    { "EaseBounceInOut", EaseBounceInOut },
    { "EaseElasticIn", EaseElasticIn },
    { "EaseElasticOut", EaseElasticOut },
    { "EaseElasticInOut", EaseElasticInOut },
    { "None", NoEase },
};

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr i32 screenWidth = 800;
    constexpr i32 screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [shapes] example - easings testbed");

    vec2 ballPosition = { 100.0f, 100.0f };

    f32 t = 0.0f;             // Current time (in any unit measure, but same unit as duration)
    f32 d = 300.0f;           // Total time it should take to complete (duration)
    bool paused = true;
    bool boundedT = true;       // If true, t will stop when d >= td, otherwise t will keep adding td to its value every loop

    i32 easingX = EASING_NONE;  // Easing selected for x axis
    i32 easingY = EASING_NONE;  // Easing selected for y axis

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        if (IsKeyPressed(KEY_T)) boundedT = !boundedT;

        // Choose easing for the X axis
        if (IsKeyPressed(KEY_RIGHT))
        {
            easingX++;

            if (easingX > EASING_NONE) easingX = 0;
        }
        else if (IsKeyPressed(KEY_LEFT))
        {
            if (easingX == 0) easingX = EASING_NONE;
            else easingX--;
        }

        // Choose easing for the Y axis
        if (IsKeyPressed(KEY_DOWN))
        {
            easingY++;

            if (easingY > EASING_NONE) easingY = 0;
        }
        else if (IsKeyPressed(KEY_UP))
        {
            if (easingY == 0) easingY = EASING_NONE;
            else easingY--;
        }

        // Change d (duration) value
        if (IsKeyPressed(KEY_W) && (d < D_MAX - D_STEP)) d += D_STEP;
        else if (IsKeyPressed(KEY_Q) && (d > D_MIN + D_STEP)) d -= D_STEP;

        if (IsKeyDown(KEY_S) && (d < D_MAX - D_STEP_FINE)) d += D_STEP_FINE;
        else if (IsKeyDown(KEY_A) && (d > D_MIN + D_STEP_FINE)) d -= D_STEP_FINE;

        // Play, pause and restart controls
        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_T) ||
            IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_LEFT) ||
            IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_UP) ||
            IsKeyPressed(KEY_W) || IsKeyPressed(KEY_Q) ||
            IsKeyDown(KEY_S)  || IsKeyDown(KEY_A) ||
            (IsKeyPressed(KEY_ENTER) && (boundedT == true) && (t >= d)))
        {
            t = 0.0f;
            ballPosition.x = 100.0f;
            ballPosition.y = 100.0f;
            paused = true;
        }

        if (IsKeyPressed(KEY_ENTER)) paused = !paused;

        // Movement computation
        if (!paused && ((boundedT && t < d) || !boundedT))
        {
            ballPosition.x = easings[easingX].func(t, 100.0f, 700.0f - 170.0f, d);
            ballPosition.y = easings[easingY].func(t, 100.0f, 400.0f - 170.0f, d);
            t += 1.0f;
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            // Draw information text
            DrawText(TextFormat("Easing x: %s", easings[easingX].name), 20, FONT_SIZE, FONT_SIZE, LIGHTGRAY);
            DrawText(TextFormat("Easing y: %s", easings[easingY].name), 20, FONT_SIZE*2, FONT_SIZE, LIGHTGRAY);
            DrawText(TextFormat("t (%c) = %.2f d = %.2f", (boundedT == true)? 'b' : 'u', t, d), 20, FONT_SIZE*3, FONT_SIZE, LIGHTGRAY);

            // Draw instructions text
            DrawText("Use ENTER to play or pause movement, use SPACE to restart", 20, GetScreenHeight() - FONT_SIZE*2, FONT_SIZE, LIGHTGRAY);
            DrawText("Use Q and W or A and S keys to change duration", 20, GetScreenHeight() - FONT_SIZE*3, FONT_SIZE, LIGHTGRAY);
            DrawText("Use LEFT or RIGHT keys to choose easing for the x axis", 20, GetScreenHeight() - FONT_SIZE*4, FONT_SIZE, LIGHTGRAY);
            DrawText("Use UP or DOWN keys to choose easing for the y axis", 20, GetScreenHeight() - FONT_SIZE*5, FONT_SIZE, LIGHTGRAY);

            // Draw ball
            DrawCircleV(ballPosition, 16.0f, MAROON);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();
    //--------------------------------------------------------------------------------------

    return 0;
}

//------------------------------------------------------------------------------------
// Module Functions Declaration
//------------------------------------------------------------------------------------
// NoEase function, used when "no easing" is selected for any axis
// It just ignores all parameters besides b
static f32 NoEase(f32 t, f32 b, f32 c, f32 d)
{
    // Hack to avoid compiler warning (about unused variables)
    const f32 burn = t + b + c + d;
    d += burn;

    return b;
}
