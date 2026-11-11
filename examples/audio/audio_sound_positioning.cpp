/*******************************************************************************************
*
*   raylib [audio] example - sound positioning
*
*   Example complexity rating: [★★☆☆] 2/4
*
*   Example originally created with raylib 5.5, last time updated with raylib 6.0
*
*   Example contributed by Le Juez Victor (@Bigfoot71) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2025 Le Juez Victor (@Bigfoot71)
*
********************************************************************************************/

#include "raylib.h"

#include "raymath.h"

//------------------------------------------------------------------------------------
// Module Functions Declaration
//------------------------------------------------------------------------------------
static void SetSoundPosition(Camera listener, Sound sound, glm::vec3 position, float maxDist);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr int screenWidth = 800;
    constexpr int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [audio] example - sound positioning");

    InitAudioDevice();

    const Sound sound = LoadSound("resources/coin.wav");

    Camera camera = {
        .position = glm::vec3{ 0, 5, 5 },
        .target = glm::vec3{ 0, 0, 0 },
        .up = glm::vec3{ 0, 1, 0 },
        .fovy = 60,
        .projection = CAMERA_PERSPECTIVE
    };

    DisableCursor();

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())
    {
        // Update
        //----------------------------------------------------------------------------------
        UpdateCamera(&camera, CAMERA_FREE);

        const float th = static_cast<float>(GetTime());

        const glm::vec3 spherePos = { 5.0f*cosf(th), 0.0f, 5.0f*sinf(th) };

        SetSoundPosition(camera, sound, spherePos, 1.0f);

        if (!IsSoundPlaying(sound)) PlaySound(sound);
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            BeginMode3D(camera);
                DrawGrid(10, 2);
                DrawSphere(spherePos, 0.5f, RED);
            EndMode3D();

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadSound(sound);
    CloseAudioDevice();     // Close audio device

    CloseWindow();          // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//------------------------------------------------------------------------------------
// Module Functions Definition
//------------------------------------------------------------------------------------
// Set sound 3d position
static void SetSoundPosition(Camera listener, Sound sound, glm::vec3 position, float maxDist)
{
    // Calculate direction vector and distance between listener and sound source
    const glm::vec3 direction = ((position) - (listener.position));
    const float distance = glm::length(direction);

    // Apply logarithmic distance attenuation and clamp between 0-1
    float attenuation = 1.0f/(1.0f + (distance/maxDist));
    attenuation = Clamp(attenuation, 0.0f, 1.0f);

    // Calculate normalized vectors for spatial positioning
    const glm::vec3 normalizedDirection = glm::normalize(direction);
    const glm::vec3 forward = glm::normalize(((listener.target) - (listener.position)));
    const glm::vec3 right = glm::normalize(glm::cross(forward, listener.up));

    // Reduce volume for sounds behind the listener
    const float dotProduct = glm::dot(forward, normalizedDirection);
    if (dotProduct < 0.0f) attenuation *= (1.0f + dotProduct*0.5f);

    // Set stereo panning based on sound position relative to listener
    const float pan = glm::dot(normalizedDirection, right);

    // Apply final sound properties
    SetSoundVolume(sound, attenuation);
    SetSoundPan(sound, pan);
}
