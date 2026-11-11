/*******************************************************************************************
*
*   raylib [shaders] example - mesh instancing
*
*   Example complexity rating: [★★★★] 4/4
*
*   Example originally created with raylib 3.7, last time updated with raylib 4.2
*
*   Example contributed by seanpringle (@seanpringle) and reviewed by Max (@moliad) and Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2020-2025 seanpringle (@seanpringle), Max (@moliad) and Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"
#include "raymath.h"

#include <initializer_list>

#define RLIGHTS_IMPLEMENTATION
#include "rlights.h"

#include <stdlib.h>         // Required for: calloc(), free()

#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION            330
#else   // PLATFORM_ANDROID, PLATFORM_WEB
    #define GLSL_VERSION            100
#endif

#define MAX_INSTANCES  10000

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr i32 screenWidth = 800;
    constexpr i32 screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [shaders] example - mesh instancing");

    // Define the camera to look into our 3d world
    Camera camera = {};
    camera.position = vec3{ -125.0f, 125.0f, -125.0f };         // Camera position
    camera.target = vec3{ 0.0f, 0.0f, 0.0f };                   // Camera looking at point
    camera.up = vec3{ 0.0f, 1.0f, 0.0f };                       // Camera up vector (rotation towards target)
    camera.fovy = 45.0f;                                        // Camera field-of-view Y
    camera.projection = CAMERA_PERSPECTIVE;                     // Camera projection type

    // Define mesh to be instanced
    const Mesh cube = GenMeshCube(1.0f, 1.0f, 1.0f);

    // Define transforms to be uploaded to GPU for instances
    mat4 *transforms = static_cast<mat4 *>(calloc(MAX_INSTANCES, sizeof(mat4)));   // Pre-multiplied transformations passed to rlgl

    // Translate and rotate cubes randomly
    for (i32 i = 0; i < MAX_INSTANCES; i++)
    {
        const mat4 translation = glm::translate(mat4(1.0f), vec3{static_cast<f32>(GetRandomValue(-50, 50)), static_cast<f32>(GetRandomValue(-50, 50)), static_cast<f32>(GetRandomValue(-50, 50))});
        const vec3 axis = glm::normalize(vec3{ static_cast<f32>(GetRandomValue(0, 360)), static_cast<f32>(GetRandomValue(0, 360)), static_cast<f32>(GetRandomValue(0, 360)) });
        const f32 angle = static_cast<f32>(GetRandomValue(0, 180))*DEG2RAD;
        const mat4 rotation = glm::rotate(mat4(1.0f), angle, axis);

        transforms[i] = ((translation) * (rotation));
    }

    // Load lighting shader
    const Shader shader = LoadShader(TextFormat("resources/shaders/glsl%i/lighting_instancing.vs", GLSL_VERSION),
                               TextFormat("resources/shaders/glsl%i/lighting.fs", GLSL_VERSION));
    // Get shader locations
    shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(shader, "mvp");
    shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");

    // Set shader value: ambient light level
    const i32 ambientLoc = GetShaderLocation(shader, "ambient");
    SetShaderValue(shader, ambientLoc, std::initializer_list<f32>{ 0.2f, 0.2f, 0.2f, 1.0f }.begin(), SHADER_UNIFORM_VEC4);

    // Create one light
    CreateLight(LIGHT_DIRECTIONAL, vec3{ 50.0f, 50.0f, 0.0f }, vec3{}, WHITE, shader);

    // NOTE: We are assigning the intancing shader to material.shader
    // to be used on mesh drawing with DrawMeshInstanced()
    Material matInstances = LoadMaterialDefault();
    matInstances.shader = shader;
    matInstances.maps[MATERIAL_MAP_DIFFUSE].color = RED;

    // Load default material (using raylib intenral default shader) for non-instanced mesh drawing
    // WARNING: Default shader enables vertex color attribute BUT GenMeshCube() does not generate vertex colors, so,
    // when drawing the color attribute is disabled and a default color value is provided as input for thevertex attribute
    const Material matDefault = LoadMaterialDefault();
    matDefault.maps[MATERIAL_MAP_DIFFUSE].color = BLUE;

    SetTargetFPS(60);                   // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())        // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        UpdateCamera(&camera, CAMERA_ORBITAL);

        // Update the light shader with the camera view position
        f32 cameraPos[3] = { camera.position.x, camera.position.y, camera.position.z };
        SetShaderValue(shader, shader.locs[SHADER_LOC_VECTOR_VIEW], cameraPos, SHADER_UNIFORM_VEC3);
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            BeginMode3D(camera);

                // Draw cube mesh with default material (BLUE)
                DrawMesh(cube, matDefault, glm::translate(mat4(1.0f), vec3{-10.0f, 0.0f, 0.0f}));

                // Draw meshes instanced using material containing instancing shader (RED + lighting),
                // transforms[] for the instances should be provided, they are dynamically
                // updated in GPU every frame, so we can animate the different mesh instances
                DrawMeshInstanced(cube, matInstances, transforms, MAX_INSTANCES);

                // Draw cube mesh with default material (BLUE)
                DrawMesh(cube, matDefault, glm::translate(mat4(1.0f), vec3{10.0f, 0.0f, 0.0f}));

            EndMode3D();

            DrawFPS(10, 10);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    free(transforms);             // Free transforms
    UnloadMaterial(matInstances);    // Also unloads the instancing shader
    UnloadMaterial(matDefault);      // Unload default material
    UnloadMesh(cube);                // Unload cube mesh

    CloseWindow();          // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
