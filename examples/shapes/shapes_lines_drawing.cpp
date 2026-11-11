/*******************************************************************************************
*
*   raylib [shapes] example - lines drawing
*
*   Example complexity rating: [★☆☆☆] 1/4
*
*   Example originally created with raylib 6.0, last time updated with raylib 5.6
*
*   Example contributed by Robin (@RobinsAviary) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2025 Robin (@RobinsAviary)
*
********************************************************************************************/

#include "raylib.h"

#include "raymath.h"

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr auto screenWidth = 800;
    constexpr auto screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [shapes] example - lines drawing");

    // Hint text that shows before you click the screen
    auto startText = true;

    // The mouse's position on the previous frame
    auto mousePositionPrevious = GetMousePosition();

    // The canvas to draw lines on
    const RenderTexture canvas = LoadRenderTexture(screenWidth, screenHeight);

    // The line's thickness
    auto lineThickness = 8.0f;
    // The lines hue (in HSV, from 0-360)
    auto lineHue = 0.0f;

    // Clear the canvas to the background color
    BeginTextureMode(canvas);
        ClearBackground(RAYWHITE);
    EndTextureMode();

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        // Disable the hint text once the user clicks
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && startText) startText = false;

        // Clear the canvas when the user middle-clicks
        if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE))
        {
            BeginTextureMode(canvas);
                ClearBackground(RAYWHITE);
            EndTextureMode();
        }

        // Store whether the left and right buttons are down
        const auto leftButtonDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        const auto rightButtonDown = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);

        if (leftButtonDown || rightButtonDown)
        {
            // The color for the line
            Color drawColor = WHITE;

            if (leftButtonDown)
            {
                // Increase the hue value by the distance our cursor has moved since the last frame (divided by 3)
                lineHue += glm::distance(mousePositionPrevious, GetMousePosition())/3.0f;

                // While the hue is >=360, subtract it to bring it down into the range 0-360
                // This is more visually accurate than resetting to zero
                while (lineHue >= 360.0f) lineHue -= 360.0f;

                // Create the final color
                drawColor = ColorFromHSV(lineHue, 1.0f, 1.0f);
            }
            else if (rightButtonDown) drawColor = RAYWHITE; // Use the background color as an "eraser"

            // Draw the line onto the canvas
            BeginTextureMode(canvas);
                // Circles act as "caps", smoothing corners
                DrawCircleV(mousePositionPrevious, lineThickness/2.0f, drawColor);
                DrawCircleV(GetMousePosition(), lineThickness/2.0f, drawColor);
                DrawLineEx(mousePositionPrevious, GetMousePosition(), lineThickness, drawColor);
            EndTextureMode();
        }

        // Update line thickness based on mousewheel
        lineThickness += GetMouseWheelMove();
        lineThickness = Clamp(lineThickness, 1.0, 500.0f);

        // Update mouse's previous position
        mousePositionPrevious = GetMousePosition();
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            // Draw the render texture to the screen, flipped vertically to make it appear top-side up
            DrawTextureRec(canvas.texture, Rectangle{ 0.0f, 0.0f, static_cast<f32>(canvas.texture.width),static_cast<f32>(-canvas.texture.height) }, vec2{}, WHITE);

            // Draw the preview circle
            if (!leftButtonDown) DrawCircleLinesV(GetMousePosition(), lineThickness/2.0f, Color{ 127, 127, 127, 127 });

            // Draw the hint text
            if (startText) DrawText("try clicking and dragging!", 275, 215, 20, LIGHTGRAY);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadRenderTexture(canvas); // Unload the canvas render texture

    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}