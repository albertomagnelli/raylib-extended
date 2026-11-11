/*******************************************************************************************
*
*   raylib [shaders] example - spotlight rendering
*
*   Example complexity rating: [★★☆☆] 2/4
*
*   Example originally created with raylib 2.5, last time updated with raylib 3.7
*
*   Example contributed by Chris Camacho (@chriscamacho) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2019-2025 Chris Camacho (@chriscamacho) and Ramon Santamaria (@raysan5)
*
********************************************************************************************
*
*   The shader makes alpha holes in the forground to give the appearance of a top
*   down look at a spotlight casting a pool of light...
*
*   The right hand side of the screen there is just enough light to see whats
*   going on without the spot light, great for a stealth type game where you
*   have to avoid the spotlights
*
*   The left hand side of the screen is in pitch dark except for where the spotlights are
*
*   Although this example doesn't scale like the letterbox example, you could integrate
*   the two techniques, but by scaling the actual colour of the render texture rather
*   than using alpha as a mask
*
********************************************************************************************/

#include "raylib.h"

#include "raymath.h"

#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION            330
#else   // PLATFORM_ANDROID, PLATFORM_WEB
    #define GLSL_VERSION            100
#endif

#define MAX_SPOTS         3        // NOTE: It must be the same as define in shader
#define MAX_STARS       400

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// Spot data
typedef struct Spot {
    vec2 position;
    vec2 speed;
    f32 inner;
    f32 radius;

    // Shader locations
    u32 positionLoc;
    u32 innerLoc;
    u32 radiusLoc;
} Spot;

// Stars in the star field have a position and velocity
typedef struct Star {
    vec2 position;
    vec2 speed;
} Star;

//--------------------------------------------------------------------------------------
// Module Functions Declaration
//--------------------------------------------------------------------------------------
static void UpdateStar(Star *star);
static void ResetStar(Star *star);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr i32 screenWidth = 800;
    constexpr i32 screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [shaders] example - spotlight rendering");
    HideCursor();

    const Texture texRay = LoadTexture("resources/raysan.png");

    Star stars[MAX_STARS] = {};

    for (i32 n = 0; n < MAX_STARS; n++) ResetStar(&stars[n]);

    // Progress all the stars on, so they don't all start in the centre
    for (i32 m = 0; m < screenWidth/2.0; m++)
    {
        for (i32 n = 0; n < MAX_STARS; n++) UpdateStar(&stars[n]);
    }

    i32 frameCounter = 0;

    // Use default vert shader
    const Shader shdrSpot = LoadShader({}, TextFormat("resources/shaders/glsl%i/spotlight.fs", GLSL_VERSION));

    // Get the locations of spots in the shader
    Spot spots[MAX_SPOTS];

    for (i32 i = 0; i < MAX_SPOTS; i++)
    {
        char posName[32] = "spots[x].pos\0";
        char innerName[32] = "spots[x].inner\0";
        char radiusName[32] = "spots[x].radius\0";

        posName[6] = '0' + i;
        innerName[6] = '0' + i;
        radiusName[6] = '0' + i;

        spots[i].positionLoc = GetShaderLocation(shdrSpot, posName);
        spots[i].innerLoc = GetShaderLocation(shdrSpot, innerName);
        spots[i].radiusLoc = GetShaderLocation(shdrSpot, radiusName);
    }

    // Tell the shader how wide the screen is so we can have
    // a pitch black half and a dimly lit half
    const u32 wLoc = GetShaderLocation(shdrSpot, "screenWidth");
    f32 sw = static_cast<f32>(GetScreenWidth());
    SetShaderValue(shdrSpot, wLoc, &sw, SHADER_UNIFORM_FLOAT);

    // Randomize the locations and velocities of the spotlights
    // and initialize the shader locations
    for (i32 i = 0; i < MAX_SPOTS; i++)
    {
        spots[i].position.x = static_cast<f32>(GetRandomValue(64, screenWidth - 64));
        spots[i].position.y = static_cast<f32>(GetRandomValue(64, screenHeight - 64));
        spots[i].speed = vec2{ 0, 0 };

        while ((fabs(spots[i].speed.x) + fabs(spots[i].speed.y)) < 2)
        {
            spots[i].speed.x = GetRandomValue(-400, 40)/25.0f;
            spots[i].speed.y = GetRandomValue(-400, 40)/25.0f;
        }

        spots[i].inner = 28.0f*(i + 1);
        spots[i].radius = 48.0f*(i + 1);

        SetShaderValue(shdrSpot, spots[i].positionLoc, &spots[i].position.x, SHADER_UNIFORM_VEC2);
        SetShaderValue(shdrSpot, spots[i].innerLoc, &spots[i].inner, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shdrSpot, spots[i].radiusLoc, &spots[i].radius, SHADER_UNIFORM_FLOAT);
    }

    SetTargetFPS(60);               // Set  to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        frameCounter++;

        // Move the stars, resetting them if the go offscreen
        for (i32 n = 0; n < MAX_STARS; n++) UpdateStar(&stars[n]);

        // Update the spots, send them to the shader
        for (i32 i = 0; i < MAX_SPOTS; i++)
        {
            if (i == 0)
            {
                const vec2 mp = GetMousePosition();
                spots[i].position.x = mp.x;
                spots[i].position.y = screenHeight - mp.y;
            }
            else
            {
                spots[i].position.x += spots[i].speed.x;
                spots[i].position.y += spots[i].speed.y;

                if (spots[i].position.x < 64) spots[i].speed.x = -spots[i].speed.x;
                if (spots[i].position.x > (screenWidth - 64)) spots[i].speed.x = -spots[i].speed.x;
                if (spots[i].position.y < 64) spots[i].speed.y = -spots[i].speed.y;
                if (spots[i].position.y > (screenHeight - 64)) spots[i].speed.y = -spots[i].speed.y;
            }

            SetShaderValue(shdrSpot, spots[i].positionLoc, &spots[i].position.x, SHADER_UNIFORM_VEC2);
        }

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(DARKBLUE);

            // Draw stars and bobs
            for (i32 n = 0; n < MAX_STARS; n++)
            {
                // Single pixel is just too small these days!
                DrawRectangle(static_cast<i32>(stars[n].position.x), static_cast<i32>(stars[n].position.y), 2, 2, WHITE);
            }

            for (i32 i = 0; i < 16; i++)
            {
                DrawTexture(texRay,
                    static_cast<i32>((screenWidth/2.0f) + cos((frameCounter + i*8)/51.45f)*(screenWidth/2.2f) - 32),
                    static_cast<i32>((screenHeight/2.0f) + sin((frameCounter + i*8)/17.87f)*(screenHeight/4.2f)), WHITE);
            }

            // Draw spot lights
            BeginShaderMode(shdrSpot);
                // Instead of a blank rectangle you could render here
                // a render texture of the full screen used to do screen
                // scaling (slight adjustment to shader would be required
                // to actually pay attention to the colour!)
                DrawRectangle(0, 0, screenWidth, screenHeight, WHITE);
            EndShaderMode();

            DrawFPS(10, 10);

            DrawText("Move the mouse!", 10, 30, 20, GREEN);
            DrawText("Pitch Black", static_cast<i32>(screenWidth*0.2f), screenHeight/2, 20, GREEN);
            DrawText("Dark", static_cast<i32>(screenWidth*.66f), screenHeight/2, 20, GREEN);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadTexture(texRay);
    UnloadShader(shdrSpot);

    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//--------------------------------------------------------------------------------------
// Module Functions Definition
//--------------------------------------------------------------------------------------
static void ResetStar(Star *star)
{
    star->position = vec2{ GetScreenWidth()/2.0f, GetScreenHeight()/2.0f };

    star->speed.x = static_cast<f32>(GetRandomValue(-1000, 1000))/100.0f;
    star->speed.y = static_cast<f32>(GetRandomValue(-1000, 1000))/100.0f;

    while (!(fabs(star->speed.x) + (fabs(star->speed.y) > 1)))
    {
        star->speed.x = static_cast<f32>(GetRandomValue(-1000, 1000))/100.0f;
        star->speed.y = static_cast<f32>(GetRandomValue(-1000, 1000))/100.0f;
    }

    star->position = ((star->position) + (((star->speed) * (vec2{ 8.0f, 8.0f }))));
}

static void UpdateStar(Star *star)
{
    star->position = ((star->position) + (star->speed));

    if ((star->position.x < 0) || (star->position.x > GetScreenWidth()) ||
        (star->position.y < 0) || (star->position.y > GetScreenHeight())) ResetStar(star);
}
