/*******************************************************************************************
*
*   raylib [models] example - heightmap rendering
*
*   Example complexity rating: [★☆☆☆] 1/4
*
*   Example originally created with raylib 1.8, last time updated with raylib 3.5
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2015-2025 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr int screenWidth = 800;
    constexpr int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [models] example - heightmap rendering");

    // Define our custom camera to look into our 3d world
    Camera camera = {};
    camera.position = glm::vec3{ 18.0f, 21.0f, 18.0f };   // Camera position
    camera.target = glm::vec3{ 0.0f, 0.0f, 0.0f };        // Camera looking at point
    camera.up = glm::vec3{ 0.0f, 1.0f, 0.0f };            // Camera up vector (rotation towards target)
    camera.fovy = 45.0f;                                    // Camera field-of-view Y
    camera.projection = CAMERA_PERSPECTIVE;                 // Camera projection type

    const Image image = LoadImage("resources/heightmap.png");     // Load heightmap image (RAM)
    const Texture2D texture = LoadTextureFromImage(image);        // Convert image to texture (VRAM)

    const Mesh mesh = GenMeshHeightmap(image, glm::vec3{ 16, 8, 16 }); // Generate heightmap mesh (RAM and VRAM)
    const Model model = LoadModelFromMesh(mesh);                  // Load model from generated mesh

    model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture; // Set map diffuse texture
    const glm::vec3 mapPosition = { -8.0f, 0.0f, -8.0f };         // Define model position

    UnloadImage(image);             // Unload heightmap image from RAM, already uploaded to VRAM

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        UpdateCamera(&camera, CAMERA_ORBITAL);
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            BeginMode3D(camera);

                DrawModel(model, mapPosition, 1.0f, RED);

                DrawGrid(20, 1.0f);

            EndMode3D();

            DrawTexture(texture, screenWidth - texture.width - 20, 20, WHITE);
            DrawRectangleLines(screenWidth - texture.width - 20, 20, texture.width, texture.height, GREEN);

            DrawFPS(10, 10);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadTexture(texture);     // Unload texture
    UnloadModel(model);         // Unload model

    CloseWindow();              // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}