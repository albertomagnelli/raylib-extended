/*******************************************************************************************
*
*   raylib [shaders] example - shadowmap rendering
*
*   Example complexity rating: [★★★★] 4/4
*
*   Example originally created with raylib 5.0, last time updated with raylib 5.0
*
*   Example contributed by TheManTheMythTheGameDev (@TheManTheMythTheGameDev) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2023-2025 TheManTheMythTheGameDev (@TheManTheMythTheGameDev)
*
********************************************************************************************/

#include "raylib.h"

#include "raymath.h"
#include "rlgl.h"

#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION            330
#else   // PLATFORM_ANDROID, PLATFORM_WEB
    #define GLSL_VERSION            100
#endif

#define SHADOWMAP_RESOLUTION 1024

static RenderTexture2D LoadShadowmapRenderTexture(i32 width, i32 height);
static void UnloadShadowmapRenderTexture(RenderTexture2D target);
static void DrawScene(Model cube, Model robot);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr auto screenWidth = 800;
    constexpr auto screenHeight = 450;

    // Shadows are a HUGE topic, and this example shows an extremely simple implementation of the shadowmapping algorithm,
    // which is the industry standard for shadows. This algorithm can be extended in a ridiculous number of ways to improve
    // realism and also adapt it for different scenes. This is pretty much the simplest possible implementation

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "raylib [shaders] example - shadowmap rendering");

    auto camera = Camera3D{};
    camera.position = vec3{ 10.0f, 10.0f, 10.0f };
    camera.target = vec3{};
    camera.projection = CAMERA_PERSPECTIVE;
    camera.up = vec3{ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;

    const Shader shadowShader = LoadShader(TextFormat("resources/shaders/glsl%i/shadowmap.vs", GLSL_VERSION),
                                     TextFormat("resources/shaders/glsl%i/shadowmap.fs", GLSL_VERSION));
    shadowShader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shadowShader, "viewPos");

    vec3 lightDir = glm::normalize(vec3{ 0.35f, -1.0f, -0.35f });
    const Color lightColor = WHITE;
    auto lightColorNormalized = ColorNormalize(lightColor);
    const i32 lightDirLoc = GetShaderLocation(shadowShader, "lightDir");
    const i32 lightColLoc = GetShaderLocation(shadowShader, "lightColor");
    SetShaderValue(shadowShader, lightDirLoc, &lightDir, SHADER_UNIFORM_VEC3);
    SetShaderValue(shadowShader, lightColLoc, &lightColorNormalized, SHADER_UNIFORM_VEC4);
    const i32 ambientLoc = GetShaderLocation(shadowShader, "ambient");
    f32 ambient[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
    SetShaderValue(shadowShader, ambientLoc, ambient, SHADER_UNIFORM_VEC4);
    const i32 lightVPLoc = GetShaderLocation(shadowShader, "lightVP");
    const i32 shadowMapLoc = GetShaderLocation(shadowShader, "shadowMap");
    i32 shadowMapResolution = SHADOWMAP_RESOLUTION;
    SetShaderValue(shadowShader, GetShaderLocation(shadowShader, "shadowMapResolution"), &shadowMapResolution, SHADER_UNIFORM_INT);

    const auto cube = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
    cube.materials[0].shader = shadowShader;
    const Model robot = LoadModel("resources/models/robot.glb");
    for (i32 i = 0; i < robot.materialCount; i++) robot.materials[i].shader = shadowShader;

    auto animCount = 0;
    ModelAnimation* anims = LoadModelAnimations("resources/models/robot.glb", &animCount);

    const auto shadowMap = LoadShadowmapRenderTexture(SHADOWMAP_RESOLUTION, SHADOWMAP_RESOLUTION);

    // For the shadowmapping algorithm, we will be rendering everything from the light's point of view
    auto lightCamera = Camera3D{};
    lightCamera.position = ((lightDir) * (-15.0f));
    lightCamera.target = vec3{};
    lightCamera.projection = CAMERA_ORTHOGRAPHIC; // Use an orthographic projection for directional lights
    lightCamera.up = vec3{ 0.0f, 1.0f, 0.0f };
    lightCamera.fovy = 20.0f;

    auto frameCounter = 0;

    // Store the light matrices
    auto lightView = mat4{};
    auto lightProj = mat4{};
    auto lightViewProj = mat4{};
    auto textureActiveSlot = 10; // Can be anything 0 to 15, but 0 will probably be taken up

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        const auto deltaTime = GetFrameTime();

        UpdateCamera(&camera, CAMERA_ORBITAL);

        // send the updated camera position to the shader so that it can calculate the correct lighting for the scene
        vec3 cameraPos = camera.position;
        SetShaderValue(shadowShader, shadowShader.locs[SHADER_LOC_VECTOR_VIEW], &cameraPos, SHADER_UNIFORM_VEC3);

        frameCounter++;
        frameCounter %= (anims[0].keyframeCount);
        UpdateModelAnimation(robot, anims[0], static_cast<f32>(frameCounter));

        // Move light with arrow keys
        constexpr auto cameraSpeed = 0.05f;
        if (IsKeyDown(KEY_LEFT))
        {
            if (lightDir.x < 0.6f) lightDir.x += cameraSpeed*60.0f*deltaTime;
        }
        if (IsKeyDown(KEY_RIGHT))
        {
            if (lightDir.x > -0.6f) lightDir.x -= cameraSpeed*60.0f*deltaTime;
        }
        if (IsKeyDown(KEY_UP))
        {
            if (lightDir.z < 0.6f) lightDir.z += cameraSpeed*60.0f*deltaTime;
        }
        if (IsKeyDown(KEY_DOWN))
        {
            if (lightDir.z > -0.6f) lightDir.z -= cameraSpeed*60.0f*deltaTime;
        }

        lightDir = glm::normalize(lightDir);
        lightCamera.position = ((lightDir) * (-15.0f));
        SetShaderValue(shadowShader, lightDirLoc, &lightDir, SHADER_UNIFORM_VEC3);
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        // PASS 01: Render all objects into the shadowmap render texture
        // We record all the objects' depths (as rendered from the light source's point of view) in a buffer
        // Anything that is "visible" to the light is in light, anything that isn't is in shadow
        // We can later use the depth buffer when rendering everything from the player's point of view
        // to determine whether a given point is "visible" to the light
        BeginTextureMode(shadowMap);
            ClearBackground(WHITE);

            BeginMode3D(lightCamera);
                lightView = rlGetMatrixModelview();
                lightProj = rlGetMatrixProjection();
                DrawScene(cube, robot);
            EndMode3D();

        EndTextureMode();

        lightViewProj = ((lightProj) * (lightView));

        // PASS 02: Draw the scene into main framebuffer, using the generated shadowmap
        BeginDrawing();
            ClearBackground(RAYWHITE);

            SetShaderValueMatrix(shadowShader, lightVPLoc, lightViewProj);
            rlEnableShader(shadowShader.id);

            rlActiveTextureSlot(textureActiveSlot);
            rlEnableTexture(shadowMap.depth.id);
            rlSetUniform(shadowMapLoc, &textureActiveSlot, SHADER_UNIFORM_INT, 1);

            BeginMode3D(camera);
                DrawScene(cube, robot); // Draw the same exact things as we drew in the shadowmap!
            EndMode3D();

            rlActiveTextureSlot(textureActiveSlot);
            rlDisableTexture();

            DrawText("Use the arrow keys to rotate the light!", 10, 10, 30, RED);
            DrawText("Shadows in raylib using the shadowmapping algorithm!", screenWidth - 280, screenHeight - 20, 10, GRAY);

        EndDrawing();

        if (IsKeyPressed(KEY_F)) TakeScreenshot("shaders_shadowmap.png");
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadShader(shadowShader);
    UnloadModel(cube);
    UnloadModel(robot);
    UnloadModelAnimations(anims, animCount);
    UnloadShadowmapRenderTexture(shadowMap);

    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

// Load render texture for shadowmap projection
// NOTE: Load framebuffer with only a texture depth attachment,
// no color attachment required for shadowmap
static RenderTexture2D LoadShadowmapRenderTexture(i32 width, i32 height)
{
    auto target = RenderTexture2D{};

    target.id = rlLoadFramebuffer(); // Load an empty framebuffer
    target.texture.width = width;
    target.texture.height = height;

    if (target.id > 0)
    {
        rlEnableFramebuffer(target.id);

        // Create depth texture
        // NOTE: No need a color texture attachment for the shadowmap
        target.depth.id = rlLoadTextureDepth(width, height, false);
        target.depth.width = width;
        target.depth.height = height;
        target.depth.format = 19; // DEPTH_COMPONENT_24BIT?
        target.depth.mipmaps = 1;

        // Attach depth texture to FBO
        rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);

        // Check if fbo is complete with attachments (valid)
        if (rlFramebufferComplete(target.id)) TRACELOG(LOG_INFO, "FBO: [ID %i] Framebuffer object created successfully", target.id);

        rlDisableFramebuffer();
    }
    else TRACELOG(LOG_WARNING, "FBO: Framebuffer object can not be created");

    return target;
}

// Unload shadowmap render texture from GPU memory (VRAM)
static void UnloadShadowmapRenderTexture(RenderTexture2D target)
{
    if (target.id > 0)
    {
        // NOTE: Depth texture/renderbuffer is automatically
        // queried and deleted before deleting framebuffer
        rlUnloadFramebuffer(target.id);
    }
}

// Draw full scene projecting shadows
// NOTE: Required  to be called several time to generate shadowmap
static void DrawScene(Model cube, Model robot)
{
    DrawModelEx(cube, vec3{}, vec3{ 0.0f, 1.0f, 0.0f }, 0.0f, vec3{ 10.0f, 1.0f, 10.0f }, BLUE);
    DrawModelEx(cube, vec3{ 1.5f, 1.0f, -1.5f }, vec3{ 0.0f, 1.0f, 0.0f }, 0.0f, vec3(1.0f), WHITE);
    DrawModelEx(robot, vec3{ 0.0f, 0.5f, 0.0f }, vec3{ 0.0f, 1.0f, 0.0f }, 0.0f, vec3{ 1.0f, 1.0f, 1.0f }, RED);
}
