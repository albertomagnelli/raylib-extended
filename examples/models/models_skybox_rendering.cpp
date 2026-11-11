/*******************************************************************************************
*
*   raylib [models] example - skybox rendering
*
*   Example complexity rating: [★★☆☆] 2/4
*
*   Example originally created with raylib 1.8, last time updated with raylib 4.0
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2017-2025 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"

#include "rlgl.h"
#include "raymath.h"

#include <initializer_list>      // Required for: MatrixPerspective(), MatrixLookAt()

#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION            330
#else   // PLATFORM_ANDROID, PLATFORM_WEB
    #define GLSL_VERSION            100
#endif

//------------------------------------------------------------------------------------
// Module Functions Declaration
//------------------------------------------------------------------------------------
// Generate cubemap (6 faces) from equirectangular (panorama) texture
static TextureCubemap GenTextureCubemap(Shader shader, Texture2D panorama, i32 size, i32 format);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr auto screenWidth = 800;
    constexpr auto screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [models] example - skybox rendering");

    // Define the camera to look into our 3d world
    auto camera = Camera{};
    camera.position = vec3{ 1.0f, 1.0f, 1.0f };         // Camera position
    camera.target = vec3{ 4.0f, 1.0f, 4.0f };           // Camera looking at point
    camera.up = vec3{ 0.0f, 1.0f, 0.0f };               // Camera up vector (rotation towards target)
    camera.fovy = 45.0f;                                // Camera field-of-view Y
    camera.projection = CAMERA_PERSPECTIVE;             // Camera projection type

    // Load skybox model
    const auto cube = GenMeshCube(1.0f, 1.0f, 1.0f);
    const auto skybox = LoadModelFromMesh(cube);

    // Set this to true to use an HDR Texture
    // NOTE: raylib must be built with HDR Support for this to work: SUPPORT_FILEFORMAT_HDR
    constexpr auto useHDR = false;

    // Load skybox shader and set required locations
    // NOTE: Some locations are automatically set at shader loading
    skybox.materials[0].shader = LoadShader(TextFormat("resources/shaders/glsl%i/skybox.vs", GLSL_VERSION),
                                            TextFormat("resources/shaders/glsl%i/skybox.fs", GLSL_VERSION));

    SetShaderValue(skybox.materials[0].shader, GetShaderLocation(skybox.materials[0].shader, "environmentMap"), std::initializer_list<i32>{ MATERIAL_MAP_CUBEMAP }.begin(), SHADER_UNIFORM_INT);
    SetShaderValue(skybox.materials[0].shader, GetShaderLocation(skybox.materials[0].shader, "doGamma"), std::initializer_list<i32>{ useHDR? 1 : 0 }.begin(), SHADER_UNIFORM_INT);
    SetShaderValue(skybox.materials[0].shader, GetShaderLocation(skybox.materials[0].shader, "vflipped"), std::initializer_list<i32>{ useHDR? 1 : 0 }.begin(), SHADER_UNIFORM_INT);

    // Load cubemap shader and setup required shader locations
    const Shader shdrCubemap = LoadShader(TextFormat("resources/shaders/glsl%i/cubemap.vs", GLSL_VERSION),
                                    TextFormat("resources/shaders/glsl%i/cubemap.fs", GLSL_VERSION));

    SetShaderValue(shdrCubemap, GetShaderLocation(shdrCubemap, "equirectangularMap"), std::initializer_list<i32>{ 0 }.begin(), SHADER_UNIFORM_INT);

    std::string skyboxFileName;

    if (useHDR)
    {
        skyboxFileName = "resources/dresden_square_2k.hdr";

        // Load HDR panorama (sphere) texture
        const auto panorama = LoadTexture(skyboxFileName);

        // Generate cubemap (texture with 6 quads-cube-mapping) from panorama HDR texture
        // NOTE 1: New texture is generated rendering to texture, shader calculates the sphere->cube coordinates mapping
        // NOTE 2: It seems on some Android devices WebGL, fbo does not properly support a FLOAT-based attachment,
        // despite texture can be successfully created.. so using PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 instead of PIXELFORMAT_UNCOMPRESSED_R32G32B32A32
        skybox.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture = GenTextureCubemap(shdrCubemap, panorama, 1024, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

        UnloadTexture(panorama);        // Texture not required anymore, cubemap already generated
    }
    else
    {
        // TODO: WARNING: On PLATFORM_WEB it requires a big amount of memory to process input image
        // and generate the required cubemap image to be passed to rlLoadTextureCubemap()
        const Image image = LoadImage("resources/skybox.png");
        skybox.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture = LoadTextureCubemap(image, CUBEMAP_LAYOUT_AUTO_DETECT);
        UnloadImage(image);
    }

    DisableCursor();                    // Limit cursor to relative movement inside the window

    SetTargetFPS(60);                   // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())        // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        UpdateCamera(&camera, CAMERA_FIRST_PERSON);

        // Load new cubemap texture on drag&drop
        if (IsFileDropped())
        {
            auto droppedFiles = LoadDroppedFiles();

            if (droppedFiles.paths.size() == 1)         // Only support one file dropped
            {
                if (IsFileExtension(droppedFiles.paths[0], ".png;.jpg;.hdr;.bmp;.tga"))
                {
                    // Unload current cubemap texture to load new one
                    UnloadTexture(skybox.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture);

                    if (useHDR)
                    {
                        // Load HDR panorama (sphere) texture
                        const auto panorama = LoadTexture(droppedFiles.paths[0]);

                        // Generate cubemap from panorama texture
                        skybox.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture = GenTextureCubemap(shdrCubemap, panorama, 1024, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

                        UnloadTexture(panorama);    // Texture not required anymore, cubemap already generated
                    }
                    else
                    {
                        const auto image = LoadImage(droppedFiles.paths[0]);
                        skybox.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture = LoadTextureCubemap(image, CUBEMAP_LAYOUT_AUTO_DETECT);
                        UnloadImage(image);
                    }

                    skyboxFileName = droppedFiles.paths[0];
                }
            }

        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            BeginMode3D(camera);

                // We are inside the cube, we need to disable backface culling!
                rlDisableBackfaceCulling();
                rlDisableDepthMask();
                    DrawModel(skybox, vec3{0, 0, 0}, 1.0f, WHITE);
                rlEnableBackfaceCulling();
                rlEnableDepthMask();

                DrawGrid(10, 1.0f);

            EndMode3D();

            if (useHDR) DrawText(TextFormat("Panorama image from hdrihaven.com: %s", GetFileName(skyboxFileName).c_str()), 10, GetScreenHeight() - 20, 10, BLACK);
            else DrawText(TextFormat(": %s", GetFileName(skyboxFileName).c_str()), 10, GetScreenHeight() - 20, 10, BLACK);

            DrawFPS(10, 10);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadShader(skybox.materials[0].shader);
    UnloadTexture(skybox.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture);
    UnloadModel(skybox);        // Unload skybox model
    UnloadShader(shdrCubemap);

    CloseWindow();              // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//------------------------------------------------------------------------------------
// Module Functions Definition
//------------------------------------------------------------------------------------
// Generate cubemap texture from HDR texture
static TextureCubemap GenTextureCubemap(Shader shader, Texture2D panorama, i32 size, i32 format)
{
    auto cubemap = TextureCubemap{};

    rlDisableBackfaceCulling();     // Disable backface culling to render inside the cube

    // STEP 1: Setup framebuffer
    //------------------------------------------------------------------------------------------
    const auto rbo = rlLoadTextureDepth(size, size, true);
    cubemap.id = rlLoadTextureCubemap(nullptr, size, format, 1);

    const auto fbo = rlLoadFramebuffer();
    rlFramebufferAttach(fbo, rbo, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);
    rlFramebufferAttach(fbo, cubemap.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_CUBEMAP_POSITIVE_X, 0);

    // Check if framebuffer is complete with attachments (valid)
    if (rlFramebufferComplete(fbo)) TraceLog(LOG_INFO, "FBO: [ID %i] Framebuffer object created successfully", fbo);
    //------------------------------------------------------------------------------------------

    // STEP 2: Draw to framebuffer
    //------------------------------------------------------------------------------------------
    // NOTE: Shader is used to convert HDR equirectangular environment map to cubemap equivalent (6 faces)
    rlEnableShader(shader.id);

    // Define projection matrix and send it to shader
    const mat4 matFboProjection = glm::perspective(90.0f*DEG2RAD, 1.0f, rlGetCullDistanceNear(), rlGetCullDistanceFar());
    rlSetUniformMatrix(shader.locs[SHADER_LOC_MATRIX_PROJECTION], matFboProjection);

    // Define view matrix for every side of the cubemap
    const mat4 fboViews[6] = {
        glm::lookAt(vec3{ 0.0f, 0.0f, 0.0f }, vec3{  1.0f,  0.0f,  0.0f }, vec3{ 0.0f, -1.0f,  0.0f }),
        glm::lookAt(vec3{ 0.0f, 0.0f, 0.0f }, vec3{ -1.0f,  0.0f,  0.0f }, vec3{ 0.0f, -1.0f,  0.0f }),
        glm::lookAt(vec3{ 0.0f, 0.0f, 0.0f }, vec3{  0.0f,  1.0f,  0.0f }, vec3{ 0.0f,  0.0f,  1.0f }),
        glm::lookAt(vec3{ 0.0f, 0.0f, 0.0f }, vec3{  0.0f, -1.0f,  0.0f }, vec3{ 0.0f,  0.0f, -1.0f }),
        glm::lookAt(vec3{ 0.0f, 0.0f, 0.0f }, vec3{  0.0f,  0.0f,  1.0f }, vec3{ 0.0f, -1.0f,  0.0f }),
        glm::lookAt(vec3{ 0.0f, 0.0f, 0.0f }, vec3{  0.0f,  0.0f, -1.0f }, vec3{ 0.0f, -1.0f,  0.0f })
    };

    rlViewport(0, 0, size, size);   // Set viewport to current fbo dimensions

    // Activate and enable texture for drawing to cubemap faces
    rlActiveTextureSlot(0);
    rlEnableTexture(panorama.id);

    for (i32 i = 0; i < 6; i++)
    {
        // Set the view matrix for the current cube face
        rlSetUniformMatrix(shader.locs[SHADER_LOC_MATRIX_VIEW], fboViews[i]);

        // Select the current cubemap face attachment for the fbo
        // WARNING: This function by default enables->attach->disables fbo!!!
        rlFramebufferAttach(fbo, cubemap.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_CUBEMAP_POSITIVE_X + i, 0);
        rlEnableFramebuffer(fbo);

        // Load and draw a cube, it uses the current enabled texture
        rlClearScreenBuffers();
        rlLoadDrawCube();

        // ALTERNATIVE: Try to use internal batch system to draw the cube instead of rlLoadDrawCube
        // for some reason this method does not work, maybe due to cube triangles definition? normals pointing out?
        // TODO: Investigate this issue...
        //rlSetTexture(panorama.id); // WARNING: It must be called after enabling current framebuffer if using internal batch system!
        //rlClearScreenBuffers();
        //DrawCubeV(Vector3Zero(), Vector3One(), WHITE);
        //rlDrawRenderBatchActive();
    }
    //------------------------------------------------------------------------------------------

    // STEP 3: Unload framebuffer and reset state
    //------------------------------------------------------------------------------------------
    rlDisableShader();          // Unbind shader
    rlDisableTexture();         // Unbind texture
    rlDisableFramebuffer();     // Unbind framebuffer
    rlUnloadFramebuffer(fbo);   // Unload framebuffer (and automatically attached depth texture/renderbuffer)

    // Reset viewport dimensions to default
    rlViewport(0, 0, rlGetFramebufferWidth(), rlGetFramebufferHeight());
    rlEnableBackfaceCulling();
    //------------------------------------------------------------------------------------------

    cubemap.width = size;
    cubemap.height = size;
    cubemap.mipmaps = 1;
    cubemap.format = format;

    return cubemap;
}
