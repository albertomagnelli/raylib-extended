/*******************************************************************************************
*
*   raylib [textures] example - framebuffer rendering
*
*   Example complexity rating: [★★☆☆] 2/4
*
*   Example originally created with raylib 5.6, last time updated with raylib 5.6
*
*   Example contributed by Jack Boakes (@jackboakes) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2026 Jack Boakes (@jackboakes)
*
********************************************************************************************/

#include "raylib.h"
#include "raymath.h"

//------------------------------------------------------------------------------------
// Module Functions Declaration
//------------------------------------------------------------------------------------
static void DrawCameraPrism(Camera3D camera, f32 aspect, Color color);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr i32 screenWidth = 800;
    constexpr i32 screenHeight = 450;
    const i32 splitWidth = screenWidth/2;

    InitWindow(screenWidth, screenHeight, "raylib [textures] example - framebuffer rendering");

    // Camera to look at the 3D world
    Camera3D subjectCamera = {};
    subjectCamera.position = vec3{ 5.0f, 5.0f, 5.0f };
    subjectCamera.target = vec3{ 0.0f, 0.0f, 0.0f };
    subjectCamera.up = vec3{ 0.0f, 1.0f, 0.0f };
    subjectCamera.fovy = 45.0f;
    subjectCamera.projection = CAMERA_PERSPECTIVE;

    // Camera to observe the subject camera and 3D world
    Camera3D observerCamera = {};
    observerCamera.position = vec3{ 10.0f, 10.0f, 10.0f };
    observerCamera.target = vec3{ 0.0f, 0.0f, 0.0f };
    observerCamera.up = vec3{ 0.0f, 1.0f, 0.0f };
    observerCamera.fovy = 45.0f;
    observerCamera.projection = CAMERA_PERSPECTIVE;

    // Set up render textures
    const RenderTexture2D observerTarget = LoadRenderTexture(splitWidth, screenHeight);
    const Rectangle observerSource = { 0.0f, 0.0f, static_cast<f32>(observerTarget.texture.width), -static_cast<f32>(observerTarget.texture.height) };
    const Rectangle observerDest = { 0.0f, 0.0f, static_cast<f32>(splitWidth), static_cast<f32>(screenHeight) };

    const RenderTexture2D subjectTarget = LoadRenderTexture(splitWidth, screenHeight);
    const Rectangle subjectSource = { 0.0f, 0.0f, static_cast<f32>(subjectTarget.texture.width), -static_cast<f32>(subjectTarget.texture.height) };
    const Rectangle subjectDest = { static_cast<f32>(splitWidth), 0.0f, static_cast<f32>(splitWidth), static_cast<f32>(screenHeight) };
    const f32 textureAspectRatio = static_cast<f32>(subjectTarget.texture.width)/static_cast<f32>(subjectTarget.texture.height);

    // Rectangles for cropping render texture
    constexpr f32 captureSize = 128.0f;
    const Rectangle cropSource = { (subjectTarget.texture.width - captureSize)/2.0f, (subjectTarget.texture.height - captureSize)/2.0f, captureSize, -captureSize };
    const Rectangle cropDest = { splitWidth + 20.0f, 20.0f, captureSize, captureSize};

    SetTargetFPS(60);
    DisableCursor();
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        UpdateCamera(&observerCamera, CAMERA_FREE);
        UpdateCamera(&subjectCamera, CAMERA_ORBITAL);

        if (IsKeyPressed(KEY_R)) observerCamera.target = vec3{ 0.0f, 0.0f, 0.0f };

        // Build LHS observer view texture
        BeginTextureMode(observerTarget);

            ClearBackground(RAYWHITE);

            BeginMode3D(observerCamera);

                DrawGrid(10, 1.0f);
                DrawCube(vec3{ 0.0f, 0.0f, 0.0f }, 2.0f, 2.0f, 2.0f, GOLD);
                DrawCubeWires(vec3{ 0.0f, 0.0f, 0.0f }, 2.0f, 2.0f, 2.0f, PINK);
                DrawCameraPrism(subjectCamera, textureAspectRatio, GREEN);

            EndMode3D();

            DrawText("Observer View", 10, observerTarget.texture.height - 30, 20, BLACK);
            DrawText("WASD + Mouse to Move", 10, 10, 20, DARKGRAY);
            DrawText("Scroll to Zoom", 10, 30, 20, DARKGRAY);
            DrawText("R to Reset Observer Target", 10, 50, 20, DARKGRAY);

        EndTextureMode();

        // Build RHS subject view texture
        BeginTextureMode(subjectTarget);

            ClearBackground(RAYWHITE);

            BeginMode3D(subjectCamera);

                DrawCube(vec3{ 0.0f, 0.0f, 0.0f }, 2.0f, 2.0f, 2.0f, GOLD);
                DrawCubeWires(vec3{ 0.0f, 0.0f, 0.0f }, 2.0f, 2.0f, 2.0f, PINK);
                DrawGrid(10, 1.0f);

            EndMode3D();

            DrawRectangleLines(static_cast<i32>((subjectTarget.texture.width - captureSize)/2.0f), static_cast<i32>((subjectTarget.texture.height - captureSize)/2.0f), static_cast<i32>(captureSize), static_cast<i32>(captureSize), GREEN);
            DrawText("Subject View", 10, subjectTarget.texture.height - 30, 20, BLACK);

        EndTextureMode();
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(BLACK);

            // Draw observer texture LHS
            DrawTexturePro(observerTarget.texture, observerSource, observerDest, vec2{0.0f, 0.0f }, 0.0f, WHITE);

            // Draw subject texture RHS
            DrawTexturePro(subjectTarget.texture, subjectSource, subjectDest, vec2{ 0.0f, 0.0f }, 0.0f, WHITE);

            // Draw the small crop overlay on top
            DrawTexturePro(subjectTarget.texture, cropSource, cropDest, vec2{ 0.0f, 0.0f }, 0.0f, WHITE);
            DrawRectangleLinesEx(cropDest, 2, BLACK);

            // Draw split screen divider line
            DrawLine(splitWidth, 0, splitWidth, screenHeight, BLACK);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadRenderTexture(observerTarget);
    UnloadRenderTexture(subjectTarget);

    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------
static void DrawCameraPrism(Camera3D camera, f32 aspect, Color color)
{
    const f32 length = glm::distance(camera.position, camera.target);
    // Define the 4 corners of the camera's prism plane sliced at the target in Normalized Device Coordinates
    const vec3 planeNDC[4] = {
        { -1.0f, -1.0f, 1.0f }, // Bottom Left
        {  1.0f, -1.0f, 1.0f }, // Bottom Right
        {  1.0f,  1.0f, 1.0f }, // Top Right
        { -1.0f,  1.0f, 1.0f }  // Top Left
    };

    // Build the matrices
    const mat4 view = GetCameraMatrix(camera);
    const mat4 proj = glm::perspective(camera.fovy * DEG2RAD, aspect, 0.05f, length);
    // Combine view and projection so we can reverse the full camera transform
    const mat4 viewProj = ((proj) * (view));
    // Invert the view-projection matrix to unproject points from NDC space back into world space
    mat4 inverseViewProj = glm::inverse(viewProj);

    // Transform the 4 plane corners from NDC into world space
    vec3 corners[4];
    for (i32 i = 0; i < 4; i++)
    {
        const f32 x = planeNDC[i].x;
        const f32 y = planeNDC[i].y;
        const f32 z = planeNDC[i].z;

        // Multiply NDC position by the inverse view-projection matrix
        // This produces a homogeneous (x, y, z, w) position in world space
        const f32 vx = inverseViewProj[0][0]*x + inverseViewProj[1][0]*y + inverseViewProj[2][0]*z + inverseViewProj[3][0];
        const f32 vy = inverseViewProj[0][1]*x + inverseViewProj[1][1]*y + inverseViewProj[2][1]*z + inverseViewProj[3][1];
        const f32 vz = inverseViewProj[0][2]*x + inverseViewProj[1][2]*y + inverseViewProj[2][2]*z + inverseViewProj[3][2];
        const f32 vw = inverseViewProj[0][3]*x + inverseViewProj[1][3]*y + inverseViewProj[2][3]*z + inverseViewProj[3][3];

        corners[i] = vec3{ vx/vw, vy/vw, vz/vw };
    }

    // Draw the far plane sliced at the target
    DrawLine3D(corners[0], corners[1], color);
    DrawLine3D(corners[1], corners[2], color);
    DrawLine3D(corners[2], corners[3], color);
    DrawLine3D(corners[3], corners[0], color);

    // Draw the prism lines from the far plane to the camera position
    for (i32 i = 0; i < 4; i++)
    {
        DrawLine3D(camera.position, corners[i], color);
    }
}