/*******************************************************************************************
*
*   raylib [core] example - compute hash
*
*   Example complexity rating: [★★☆☆] 2/4
*
*   Example originally created with raylib 6.0, last time updated with raylib 6.0
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2025 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
static char *GetDataAsHexText(const u32 *data, i32 dataSize);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr auto screenWidth = 800;
    constexpr auto screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [core] example - compute hash");

    // UI controls variables
    char textInput[96] = "The quick brown fox jumps over the lazy dog.";
    auto textBoxEditMode = false;
    auto btnComputeHashes = false;

    // Data hash values
    u32 hashCRC32 = 0;
    u32 *hashMD5 = nullptr;
    u32 *hashSHA1 = nullptr;
    u32 *hashSHA256 = nullptr;

    // Base64 encoded data
    std::string base64Text;
    auto base64TextSize = 0;

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        if (btnComputeHashes)
        {
            const auto textInputLen = static_cast<i32>(strlen(textInput));

            // Encode data to Base64 string (includes NULL terminator), memory must be MemFree()
            base64Text = EncodeDataBase64(reinterpret_cast<u8 *>(textInput), textInputLen, &base64TextSize);

            hashCRC32 = ComputeCRC32(reinterpret_cast<u8 *>(textInput), textInputLen);     // Compute CRC32 hash code (4 bytes)
            hashMD5 = ComputeMD5(reinterpret_cast<u8 *>(textInput), textInputLen);         // Compute MD5 hash code, returns static int[4] (16 bytes)
            hashSHA1 = ComputeSHA1(reinterpret_cast<u8 *>(textInput), textInputLen);       // Compute SHA1 hash code, returns static int[5] (20 bytes)
            hashSHA256 = ComputeSHA256(reinterpret_cast<u8 *>(textInput), textInputLen);   // Compute SHA256 hash code, returns static int[8] (32 bytes)
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            GuiSetStyle(DEFAULT, TEXT_SIZE, 20);
            GuiSetStyle(DEFAULT, TEXT_SPACING, 2);
            GuiLabel(Rectangle{ 40, 26, 720, 32 }, "INPUT DATA (TEXT):");
            GuiSetStyle(DEFAULT, TEXT_SPACING, 1);
            GuiSetStyle(DEFAULT, TEXT_SIZE, 10);

            if (GuiTextBox(Rectangle{ 40, 64, 720, 32 }, textInput, 95, textBoxEditMode)) textBoxEditMode = !textBoxEditMode;

            btnComputeHashes = GuiButton(Rectangle{ 40, 64 + 40, 720, 32 }, "COMPUTE INPUT DATA HASHES");

            GuiSetStyle(DEFAULT, TEXT_SIZE, 20);
            GuiSetStyle(DEFAULT, TEXT_SPACING, 2);
            GuiLabel(Rectangle{ 40, 160, 720, 32 }, "INPUT DATA HASH VALUES:");
            GuiSetStyle(DEFAULT, TEXT_SPACING, 1);
            GuiSetStyle(DEFAULT, TEXT_SIZE, 10);

            GuiSetStyle(TEXTBOX, TEXT_READONLY, 1);
            GuiLabel(Rectangle{ 40, 200, 120, 32 }, "CRC32 [32 bit]:");
            GuiTextBox(Rectangle{ 40 + 120, 200, 720 - 120, 32 }, GetDataAsHexText(&hashCRC32, 1), 120, false);
            GuiLabel(Rectangle{ 40, 200 + 36, 120, 32 }, "MD5 [128 bit]:");
            GuiTextBox(Rectangle{ 40 + 120, 200 + 36, 720 - 120, 32 }, GetDataAsHexText(hashMD5, 4), 120, false);
            GuiLabel(Rectangle{ 40, 200 + 36*2, 120, 32 }, "SHA1 [160 bit]:");
            GuiTextBox(Rectangle{ 40 + 120, 200 + 36*2, 720 - 120, 32 }, GetDataAsHexText(hashSHA1, 5), 120, false);
            GuiLabel(Rectangle{ 40, 200 + 36*3, 120, 32 }, "SHA256 [256 bit]:");
            GuiTextBox(Rectangle{ 40 + 120, 200 + 36*3, 720 - 120, 32 }, GetDataAsHexText(hashSHA256, 8), 120, false);

            GuiSetState(STATE_FOCUSED);
            GuiLabel(Rectangle{ 40, 200 + 36*5 - 30, 320, 32 }, "BONUS - BAS64 ENCODED STRING:");
            GuiSetState(STATE_NORMAL);
            GuiLabel(Rectangle{ 40, 200 + 36*5, 120, 32 }, "BASE64 ENCODING:");
            GuiTextBox(Rectangle{ 40 + 120, 200 + 36*5, 720 - 120, 32 }, base64Text.data(), 120, false);
            GuiSetStyle(TEXTBOX, TEXT_READONLY, 0);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------

    CloseWindow();          // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------
static char *GetDataAsHexText(const u32 *data, i32 dataSize)
{
    static char text[128] = {};
    memset(text, 0, 128);

    if ((data != nullptr) && (dataSize > 0) && (dataSize < ((128/8) - 1)))
    {
        for (i32 i = 0; i < dataSize; i++) snprintf(text + i*8, 128 - i*8, "%08X", data[i]);
    }
    else snprintf(text, 128, "00000000");

    return text;
}
