/*******************************************************************************************
*
*   raylib [core] example - viewport scaling
*
*   Example complexity rating: [★★☆☆] 2/4
*
*   Example originally created with raylib 5.5, last time updated with raylib 5.5
*
*   Example contributed by Agnis Aldiņš (@nezvers) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2025 Agnis Aldiņš (@nezvers)
*
********************************************************************************************/

#include "raylib.h"

#define RESOLUTION_COUNT    4   // For iteration purposes and teaching example

typedef enum {
    // Only upscale, useful for pixel art
    KEEP_ASPECT_INTEGER,
    KEEP_HEIGHT_INTEGER,
    KEEP_WIDTH_INTEGER,
    // Can also downscale
    KEEP_ASPECT,
    KEEP_HEIGHT,
    KEEP_WIDTH,
    // For itteration purposes and as a teaching example
    VIEWPORT_TYPE_COUNT,
} ViewportType;

// For displaying on GUI
const char *ViewportTypeNames[VIEWPORT_TYPE_COUNT] = {
    "KEEP_ASPECT_INTEGER",
    "KEEP_HEIGHT_INTEGER",
    "KEEP_WIDTH_INTEGER",
    "KEEP_ASPECT",
    "KEEP_HEIGHT",
    "KEEP_WIDTH",
};

//--------------------------------------------------------------------------------------
// Module Functions Declaration
//--------------------------------------------------------------------------------------
static void KeepAspectCenteredInteger(i32 screenWidth, i32 screenHeight, i32 gameWidth, i32 gameHeight, Rectangle *sourceRect, Rectangle *destRect);
static void KeepHeightCenteredInteger(i32 screenWidth, i32 screenHeight, i32 gameWidth, i32 gameHeight, Rectangle *sourceRect, Rectangle *destRect);
static void KeepWidthCenteredInteger(i32 screenWidth, i32 screenHeight, i32 gameWidth, i32 gameHeight, Rectangle *sourceRect, Rectangle *destRect);
static void KeepAspectCentered(i32 screenWidth, i32 screenHeight, i32 gameWidth, i32 gameHeight, Rectangle *sourceRect, Rectangle *destRect);
static void KeepHeightCentered(i32 screenWidth, i32 screenHeight, i32 gameWidth, i32 gameHeight, Rectangle *sourceRect, Rectangle *destRect);
static void KeepWidthCentered(i32 screenWidth, i32 screenHeight, i32 gameWidth, i32 gameHeight, Rectangle *sourceRect, Rectangle *destRect);
static void ResizeRenderSize(ViewportType viewportType, i32 *screenWidth, i32 *screenHeight, i32 gameWidth, i32 gameHeight, Rectangle *sourceRect, Rectangle *destRect, RenderTexture2D *target);

// Example how to calculate position on RenderTexture
static vec2 Screen2RenderTexturePosition(vec2 point, Rectangle *textureRect, Rectangle *scaledRect);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //---------------------------------------------------------
    i32 screenWidth = 800;
    i32 screenHeight = 450;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, "raylib [core] example - viewport scaling");

    // Preset resolutions that could be created by subdividing screen resolution
    const vec2 resolutionList[RESOLUTION_COUNT] = {
        vec2{ 64, 64 },
        vec2{ 256, 240 },
        vec2{ 320, 180 },
        // 4K doesn't work with integer scaling but included for example purposes with non-integer scaling
        vec2{ 3840, 2160 },
    };

    i32 resolutionIndex = 0;
    i32 gameWidth = 64;
    i32 gameHeight = 64;

    RenderTexture2D target = {};
    Rectangle sourceRect = Rectangle{};
    Rectangle destRect = Rectangle{};

    ViewportType viewportType = KEEP_ASPECT_INTEGER;
    ResizeRenderSize(viewportType, &screenWidth, &screenHeight, gameWidth, gameHeight, &sourceRect, &destRect, &target);

    // Button rectangles
    const Rectangle decreaseResolutionButton = Rectangle{ 200, 30, 10, 10 };
    const Rectangle increaseResolutionButton = Rectangle{ 215, 30, 10, 10 };
    const Rectangle decreaseTypeButton = Rectangle{ 200, 45, 10, 10 };
    const Rectangle increaseTypeButton = Rectangle{ 215, 45, 10, 10 };

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //----------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        if (IsWindowResized()) ResizeRenderSize(viewportType, &screenWidth, &screenHeight, gameWidth, gameHeight, &sourceRect, &destRect, &target);

        const vec2 mousePosition = GetMousePosition();
        const bool mousePressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

        // Check buttons and rescale
        if (CheckCollisionPointRec(mousePosition, decreaseResolutionButton) && mousePressed)
        {
            resolutionIndex = (resolutionIndex + RESOLUTION_COUNT - 1)%RESOLUTION_COUNT;
            gameWidth = static_cast<i32>(resolutionList[resolutionIndex].x);
            gameHeight = static_cast<i32>(resolutionList[resolutionIndex].y);
            ResizeRenderSize(viewportType, &screenWidth, &screenHeight, gameWidth, gameHeight, &sourceRect, &destRect, &target);
        }

        if (CheckCollisionPointRec(mousePosition, increaseResolutionButton) && mousePressed)
        {
            resolutionIndex = (resolutionIndex + 1)%RESOLUTION_COUNT;
            gameWidth = static_cast<i32>(resolutionList[resolutionIndex].x);
            gameHeight = static_cast<i32>(resolutionList[resolutionIndex].y);
            ResizeRenderSize(viewportType, &screenWidth, &screenHeight, gameWidth, gameHeight, &sourceRect, &destRect, &target);
        }

        if (CheckCollisionPointRec(mousePosition, decreaseTypeButton) && mousePressed)
        {
            viewportType = static_cast<ViewportType>((static_cast<i32>(viewportType) + VIEWPORT_TYPE_COUNT - 1)%VIEWPORT_TYPE_COUNT);
            ResizeRenderSize(viewportType, &screenWidth, &screenHeight, gameWidth, gameHeight, &sourceRect, &destRect, &target);
        }

        if (CheckCollisionPointRec(mousePosition, increaseTypeButton) && mousePressed)
        {
            viewportType = static_cast<ViewportType>((static_cast<i32>(viewportType) + 1)%VIEWPORT_TYPE_COUNT);
            ResizeRenderSize(viewportType, &screenWidth, &screenHeight, gameWidth, gameHeight, &sourceRect, &destRect, &target);
        }

        const vec2 textureMousePosition = Screen2RenderTexturePosition(mousePosition, &sourceRect, &destRect);
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        // Draw our scene to the render texture
        BeginTextureMode(target);
            ClearBackground(WHITE);
            DrawCircleV(textureMousePosition, 20.0f, LIME);
        EndTextureMode();

        // Draw render texture to main framebuffer
        BeginDrawing();
            ClearBackground(BLACK);

            // Draw our render texture with rotation applied
            DrawTexturePro(target.texture, sourceRect, destRect, vec2{ 0.0f, 0.0f }, 0.0f, WHITE);

            // Draw Native resolution (GUI or anything)
            // Draw info box
            const Rectangle infoRect = Rectangle{5, 5, 330, 105};
            DrawRectangleRec(infoRect, Fade(LIGHTGRAY, 0.7f));
            DrawRectangleLinesEx(infoRect, 1, BLUE);

            DrawText(TextFormat("Window Resolution: %d x %d", screenWidth, screenHeight), 15, 15, 10, BLACK);
            DrawText(TextFormat("Game Resolution: %d x %d", gameWidth, gameHeight), 15, 30, 10, BLACK);

            DrawText(TextFormat("Type: %s", ViewportTypeNames[viewportType]), 15, 45, 10, BLACK);
            const vec2 scaleRatio = vec2{destRect.width/sourceRect.width, -destRect.height/sourceRect.height};
            if (scaleRatio.x < 0.001f || scaleRatio.y < 0.001f) DrawText(TextFormat("Scale ratio: INVALID"), 15, 60, 10, BLACK);
            else DrawText(TextFormat("Scale ratio: %.2f x %.2f", scaleRatio.x, scaleRatio.y), 15, 60, 10, BLACK);

            DrawText(TextFormat("Source size: %.2f x %.2f", sourceRect.width, -sourceRect.height), 15, 75, 10, BLACK);
            DrawText(TextFormat("Destination size: %.2f x %.2f", destRect.width, destRect.height), 15, 90, 10, BLACK);

            // Draw buttons
            DrawRectangleRec(decreaseTypeButton, SKYBLUE);
            DrawRectangleRec(increaseTypeButton, SKYBLUE);
            DrawRectangleRec(decreaseResolutionButton, SKYBLUE);
            DrawRectangleRec(increaseResolutionButton, SKYBLUE);
            DrawText("<", static_cast<i32>(decreaseTypeButton.x) + 3, static_cast<i32>(decreaseTypeButton.y) + 1, 10, BLACK);
            DrawText(">", static_cast<i32>(increaseTypeButton.x) + 3, static_cast<i32>(increaseTypeButton.y) + 1, 10, BLACK);
            DrawText("<", static_cast<i32>(decreaseResolutionButton.x) + 3, static_cast<i32>(decreaseResolutionButton.y) + 1, 10, BLACK);
            DrawText(">", static_cast<i32>(increaseResolutionButton.x) + 3, static_cast<i32>(increaseResolutionButton.y) + 1, 10, BLACK);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //----------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //----------------------------------------------------------------------------------

    return 0;
}

//--------------------------------------------------------------------------------------
// Module Functions Definition
//--------------------------------------------------------------------------------------
static void KeepAspectCenteredInteger(i32 screenWidth, i32 screenHeight, i32 gameWidth, i32 gameHeight, Rectangle *sourceRect, Rectangle *destRect)
{
    sourceRect->x = 0.0f;
    sourceRect->y = static_cast<f32>(gameHeight);
    sourceRect->width = static_cast<f32>(gameWidth);
    sourceRect->height = static_cast<f32>(-gameHeight);

    const i32 ratio_x = (screenWidth/gameWidth);
    const i32 ratio_y = (screenHeight/gameHeight);
    const f32 resizeRatio = static_cast<f32>((ratio_x < ratio_y)? ratio_x : ratio_y);

    destRect->x = static_cast<f32>(static_cast<i32>((screenWidth - (gameWidth*resizeRatio))*0.5f));
    destRect->y = static_cast<f32>(static_cast<i32>((screenHeight - (gameHeight*resizeRatio))*0.5f));
    destRect->width = static_cast<f32>(static_cast<i32>(gameWidth*resizeRatio));
    destRect->height = static_cast<f32>(static_cast<i32>(gameHeight*resizeRatio));
}

static void KeepHeightCenteredInteger(i32 screenWidth, i32 screenHeight, i32 gameWidth, i32 gameHeight, Rectangle *sourceRect, Rectangle *destRect)
{
    const f32 resizeRatio = static_cast<f32>(screenHeight)/gameHeight;
    sourceRect->x = 0.0f;
    sourceRect->y = 0.0f;
    sourceRect->width = static_cast<f32>(static_cast<i32>(screenWidth/resizeRatio));
    sourceRect->height = static_cast<f32>(-gameHeight);

    destRect->x = static_cast<f32>(static_cast<i32>((screenWidth - (sourceRect->width*resizeRatio))*0.5f));
    destRect->y = static_cast<f32>(static_cast<i32>((screenHeight - (gameHeight*resizeRatio))*0.5f));
    destRect->width = static_cast<f32>(static_cast<i32>(sourceRect->width*resizeRatio));
    destRect->height = static_cast<f32>(static_cast<i32>(gameHeight*resizeRatio));
}

static void KeepWidthCenteredInteger(i32 screenWidth, i32 screenHeight, i32 gameWidth, i32 gameHeight, Rectangle *sourceRect, Rectangle *destRect)
{
    const f32 resizeRatio = static_cast<f32>(screenWidth)/gameWidth;
    sourceRect->x = 0.0f;
    sourceRect->y = 0.0f;
    sourceRect->width = static_cast<f32>(gameWidth);
    sourceRect->height = static_cast<f32>(static_cast<i32>(screenHeight/resizeRatio));

    destRect->x = static_cast<f32>(static_cast<i32>((screenWidth - (gameWidth*resizeRatio))*0.5f));
    destRect->y = static_cast<f32>(static_cast<i32>((screenHeight - (sourceRect->height*resizeRatio))*0.5f));
    destRect->width = static_cast<f32>(static_cast<i32>(gameWidth*resizeRatio));
    destRect->height = static_cast<f32>(static_cast<i32>(sourceRect->height*resizeRatio));

    sourceRect->height *= -1.0f;
}

static void KeepAspectCentered(i32 screenWidth, i32 screenHeight, i32 gameWidth, i32 gameHeight, Rectangle *sourceRect, Rectangle *destRect)
{
    sourceRect->x = 0.0f;
    sourceRect->y = static_cast<f32>(gameHeight);
    sourceRect->width = static_cast<f32>(gameWidth);
    sourceRect->height = static_cast<f32>(-gameHeight);

    const f32 ratio_x = (static_cast<f32>(screenWidth)/static_cast<f32>(gameWidth));
    const f32 ratio_y = (static_cast<f32>(screenHeight)/static_cast<f32>(gameHeight));
    const f32 resizeRatio = (ratio_x < ratio_y ? ratio_x : ratio_y);

    destRect->x = static_cast<f32>(static_cast<i32>((screenWidth - (gameWidth*resizeRatio))*0.5f));
    destRect->y = static_cast<f32>(static_cast<i32>((screenHeight - (gameHeight*resizeRatio))*0.5f));
    destRect->width = static_cast<f32>(static_cast<i32>(gameWidth*resizeRatio));
    destRect->height = static_cast<f32>(static_cast<i32>(gameHeight*resizeRatio));
}

static void KeepHeightCentered(i32 screenWidth, i32 screenHeight, i32 gameWidth, i32 gameHeight, Rectangle *sourceRect, Rectangle *destRect)
{
    const f32 resizeRatio = (static_cast<f32>(screenHeight)/static_cast<f32>(gameHeight));
    sourceRect->x = 0.0f;
    sourceRect->y = 0.0f;
    sourceRect->width = static_cast<f32>(static_cast<i32>(static_cast<f32>(screenWidth)/resizeRatio));
    sourceRect->height = static_cast<f32>(-gameHeight);

    destRect->x = static_cast<f32>(static_cast<i32>((screenWidth - (sourceRect->width*resizeRatio))*0.5f));
    destRect->y = static_cast<f32>(static_cast<i32>((screenHeight - (gameHeight*resizeRatio))*0.5f));
    destRect->width = static_cast<f32>(static_cast<i32>(sourceRect->width*resizeRatio));
    destRect->height = static_cast<f32>(static_cast<i32>(gameHeight*resizeRatio));
}

static void KeepWidthCentered(i32 screenWidth, i32 screenHeight, i32 gameWidth, i32 gameHeight, Rectangle *sourceRect, Rectangle *destRect)
{
    const f32 resizeRatio = (static_cast<f32>(screenWidth)/static_cast<f32>(gameWidth));
    sourceRect->x = 0.0f;
    sourceRect->y = 0.0f;
    sourceRect->width = static_cast<f32>(gameWidth);
    sourceRect->height = static_cast<f32>(static_cast<i32>(static_cast<f32>(screenHeight)/resizeRatio));

    destRect->x = static_cast<f32>(static_cast<i32>((screenWidth - (gameWidth*resizeRatio))*0.5f));
    destRect->y = static_cast<f32>(static_cast<i32>((screenHeight - (sourceRect->height*resizeRatio))*0.5f));
    destRect->width = static_cast<f32>(static_cast<i32>(gameWidth*resizeRatio));
    destRect->height = static_cast<f32>(static_cast<i32>(sourceRect->height*resizeRatio));

    sourceRect->height *= -1.f;
}

static void ResizeRenderSize(ViewportType viewportType, i32 *screenWidth, i32 *screenHeight, i32 gameWidth, i32 gameHeight, Rectangle *sourceRect, Rectangle *destRect, RenderTexture2D *target)
{
    *screenWidth = GetScreenWidth();
    *screenHeight = GetScreenHeight();

    switch(viewportType)
    {
        case KEEP_ASPECT_INTEGER: KeepAspectCenteredInteger(*screenWidth, *screenHeight, gameWidth, gameHeight, sourceRect, destRect); break;
        case KEEP_HEIGHT_INTEGER: KeepHeightCenteredInteger(*screenWidth, *screenHeight, gameWidth, gameHeight, sourceRect, destRect); break;
        case KEEP_WIDTH_INTEGER: KeepWidthCenteredInteger(*screenWidth, *screenHeight, gameWidth, gameHeight, sourceRect, destRect); break;
        case KEEP_ASPECT: KeepAspectCentered(*screenWidth, *screenHeight, gameWidth, gameHeight, sourceRect, destRect); break;
        case KEEP_HEIGHT: KeepHeightCentered(*screenWidth, *screenHeight, gameWidth, gameHeight, sourceRect, destRect); break;
        case KEEP_WIDTH: KeepWidthCentered(*screenWidth, *screenHeight, gameWidth, gameHeight, sourceRect, destRect); break;
        default: break;
    }

    UnloadRenderTexture(*target);
    *target = LoadRenderTexture(static_cast<i32>(sourceRect->width), -static_cast<i32>(sourceRect->height));
}

// Example how to calculate position on RenderTexture
static vec2 Screen2RenderTexturePosition(vec2 point, Rectangle *textureRect, Rectangle *scaledRect)
{
    const vec2 relativePosition = {point.x - scaledRect->x, point.y - scaledRect->y};
    const vec2 ratio = {textureRect->width/scaledRect->width, -textureRect->height/scaledRect->height};

    return vec2{relativePosition.x*ratio.x, relativePosition.y*ratio.x};
}