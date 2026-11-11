/*******************************************************************************************
*
*   raylib [models] example - animation timing
*
*   Example complexity rating: [★★★☆] 3/4
*
*   Example originally created with raylib 6.0, last time updated with raylib 6.0
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2026 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"             // Required for: UI controls

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr i32 screenWidth = 800;
    constexpr i32 screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [models] example - animation timing");

    // Define the camera to look into our 3d world
    Camera camera = {};
    camera.position = vec3{ 6.0f, 6.0f, 6.0f };         // Camera position
    camera.target = vec3{ 0.0f, 2.0f, 0.0f };           // Camera looking at point
    camera.up = vec3{ 0.0f, 1.0f, 0.0f };               // Camera up vector (rotation towards target)
    camera.fovy = 45.0f;                                // Camera field-of-view Y
    camera.projection = CAMERA_PERSPECTIVE;             // Camera projection type

    // Load model
    const Model model = LoadModel("resources/models/gltf/robot.glb");
    const vec3 position = { 0.0f, 0.0f, 0.0f }; // Set model world position

    // Load model animations
    i32 animCount = 0;
    ModelAnimation *anims = LoadModelAnimations("resources/models/gltf/robot.glb", &animCount);

    // Animation playing variables
    i32 animIndex = 10;                  // Current animation playing
    f32 animCurrentFrame = 0.0f;       // Current animation frame (supporting interpolated frames)
    f32 animFrameSpeed = 0.5f;         // Animation play speed
    bool animPause = false;              // Pause animation

    // UI required variables
    std::vector<std::string> animNames;
    for (i32 i = 0; i < animCount; i++) animNames.push_back(anims[i].name);

    bool dropdownEditMode = false;
    f32 animFrameProgress = 0.0f;

    SetTargetFPS(60);                   // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())        // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        UpdateCamera(&camera, CAMERA_ORBITAL);

        if (IsKeyPressed(KEY_P)) animPause = !animPause;

        if (!animPause && (animIndex < animCount))
        {
            // Update model animation
            animCurrentFrame += animFrameSpeed;
            if (animCurrentFrame >= anims[animIndex].keyframeCount) animCurrentFrame = 0.0f;
            UpdateModelAnimation(model, anims[animIndex], animCurrentFrame);
        }

        // NOTE: Animation and playing speed selected through UI

        // Update progressbar value with current frame
        animFrameProgress = animCurrentFrame;
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            BeginMode3D(camera);

                DrawModel(model, position, 1.0f, WHITE);

                DrawGrid(10, 1.0f);

            EndMode3D();

            // Draw UI, select anim and playing speed
            GuiSetStyle(DROPDOWNBOX, DROPDOWN_ITEMS_SPACING, 1);
            if (GuiDropdownBox(Rectangle{ 10, 10, 140, 24 }, TextJoin(animNames, ";").c_str(),
                &animIndex, dropdownEditMode)) dropdownEditMode = !dropdownEditMode;

            GuiSlider(Rectangle{ 260, 10, 500, 24 }, "FRAME SPEED: ", TextFormat("x%.1f", animFrameSpeed).c_str(),
                &animFrameSpeed, 0.1f, 2.0f);

            // Draw playing timeline with keyframes
            GuiLabel(Rectangle{ 10, GetScreenHeight() - 64.0f, GetScreenWidth() - 20.0f, 24 },
                TextFormat("CURRENT FRAME: %.2f / %i", animFrameProgress, anims[animIndex].keyframeCount).c_str());
            GuiProgressBar(Rectangle{ 10, GetScreenHeight() - 40.0f, GetScreenWidth() - 20.0f, 24 }, nullptr, nullptr,
                &animFrameProgress, 0.0f, static_cast<f32>(anims[animIndex].keyframeCount));
            for (i32 i = 0; i < anims[animIndex].keyframeCount; i++)
                DrawRectangle(10 + static_cast<i32>((static_cast<f32>(GetScreenWidth() - 20)/static_cast<f32>(anims[animIndex].keyframeCount))*static_cast<f32>(i)),
                    GetScreenHeight() - 40, 1, 24, BLUE);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadModelAnimations(anims, animCount); // Unload model animation
    UnloadModel(model);                      // Unload model and meshes/material

    CloseWindow();              // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

