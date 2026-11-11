/*******************************************************************************************
*
*   raylib [shaders] example - depth writing
*
*   Example complexity rating: [★★☆☆] 2/4
*
*   Example originally created with raylib 4.2, last time updated with raylib 4.2
*
*   Example contributed by Buğra Alptekin Sarı (@BugraAlptekinSari) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2022-2025 Buğra Alptekin Sarı (@BugraAlptekinSari)
*
********************************************************************************************/

#include "raylib.h"

#include "rlgl.h"

#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION            330
#else   // PLATFORM_ANDROID, PLATFORM_WEB
    #define GLSL_VERSION            100
#endif

//--------------------------------------------------------------------------------------
// Module Functions Declaration
//--------------------------------------------------------------------------------------
// Load custom render texture, create a writable depth texture buffer
static RenderTexture2D LoadRenderTextureDepthTex(i32 width, i32 height);

// Unload render texture from GPU memory (VRAM)
static void UnloadRenderTextureDepthTex(RenderTexture2D target);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr auto screenWidth = 800;
    constexpr auto screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [shaders] example - depth writing");

    // Define the camera to look into our 3d world
    Camera camera = {
        .position = vec3{ 2.0f, 2.0f, 3.0f },         // Camera position
        .target = vec3{ 0.0f, 0.5f, 0.0f },           // Camera looking at point
        .up = vec3{ 0.0f, 1.0f, 0.0f },               // Camera up vector (rotation towards target)
        .fovy = 45.0f,                                // Camera field-of-view Y
        .projection = CAMERA_PERSPECTIVE              // Camera projection type
    };

    // Load custom render texture with writable depth texture buffer
    const auto target = LoadRenderTextureDepthTex(screenWidth, screenHeight);

    // Load depth writing shader
    // NOTE: The shader inverts the depth buffer by writing into it by `gl_FragDepth = 1 - gl_FragCoord.z;`
    const Shader shader = LoadShader({}, TextFormat("resources/shaders/glsl%i/depth_write.fs", GLSL_VERSION));
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
        // Draw into our custom render texture
        BeginTextureMode(target);
            ClearBackground(WHITE);

            BeginMode3D(camera);
                BeginShaderMode(shader);
                    DrawCubeWiresV(vec3{ 0.0f, 0.5f, 1.0f }, vec3{ 1.0f, 1.0f, 1.0f }, RED);
                    DrawCubeV(vec3{ 0.0f, 0.5f, 1.0f }, vec3{ 1.0f, 1.0f, 1.0f }, PURPLE);
                    DrawCubeWiresV(vec3{ 0.0f, 0.5f, -1.0f }, vec3{ 1.0f, 1.0f, 1.0f }, DARKGREEN);
                    DrawCubeV(vec3{ 0.0f, 0.5f, -1.0f }, vec3{ 1.0f, 1.0f, 1.0f }, YELLOW);
                    DrawGrid(10, 1.0f);
                EndShaderMode();
            EndMode3D();
        EndTextureMode();

        // Draw into screen our custom render texture
        BeginDrawing();
            ClearBackground(RAYWHITE);

            DrawTextureRec(target.texture, Rectangle{ 0, 0, static_cast<f32>(screenWidth), static_cast<f32>(-screenHeight) }, vec2{ 0, 0 }, WHITE);

            DrawFPS(10, 10);
        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadRenderTextureDepthTex(target);
    UnloadShader(shader);

    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//--------------------------------------------------------------------------------------
// Module Functions Definition
//--------------------------------------------------------------------------------------
// Load custom render texture, create a writable depth texture buffer
static RenderTexture2D LoadRenderTextureDepthTex(i32 width, i32 height)
{
    auto target = RenderTexture2D{};

    target.id = rlLoadFramebuffer(); // Load an empty framebuffer

    if (target.id > 0)
    {
        rlEnableFramebuffer(target.id);

        // Create color texture (default to RGBA)
        target.texture.id = rlLoadTexture(nullptr, width, height, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
        target.texture.width = width;
        target.texture.height = height;
        target.texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        target.texture.mipmaps = 1;

        // Create depth texture buffer (instead of raylib default renderbuffer)
        target.depth.id = rlLoadTextureDepth(width, height, false);
        target.depth.width = width;
        target.depth.height = height;
        target.depth.format = 19;       // DEPTH_COMPONENT_24BIT: Not defined in raylib
        target.depth.mipmaps = 1;

        // Attach color texture and depth texture to FBO
        rlFramebufferAttach(target.id, target.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
        rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);

        // Check if fbo is complete with attachments (valid)
        if (rlFramebufferComplete(target.id)) TRACELOG(LOG_INFO, "FBO: [ID %i] Framebuffer object created successfully", target.id);

        rlDisableFramebuffer();
    }
    else TRACELOG(LOG_WARNING, "FBO: Framebuffer object can not be created");

    return target;
}

// Unload render texture from GPU memory (VRAM)
void UnloadRenderTextureDepthTex(RenderTexture2D target)
{
    if (target.id > 0)
    {
        // Color texture attached to FBO is deleted
        rlUnloadTexture(target.texture.id);
        rlUnloadTexture(target.depth.id);

        // NOTE: Depth texture is automatically
        // queried and deleted before deleting framebuffer
        rlUnloadFramebuffer(target.id);
    }
}