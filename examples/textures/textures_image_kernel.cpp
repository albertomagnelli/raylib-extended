/*******************************************************************************************
*
*   raylib [textures] example - image kernel
*
*   Example complexity rating: [★★★★] 4/4
*
*   NOTE: Images are loaded in CPU memory (RAM); textures are loaded in GPU memory (VRAM)
*
*   Example contributed by Karim Salem (@kimo-s) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example originally created with raylib 1.3, last time updated with raylib 1.3
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2015-2025 Karim Salem (@kimo-s)
*
********************************************************************************************/

#include "raylib.h"

//------------------------------------------------------------------------------------
// Module Functions Declaration
//------------------------------------------------------------------------------------
static void NormalizeKernel(f32 *kernel, i32 size);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr i32 screenWidth = 800;
    constexpr i32 screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [textures] example - image kernel");

    Image image = LoadImage("resources/cat.png"); // Loaded in CPU memory (RAM)

    f32 gaussiankernel[] = {
        1.0f, 2.0f, 1.0f,
        2.0f, 4.0f, 2.0f,
        1.0f, 2.0f, 1.0f
    };

    f32 sobelkernel[] = {
        1.0f, 0.0f, -1.0f,
        2.0f, 0.0f, -2.0f,
        1.0f, 0.0f, -1.0f
    };

    f32 sharpenkernel[] = {
        0.0f, -1.0f, 0.0f,
       -1.0f, 5.0f, -1.0f,
        0.0f, -1.0f, 0.0f
    };

    NormalizeKernel(gaussiankernel, 9);
    NormalizeKernel(sharpenkernel, 9);
    NormalizeKernel(sobelkernel, 9);

    Image catSharpend = ImageCopy(image);
    ImageKernelConvolution(&catSharpend, sharpenkernel, 9);

    Image catSobel = ImageCopy(image);
    ImageKernelConvolution(&catSobel, sobelkernel, 9);

    Image catGaussian = ImageCopy(image);

    for (i32 i = 0; i < 6; i++)
    {
        ImageKernelConvolution(&catGaussian, gaussiankernel, 9);
    }

    ImageCrop(&image, Rectangle{ 0, 0, static_cast<f32>(200), static_cast<f32>(450) });
    ImageCrop(&catGaussian, Rectangle{ 0, 0, static_cast<f32>(200), static_cast<f32>(450) });
    ImageCrop(&catSobel, Rectangle{ 0, 0, static_cast<f32>(200), static_cast<f32>(450) });
    ImageCrop(&catSharpend, Rectangle{ 0, 0, static_cast<f32>(200), static_cast<f32>(450) });

    // Images converted to texture, GPU memory (VRAM)
    const Texture2D texture = LoadTextureFromImage(image);
    const Texture2D catSharpendTexture = LoadTextureFromImage(catSharpend);
    const Texture2D catSobelTexture = LoadTextureFromImage(catSobel);
    const Texture2D catGaussianTexture = LoadTextureFromImage(catGaussian);

    // Once images have been converted to texture and uploaded to VRAM,
    // they can be unloaded from RAM
    UnloadImage(image);
    UnloadImage(catGaussian);
    UnloadImage(catSobel);
    UnloadImage(catSharpend);

    SetTargetFPS(60);     // Set our game to run at 60 frames-per-second
    //---------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        // TODO: Update your variables here
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawTexture(catSharpendTexture, 0, 0, WHITE);
            DrawTexture(catSobelTexture, 200, 0, WHITE);
            DrawTexture(catGaussianTexture, 400, 0, WHITE);
            DrawTexture(texture, 600, 0, WHITE);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadTexture(texture);
    UnloadTexture(catGaussianTexture);
    UnloadTexture(catSobelTexture);
    UnloadTexture(catSharpendTexture);

    CloseWindow();                // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//------------------------------------------------------------------------------------
// Module Functions Definition
//------------------------------------------------------------------------------------
static void NormalizeKernel(f32 *kernel, i32 size)
{
    f32 sum = 0.0f;
    for (i32 i = 0; i < size; i++) sum += kernel[i];

    if (sum != 0.0f)
    {
        for (i32 i = 0; i < size; i++) kernel[i] /= sum;
    }
}
