/*******************************************************************************************
*
*   raylib [shaders] example - normalmap rendering
*
*   Example complexity rating: [★★★★] 4/4
*
*   NOTE: This example requires raylib OpenGL 3.3 or ES2 versions for shaders support,
*        OpenGL 1.1 does not support shaders, recompile raylib to OpenGL 3.3 version
*
*   Example originally created with raylib 6.0, last time updated with raylib 6.0
*
*   Example contributed by Jeremy Montgomery (@Sir_Irk) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2025 Jeremy Montgomery (@Sir_Irk) and Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"

#include "raymath.h"

#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION            330
#else   // PLATFORM_ANDROID, PLATFORM_WEB
    #define GLSL_VERSION            100
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

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "raylib [shaders] example - normalmap rendering");

    auto camera = Camera{};
    camera.position = vec3{ 0.0f, 2.0f, -4.0f };
    camera.target = vec3{ 0.0f, 0.0f, 0.0f };
    camera.up = vec3{ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Load basic normal map lighting shader
    const Shader shader = LoadShader(TextFormat("resources/shaders/glsl%i/normalmap.vs", GLSL_VERSION),
                               TextFormat("resources/shaders/glsl%i/normalmap.fs", GLSL_VERSION));

    // Get some required shader locations
    shader.locs[SHADER_LOC_MAP_NORMAL] = GetShaderLocation(shader, "normalMap");
    shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");

    // NOTE: "matModel" location name is automatically assigned on shader loading,
    // no need to get the location again if using that uniform name
    // shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");

    // This example uses just 1 point light
    auto lightPosition = vec3{ 0.0f, 1.0f, 0.0f };
    const i32 lightPosLoc = GetShaderLocation(shader, "lightPos");

    // Load a plane model that has proper normals and tangents
    Model plane = LoadModel("resources/models/plane.glb");

    // Set the plane model's shader and texture maps
    plane.materials[0].shader = shader;
    plane.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = LoadTexture("resources/tiles_diffuse.png");
    plane.materials[0].maps[MATERIAL_MAP_NORMAL].texture = LoadTexture("resources/tiles_normal.png");

    // Generate Mipmaps and use TRILINEAR filtering to help with texture aliasing
    GenTextureMipmaps(&plane.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture);
    GenTextureMipmaps(&plane.materials[0].maps[MATERIAL_MAP_NORMAL].texture);

    SetTextureFilter(plane.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture, TEXTURE_FILTER_TRILINEAR);
    SetTextureFilter(plane.materials[0].maps[MATERIAL_MAP_NORMAL].texture, TEXTURE_FILTER_TRILINEAR);

    // Specular exponent AKA shininess of the material
    auto specularExponent = 8.0f;
    const i32 specularExponentLoc = GetShaderLocation(shader, "specularExponent");

    // Allow toggling the normal map on and off for comparison purposes
    auto useNormalMap = 1;
    const i32 useNormalMapLoc = GetShaderLocation(shader, "useNormalMap");

    SetTargetFPS(60); // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose()) // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        // Move the light around on the X and Z axis using WASD keys
        auto direction = vec3{};
        if (IsKeyDown(KEY_W)) direction = ((direction) + (vec3{ 0.0f, 0.0f, 1.0f }));
        if (IsKeyDown(KEY_S)) direction = ((direction) + (vec3{ 0.0f, 0.0f, -1.0f }));
        if (IsKeyDown(KEY_D)) direction = ((direction) + (vec3{ -1.0f, 0.0f, 0.0f }));
        if (IsKeyDown(KEY_A)) direction = ((direction) + (vec3{ 1.0f, 0.0f, 0.0f }));

        direction = glm::normalize(direction);
        lightPosition = ((lightPosition) + (((direction) * (GetFrameTime()*3.0f))));

        // Increase/Decrease the specular exponent(shininess)
        if (IsKeyDown(KEY_UP)) specularExponent = Clamp(specularExponent + 40.0f*GetFrameTime(), 2.0f, 128.0f);
        if (IsKeyDown(KEY_DOWN)) specularExponent = Clamp(specularExponent - 40.0f*GetFrameTime(), 2.0f, 128.0f);

        // Toggle normal map on and off
        if (IsKeyPressed(KEY_N)) useNormalMap = !useNormalMap;

        // Spin plane model at a constant rate
        plane.transform = glm::rotate(mat4(1.0f), static_cast<f32>(GetTime())*0.5f, vec3{0.0f, 1.0f, 0.0f});

        // Update shader values
        f32 lightPos[3] = {lightPosition.x, lightPosition.y, lightPosition.z};
        SetShaderValue(shader, lightPosLoc, lightPos, SHADER_UNIFORM_VEC3);

        f32 camPos[3] = {camera.position.x, camera.position.y, camera.position.z};
        SetShaderValue(shader, shader.locs[SHADER_LOC_VECTOR_VIEW], camPos, SHADER_UNIFORM_VEC3);

        SetShaderValue(shader, specularExponentLoc, &specularExponent, SHADER_UNIFORM_FLOAT);

        SetShaderValue(shader, useNormalMapLoc, &useNormalMap, SHADER_UNIFORM_INT);
        //--------------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            BeginMode3D(camera);

                BeginShaderMode(shader);

                    DrawModel(plane, vec3{}, 2.0f, WHITE);

                EndShaderMode();

                // Draw sphere to show light position
                DrawSphereWires(lightPosition, 0.2f, 8, 8, ORANGE);

            EndMode3D();

            const Color textColor = (useNormalMap) ? DARKGREEN : RED;
            const char *toggleStr = (useNormalMap) ? "On" : "Off";
            DrawText(TextFormat("Use key [N] to toggle normal map: %s", toggleStr), 10, 10, 10, textColor);

            constexpr auto yOffset = 24;
            DrawText("Use keys [W][A][S][D] to move the light", 10, 10 + yOffset*1, 10, BLACK);
            DrawText("Use keys [Up][Down] to change specular exponent", 10, 10 + yOffset*2, 10, BLACK);
            DrawText(TextFormat("Specular Exponent: %.2f", specularExponent), 10, 10 + yOffset*3, 10, BLUE);

            DrawFPS(screenWidth - 90, 10);

        EndDrawing();
        //--------------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadShader(shader);
    UnloadModel(plane);

    CloseWindow(); // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
