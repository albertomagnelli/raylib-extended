/*******************************************************************************************
*
*   raylib [shaders] example - lightmap rendering
*
*   Example complexity rating: [★★★☆] 3/4
*
*   NOTE: This example requires raylib OpenGL 3.3 or ES2 versions for shaders support,
*         OpenGL 1.1 does not support shaders, recompile raylib to OpenGL 3.3 version
*
*   NOTE: Shaders used in this example are #version 330 (OpenGL 3.3)
*
*   Example originally created with raylib 4.5, last time updated with raylib 4.5
*
*   Example contributed by Jussi Viitala (@nullstare) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2019-2025 Jussi Viitala (@nullstare) and Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION            330
#else   // PLATFORM_ANDROID, PLATFORM_WEB
    #define GLSL_VERSION            100
#endif

#define MAP_SIZE 16

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr i32 screenWidth = 800;
    constexpr i32 screenHeight = 450;

    SetConfigFlags(FLAG_MSAA_4X_HINT);  // Enable Multi Sampling Anti Aliasing 4x (if available)
    InitWindow(screenWidth, screenHeight, "raylib [shaders] example - lightmap rendering");

    // Define the camera to look into our 3d world
    Camera camera = {};
    camera.position = vec3{ 4.0f, 6.0f, 8.0f };         // Camera position
    camera.target = vec3{ 0.0f, 0.0f, 0.0f };           // Camera looking at point
    camera.up = vec3{ 0.0f, 1.0f, 0.0f };               // Camera up vector (rotation towards target)
    camera.fovy = 45.0f;                                // Camera field-of-view Y
    camera.projection = CAMERA_PERSPECTIVE;             // Camera projection type

    Mesh mesh = GenMeshPlane(static_cast<f32>(MAP_SIZE), static_cast<f32>(MAP_SIZE), 1, 1);

    // GenMeshPlane doesn't generate texcoords2 so we will upload them separately
    mesh.texcoords2 = static_cast<f32 *>(malloc(mesh.vertexCount*2*sizeof(f32)));

    // X                          // Y
    mesh.texcoords2[0] = 0.0f;    mesh.texcoords2[1] = 0.0f;
    mesh.texcoords2[2] = 1.0f;    mesh.texcoords2[3] = 0.0f;
    mesh.texcoords2[4] = 0.0f;    mesh.texcoords2[5] = 1.0f;
    mesh.texcoords2[6] = 1.0f;    mesh.texcoords2[7] = 1.0f;

    // Load a new texcoords2 attributes buffer
    mesh.vboId[SHADER_LOC_VERTEX_TEXCOORD02] = rlLoadVertexBuffer(mesh.texcoords2, mesh.vertexCount*2*sizeof(f32), false);
    rlEnableVertexArray(mesh.vaoId);

    // Index 5 is for texcoords2
    rlSetVertexAttribute(5, 2, RL_FLOAT, 0, 0, 0);
    rlEnableVertexAttribute(5);
    rlDisableVertexArray();

    // Load lightmap shader
    const Shader shader = LoadShader(TextFormat("resources/shaders/glsl%i/lightmap.vs", GLSL_VERSION),
                               TextFormat("resources/shaders/glsl%i/lightmap.fs", GLSL_VERSION));

    Texture texture = LoadTexture("resources/cubicmap_atlas.png");
    const Texture light = LoadTexture("resources/spark_flame.png");

    GenTextureMipmaps(&texture);
    SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);

    RenderTexture lightmap = LoadRenderTexture(MAP_SIZE, MAP_SIZE);

    Material material = LoadMaterialDefault();
    material.shader = shader;
    material.maps[MATERIAL_MAP_ALBEDO].texture = texture;
    material.maps[MATERIAL_MAP_METALNESS].texture = lightmap.texture;

    // Drawing to lightmap
    BeginTextureMode(lightmap);
        ClearBackground(BLACK);

        BeginBlendMode(BLEND_ADDITIVE);
            DrawTexturePro(
                light,
                Rectangle{ 0, 0, static_cast<f32>(light.width), static_cast<f32>(light.height) },
                Rectangle{ 0, 0, 2.0f*MAP_SIZE, 2.0f*MAP_SIZE },
                vec2{ static_cast<f32>(MAP_SIZE), static_cast<f32>(MAP_SIZE) },
                0.0,
                RED
            );
            DrawTexturePro(
                light,
                Rectangle{ 0, 0, static_cast<f32>(light.width), static_cast<f32>(light.height) },
                Rectangle{ static_cast<f32>(MAP_SIZE)*0.8f, static_cast<f32>(MAP_SIZE)/2.0f, 2.0f*MAP_SIZE, 2.0f*MAP_SIZE },
                vec2{ static_cast<f32>(MAP_SIZE), static_cast<f32>(MAP_SIZE) },
                0.0,
                BLUE
            );
            DrawTexturePro(
                light,
                Rectangle{ 0, 0, static_cast<f32>(light.width), static_cast<f32>(light.height) },
                Rectangle{ static_cast<f32>(MAP_SIZE)*0.8f, static_cast<f32>(MAP_SIZE)*0.8f, static_cast<f32>(MAP_SIZE), static_cast<f32>(MAP_SIZE) },
                vec2{ static_cast<f32>(MAP_SIZE)/2.0f, static_cast<f32>(MAP_SIZE)/2.0f },
                0.0,
                GREEN
            );
        BeginBlendMode(BLEND_ALPHA);
    EndTextureMode();

    // NOTE: To enable trilinear filtering we need mipmaps available for texture
    GenTextureMipmaps(&lightmap.texture);
    SetTextureFilter(lightmap.texture, TEXTURE_FILTER_TRILINEAR);

    SetTargetFPS(60);                   // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())        // Detect window close button or ESC key
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
                DrawMesh(mesh, material, mat4(1.0f));
            EndMode3D();

            DrawTexturePro(lightmap.texture, Rectangle{ 0, 0, -MAP_SIZE, -MAP_SIZE },
                Rectangle{ static_cast<f32>(GetRenderWidth()) - MAP_SIZE*8 - 10, 10, static_cast<f32>(MAP_SIZE)*8, static_cast<f32>(MAP_SIZE)*8 },
                vec2{ 0.0, 0.0 }, 0.0, WHITE);

            DrawText(TextFormat("LIGHTMAP: %ix%i pixels", MAP_SIZE, MAP_SIZE), GetRenderWidth() - 130, 20 + MAP_SIZE*8, 10, GREEN);

            DrawFPS(10, 10);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadMesh(mesh);              // Unload the mesh
    UnloadShader(shader);          // Unload shader
    UnloadTexture(texture);        // Unload texture
    UnloadTexture(light);          // Unload texture
    UnloadRenderTexture(lightmap); // Unload lightmap render texture

    CloseWindow();          // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
