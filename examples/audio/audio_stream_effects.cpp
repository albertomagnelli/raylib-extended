/*******************************************************************************************
*
*   raylib [audio] example - stream effects
*
*   Example complexity rating: [★★★★] 4/4
*
*   Example originally created with raylib 4.2, last time updated with raylib 5.0
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2022-2025 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"

#include <stdlib.h> // Required for: NULL

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
static f32 *delayBuffer = nullptr;
static u32 delayBufferSize = 0;
static u32 delayReadIndex = 2;
static u32 delayWriteIndex = 0;

//------------------------------------------------------------------------------------
// Module Functions Declaration
//------------------------------------------------------------------------------------
static void AudioProcessEffectLPF(void *buffer, u32 frames);   // Audio effect: lowpass filter
static void AudioProcessEffectDelay(void *buffer, u32 frames); // Audio effect: delay

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr auto screenWidth = 800;
    constexpr auto screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [audio] example - stream effects");

    InitAudioDevice();              // Initialize audio device

    const Music music = LoadMusicStream("resources/country.mp3");

    // Allocate buffer for the delay effect
    delayBufferSize = 48000*2;      // 1 second delay (device sampleRate*channels)
    delayBuffer = static_cast<f32 *>(calloc(delayBufferSize, sizeof(f32)));

    PlayMusicStream(music);

    auto timePlayed = 0.0f;        // Time played normalized [0.0f..1.0f]
    auto pause = false;             // Music playing paused

    auto enableEffectLPF = false;   // Enable effect low-pass-filter
    auto enableEffectDelay = false; // Enable effect delay (1 second)

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        UpdateMusicStream(music);   // Update music buffer with new stream data

        // Restart music playing (stop and play)
        if (IsKeyPressed(KEY_SPACE))
        {
            StopMusicStream(music);
            PlayMusicStream(music);
        }

        // Pause/Resume music playing
        if (IsKeyPressed(KEY_P))
        {
            pause = !pause;

            if (pause) PauseMusicStream(music);
            else ResumeMusicStream(music);
        }

        // Add/Remove effect: lowpass filter
        if (IsKeyPressed(KEY_F))
        {
            enableEffectLPF = !enableEffectLPF;
            if (enableEffectLPF) AttachAudioStreamProcessor(music.stream, AudioProcessEffectLPF);
            else DetachAudioStreamProcessor(music.stream, AudioProcessEffectLPF);
        }

        // Add/Remove effect: delay
        if (IsKeyPressed(KEY_D))
        {
            enableEffectDelay = !enableEffectDelay;
            if (enableEffectDelay) AttachAudioStreamProcessor(music.stream, AudioProcessEffectDelay);
            else DetachAudioStreamProcessor(music.stream, AudioProcessEffectDelay);
        }

        // Get normalized time played for current music stream
        timePlayed = GetMusicTimePlayed(music)/GetMusicTimeLength(music);

        if (timePlayed > 1.0f) timePlayed = 1.0f;   // Make sure time played is no longer than music
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawText("MUSIC SHOULD BE PLAYING!", 245, 150, 20, LIGHTGRAY);

            DrawRectangle(200, 180, 400, 12, LIGHTGRAY);
            DrawRectangle(200, 180, static_cast<i32>(timePlayed*400.0f), 12, MAROON);
            DrawRectangleLines(200, 180, 400, 12, GRAY);

            DrawText("PRESS SPACE TO RESTART MUSIC", 215, 230, 20, LIGHTGRAY);
            DrawText("PRESS P TO PAUSE/RESUME MUSIC", 208, 260, 20, LIGHTGRAY);

            DrawText(TextFormat("PRESS F TO TOGGLE LPF EFFECT: %s", enableEffectLPF? "ON" : "OFF"), 200, 320, 20, GRAY);
            DrawText(TextFormat("PRESS D TO TOGGLE DELAY EFFECT: %s", enableEffectDelay? "ON" : "OFF"), 180, 350, 20, GRAY);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadMusicStream(music);   // Unload music stream buffers from RAM

    CloseAudioDevice();         // Close audio device (music streaming is automatically stopped)

    free(delayBuffer);       // Free delay buffer

    CloseWindow();              // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//------------------------------------------------------------------------------------
// Module Functions Definition
//------------------------------------------------------------------------------------
// Audio effect: lowpass filter
static void AudioProcessEffectLPF(void *buffer, u32 frames)
{
    static f32 low[2] = { 0.0f, 0.0f };
    static const f32 cutoff = 70.0f/44100.0f; // 70 Hz lowpass filter
    const f32 k = cutoff/(cutoff + 0.1591549431f); // RC filter formula

    // Converts the buffer data before using it
    auto bufferData = static_cast<f32 *>(buffer);
    for (u32 i = 0; i < frames*2; i += 2)
    {
        const f32 l = bufferData[i];
        const f32 r = bufferData[i + 1];

        low[0] += k*(l - low[0]);
        low[1] += k*(r - low[1]);
        bufferData[i] = low[0];
        bufferData[i + 1] = low[1];
    }
}

// Audio effect: delay
static void AudioProcessEffectDelay(void *buffer, u32 frames)
{
    for (u32 i = 0; i < frames*2; i += 2)
    {
        const f32 leftDelay = delayBuffer[delayReadIndex++];    // ERROR: Reading buffer -> WHY??? Maybe thread related???
        const f32 rightDelay = delayBuffer[delayReadIndex++];

        if (delayReadIndex == delayBufferSize) delayReadIndex = 0;

        (static_cast<f32 *>(buffer))[i] = 0.5f*(static_cast<f32 *>(buffer))[i] + 0.5f*leftDelay;
        (static_cast<f32 *>(buffer))[i + 1] = 0.5f*(static_cast<f32 *>(buffer))[i + 1] + 0.5f*rightDelay;

        delayBuffer[delayWriteIndex++] = (static_cast<f32 *>(buffer))[i];
        delayBuffer[delayWriteIndex++] = (static_cast<f32 *>(buffer))[i + 1];
        if (delayWriteIndex == delayBufferSize) delayWriteIndex = 0;
    }
}
