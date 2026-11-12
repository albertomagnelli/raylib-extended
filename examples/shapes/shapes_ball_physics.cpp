/*******************************************************************************************
*
*   raylib [shapes] example - ball physics
*
*   Example complexity rating: [★★☆☆] 2/4
*
*   Example originally created with raylib 6.0, last time updated with raylib 6.0
*
*   Example contributed by David Buzatto (@davidbuzatto) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2025 David Buzatto (@davidbuzatto)
*
********************************************************************************************/

#include "raylib.h"

#include "raymath.h"

#include <stdlib.h>         // Required for: malloc(), free()
#include <math.h>           // Required for: hypot()

#define MAX_BALLS     5000  // Maximum quantity of balls

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// Ball data type
typedef struct Ball {
    vec2 position;
    vec2 speed;
    vec2 prevPosition;
    f32 radius;
    f32 friction;
    f32 elasticity;
    Color color;
    bool grabbed;
} Ball;

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr auto screenWidth = 800;
    constexpr auto screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [shapes] example - ball physics");

    auto balls = static_cast<Ball *>(malloc(sizeof(Ball)*MAX_BALLS));

    // Init first ball in the array
    balls[0] = Ball{
        .position = { GetScreenWidth()/2.0f, GetScreenHeight()/2.0f },
        .speed = { 200, 200 },
        .prevPosition = {},
        .radius = 40,
        .friction = 0.99f,
        .elasticity = 0.9f,
        .color = BLUE,
        .grabbed = false
    };

    auto ballCount = 1;
    Ball *grabbedBall = nullptr;       // A pointer to the current ball that is grabbed
    auto pressOffset = vec2{};             // Mouse press offset relative to the ball that grabbedd

    f32 gravity = 100;            // World gravity
    
    auto windowPosition = GetWindowPosition();

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //---------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        const auto delta = GetFrameTime();
        const auto mousePos = GetMousePosition();

        // Checks if a ball was grabbed
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            for (i32 i = ballCount - 1; i >= 0; i--)
            {
                Ball *ball = &balls[i];
                pressOffset.x = mousePos.x - ball->position.x;
                pressOffset.y = mousePos.y - ball->position.y;

                // If the distance between the ball position and the mouse press position
                // is less than or equal to the ball radius, the event occurred inside the ball
                if (hypot(pressOffset.x, pressOffset.y) <= ball->radius)
                {
                    ball->grabbed = true;
                    grabbedBall = ball;
                    break;
                }
            }
        }

        // Releases any ball the was grabbed
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        {
            if (grabbedBall != nullptr)
            {
                grabbedBall->grabbed = false;
                grabbedBall = nullptr;
            }
        }

        // Creates a new ball
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || (IsKeyDown(KEY_LEFT_CONTROL) && IsMouseButtonDown(MOUSE_BUTTON_RIGHT)))
        {
            if (ballCount < MAX_BALLS)
            {
                balls[ballCount++] = Ball{
                    .position = mousePos,
                    .speed = { static_cast<f32>(GetRandomValue(-300, 300)), static_cast<f32>(GetRandomValue(-300, 300)) },
                    .prevPosition = {},
                    .radius = 20.0f + static_cast<f32>(GetRandomValue(0, 30)),
                    .friction = 0.99f,
                    .elasticity = 0.9f,
                    .color = Color{ static_cast<u8>(GetRandomValue(0, 255)), static_cast<u8>(GetRandomValue(0, 255)), static_cast<u8>(GetRandomValue(0, 255)), 255 },
                    .grabbed = false
                };
            }
        }

        // Get window position change for shaking
        const vec2 windowPositionDelta = ((windowPosition) - (GetWindowPosition()));
        
        if (glm::length(windowPositionDelta) > 5.0f)
        {
            for (i32 i = 0; i < ballCount; i++)
            {
                if (!balls[i].grabbed) balls[i].speed = ((balls[i].speed) + (((windowPositionDelta) * (10.0f))));
            }
        }

        // Shake balls
        if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE))
        {
            for (i32 i = 0; i < ballCount; i++)
            {
                if (!balls[i].grabbed) balls[i].speed = vec2{ static_cast<f32>(GetRandomValue(-2000, 2000)), static_cast<f32>(GetRandomValue(-2000, 2000)) };
            }
        }

        // Changes gravity
        gravity += GetMouseWheelMove()*5;

        // Updates each ball state
        for (i32 i = 0; i < ballCount; i++)
        {
            Ball *ball = &balls[i];

            // The ball is not grabbed
            if (!ball->grabbed)
            {
                // Ball repositioning using the velocity
                ball->position.x += ball->speed.x * delta;
                ball->position.y += ball->speed.y * delta;

                // Does the ball hit the screen right boundary?
                if ((ball->position.x + ball->radius) >= screenWidth)
                {
                    ball->position.x = screenWidth - ball->radius;    // Ball repositioning
                    ball->speed.x = -ball->speed.x*ball->elasticity;  // Elasticity makes the ball lose 10% of its velocity on hit
                }
                // Does the ball hit the screen left boundary?
                else if ((ball->position.x - ball->radius) <= 0)
                {
                    ball->position.x = ball->radius;
                    ball->speed.x = -ball->speed.x*ball->elasticity;
                }

                // The same for y axis
                if ((ball->position.y + ball->radius) >= screenHeight)
                {
                    ball->position.y = screenHeight - ball->radius;
                    ball->speed.y = -ball->speed.y*ball->elasticity;
                }
                else if ((ball->position.y - ball->radius) <= 0)
                {
                    ball->position.y = ball->radius;
                    ball->speed.y = -ball->speed.y*ball->elasticity;
                }

                // Friction makes the ball lose 1% of its velocity each frame
                ball->speed.x = ball->speed.x*ball->friction;
                // Gravity affects only the y axis
                ball->speed.y = ball->speed.y*ball->friction + gravity;
            }
            else
            {
                // Ball repositioning using the mouse position
                ball->position.x = mousePos.x - pressOffset.x;
                ball->position.y = mousePos.y - pressOffset.y;

                // While the ball is grabbed, recalculates its velocity
                ball->speed.x = (ball->position.x - ball->prevPosition.x)/delta;
                ball->speed.y = (ball->position.y - ball->prevPosition.y)/delta;
                ball->prevPosition = ball->position;
            }
        }

        windowPosition = GetWindowPosition();
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            for (i32 i = 0; i < ballCount; i++)
            {
                DrawCircleV(balls[i].position, balls[i].radius, balls[i].color);
                DrawCircleLinesV(balls[i].position, balls[i].radius, BLACK);
            }

            DrawText("grab a ball by pressing with the mouse and throw it by releasing", 10, 10, 10, DARKGRAY);
            DrawText("right click to create new balls (keep left control pressed to create a lot)", 10, 30, 10, DARKGRAY);
            DrawText("use mouse wheel to change gravity", 10, 50, 10, DARKGRAY);
            DrawText("middle click to shake", 10, 70, 10, DARKGRAY);
            DrawText(TextFormat("BALL COUNT: %d", ballCount), 10, GetScreenHeight() - 70, 20, BLACK);
            DrawText(TextFormat("GRAVITY: %.2f", gravity), 10, GetScreenHeight() - 40, 20, BLACK);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    free(balls);

    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
