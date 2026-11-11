/*******************************************************************************************
*
*   raylib [shapes] example - kaleidoscope
*
*   Example complexity rating: [★★☆☆] 2/4
*
*   Example originally created with raylib 5.5, last time updated with raylib 5.6
*
*   Example contributed by Hugo ARNAL (@hugoarnal) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2025 Hugo ARNAL (@hugoarnal) and Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"
#include <string.h>

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include "raymath.h"

#define MAX_DRAW_LINES  8192

// Line data type
typedef struct {
    glm::vec2 start;
    glm::vec2 end;
} Line;

// Lines array as a global static variable to be stored
// in heap and avoid potential stack overflow (on Web platform)
static Line lines[MAX_DRAW_LINES] = {};

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr int screenWidth = 800;
    constexpr int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [shapes] example - kaleidoscope");

    // Line drawing properties
    constexpr int symmetry = 6;
    const float angle = 360.0f/static_cast<float>(symmetry);
    constexpr float thickness = 3.0f;
    const Rectangle resetButtonRec = { screenWidth - 55.0f, 5.0f, 50, 25 };
    const Rectangle backButtonRec = { screenWidth - 55.0f, screenHeight - 30.0f, 25, 25 };
    const Rectangle nextButtonRec = { screenWidth - 30.0f, screenHeight - 30.0f, 25, 25 };
    glm::vec2 mousePos = {};
    glm::vec2 prevMousePos = {};
    const glm::vec2 scaleVector = { 1.0f, -1.0f };
    const glm::vec2 offset = { GetRenderWidth()/2.0f, GetRenderHeight()/2.0f };

    Camera2D camera = {};
    camera.target = glm::vec2{};
    camera.offset = offset;
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    int currentLineCounter = 0;
    int totalLineCounter = 0;
    int resetButtonClicked = false;
    int backButtonClicked = false;
    int nextButtonClicked = false;

    SetTargetFPS(20);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        prevMousePos = mousePos;
        mousePos = GetMousePosition();

        glm::vec2 lineStart = ((mousePos) - (offset));
        glm::vec2 lineEnd = ((prevMousePos) - (offset));

        if (
            IsMouseButtonDown(MOUSE_LEFT_BUTTON)
            && (CheckCollisionPointRec(mousePos, resetButtonRec) == false)
            && (CheckCollisionPointRec(mousePos, backButtonRec) == false)
            && (CheckCollisionPointRec(mousePos, nextButtonRec) == false)
        )
        {
            for (int s = 0; (s < symmetry) && (totalLineCounter < (MAX_DRAW_LINES - 1)); s++)
            {
                lineStart = Vector2Rotate(lineStart, angle*DEG2RAD);
                lineEnd = Vector2Rotate(lineEnd, angle*DEG2RAD);

                // Store mouse line
                lines[totalLineCounter].start = lineStart;
                lines[totalLineCounter].end = lineEnd;

                // Store reflective line
                lines[totalLineCounter + 1].start = ((lineStart) * (scaleVector));
                lines[totalLineCounter + 1].end = ((lineEnd) * (scaleVector));

                totalLineCounter += 2;
                currentLineCounter = totalLineCounter;
            }
        }

        if (resetButtonClicked)
        {
            memset(&lines, 0, sizeof(Line)*MAX_DRAW_LINES);
            currentLineCounter = 0;
            totalLineCounter = 0;
        }

        if (backButtonClicked && (currentLineCounter > 0))
        {
            currentLineCounter -= 1;
        }

        if (nextButtonClicked && (currentLineCounter < MAX_DRAW_LINES) && ((currentLineCounter + 1) <= totalLineCounter))
        {
            currentLineCounter += 1;
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);
            BeginMode2D(camera);

                for (int s = 0; s < symmetry; s++)
                {
                    for (int i = 0; i < currentLineCounter; i += 2)
                    {
                        DrawLineEx(lines[i].start, lines[i].end, thickness, BLACK);
                        DrawLineEx(lines[i + 1].start, lines[i + 1].end, thickness, BLACK);
                    }
                }

            EndMode2D();

            if ((currentLineCounter - 1) < 0) GuiDisable();

            backButtonClicked = GuiButton(backButtonRec, "<");
            GuiEnable();

            if ((currentLineCounter + 1) > totalLineCounter) GuiDisable();

            nextButtonClicked = GuiButton(nextButtonRec, ">");
            GuiEnable();
            resetButtonClicked = GuiButton(resetButtonRec, "Reset");

            DrawText(TextFormat("LINES: %i/%i", currentLineCounter, MAX_DRAW_LINES), 10, screenHeight - 30, 20, MAROON);
            DrawFPS(10, 10);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
