/*******************************************************************************************
*
*   raylib [textures] example - textured curve
*
*   Example complexity rating: [★★★☆] 3/4
*
*   Example originally created with raylib 4.5, last time updated with raylib 4.5
*
*   Example contributed by Jeffery Myers (@JeffM2501) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2022-2025 Jeffery Myers (@JeffM2501) and Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"

#include "raymath.h"
#include "rlgl.h"

#include <math.h>           // Required for: powf()
#include <stdlib.h>         // Required for: NULL

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
static auto texRoad = Texture{};

static auto showCurve = false;

static f32 curveWidth = 50;
static auto curveSegments = 24;

static auto curveStartPosition = vec2{};
static auto curveStartPositionTangent = vec2{};

static auto curveEndPosition = vec2{};
static auto curveEndPositionTangent = vec2{};

static vec2 *curveSelectedPoint = nullptr;

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
static void DrawTexturedCurve(void);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr auto screenWidth = 800;
    constexpr auto screenHeight = 450;

    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "raylib [textures] example - textured curve");

    // Load the road texture
    texRoad = LoadTexture("resources/road.png");
    SetTextureFilter(texRoad, TEXTURE_FILTER_BILINEAR);

    // Setup the curve
    curveStartPosition = vec2{ 80, 100 };
    curveStartPositionTangent = vec2{ 100, 300 };

    curveEndPosition = vec2{ 700, 350 };
    curveEndPositionTangent = vec2{ 600, 100 };

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        // Curve config options
        if (IsKeyPressed(KEY_SPACE)) showCurve = !showCurve;
        if (IsKeyPressed(KEY_EQUAL)) curveWidth += 2;
        if (IsKeyPressed(KEY_MINUS)) curveWidth -= 2;
        if (curveWidth < 2) curveWidth = 2;

        // Update segments
        if (IsKeyPressed(KEY_LEFT)) curveSegments -= 2;
        if (IsKeyPressed(KEY_RIGHT)) curveSegments += 2;

        if (curveSegments < 2) curveSegments = 2;

        // Update curve logic
        // If the mouse is not down, we are not editing the curve so clear the selection
        if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON))  curveSelectedPoint = nullptr;

        // If a point was selected, move it
        if (curveSelectedPoint != nullptr) *curveSelectedPoint = ((*curveSelectedPoint) + (GetMouseDelta()));

        // The mouse is down, and nothing was selected, so see if anything was picked
        const auto mouse = GetMousePosition();
        if (CheckCollisionPointCircle(mouse, curveStartPosition, 6)) curveSelectedPoint = &curveStartPosition;
        else if (CheckCollisionPointCircle(mouse, curveStartPositionTangent, 6)) curveSelectedPoint = &curveStartPositionTangent;
        else if (CheckCollisionPointCircle(mouse, curveEndPosition, 6)) curveSelectedPoint = &curveEndPosition;
        else if (CheckCollisionPointCircle(mouse, curveEndPositionTangent, 6)) curveSelectedPoint = &curveEndPositionTangent;
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawTexturedCurve();    // Draw a textured Spline Cubic Bezier

            // Draw spline for reference
            if (showCurve) DrawSplineSegmentBezierCubic(curveStartPosition, curveEndPosition, curveStartPositionTangent, curveEndPositionTangent, 2, BLUE);

            // Draw the various control points and highlight where the mouse is
            DrawLineV(curveStartPosition, curveStartPositionTangent, SKYBLUE);
            DrawLineV(curveStartPositionTangent, curveEndPositionTangent, Fade(LIGHTGRAY, 0.4f));
            DrawLineV(curveEndPosition, curveEndPositionTangent, PURPLE);

            if (CheckCollisionPointCircle(mouse, curveStartPosition, 6)) DrawCircleV(curveStartPosition, 7, YELLOW);
            DrawCircleV(curveStartPosition, 5, RED);

            if (CheckCollisionPointCircle(mouse, curveStartPositionTangent, 6)) DrawCircleV(curveStartPositionTangent, 7, YELLOW);
            DrawCircleV(curveStartPositionTangent, 5, MAROON);

            if (CheckCollisionPointCircle(mouse, curveEndPosition, 6)) DrawCircleV(curveEndPosition, 7, YELLOW);
            DrawCircleV(curveEndPosition, 5, GREEN);

            if (CheckCollisionPointCircle(mouse, curveEndPositionTangent, 6)) DrawCircleV(curveEndPositionTangent, 7, YELLOW);
            DrawCircleV(curveEndPositionTangent, 5, DARKGREEN);

            // Draw usage info
            DrawText("Drag points to move curve, press SPACE to show/hide base curve", 10, 10, 10, DARKGRAY);
            DrawText(TextFormat("Curve width: %2.0f (Use + and - to adjust)", curveWidth), 10, 30, 10, DARKGRAY);
            DrawText(TextFormat("Curve segments: %d (Use LEFT and RIGHT to adjust)", curveSegments), 10, 50, 10, DARKGRAY);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadTexture(texRoad);

    CloseWindow();              // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------
// Draw textured curve using Spline Cubic Bezier
static void DrawTexturedCurve(void)
{
    const f32 step = 1.0f/curveSegments;

    vec2 previous = curveStartPosition;
    auto previousTangent = vec2{};
    f32 previousV = 0;

    // We can't compute a tangent for the first point, so we need to reuse the tangent from the first segment
    auto tangentSet = false;

    auto current = vec2{};
    auto t = 0.0f;

    for (i32 i = 1; i <= curveSegments; i++)
    {
        t = step*static_cast<f32>(i);

        const f32 a = powf(1.0f - t, 3);
        const f32 b = 3.0f*powf(1.0f - t, 2)*t;
        const f32 c = 3.0f*(1.0f - t)*powf(t, 2);
        const f32 d = powf(t, 3);

        // Compute the endpoint for this segment
        current.y = a*curveStartPosition.y + b*curveStartPositionTangent.y + c*curveEndPositionTangent.y + d*curveEndPosition.y;
        current.x = a*curveStartPosition.x + b*curveStartPositionTangent.x + c*curveEndPositionTangent.x + d*curveEndPosition.x;

        // Vector from previous to current
        const auto delta = vec2{ current.x - previous.x, current.y - previous.y };

        // The right hand normal to the delta vector
        const vec2 normal = glm::normalize(vec2{ -delta.y, delta.x });

        // The v texture coordinate of the segment (add up the length of all the segments so far)
        const f32 v = previousV + glm::length(delta) / static_cast<f32>(texRoad.height * 2);

        // Make sure the start point has a normal
        if (!tangentSet)
        {
            previousTangent = normal;
            tangentSet = true;
        }

        // Extend out the normals from the previous and current points to get the quad for this segment
        const vec2 prevPosNormal = ((previous) + (((previousTangent) * (curveWidth))));
        const vec2 prevNegNormal = ((previous) + (((previousTangent) * (-curveWidth))));

        const vec2 currentPosNormal = ((current) + (((normal) * (curveWidth))));
        const vec2 currentNegNormal = ((current) + (((normal) * (-curveWidth))));

        // Draw the segment as a quad
        rlSetTexture(texRoad.id);
        rlBegin(RL_QUADS);
            rlColor4ub(255,255,255,255);
            rlNormal3f(0.0f, 0.0f, 1.0f);

            rlTexCoord2f(0, previousV);
            rlVertex2f(prevNegNormal.x, prevNegNormal.y);

            rlTexCoord2f(1, previousV);
            rlVertex2f(prevPosNormal.x, prevPosNormal.y);

            rlTexCoord2f(1, v);
            rlVertex2f(currentPosNormal.x, currentPosNormal.y);

            rlTexCoord2f(0, v);
            rlVertex2f(currentNegNormal.x, currentNegNormal.y);
        rlEnd();

        // The current step is the start of the next step
        previous = current;
        previousTangent = normal;
        previousV = v;
    }
}

