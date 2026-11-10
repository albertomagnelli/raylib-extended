/*******************************************************************************************
*
*   raylib [shapes] example - penrose tile
*
*   Example complexity rating: [★★★★] 4/4
*
*   Example originally created with raylib 5.5, last time updated with raylib 6.0
*   Based on: https://processing.org/examples/penrosetile.html
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

#include <stdlib.h>
#include <string.h>
#include <math.h>

#define STR_MAX_SIZE            10000
#define TURTLE_STACK_MAX_SIZE      50

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct TurtleState {
    Vector2 origin;
    float angle;
} TurtleState;

typedef struct PenroseLSystem {
    int steps;
    char *production;
    const char *ruleW;
    const char *ruleX;
    const char *ruleY;
    const char *ruleZ;
    float drawLength;
    float theta;
} PenroseLSystem;

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
static TurtleState turtleStack[TURTLE_STACK_MAX_SIZE];
static int turtleTop = -1;

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
static void PushTurtleState(TurtleState state);
static TurtleState PopTurtleState(void);
static PenroseLSystem LoadPenroseLSystem(float drawLength);
static void UnloadPenroseLSystem(PenroseLSystem ls);
static void BuildProductionStep(PenroseLSystem *ls);
static void DrawPenroseLSystem(PenroseLSystem *ls);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr int screenWidth = 800;
    constexpr int screenHeight = 450;

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "raylib [shapes] example - penrose tile");

    constexpr float drawLength = 460.0f;
    constexpr int minGenerations = 0;
    constexpr int maxGenerations = 4;
    int generations = 1;

    // Initialize new penrose tile
    PenroseLSystem ls = LoadPenroseLSystem(drawLength*(generations/static_cast<float>(maxGenerations)));
    for (int i = 0; i < generations; i++) BuildProductionStep(&ls);

    SetTargetFPS(120);              // Set our game to run at 120 frames-per-second
    //---------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        bool rebuild = false;
        if (IsKeyPressed(KEY_UP))
        {
            if (generations < maxGenerations)
            {
                generations++;
                rebuild = true;
            }
        }
        else if (IsKeyPressed(KEY_DOWN))
        {
            if (generations > minGenerations)
            {
                generations--;
                if (generations > 0) rebuild = true;
            }
        }

        if (rebuild)
        {
            UnloadPenroseLSystem(ls); // Free previous production for re-creation
            ls = LoadPenroseLSystem(drawLength*(generations/static_cast<float>(maxGenerations)));
            for (int i = 0; i < generations; i++) BuildProductionStep(&ls);
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            if (generations > 0) DrawPenroseLSystem(&ls);

            DrawText("penrose l-system", 10, 10, 20, DARKGRAY);
            DrawText("press up or down to change generations", 10, 30, 20, DARKGRAY);
            DrawText(TextFormat("generations: %d", generations), 10, 50, 20, DARKGRAY);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadPenroseLSystem(ls); // Free production string

    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------
// Push turtle state for next step
static void PushTurtleState(TurtleState state)
{
    if (turtleTop < (TURTLE_STACK_MAX_SIZE - 1)) turtleStack[++turtleTop] = state;
    else TraceLog(LOG_WARNING, "TURTLE STACK OVERFLOW!");
}

// Pop turtle state step
static TurtleState PopTurtleState(void)
{
    if (turtleTop >= 0) return turtleStack[turtleTop--];
    else TraceLog(LOG_WARNING, "TURTLE STACK UNDERFLOW!");

    return {};
}

// Load a new penrose tile structure
static PenroseLSystem LoadPenroseLSystem(float drawLength)
{
    PenroseLSystem ls = {
        .steps = 0,
        .ruleW = "YF++ZF4-XF[-YF4-WF]++",
        .ruleX = "+YF--ZF[3-WF--XF]+",
        .ruleY = "-WF++XF[+++YF++ZF]-",
        .ruleZ = "--YF++++WF[+ZF++++XF]--XF",
        .drawLength = drawLength,
        .theta = 36.0f // Degrees
    };

    ls.production = static_cast<char *>(RL_MALLOC(sizeof(char)*STR_MAX_SIZE));
    ls.production[0] = '\0';
    strncpy(ls.production, "[X]++[X]++[X]++[X]++[X]", STR_MAX_SIZE);

    return ls;
}

// Unload penrose tile structure
static void UnloadPenroseLSystem(PenroseLSystem ls)
{
    RL_FREE(ls.production);
}

// Build next penrose step
static void BuildProductionStep(PenroseLSystem *ls)
{
    char *newProduction = static_cast<char *>(RL_MALLOC(sizeof(char)*STR_MAX_SIZE));
    newProduction[0] = '\0';

    const int productionLength = static_cast<int>(strnlen(ls->production, STR_MAX_SIZE));

    for (int i = 0; i < productionLength; i++)
    {
        const char step = ls->production[i];
        const int remainingSpace = STR_MAX_SIZE - static_cast<int>(strnlen(newProduction, STR_MAX_SIZE)) - 1;
        switch (step)
        {
            case 'W': strncat(newProduction, ls->ruleW, remainingSpace); break;
            case 'X': strncat(newProduction, ls->ruleX, remainingSpace); break;
            case 'Y': strncat(newProduction, ls->ruleY, remainingSpace); break;
            case 'Z': strncat(newProduction, ls->ruleZ, remainingSpace); break;
            default:
            {
                if (step != 'F')
                {
                    const int t = static_cast<int>(strnlen(newProduction, STR_MAX_SIZE));
                    newProduction[t] = step;
                    newProduction[t + 1] = '\0';
                }
            } break;
        }
    }

    ls->drawLength *= 0.5f;
    strncpy(ls->production, newProduction, STR_MAX_SIZE);

    RL_FREE(newProduction);
}

// Draw penrose tile lines
static void DrawPenroseLSystem(PenroseLSystem *ls)
{
    const Vector2 screenCenter = { GetScreenWidth()/2.0f, GetScreenHeight()/2.0f };

    TurtleState turtle = {
        .origin = {},
        .angle = -90.0f
    };

    int repeats = 1;
    const int productionLength = static_cast<int>(strnlen(ls->production, STR_MAX_SIZE));
    ls->steps += 12;

    if (ls->steps > productionLength) ls->steps = productionLength;

    for (int i = 0; i < ls->steps; i++)
    {
        const char step = ls->production[i];
        if (step == 'F')
        {
            for (int j = 0; j < repeats; j++)
            {
                const Vector2 startPosWorld = turtle.origin;
                const float radAngle = DEG2RAD*turtle.angle;
                turtle.origin.x += ls->drawLength*cosf(radAngle);
                turtle.origin.y += ls->drawLength*sinf(radAngle);
                const Vector2 startPosScreen = { startPosWorld.x + screenCenter.x, startPosWorld.y + screenCenter.y };
                const Vector2 endPosScreen = { turtle.origin.x + screenCenter.x, turtle.origin.y + screenCenter.y };

                DrawLineEx(startPosScreen, endPosScreen, 2, Fade(BLACK, 0.2f));
            }

            repeats = 1;
        }
        else if (step == '+')
        {
            for (int j = 0; j < repeats; j++) turtle.angle += ls->theta;

            repeats = 1;
        }
        else if (step == '-')
        {
            for (int j = 0; j < repeats; j++) turtle.angle += -ls->theta;

            repeats = 1;
        }
        else if (step == '[') PushTurtleState(turtle);
        else if (step == ']') turtle = PopTurtleState();
        else if ((step >= 48) && (step <= 57)) repeats = static_cast<int>(step) - 48;
    }

    turtleTop = -1;
}
