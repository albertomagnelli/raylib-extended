/*******************************************************************************************
*
*   raylib [core] example - text file loading
*
*   Example complexity rating: [★☆☆☆] 1/4
*
*   Example originally created with raylib 5.5, last time updated with raylib 5.6
*
*   Example contributed by Aanjishnu Bhattacharyya (@NimComPoo-04) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 0 Aanjishnu Bhattacharyya (@NimComPoo-04)
*
********************************************************************************************/

#include "raylib.h"

#include "raymath.h"        // Required for: Lerp()

#include <string.h>

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr auto screenWidth = 800;
    constexpr auto screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [core] example - text file loading");

    // Setting up the camera
    Camera2D cam = {
        .offset = {0, 0},
        .target = {0, 0},
        .rotation = 0,
        .zoom = 1
    };

    // Loading text file from resources/text_file.txt
    constexpr const char *fileName = "resources/text_file.txt";
    const auto text = LoadFileText(fileName);

    // Loading all the text lines
    auto lines = LoadTextLines(text);
    const auto lineCount = static_cast<i32>(lines.size());

    // Stylistic choises
    constexpr auto fontSize = 20;
    const i32 textTop = 25 + fontSize; // Top of the screen from where the text is rendered
    const i32 wrapWidth = screenWidth - 20;

    // Wrap the lines as needed
    for (i32 i = 0; i < lineCount; i++)
    {
        auto j = 0;
        auto lastSpace = 0;          // Keeping track of last valid space to insert '\n'
        auto lastWrapStart = 0;      // Keeping track of the start of this wrapped line.

        while (j <= strlen(lines[i].c_str()))
        {
            if (lines[i][j] == ' ' || lines[i][j] == '\0')
            {
                const char before = lines[i][j];
                // Making a C Style string by adding a '\0' at the required location so that we can use the MeasureText function
                lines[i][j] = '\0';

                // Checking if the text has crossed the wrapWidth, then going back and inserting a newline
                if (MeasureText(lines[i].c_str() + lastWrapStart, fontSize) > wrapWidth)
                {
                    lines[i][lastSpace] = '\n';

                    // Since we added a newline the place of wrap changed so we update our lastWrapStart
                    lastWrapStart = lastSpace + 1;
                }

                if(before != '\0') lines[i][j] = ' ';  // Resetting the space back
                lastSpace = j;                         // Since we encountered a new space we update our last encountered space location
            }

            j++;
        }
    }

    // Calculating the total height so that we can show a scrollbar
    auto textHeight = 0;

    for (i32 i = 0; i < lineCount; i++)
    {
        const auto size = MeasureTextEx(GetFontDefault(), lines[i], static_cast<f32>(fontSize), 2);
        textHeight += static_cast<i32>(size.y) + 10;
    }

    // A simple scrollbar on the side to show how far we have read into the file
    Rectangle scrollBar = {
        static_cast<f32>(screenWidth) - 5, 0, 5,
        screenHeight*100.0f/(textHeight - screenHeight) // Scrollbar height is just a percentage
    };

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        const auto scroll = GetMouseWheelMove();
        cam.target.y -= scroll*fontSize*1.5f;   // Choosing an arbitrary speed for scroll

        if (cam.target.y < 0) cam.target.y = 0;  // Snapping to 0 if we go too far back

        // Ensuring that the camera does not scroll past all text
        if (cam.target.y > textHeight - screenHeight + textTop)
            cam.target.y = static_cast<f32>(textHeight) - screenHeight + textTop;

        // Computing the position of the scrollBar depending on the percentage of text covered
        scrollBar.y = Lerp(static_cast<f32>(textTop), static_cast<f32>(screenHeight) - scrollBar.height, static_cast<f32>(cam.target.y - textTop)/(textHeight - screenHeight));
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            BeginMode2D(cam);
                // Going through all the read lines
                for (i32 i = 0, t = textTop; i < lineCount; i++)
                {
                    // Each time we go through and calculate the height of the text to move the cursor appropriately
                    vec2 size;
                    if(strcmp(lines[i].c_str(), "")){
                        // Fix for empty line in the text file
                        size = MeasureTextEx( GetFontDefault(), lines[i], static_cast<f32>(fontSize), 2);
                    }else{
                        size = MeasureTextEx( GetFontDefault(), " ", static_cast<f32>(fontSize), 2);
                    }

                    DrawText(lines[i], 10, t, fontSize, RED);

                    // Inserting extra space for real newlines,
                    // wrapped lines are rendered closer together
                    t += static_cast<i32>(size.y) + 10;
                }
            EndMode2D();

            // Header displaying which file is being read currently
            DrawRectangle(0, 0, screenWidth, textTop - 10, BEIGE);
            DrawText(TextFormat("File: %s", fileName), 10, 10, fontSize, MAROON);

            DrawRectangleRec(scrollBar, MAROON);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------

    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
