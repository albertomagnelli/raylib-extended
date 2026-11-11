/*******************************************************************************************
*
*   raylib [shaders] example - rlgl compute
*
*   WARNING: This example requires raylib compiled with OpenGL 4.3 version for
*         compute shaders support, shaders used in this example are #version 430
*
*   Example complexity rating: [★★★★] 4/4
*
*   Example originally created with raylib 4.0, last time updated with raylib 4.0
*
*   Example contributed by Teddy Astie (@tsnake41) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2021-2025 Teddy Astie (@tsnake41)
*
********************************************************************************************/

#include "raylib.h"

#include "rlgl.h"

#include <stdlib.h>     // Required for: NULL

// IMPORTANT: This must match gol*.glsl GOL_WIDTH constant
// This must be a multiple of 16 (check golLogic compute dispatch)
#define GOL_WIDTH 768

// Maximum amount of queued draw commands (squares draw from mouse down events)
#define MAX_BUFFERED_TRANSFERTS 48

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// Game Of Life Update Command
typedef struct GolUpdateCmd {
    u32 x;         // x coordinate of the gol command
    u32 y;         // y coordinate of the gol command
    u32 w;         // width of the filled zone
    u32 enabled;   // whether to enable or disable zone
} GolUpdateCmd;

// Game Of Life Update Commands SSBO
typedef struct GolUpdateSSBO {
    u32 count;
    GolUpdateCmd commands[MAX_BUFFERED_TRANSFERTS];
} GolUpdateSSBO;

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const i32 screenWidth = GOL_WIDTH;
    const i32 screenHeight = GOL_WIDTH;

    InitWindow(screenWidth, screenHeight, "raylib [shaders] example - rlgl compute");

    const auto resolution = vec2{ static_cast<f32>(screenWidth), static_cast<f32>(screenHeight) };
    u32 brushSize = 8;

    // Game of Life logic compute shader
    const std::string golLogicCode = LoadFileText("resources/shaders/glsl430/gol.glsl");
    const auto golLogicShader = rlLoadShader(golLogicCode, RL_COMPUTE_SHADER);
    const auto golLogicProgram = rlLoadShaderProgramCompute(golLogicShader);

    // Game of Life logic render shader
    const Shader golRenderShader = LoadShader({}, "resources/shaders/glsl430/gol_render.glsl");
    const i32 resUniformLoc = GetShaderLocation(golRenderShader, "resolution");

    // Game of Life transfert shader (CPU<->GPU download and upload)
    const std::string golTransfertCode = LoadFileText("resources/shaders/glsl430/gol_transfert.glsl");
    const auto golTransfertShader = rlLoadShader(golTransfertCode, RL_COMPUTE_SHADER);
    const auto golTransfertProgram = rlLoadShaderProgramCompute(golTransfertShader);

    // Load shader storage buffer object (SSBO), id returned
    auto ssboA = rlLoadShaderBuffer(GOL_WIDTH*GOL_WIDTH*sizeof(u32), nullptr, RL_DYNAMIC_COPY);
    auto ssboB = rlLoadShaderBuffer(GOL_WIDTH*GOL_WIDTH*sizeof(u32), nullptr, RL_DYNAMIC_COPY);
    const auto ssboTransfert = rlLoadShaderBuffer(sizeof(GolUpdateSSBO), nullptr, RL_DYNAMIC_COPY);

    auto transfertBuffer = GolUpdateSSBO{};

    // Create a white texture of the size of the window to update
    // each pixel of the window using the fragment shader: golRenderShader
    const auto whiteImage = GenImageColor(GOL_WIDTH, GOL_WIDTH, WHITE);
    const Texture whiteTex = LoadTextureFromImage(whiteImage);
    UnloadImage(whiteImage);

    SetTargetFPS(0);                    // Set our game to run with an uncapped framerate
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())
    {
        // Update
        //----------------------------------------------------------------------------------
        brushSize += static_cast<i32>(GetMouseWheelMove());

        if ((IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
            && (transfertBuffer.count < MAX_BUFFERED_TRANSFERTS))
        {
            // Buffer a new command
            transfertBuffer.commands[transfertBuffer.count].x = GetMouseX() - brushSize/2;
            transfertBuffer.commands[transfertBuffer.count].y = GetMouseY() - brushSize/2;
            transfertBuffer.commands[transfertBuffer.count].w = brushSize;
            transfertBuffer.commands[transfertBuffer.count].enabled = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
            transfertBuffer.count++;
        }
        else if (transfertBuffer.count > 0)  // Process transfert buffer
        {
            // Send SSBO buffer to GPU
            rlUpdateShaderBuffer(ssboTransfert, &transfertBuffer, sizeof(GolUpdateSSBO), 0);

            // Process SSBO commands on GPU
            rlEnableShader(golTransfertProgram);
            rlBindShaderBuffer(ssboA, 1);
            rlBindShaderBuffer(ssboTransfert, 3);
            rlComputeShaderDispatch(transfertBuffer.count, 1, 1); // Each GPU unit will process a command!
            rlDisableShader();

            transfertBuffer.count = 0;
        }
        else
        {
            // Process game of life logic
            rlEnableShader(golLogicProgram);
            rlBindShaderBuffer(ssboA, 1);
            rlBindShaderBuffer(ssboB, 2);
            rlComputeShaderDispatch(GOL_WIDTH/16, GOL_WIDTH/16, 1);
            rlDisableShader();

            // ssboA <-> ssboB
            const i32 temp = ssboA;
            ssboA = ssboB;
            ssboB = temp;
        }

        rlBindShaderBuffer(ssboA, 1);
        SetShaderValue(golRenderShader, resUniformLoc, &resolution, SHADER_UNIFORM_VEC2);
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(BLANK);

            BeginShaderMode(golRenderShader);
                DrawTexture(whiteTex, 0, 0, WHITE);
            EndShaderMode();

            DrawRectangleLines(GetMouseX() - brushSize/2, GetMouseY() - brushSize/2, brushSize, brushSize, RED);

            DrawText("Use Mouse wheel to increase/decrease brush size", 10, 10, 20, WHITE);
            DrawFPS(GetScreenWidth() - 100, 10);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    // Unload shader buffers objects
    rlUnloadShaderBuffer(ssboA);
    rlUnloadShaderBuffer(ssboB);
    rlUnloadShaderBuffer(ssboTransfert);

    // Unload compute shader
    rlUnloadShader(golLogicShader);
    rlUnloadShader(golTransfertShader);
    rlUnloadShaderProgram(golTransfertProgram);
    rlUnloadShaderProgram(golLogicProgram);

    UnloadTexture(whiteTex);            // Unload white texture
    UnloadShader(golRenderShader);      // Unload rendering fragment shader

    CloseWindow();                      // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
