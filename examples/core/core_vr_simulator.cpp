/*******************************************************************************************
*
*   raylib [core] example - vr simulator
*
*   Example complexity rating: [★★★☆] 3/4
*
*   Example originally created with raylib 2.5, last time updated with raylib 4.0
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2017-2025 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"

#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION        330
#else   // PLATFORM_ANDROID, PLATFORM_WEB
    #define GLSL_VERSION        100
#endif

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr auto screenWidth = 800;
    constexpr auto screenHeight = 450;

    // NOTE: screenWidth/screenHeight should match VR device aspect ratio
    InitWindow(screenWidth, screenHeight, "raylib [core] example - vr simulator");

    // VR device parameters definition
    VrDeviceInfo device = {
        // Oculus Rift CV1 parameters for simulator
        2160,                                // Horizontal resolution in pixels
        1200,                                // Vertical resolution in pixels
        0.133793f,                           // Horizontal size in meters
        0.0669f,                             // Vertical size in meters
        0.041f,                              // Distance between eye and display in meters
        0.07f,                               // Lens separation distance in meters
        0.07f,                               // IPD (distance between pupils) in meters

        // NOTE: CV1 uses fresnel-hybrid-asymmetric lenses with specific compute shaders
        // Following parameters are just an approximation to CV1 distortion stereo rendering
        { 1.0f, 0.22f, 0.24f, 0.0f },        // Lens distortion constant parameters
        { 0.996f, -0.004f, 1.014f, 0.0f }    // Chromatic aberration correction parameters
    };

    // Load VR stereo config for VR device parameteres (Oculus Rift CV1 parameters)
    auto config = LoadVrStereoConfig(device);

    // Distortion shader (uses device lens distortion and chroma)
    const Shader distortion = LoadShader({}, TextFormat("resources/shaders/glsl%i/distortion.fs", GLSL_VERSION));

    // Update distortion shader with lens and distortion-scale parameters
    SetShaderValue(distortion, GetShaderLocation(distortion, "leftLensCenter"),
                   config.leftLensCenter, SHADER_UNIFORM_VEC2);
    SetShaderValue(distortion, GetShaderLocation(distortion, "rightLensCenter"),
                   config.rightLensCenter, SHADER_UNIFORM_VEC2);
    SetShaderValue(distortion, GetShaderLocation(distortion, "leftScreenCenter"),
                   config.leftScreenCenter, SHADER_UNIFORM_VEC2);
    SetShaderValue(distortion, GetShaderLocation(distortion, "rightScreenCenter"),
                   config.rightScreenCenter, SHADER_UNIFORM_VEC2);

    SetShaderValue(distortion, GetShaderLocation(distortion, "scale"),
                   config.scale, SHADER_UNIFORM_VEC2);
    SetShaderValue(distortion, GetShaderLocation(distortion, "scaleIn"),
                   config.scaleIn, SHADER_UNIFORM_VEC2);
    SetShaderValue(distortion, GetShaderLocation(distortion, "deviceWarpParam"),
                   device.lensDistortionValues, SHADER_UNIFORM_VEC4);
    SetShaderValue(distortion, GetShaderLocation(distortion, "chromaAbParam"),
                   device.chromaAbCorrection, SHADER_UNIFORM_VEC4);

    // Initialize framebuffer for stereo rendering
    // NOTE: Screen size should match HMD aspect ratio
    const auto target = LoadRenderTexture(device.hResolution, device.vResolution);

    // The target's height is flipped (in the source Rectangle), due to OpenGL reasons
    const auto sourceRec = Rectangle{ 0.0f, 0.0f, static_cast<f32>(target.texture.width), -static_cast<f32>(target.texture.height) };
    const auto destRec = Rectangle{ 0.0f, 0.0f, static_cast<f32>(GetScreenWidth()), static_cast<f32>(GetScreenHeight()) };

    // Define the camera to look into our 3d world
    auto camera = Camera{};
    camera.position = vec3{ 5.0f, 2.0f, 5.0f };         // Camera position
    camera.target = vec3{ 0.0f, 2.0f, 0.0f };           // Camera looking at point
    camera.up = vec3{ 0.0f, 1.0f, 0.0f };               // Camera up vector
    camera.fovy = 60.0f;                                // Camera field-of-view Y
    camera.projection = CAMERA_PERSPECTIVE;             // Camera projection type

    const auto cubePosition = vec3{ 0.0f, 0.0f, 0.0f };

    DisableCursor();                    // Limit cursor to relative movement inside the window

    SetTargetFPS(60);                   // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())        // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        UpdateCamera(&camera, CAMERA_FIRST_PERSON);
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginTextureMode(target);
            ClearBackground(RAYWHITE);
            BeginVrStereoMode(config);
                BeginMode3D(camera);

                    DrawCube(cubePosition, 2.0f, 2.0f, 2.0f, RED);
                    DrawCubeWires(cubePosition, 2.0f, 2.0f, 2.0f, MAROON);
                    DrawGrid(40, 1.0f);

                EndMode3D();
            EndVrStereoMode();
        EndTextureMode();

        BeginDrawing();
            ClearBackground(RAYWHITE);
            BeginShaderMode(distortion);
                DrawTexturePro(target.texture, sourceRec, destRec, vec2{ 0.0f, 0.0f }, 0.0f, WHITE);
            EndShaderMode();
            DrawFPS(10, 10);
        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadVrStereoConfig(config);   // Unload stereo config

    UnloadRenderTexture(target);    // Unload stereo render fbo
    UnloadShader(distortion);       // Unload distortion shader

    CloseWindow();                  // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}