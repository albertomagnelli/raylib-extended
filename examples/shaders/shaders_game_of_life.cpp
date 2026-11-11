/*******************************************************************************************
*
*   raylib [shaders] example - game of life
*
*   Example complexity rating: [★★★☆] 3/4
*
*   NOTE: This example requires raylib OpenGL 3.3 or ES2 versions for shaders support,
*         OpenGL 1.1 does not support shaders, recompile raylib to OpenGL 3.3 version
*
*   Example originally created with raylib 6.0, last time updated with raylib 6.0
*
*   Example contributed by Jordi Santonja (@JordSant) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2025 Jordi Santonja (@JordSant)
*
********************************************************************************************/

#include "raylib.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"                 // Required for GUI controls

#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION            330
#else   // PLATFORM_ANDROID, PLATFORM_WEB
    #define GLSL_VERSION            100
#endif

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// Interaction mode
typedef enum {
    MODE_RUN = 0,
    MODE_PAUSE,
    MODE_DRAW,
} InteractionMode;

// Struct to store example preset patterns
typedef struct {
    const char *name;
    vec2 position;
} PresetPattern;

//----------------------------------------------------------------------------------
// Functions declaration
//----------------------------------------------------------------------------------
void FreeImageToDraw(Image **imageToDraw);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr i32 screenWidth = 800;
    constexpr i32 screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [shaders] example - game of life");

    constexpr i32 menuWidth = 100;
    const i32 windowWidth = screenWidth - menuWidth;
    const i32 windowHeight = screenHeight;

    constexpr i32 worldWidth = 2048;
    constexpr i32 worldHeight = 2048;

    constexpr i32 randomTiles = 8;      // Random preset: divide the world to compute random points in each tile

    const Rectangle worldRectSource = { 0, 0, static_cast<f32>(worldWidth), static_cast<f32>(-worldHeight) };
    const Rectangle worldRectDest = { 0, 0, static_cast<f32>(worldWidth), static_cast<f32>(worldHeight) };
    const Rectangle textureOnScreen = { 0, 0, static_cast<f32>(windowWidth), static_cast<f32>(windowHeight) };

    const PresetPattern presetPatterns[] = {
        { "Glider", { 0.5f, 0.5f } }, { "R-pentomino", { 0.5f, 0.5f } }, { "Acorn", { 0.5f,0.5f } },
        { "Spaceships", { 0.1f, 0.5f } }, { "Still lifes", { 0.5f, 0.5f } }, { "Oscillators", { 0.5f, 0.5f } },
        { "Puffer train", { 0.1f, 0.5f } }, { "Glider Gun", { 0.2f, 0.2f } }, { "Breeder", { 0.1f, 0.5f } },
        { "Random", { 0.5f, 0.5f } }
    };

    const i32 numberOfPresets = sizeof(presetPatterns)/sizeof(presetPatterns[0]);

    i32 zoom = 1;
    f32 offsetX = (worldWidth - windowWidth)/2.0f;    // Centered on window
    f32 offsetY = (worldHeight - windowHeight)/2.0f;  // Centered on window
    i32 framesPerStep = 1;
    i32 frame = 0;

    i32 preset = -1;            // No button pressed for preset
    i32 mode = MODE_RUN;        // Starting mode: running
    bool buttonZoomIn = false;  // Button states: false not pressed
    bool buttonZomOut = false;
    bool buttonFaster = false;
    bool buttonSlower = false;

    // Load shader
    const Shader shdrGameOfLife = LoadShader({}, TextFormat("resources/shaders/glsl%i/game_of_life.fs", GLSL_VERSION));

    // Set shader uniform size of the world
    const i32 resolutionLoc = GetShaderLocation(shdrGameOfLife, "resolution");
    const f32 resolution[2] = { static_cast<f32>(worldWidth), static_cast<f32>(worldHeight) };
    SetShaderValue(shdrGameOfLife, resolutionLoc, resolution, SHADER_UNIFORM_VEC2);

    // Define two textures: the current world and the previous world
    RenderTexture2D world1 = LoadRenderTexture(worldWidth, worldHeight);
    RenderTexture2D world2 = LoadRenderTexture(worldWidth, worldHeight);
    BeginTextureMode(world2);
        ClearBackground(RAYWHITE);
    EndTextureMode();

    const Image startPattern = LoadImage("resources/game_of_life/r_pentomino.png");
    UpdateTextureRec(world2.texture, Rectangle{ worldWidth/2.0f, worldHeight/2.0f, static_cast<f32>(startPattern.width), static_cast<f32>(startPattern.height) }, startPattern.data);
    UnloadImage(startPattern);

    // Pointers to the two textures, to be swapped
    RenderTexture2D *currentWorld = &world2;
    RenderTexture2D *previousWorld = &world1;

    // Image to be used in DRAW mode, to be changed with mouse input
    Image *imageToDraw = nullptr;

    SetTargetFPS(60);               // Set at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        frame++;

        // Change zoom: both by buttons or by mouse wheel
        const f32 mouseWheelMove = GetMouseWheelMove();
        if (buttonZoomIn || (buttonZomOut && (zoom > 1)) || (mouseWheelMove != 0.0f))
        {
            FreeImageToDraw(&imageToDraw);  // Zoom change: free the image to draw to be recreated again

            const f32 centerX = offsetX + (windowWidth/2.0f)/zoom;
            const f32 centerY = offsetY + (windowHeight/2.0f)/zoom;
            if (buttonZoomIn || (mouseWheelMove > 0.0f)) zoom *= 2;
            if ((buttonZomOut || (mouseWheelMove < 0.0f)) && (zoom > 1)) zoom /= 2;
            offsetX = centerX - (windowWidth/2.0f)/zoom;
            offsetY = centerY - (windowHeight/2.0f)/zoom;
        }

        // Change speed: number of frames per step
        if (buttonFaster && framesPerStep > 1)    framesPerStep--;
        if (buttonSlower)                         framesPerStep++;

        // Mouse management
        if ((mode == MODE_RUN) || (mode == MODE_PAUSE))
        {
            FreeImageToDraw(&imageToDraw);  // Free the image to draw: no longer needed in these modes

            // Pan with mouse left button
            static vec2 previousMousePosition = { 0.0f, 0.0f };
            const vec2 mousePosition = GetMousePosition();
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && (mousePosition.x < windowWidth))
            {
                offsetX -= (mousePosition.x - previousMousePosition.x)/zoom;
                offsetY -= (mousePosition.y - previousMousePosition.y)/zoom;
            }
            previousMousePosition = mousePosition;
        }
        else // MODE_DRAW
        {
            const f32 offsetDecimalX = offsetX - floorf(offsetX);
            const f32 offsetDecimalY = offsetY - floorf(offsetY);
            i32 sizeInWorldX = static_cast<i32>(ceilf((windowWidth + offsetDecimalX*zoom)/zoom));
            i32 sizeInWorldY = static_cast<i32>(ceilf((windowHeight + offsetDecimalY*zoom)/zoom));
            if (offsetX + sizeInWorldX >= worldWidth) sizeInWorldX = worldWidth - static_cast<i32>(floorf(offsetX));
            if (offsetY + sizeInWorldY >= worldHeight) sizeInWorldY = worldHeight - static_cast<i32>(floorf(offsetY));

            // Create image to draw if not created yet
            if (imageToDraw == nullptr)
            {
                const RenderTexture2D worldOnScreen = LoadRenderTexture(sizeInWorldX, sizeInWorldY);
                BeginTextureMode(worldOnScreen);
                    DrawTexturePro(currentWorld->texture, Rectangle{ floorf(offsetX), floorf(offsetY), static_cast<f32>(sizeInWorldX), -static_cast<f32>(sizeInWorldY) },
                            Rectangle{ 0, 0, static_cast<f32>(sizeInWorldX), static_cast<f32>(sizeInWorldY) }, vec2{ 0, 0 }, 0.0f, WHITE);
                EndTextureMode();
                imageToDraw = static_cast<Image *>(malloc(sizeof(Image)));
                *imageToDraw = LoadImageFromTexture(worldOnScreen.texture);

                UnloadRenderTexture(worldOnScreen);
            }

            const vec2 mousePosition = GetMousePosition();
            static i32 firstColor = -1;
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && (mousePosition.x < windowWidth))
            {
                i32 mouseX = static_cast<i32>(mousePosition.x + offsetDecimalX*zoom)/zoom;
                i32 mouseY = static_cast<i32>(mousePosition.y + offsetDecimalY*zoom)/zoom;
                if (mouseX >= sizeInWorldX) mouseX = sizeInWorldX - 1;
                if (mouseY >= sizeInWorldY) mouseY = sizeInWorldY - 1;
                if (firstColor == -1) firstColor = (GetImageColor(*imageToDraw, mouseX, mouseY).r < 5)? 0 : 1;
                const i32 prevColor = (GetImageColor(*imageToDraw, mouseX, mouseY).r < 5)? 0 : 1;

                ImageDrawPixel(imageToDraw, mouseX, mouseY, (firstColor) ? BLACK : RAYWHITE);

                if (prevColor != firstColor) UpdateTextureRec(currentWorld->texture, Rectangle{ floorf(offsetX), floorf(offsetY), static_cast<f32>(sizeInWorldX), static_cast<f32>(sizeInWorldY) }, imageToDraw->data);
            }
            else firstColor = -1;
        }

        // Load selected preset
        if (preset >= 0)
        {
            Image pattern;
            if (preset < numberOfPresets - 1)   // Preset with pattern image lo load
            {
                switch (preset)
                {
                    case 0: pattern = LoadImage("resources/game_of_life/glider.png"); break;
                    case 1: pattern = LoadImage("resources/game_of_life/r_pentomino.png"); break;
                    case 2: pattern = LoadImage("resources/game_of_life/acorn.png"); break;
                    case 3: pattern = LoadImage("resources/game_of_life/spaceships.png"); break;
                    case 4: pattern = LoadImage("resources/game_of_life/still_lifes.png"); break;
                    case 5: pattern = LoadImage("resources/game_of_life/oscillators.png"); break;
                    case 6: pattern = LoadImage("resources/game_of_life/puffer_train.png"); break;
                    case 7: pattern = LoadImage("resources/game_of_life/glider_gun.png"); break;
                    case 8: pattern = LoadImage("resources/game_of_life/breeder.png"); break;
                }
                BeginTextureMode(*currentWorld);
                    ClearBackground(RAYWHITE);
                EndTextureMode();

                UpdateTextureRec(currentWorld->texture, Rectangle{ worldWidth*presetPatterns[preset].position.x - pattern.width/2.0f,
                                                                     worldHeight*presetPatterns[preset].position.y - pattern.height/2.0f,
                                                                     static_cast<f32>(pattern.width), static_cast<f32>(pattern.height) }, pattern.data);
            }
            else    // Last preset: Random values
            {
                pattern = GenImageColor(worldWidth/randomTiles, worldHeight/randomTiles, RAYWHITE);
                for (i32 i = 0; i < randomTiles; i++)
                {
                    for (i32 j = 0; j < randomTiles; j++)
                    {
                        ImageClearBackground(&pattern, RAYWHITE);
                        for (i32 x = 0; x < pattern.width; x++)
                        {
                            for (i32 y = 0; y < pattern.height; y++)
                            {
                                if (GetRandomValue(0, 100) < 15) ImageDrawPixel(&pattern, x, y, BLACK);
                            }
                        }
                        UpdateTextureRec(currentWorld->texture,
                                         Rectangle{ static_cast<f32>(pattern.width*i), static_cast<f32>(pattern.height*j),
                                                      static_cast<f32>(pattern.width), static_cast<f32>(pattern.height) }, pattern.data);
                    }
                }
            }

            UnloadImage(pattern);

            mode = MODE_PAUSE;
            offsetX = worldWidth*presetPatterns[preset].position.x - static_cast<f32>(windowWidth)/zoom/2.0f;
            offsetY = worldHeight*presetPatterns[preset].position.y - static_cast<f32>(windowHeight)/zoom/2.0f;
        }

        // Check window draw inside world limits
        if (offsetX < 0) offsetX = 0;
        if (offsetY < 0) offsetY = 0;
        if (offsetX > worldWidth - static_cast<f32>(windowWidth)/zoom) offsetX = worldWidth - static_cast<f32>(windowWidth)/zoom;
        if (offsetY > worldHeight - static_cast<f32>(windowHeight)/zoom) offsetY = worldHeight - static_cast<f32>(windowHeight)/zoom;

        // Rectangles for drawing texture portion to screen
        const Rectangle textureSourceToScreen = { offsetX, offsetY, static_cast<f32>(windowWidth)/zoom, static_cast<f32>(windowHeight)/zoom };
        //----------------------------------------------------------------------------------

        // Draw to texture
        //----------------------------------------------------------------------------------
        if ((mode == MODE_RUN) && ((frame%framesPerStep) == 0))
        {
            // Swap worlds
            RenderTexture2D *tempWorld = currentWorld;
            currentWorld = previousWorld;
            previousWorld = tempWorld;

            // Draw to texture
            BeginTextureMode(*currentWorld);
                BeginShaderMode(shdrGameOfLife);
                    DrawTexturePro(previousWorld->texture, worldRectSource, worldRectDest, vec2{ 0, 0 }, 0.0f, RAYWHITE);
                EndShaderMode();
            EndTextureMode();
        }
        //----------------------------------------------------------------------------------

        // Draw to screen
        //----------------------------------------------------------------------------------
        BeginDrawing();

            DrawTexturePro(currentWorld->texture, textureSourceToScreen, textureOnScreen, vec2{ 0, 0 }, 0.0f, WHITE);

            DrawLine(windowWidth, 0, windowWidth, screenHeight, Color{ 218, 218, 218, 255 });
            DrawRectangle(windowWidth, 0, screenWidth - windowWidth, screenHeight, Color{ 232, 232, 232, 255 });

            DrawText("Conway's", 704, 4, 20, DARKBLUE);
            DrawText(" game of", 704, 19, 20, DARKBLUE);
            DrawText("  life", 708, 34, 20, DARKBLUE);
            DrawText("in raylib", 757, 42, 6, BLACK);

            DrawText("Presets", 710, 58, 8, GRAY);
            preset = -1;
            for (i32 i = 0; i < numberOfPresets; i++)
                if (GuiButton(Rectangle{ 710.0f, 70.0f + 18*i, 80.0f, 16.0f }, presetPatterns[i].name)) preset = i;

            GuiToggleGroup(Rectangle{ 710, 258, 80, 16 }, "Run\nPause\nDraw", &mode);

            DrawText(TextFormat("Zoom: %ix", zoom), 710, 316, 8, GRAY);
            buttonZoomIn = GuiButton(Rectangle{ 710, 328, 80, 16 }, "Zoom in");
            buttonZomOut = GuiButton(Rectangle{ 710, 346, 80, 16 }, "Zoom out");

            DrawText(TextFormat("Speed: %i frame%s", framesPerStep, (framesPerStep > 1)? "s" : ""), 710, 370, 8, GRAY);
            buttonFaster = GuiButton(Rectangle{ 710, 382, 80, 16 }, "Faster");
            buttonSlower = GuiButton(Rectangle{ 710, 400, 80, 16 }, "Slower");

            DrawFPS(712, 426);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadShader(shdrGameOfLife);
    UnloadRenderTexture(world1);
    UnloadRenderTexture(world2);

    FreeImageToDraw(&imageToDraw);

    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//----------------------------------------------------------------------------------
// Functions definition
//----------------------------------------------------------------------------------
void FreeImageToDraw(Image **imageToDraw)
{
    if (*imageToDraw != nullptr)
    {
        UnloadImage(**imageToDraw);
        free(*imageToDraw);
        *imageToDraw = nullptr;
    }
}
