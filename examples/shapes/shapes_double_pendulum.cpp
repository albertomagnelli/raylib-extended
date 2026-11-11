/*******************************************************************************************
*
*   raylib [shapes] example - double pendulum
*
*   Example complexity rating: [★★☆☆] 2/4
*
*   Example originally created with raylib 5.5, last time updated with raylib 5.5
*
*   Example contributed by JoeCheong (@Joecheong2006) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2025 JoeCheong (@Joecheong2006)
*
********************************************************************************************/

#include "raylib.h"

#include <math.h>       // Required for: sin(), cos(), PI

// Constant for Simulation
#define SIMULATION_STEPS 30
#define G 9.81f

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
static vec2 CalculatePendulumEndPoint(f32 l, f32 theta);
static vec2 CalculateDoublePendulumEndPoint(f32 l1, f32 theta1, f32 l2, f32 theta2);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr auto screenWidth = 800;
    constexpr auto screenHeight = 450;

    SetConfigFlags(FLAG_WINDOW_HIGHDPI);
    InitWindow(screenWidth, screenHeight, "raylib [shapes] example - double pendulum");

    // Simulation Parameters
    constexpr f32 l1 = 15.0f, m1 = 0.2f;
    f32 theta1 = DEG2RAD*170, w1 = 0;
    constexpr f32 l2 = 15.0f, m2 = 0.1f;
    f32 theta2 = 0, w2 = 0;
    constexpr auto lengthScaler = 0.1f;
    const f32 totalM = m1 + m2;

    auto previousPosition = CalculateDoublePendulumEndPoint(l1, theta1, l2, theta2);
    previousPosition.x += (static_cast<f32>(screenWidth)/2);
    previousPosition.y += (static_cast<f32>(screenHeight)/2 - 100);

    // Scale length
    const f32 L1 = l1*lengthScaler;
    const f32 L2 = l2*lengthScaler;

    // Draw parameters
    constexpr auto lineThick = 20.0f;
    constexpr auto trailThick = 2.0f;
    constexpr auto fateAlpha = 0.01f;

    // Create framebuffer
    const auto target = LoadRenderTexture(screenWidth, screenHeight);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        const auto dt = GetFrameTime();
        const f32 step = dt/SIMULATION_STEPS;
        const f32 step2 = step*step;

        // Update Physics - larger steps = better approximation
        for (i32 i = 0; i < SIMULATION_STEPS; i++)
        {
            const f32 delta = theta1 - theta2;
            const f32 sinD = sinf(delta);
            const f32 cosD = cosf(delta), cos2D = cosf(2*delta);
            const f32 ww1 = w1*w1;
            const f32 ww2 = w2*w2;

            // Calculate a1
            const f32 a1 = (-G*(2*m1 + m2)*sinf(theta1)
                         - m2*G*sinf(theta1 - 2*theta2)
                         - 2*sinD*m2*(ww2*L2 + ww1*L1*cosD))
                        /(L1*(2*m1 + m2 - m2*cos2D));

            // Calculate a2
            const f32 a2 = (2*sinD*(ww1*L1*totalM
                         + G*totalM*cosf(theta1)
                         + ww2*L2*m2*cosD))
                        /(L2*(2*m1 + m2 - m2*cos2D));

            // Update thetas
            theta1 += w1*step + 0.5f*a1*step2;
            theta2 += w2*step + 0.5f*a2*step2;

            // Update omegas
            w1 += a1*step;
            w2 += a2*step;
        }

        // Calculate position
        auto currentPosition = CalculateDoublePendulumEndPoint(l1, theta1, l2, theta2);
        currentPosition.x += static_cast<f32>(screenWidth)/2;
        currentPosition.y += static_cast<f32>(screenHeight)/2 - 100;

        // Draw to render texture
        BeginTextureMode(target);
            // Draw a transparent rectangle - smaller alpha = longer trails
            DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, fateAlpha));

            // Draw trail
            DrawCircleV(previousPosition, trailThick, RED);
            DrawLineEx(previousPosition, currentPosition, trailThick*2, RED);
        EndTextureMode();

        // Update previous position
        previousPosition = currentPosition;
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(BLACK);

            // Draw trails texture
            DrawTextureRec(target.texture, Rectangle{ 0, 0, static_cast<f32>(target.texture.width), static_cast<f32>(-target.texture.height) }, vec2{ 0, 0 }, WHITE);

            // Draw double pendulum
            DrawRectanglePro(Rectangle{ screenWidth/2.0f, screenHeight/2.0f - 100, 10*l1, lineThick },
                vec2{0, lineThick*0.5f}, 90 - RAD2DEG*theta1, RAYWHITE);

            const auto endpoint1 = CalculatePendulumEndPoint(l1, theta1);
            DrawRectanglePro(Rectangle{ screenWidth/2.0f + endpoint1.x, screenHeight/2.0f - 100 + endpoint1.y, 10*l2, lineThick },
                vec2{0, lineThick*0.5f}, 90 - RAD2DEG*theta2, RAYWHITE);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadRenderTexture(target);

    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------
// Calculate pendulum end point
static vec2 CalculatePendulumEndPoint(f32 l, f32 theta)
{
    return vec2{ 10*l*sinf(theta), 10*l*cosf(theta) };
}

// Calculate double pendulum end point
static vec2 CalculateDoublePendulumEndPoint(f32 l1, f32 theta1, f32 l2, f32 theta2)
{
    const auto endpoint1 = CalculatePendulumEndPoint(l1, theta1);
    const auto endpoint2 = CalculatePendulumEndPoint(l2, theta2);
    return vec2{ endpoint1.x + endpoint2.x, endpoint1.y + endpoint2.y };
}
