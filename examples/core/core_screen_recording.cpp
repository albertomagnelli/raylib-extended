/*******************************************************************************************
*
*   raylib [core] example - screen recording
*
*   Example complexity rating: [★★☆☆] 2/4
*
*   Example originally created with raylib 6.0, last time updated with raylib 6.0
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2025 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"

// Using msf_gif library to record frames into GIF
#define MSF_GIF_IMPL
#include "msf_gif.h"            // GIF recording functionality

#include <math.h>               // Required for: sinf()

#define GIF_RECORD_FRAMERATE     5     // Record framerate, we get a frame every N frames

#define MAX_SINEWAVE_POINTS    256

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr i32 screenWidth = 800;
    constexpr i32 screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [core] example - screen recording");

    bool gifRecording = false;           // GIF recording state
    u32 gifFrameCounter = 0;    // GIF frames counter
    MsfGifState gifState = {};           // MSGIF context state

    vec2 circlePosition = { 0.0f, screenHeight/2.0f };
    f32 timeCounter = 0.0f;

    // Get sine wave points for line drawing
    vec2 sinePoints[MAX_SINEWAVE_POINTS] = {};
    for (i32 i = 0; i < MAX_SINEWAVE_POINTS; i++)
    {
        sinePoints[i].x = i*GetScreenWidth()/180.0f;
        sinePoints[i].y = screenHeight/2.0f + 150*sinf((2*PI/1.5f)*(1.0f/60.0f)*static_cast<f32>(i)); // Calculate for 60 fps
    }

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        // Update circle sinusoidal movement
        timeCounter += GetFrameTime();
        circlePosition.x += GetScreenWidth()/180.0f;
        circlePosition.y = screenHeight/2.0f + 150*sinf((2*PI/1.5f)*timeCounter);
        if (circlePosition.x > screenWidth)
        {
            circlePosition.x = 0.0f;
            circlePosition.y = screenHeight/2.0f;
            timeCounter = 0.0f;
        }

        // Start-Stop GIF recording on CTRL+R
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_R))
        {
            if (gifRecording)
            {
                // Stop current recording and save file
                gifRecording = false;
                const MsfGifResult result = msf_gif_end(&gifState);
                SaveFileData(TextFormat("%s/screenrecording.gif", GetApplicationDirectory().c_str()), result.data, static_cast<u32>(result.dataSize));
                msf_gif_free(result);

                TraceLog(LOG_INFO, "Finish animated GIF recording");
            }
            else
            {
                // Start a new recording
                gifRecording = true;
                gifFrameCounter = 0;
                msf_gif_begin(&gifState, GetRenderWidth(), GetRenderHeight());

                TraceLog(LOG_INFO, "Start animated GIF recording");
            }
        }

        if (gifRecording)
        {
            gifFrameCounter++;

            // NOTE: We record one gif frame depending on the desired gif framerate
            if (gifFrameCounter > GIF_RECORD_FRAMERATE)
            {
                // Get image data for the current frame (from backbuffer)
                // WARNING: This process is quite slow, it can generate stuttering
                const Image imScreen = LoadImageFromScreen();

                // Add the frame to the gif recording, providing and "estimated" time for display in centiseconds
                msf_gif_frame(&gifState, static_cast<u8 *>(imScreen.data), static_cast<i32>((1.0f/60.0f)*GIF_RECORD_FRAMERATE)/10, 16, imScreen.width*4);
                gifFrameCounter = 0;

                UnloadImage(imScreen);    // Free image data
            }
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            for (i32 i = 0; i < (MAX_SINEWAVE_POINTS - 1); i++)
            {
                DrawLineV(sinePoints[i], sinePoints[i + 1], MAROON);
                DrawCircleV(sinePoints[i], 3, MAROON);
            }

            DrawCircleV(circlePosition, 30, RED);

            DrawFPS(10, 10);

            /*
            // Draw record indicator
            // WARNING: If drawn here, it will appear in the recorded image,
            // use a render texture instead for the recording and LoadImageFromTexture(rt.texture)
            if (gifRecording)
            {
                // Display the recording indicator every half-second
                if ((int)(GetTime()/0.5)%2 == 1)
                {
                    DrawCircle(30, GetScreenHeight() - 20, 10, MAROON);
                    DrawText("GIF RECORDING", 50, GetScreenHeight() - 25, 10, RED);
                }
            }
            */
        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    // If still recording a GIF on close window, just finish
    if (gifRecording)
    {
        const MsfGifResult result = msf_gif_end(&gifState);
        msf_gif_free(result);
        gifRecording = false;
    }

    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
