/*******************************************************************************************
*
*   raylib [core] example - 2d camera split screen
*
*   Example complexity rating: [★★★★] 4/4
*
*   Addapted from the core_3d_camera_split_screen example:
*       https://github.com/raysan5/raylib/blob/master/examples/core/core_3d_camera_split_screen.c
*
*   Example originally created with raylib 4.5, last time updated with raylib 4.5
*
*   Example contributed by Gabriel dos Santos Sanches (@gabrielssanches) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2023-2025 Gabriel dos Santos Sanches (@gabrielssanches)
*
********************************************************************************************/

#include "raylib.h"

#define PLAYER_SIZE 40

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr auto screenWidth = 800;
    constexpr auto screenHeight = 440;

    InitWindow(screenWidth, screenHeight, "raylib [core] example - 2d camera split screen");

    auto player1 = Rectangle{ 200, 200, PLAYER_SIZE, PLAYER_SIZE };
    auto player2 = Rectangle{ 250, 200, PLAYER_SIZE, PLAYER_SIZE };

    auto camera1 = Camera2D{};
    camera1.target = vec2{ player1.x, player1.y };
    camera1.offset = vec2{ 200.0f, 200.0f };
    camera1.rotation = 0.0f;
    camera1.zoom = 1.0f;

    auto camera2 = Camera2D{};
    camera2.target = vec2{ player2.x, player2.y };
    camera2.offset = vec2{ 200.0f, 200.0f };
    camera2.rotation = 0.0f;
    camera2.zoom = 1.0f;

    const RenderTexture screenCamera1 = LoadRenderTexture(screenWidth/2, screenHeight);
    const RenderTexture screenCamera2 = LoadRenderTexture(screenWidth/2, screenHeight);

    // Build a flipped rectangle the size of the split view to use for drawing later
    const auto splitScreenRect = Rectangle{ 0.0f, 0.0f, static_cast<f32>(screenCamera1.texture.width), static_cast<f32>(-screenCamera1.texture.height) };

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        if (IsKeyDown(KEY_S)) player1.y += 3.0f;
        else if (IsKeyDown(KEY_W)) player1.y -= 3.0f;
        if (IsKeyDown(KEY_D)) player1.x += 3.0f;
        else if (IsKeyDown(KEY_A)) player1.x -= 3.0f;

        if (IsKeyDown(KEY_UP)) player2.y -= 3.0f;
        else if (IsKeyDown(KEY_DOWN)) player2.y += 3.0f;
        if (IsKeyDown(KEY_RIGHT)) player2.x += 3.0f;
        else if (IsKeyDown(KEY_LEFT)) player2.x -= 3.0f;

        camera1.target = vec2{ player1.x, player1.y };
        camera2.target = vec2{ player2.x, player2.y };
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginTextureMode(screenCamera1);
            ClearBackground(RAYWHITE);

            BeginMode2D(camera1);

                // Draw full scene with first camera
                for (i32 i = 0; i < screenWidth/PLAYER_SIZE + 1; i++)
                {
                    DrawLineV(vec2{static_cast<f32>(PLAYER_SIZE)*i, 0}, vec2{ static_cast<f32>(PLAYER_SIZE)*i, static_cast<f32>(screenHeight)}, LIGHTGRAY);
                }

                for (i32 i = 0; i < screenHeight/PLAYER_SIZE + 1; i++)
                {
                    DrawLineV(vec2{0, static_cast<f32>(PLAYER_SIZE)*i}, vec2{ static_cast<f32>(screenWidth), static_cast<f32>(PLAYER_SIZE)*i}, LIGHTGRAY);
                }

                for (i32 i = 0; i < screenWidth/PLAYER_SIZE; i++)
                {
                    for (i32 j = 0; j < screenHeight/PLAYER_SIZE; j++)
                    {
                        DrawText(TextFormat("[%i,%i]", i, j), 10 + PLAYER_SIZE*i, 15 + PLAYER_SIZE*j, 10, LIGHTGRAY);
                    }
                }

                DrawRectangleRec(player1, RED);
                DrawRectangleRec(player2, BLUE);
            EndMode2D();

            DrawRectangle(0, 0, GetScreenWidth()/2, 30, Fade(RAYWHITE, 0.6f));
            DrawText("PLAYER1: W/S/A/D to move", 10, 10, 10, MAROON);

        EndTextureMode();

        BeginTextureMode(screenCamera2);
            ClearBackground(RAYWHITE);

            BeginMode2D(camera2);

                // Draw full scene with second camera
                for (i32 i = 0; i < screenWidth/PLAYER_SIZE + 1; i++)
                {
                    DrawLineV(vec2{ static_cast<f32>(PLAYER_SIZE)*i, 0}, vec2{ static_cast<f32>(PLAYER_SIZE)*i, static_cast<f32>(screenHeight)}, LIGHTGRAY);
                }

                for (i32 i = 0; i < screenHeight/PLAYER_SIZE + 1; i++)
                {
                    DrawLineV(vec2{0, static_cast<f32>(PLAYER_SIZE)*i}, vec2{ static_cast<f32>(screenWidth), static_cast<f32>(PLAYER_SIZE)*i}, LIGHTGRAY);
                }

                for (i32 i = 0; i < screenWidth/PLAYER_SIZE; i++)
                {
                    for (i32 j = 0; j < screenHeight/PLAYER_SIZE; j++)
                    {
                        DrawText(TextFormat("[%i,%i]", i, j), 10 + PLAYER_SIZE*i, 15 + PLAYER_SIZE*j, 10, LIGHTGRAY);
                    }
                }

                DrawRectangleRec(player1, RED);
                DrawRectangleRec(player2, BLUE);

            EndMode2D();

            DrawRectangle(0, 0, GetScreenWidth()/2, 30, Fade(RAYWHITE, 0.6f));
            DrawText("PLAYER2: UP/DOWN/LEFT/RIGHT to move", 10, 10, 10, DARKBLUE);

        EndTextureMode();

        // Draw both views render textures to the screen side by side
        BeginDrawing();
            ClearBackground(BLACK);

            DrawTextureRec(screenCamera1.texture, splitScreenRect, vec2{ 0, 0 }, WHITE);
            DrawTextureRec(screenCamera2.texture, splitScreenRect, vec2{ screenWidth/2.0f, 0 }, WHITE);

            DrawRectangle(GetScreenWidth()/2 - 2, 0, 4, GetScreenHeight(), LIGHTGRAY);
        EndDrawing();
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadRenderTexture(screenCamera1); // Unload render texture
    UnloadRenderTexture(screenCamera2); // Unload render texture

    CloseWindow();                      // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
