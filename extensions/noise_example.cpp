/*******************************************************************************************
*
*   raylib-extended [noise] - example 03: GPU simplex noise textures (1D/2D/3D, compute shaders)
*
*   Shows how the Noise class renders simplex noise with compute shaders into
*   1D, 2D and 3D textures and how the results can be read back to the CPU.
*
*   Controls:
*       SPACE         : New random offset, renders all three textures again
*       1 / 2 / 3     : Save the 1D / 2D / 3D result as image (PNG) for inspection
*       ESC           : Close the window
*
*   NOTE: Requires OpenGL 4.3+ (compute shaders), the build has to use OPENGL_VERSION 4.3 or 4.6
*
*   License: zlib/libpng
**********************************************************************************************/

#include "raylib.h"

#include "noise.h"          // Required for: Noise (1D/2D/3D simplex noise via compute shaders)

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>

constexpr i32 screenWidth = 1280;
constexpr i32 screenHeight = 720;

constexpr i32 noise1DWidth = 1024;
constexpr i32 noise2DSize = 512;
constexpr i32 noise3DSize = 64;
constexpr f32 noise2DScale = 0.008f;
constexpr f32 noise3DScale = 0.05f;
constexpr f32 noise1DScale = 0.02f;

// Values below count as "the texture contains usable noise"
constexpr f32 minExpectedContrast = 0.35f;      // max - min over all texels

// Check a float image: value range, variation and alpha
static bool CheckNoiseImage(const char *label, const Image &image)
{
    if ((image.data == nullptr) || (image.width <= 0) || (image.height <= 0))
    {
        std::printf("FAIL  %s: no image data\n", label);
        return false;
    }

    const f32 *pixels = static_cast<const f32 *>(image.data);
    const i32 texelCount = image.width*image.height;

    f32 minimum = 1.0f, maximum = 0.0f, sum = 0.0f;
    auto outsideRange = 0, badAlpha = 0;

    for (i32 i = 0; i < texelCount; i++)
    {
        const f32 value = pixels[i*4];              // rgb holds the same noise value
        const f32 alpha = pixels[i*4 + 3];

        minimum = fminf(minimum, value);
        maximum = fmaxf(maximum, value);
        sum += value;

        if ((value < -0.001f) || (value > 1.001f)) outsideRange++;
        if (fabsf(alpha - 1.0f) > 0.001f) badAlpha++;
    }

    const f32 mean = sum/texelCount;
    const f32 contrast = maximum - minimum;
    const bool ok = (outsideRange == 0) && (badAlpha == 0) && (contrast > minExpectedContrast) && (mean > 0.2f) && (mean < 0.8f);

    std::printf("%s  %-34s size: %ix%i  min: %.3f  max: %.3f  mean: %.3f  contrast: %.3f  outside[0,1]: %i  badAlpha: %i\n",
                ok? "PASS" : "FAIL", label, image.width, image.height, minimum, maximum, mean, contrast, outsideRange, badAlpha);

    return ok;
}

// Compare two 3D layers, they must differ, otherwise the layered dispatch wrote only one layer
static bool CheckLayersDiffer(const char *label, const Image &layerA, const Image &layerB)
{
    if ((layerA.data == nullptr) || (layerB.data == nullptr)) return false;

    const f32 *a = static_cast<const f32 *>(layerA.data);
    const f32 *b = static_cast<const f32 *>(layerB.data);
    const i32 texelCount = layerA.width*layerA.height;

    auto different = 0;
    for (i32 i = 0; i < texelCount; i++) if (fabsf(a[i*4] - b[i*4]) > 0.01f) different++;

    const f32 ratio = 100.0f*different/texelCount;
    const bool ok = ratio > 50.0f;

    std::printf("%s  %-34s different texels: %.1f%%\n", ok? "PASS" : "FAIL", label, ratio);

    return ok;
}

int main()
{
    if (!Noise::IsSupported())
    {
        std::printf("FAIL  compute shaders are not available, build raylib with OPENGL_VERSION 4.3 or 4.6\n");
        return EXIT_FAILURE;
    }

    InitWindow(screenWidth, screenHeight, "raylib-extended [noise] - 1D/2D/3D simplex noise via compute shaders");
    SetTargetFPS(60);

    std::printf("INFO  GL renderer: %s\n", glGetString(GL_RENDERER));
    std::printf("INFO  GL version:  %s\n", glGetString(GL_VERSION));

    Noise noise;
    auto failures = 0;

    // Random offset, so every run produces a different noise field
    std::mt19937 rng(1234);
    std::uniform_real_distribution<f32> uniform(-100.0f, 100.0f);
    auto offset3D = vec3{ uniform(rng), uniform(rng), uniform(rng) };

    Noise::Texture1D texture1D = { };
    Texture2D texture2D = { };
    Noise::Texture3D texture3D = { };

    Texture2D sliceTexture = { };           // 3D slice converted to a 2D texture for display
    Texture2D stripTexture = { };           // 1D noise converted to a 2D texture for display
    Image sliceImage = { };
    Image stripImage = { };

    // Render the three textures and check the results on the CPU
    auto Render = [&]()
    {
        if (texture1D.IsValid()) texture1D.Unload();
        if (texture2D.id != 0) UnloadTexture(texture2D);
        if (texture3D.IsValid()) texture3D.Unload();
        if (sliceTexture.id != 0) UnloadTexture(sliceTexture);
        if (stripTexture.id != 0) UnloadTexture(stripTexture);
        if (sliceImage.data != nullptr) UnloadImage(sliceImage);
        if (stripImage.data != nullptr) UnloadImage(stripImage);

        texture1D = noise.Create1D(noise1DWidth, noise1DScale, offset3D.x);
        texture2D = noise.Create2D(noise2DSize, noise2DSize, noise2DScale, vec2{ offset3D.x, offset3D.y });
        texture3D = noise.Create3D(noise3DSize, noise3DSize, noise3DSize, noise3DScale, offset3D);

        std::printf("\n--- noise textures, offset (%.1f, %.1f, %.1f) ---\n", offset3D.x, offset3D.y, offset3D.z);

        // 1D: a single row, drawn as a thin strip
        stripImage = noise.Read(texture1D);
        if (!CheckNoiseImage("1D noise (GL_TEXTURE_1D)", stripImage)) failures++;
        stripTexture = LoadTextureFromImage(stripImage);

        // 2D
        const Image image2D = noise.Read(texture2D);
        if (!CheckNoiseImage("2D noise (Texture2D)", image2D)) failures++;
        UnloadImage(image2D);

        // 3D: check the first layer and one in the middle, then compare them
        sliceImage = noise.Read(texture3D, 0);
        const Image middleLayer = noise.Read(texture3D, texture3D.depth/2);

        if (!CheckNoiseImage("3D noise (GL_TEXTURE_3D, layer 0)", sliceImage)) failures++;
        if (!CheckNoiseImage("3D noise (GL_TEXTURE_3D, middle layer)", middleLayer)) failures++;
        if (!CheckLayersDiffer("3D layers 0 vs middle", sliceImage, middleLayer)) failures++;

        sliceTexture = LoadTextureFromImage(sliceImage);

        UnloadImage(middleLayer);
    };

    Render();

    auto frameCount = 0;
    auto showLayer = 0;

    while (!WindowShouldClose())
    {
        if (IsKeyPressed(KEY_SPACE))
        {
            offset3D = vec3{ uniform(rng), uniform(rng), uniform(rng) };
            Render();
        }

        // Cycle through the 3D layers with the mouse wheel, it shows that all layers contain noise
        if (texture3D.IsValid())
        {
            const f32 wheel = GetMouseWheelMove();
            if (wheel != 0.0f)
            {
                showLayer = (showLayer + static_cast<i32>(-wheel) + texture3D.depth)%texture3D.depth;

                if (sliceTexture.id != 0) UnloadTexture(sliceTexture);
                if (sliceImage.data != nullptr) UnloadImage(sliceImage);
                sliceImage = noise.Read(texture3D, showLayer);
                sliceTexture = LoadTextureFromImage(sliceImage);
            }
        }

        // Save the CPU copies for inspection
        if (IsKeyPressed(KEY_ONE)) ExportImage(stripImage, "noise_1d.png");
        if (IsKeyPressed(KEY_TWO))
        {
            const Image image = noise.Read(texture2D);
            ExportImage(image, "noise_2d.png");
            UnloadImage(image);
        }
        if (IsKeyPressed(KEY_THREE))
        {
            const Image image = noise.Read(texture3D, showLayer);
            ExportImage(image, "noise_3d_slice.png");
            UnloadImage(image);
        }

        BeginDrawing();
            ClearBackground(BLACK);

            // 1D noise as a strip
            const f32 stripHeight = 80.0f;
            DrawTexturePro(stripTexture, Rectangle{ 0.0f, 0.0f, static_cast<f32>(stripTexture.width), static_cast<f32>(stripTexture.height) },
                           Rectangle{ 40.0f, 60.0f, static_cast<f32>(screenWidth - 80), stripHeight }, vec2{ 0.0f, 0.0f }, 0.0f, WHITE);
            DrawText(TextFormat("1D noise - %i texels - GL_TEXTURE_1D, work group 64x1x1", texture1D.width), 40, 30, 20, LIGHTGRAY);

            // 2D noise
            DrawTexturePro(texture2D, Rectangle{ 0.0f, 0.0f, static_cast<f32>(texture2D.width), static_cast<f32>(texture2D.height) },
                           Rectangle{ 40.0f, 190.0f, 480.0f, 480.0f }, vec2{ 0.0f, 0.0f }, 0.0f, WHITE);
            DrawText(TextFormat("2D noise - %ix%i - work group 8x8x1", texture2D.width, texture2D.height), 40, 160, 20, LIGHTGRAY);

            // 3D noise, current slice
            DrawTexturePro(sliceTexture, Rectangle{ 0.0f, 0.0f, static_cast<f32>(sliceTexture.width), static_cast<f32>(sliceTexture.height) },
                           Rectangle{ 560.0f, 190.0f, 480.0f, 480.0f }, vec2{ 0.0f, 0.0f }, 0.0f, WHITE);
            DrawText(TextFormat("3D noise - %ix%ix%i - layer %i/%i - work group 8x8x8",
                                texture3D.width, texture3D.height, texture3D.depth, showLayer, texture3D.depth - 1), 560, 160, 20, LIGHTGRAY);

            // Info overlay
            DrawFPS(10, 10);
            DrawText("SPACE: new random offset | MOUSE WHEEL: 3D layer | 1/2/3: save PNG | ESC: quit", 40, screenHeight - 40, 20, DARKGRAY);
            DrawText(TextFormat("%s", (failures == 0)? "self check: all textures PASS" : "self check: FAILURES, see console"),
                     40, screenHeight - 68, 20, (failures == 0)? LIME : RED);
        EndDrawing();

        frameCount++;

        // Headless friendly: render a few frames, save a screenshot and quit
        if (frameCount == 20)
        {
            TakeScreenshot("noise_example.png");
            ExportImage(stripImage, "noise_1d.png");

            const Image image2D = noise.Read(texture2D);
            ExportImage(image2D, "noise_2d.png");
            UnloadImage(image2D);

            const Image image3D = noise.Read(texture3D, texture3D.depth/2);
            ExportImage(image3D, "noise_3d_slice.png");
            UnloadImage(image3D);
            std::printf("\nINFO  screenshots saved: noise_example.png, noise_1d.png, noise_2d.png, noise_3d_slice.png\n");
            break;
        }
    }

    if (sliceTexture.id != 0) UnloadTexture(sliceTexture);
    if (stripTexture.id != 0) UnloadTexture(stripTexture);
    if (sliceImage.data != nullptr) UnloadImage(sliceImage);
    if (stripImage.data != nullptr) UnloadImage(stripImage);
    if (texture2D.id != 0) UnloadTexture(texture2D);
    texture1D.Unload();
    texture3D.Unload();

    // NOTE: The compute programs belong to the GL context, they have to go before CloseWindow()
    noise.UnloadPrograms();

    CloseWindow();

    std::printf("\n%d failure(s)\n", failures);

    return (failures == 0)? EXIT_SUCCESS : EXIT_FAILURE;
}
