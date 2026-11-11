/*******************************************************************************************
*
*   raylib [audio] example - module playing
*
*   Example complexity rating: [★☆☆☆] 1/4
*
*   Example originally created with raylib 1.5, last time updated with raylib 3.5
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2016-2025 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"

#define MAX_CIRCLES  64

typedef struct {
    vec2 position;
    f32 radius;
    f32 alpha;
    f32 speed;
    Color color;
} CircleWave;

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr auto screenWidth = 800;
    constexpr auto screenHeight = 450;

    SetConfigFlags(FLAG_MSAA_4X_HINT);  // NOTE: Try to enable MSAA 4X

    InitWindow(screenWidth, screenHeight, "raylib [audio] example - module playing");

    InitAudioDevice();                  // Initialize audio device

    const Color colors[14] = { ORANGE, RED, GOLD, LIME, BLUE, VIOLET, BROWN, LIGHTGRAY, PINK,
                         YELLOW, GREEN, SKYBLUE, PURPLE, BEIGE };

    // Creates some circles for visual effect
    CircleWave circles[MAX_CIRCLES] = {};

    for (i32 i = MAX_CIRCLES - 1; i >= 0; i--)
    {
        circles[i].alpha = 0.0f;
        circles[i].radius = static_cast<f32>(GetRandomValue(10, 40));
        circles[i].position.x = static_cast<f32>(GetRandomValue(static_cast<i32>(circles[i].radius), static_cast<i32>(screenWidth - circles[i].radius)));
        circles[i].position.y = static_cast<f32>(GetRandomValue(static_cast<i32>(circles[i].radius), static_cast<i32>(screenHeight - circles[i].radius)));
        circles[i].speed = static_cast<f32>(GetRandomValue(1, 100))/2000.0f;
        circles[i].color = colors[GetRandomValue(0, 13)];
    }

    Music music = LoadMusicStream("resources/mini1111.xm");
    music.looping = false;
    auto pitch = 1.0f;

    PlayMusicStream(music);

    auto timePlayed = 0.0f;
    auto pause = false;

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        UpdateMusicStream(music);      // Update music buffer with new stream data

        // Restart music playing (stop and play)
        if (IsKeyPressed(KEY_SPACE))
        {
            StopMusicStream(music);
            PlayMusicStream(music);
            pause = false;
        }

        // Pause/Resume music playing
        if (IsKeyPressed(KEY_P))
        {
            pause = !pause;

            if (pause) PauseMusicStream(music);
            else ResumeMusicStream(music);
        }

        if (IsKeyDown(KEY_DOWN)) pitch -= 0.01f;
        else if (IsKeyDown(KEY_UP)) pitch += 0.01f;

        SetMusicPitch(music, pitch);

        // Get timePlayed scaled to bar dimensions
        timePlayed = GetMusicTimePlayed(music)/GetMusicTimeLength(music)*(screenWidth - 40);

        // Color circles animation
        for (i32 i = MAX_CIRCLES - 1; (i >= 0) && !pause; i--)
        {
            circles[i].alpha += circles[i].speed;
            circles[i].radius += circles[i].speed*10.0f;

            if (circles[i].alpha > 1.0f) circles[i].speed *= -1;

            if (circles[i].alpha <= 0.0f)
            {
                circles[i].alpha = 0.0f;
                circles[i].radius = static_cast<f32>(GetRandomValue(10, 40));
                circles[i].position.x = static_cast<f32>(GetRandomValue(static_cast<i32>(circles[i].radius), static_cast<i32>(screenWidth - circles[i].radius)));
                circles[i].position.y = static_cast<f32>(GetRandomValue(static_cast<i32>(circles[i].radius), static_cast<i32>(screenHeight - circles[i].radius)));
                circles[i].color = colors[GetRandomValue(0, 13)];
                circles[i].speed = static_cast<f32>(GetRandomValue(1, 100))/2000.0f;
            }
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            for (i32 i = MAX_CIRCLES - 1; i >= 0; i--)
            {
                DrawCircleV(circles[i].position, circles[i].radius, Fade(circles[i].color, circles[i].alpha));
            }

            // Draw time bar
            DrawRectangle(20, screenHeight - 20 - 12, screenWidth - 40, 12, LIGHTGRAY);
            DrawRectangle(20, screenHeight - 20 - 12, static_cast<i32>(timePlayed), 12, MAROON);
            DrawRectangleLines(20, screenHeight - 20 - 12, screenWidth - 40, 12, GRAY);

            // Draw help instructions
            DrawRectangle(20, 20, 425, 145, WHITE);
            DrawRectangleLines(20, 20, 425, 145, GRAY);
            DrawText("PRESS SPACE TO RESTART MUSIC", 40, 40, 20, BLACK);
            DrawText("PRESS P TO PAUSE/RESUME", 40, 70, 20, BLACK);
            DrawText("PRESS UP/DOWN TO CHANGE SPEED", 40, 100, 20, BLACK);
            DrawText(TextFormat("SPEED: %f", pitch), 40, 130, 20, MAROON);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadMusicStream(music);          // Unload music stream buffers from RAM

    CloseAudioDevice();     // Close audio device (music streaming is automatically stopped)

    CloseWindow();          // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
