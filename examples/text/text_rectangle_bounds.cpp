/*******************************************************************************************
*
*   raylib [text] example - rectangle bounds
*
*   Example complexity rating: [★★★★] 4/4
*
*   Example originally created with raylib 2.5, last time updated with raylib 4.0
*
*   Example contributed by Vlad Adrian (@demizdor) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2018-2025 Vlad Adrian (@demizdor) and Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
// Draw text using font inside rectangle limits
static void DrawTextBoxed(Font font, const char *text, Rectangle rec, f32 fontSize, f32 spacing, bool wordWrap, Color tint);
// Draw text using font inside rectangle limits with support for text selection
static void DrawTextBoxedSelectable(Font font, const char *text, Rectangle rec, f32 fontSize, f32 spacing, bool wordWrap, Color tint, i32 selectStart, i32 selectLength, Color selectTint, Color selectBackTint);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr auto screenWidth = 800;
    constexpr auto screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [text] example - rectangle bounds");

    const char text[] = "Text cannot escape\tthis container\t...word wrap also works when active so here's \
a long text for testing.\n\nLorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod \
tempor incididunt ut labore et dolore magna aliqua. Nec ullamcorper sit amet risus nullam eget felis eget.";

    auto resizing = false;
    auto wordWrap = true;

    auto container = Rectangle{ 25.0f, 25.0f, screenWidth - 50.0f, screenHeight - 250.0f };
    auto resizer = Rectangle{ container.x + container.width - 17, container.y + container.height - 17, 14, 14 };

    // Minimum width and heigh for the container rectangle
    constexpr f32 minWidth = 60;
    constexpr f32 minHeight = 60;
    const f32 maxWidth = screenWidth - 50.0f;
    const f32 maxHeight = screenHeight - 160.0f;

    auto lastMouse = vec2{ 0.0f, 0.0f };          // Stores last mouse coordinates
    Color borderColor = MAROON;               // Container border color
    const auto font = GetFontDefault();       // Get default system font

    SetTargetFPS(60);                   // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())        // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        if (IsKeyPressed(KEY_SPACE)) wordWrap = !wordWrap;

        const auto mouse = GetMousePosition();

        // Check if the mouse is inside the container and toggle border color
        if (CheckCollisionPointRec(mouse, container)) borderColor = Fade(MAROON, 0.4f);
        else if (!resizing) borderColor = MAROON;

        // Container resizing logic
        if (resizing)
        {
            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) resizing = false;

            const f32 width = container.width + (mouse.x - lastMouse.x);
            container.width = (width > minWidth)? ((width < maxWidth)? width : maxWidth) : minWidth;

            const f32 height = container.height + (mouse.y - lastMouse.y);
            container.height = (height > minHeight)? ((height < maxHeight)? height : maxHeight) : minHeight;
        }
        else
        {
            // Check if we're resizing
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, resizer)) resizing = true;
        }

        // Move resizer rectangle properly
        resizer.x = container.x + container.width - 17;
        resizer.y = container.y + container.height - 17;

        lastMouse = mouse; // Update mouse
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawRectangleLinesEx(container, 3, borderColor);    // Draw container border

            // Draw text in container (add some padding)
            DrawTextBoxed(font, text, Rectangle{ container.x + 4, container.y + 4, container.width - 4, container.height - 4 }, 20.0f, 2.0f, wordWrap, GRAY);

            DrawRectangleRec(resizer, borderColor);             // Draw the resize box

            // Draw bottom info
            DrawRectangle(0, screenHeight - 54, screenWidth, 54, GRAY);
            DrawRectangleRec(Rectangle{ 382.0f, screenHeight - 34.0f, 12.0f, 12.0f }, MAROON);

            DrawText("Word Wrap: ", 313, screenHeight-115, 20, BLACK);
            if (wordWrap) DrawText("ON", 447, screenHeight - 115, 20, RED);
            else DrawText("OFF", 447, screenHeight - 115, 20, BLACK);

            DrawText("Press [SPACE] to toggle word wrap", 218, screenHeight - 86, 20, GRAY);

            DrawText("Click hold & drag the    to resize the container", 155, screenHeight - 38, 20, RAYWHITE);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//--------------------------------------------------------------------------------------
// Module Functions Definition
//--------------------------------------------------------------------------------------

// Draw text using font inside rectangle limits
static void DrawTextBoxed(Font font, const char *text, Rectangle rec, f32 fontSize, f32 spacing, bool wordWrap, Color tint)
{
    DrawTextBoxedSelectable(font, text, rec, fontSize, spacing, wordWrap, tint, 0, 0, WHITE, WHITE);
}

// Draw text using font inside rectangle limits with support for text selection
static void DrawTextBoxedSelectable(Font font, const char *text, Rectangle rec, f32 fontSize, f32 spacing, bool wordWrap, Color tint, i32 selectStart, i32 selectLength, Color selectTint, Color selectBackTint)
{
    const i32 length = TextLength(text);  // Total length in bytes of the text, scanned by codepoints in loop

    f32 textOffsetY = 0;          // Offset between lines (on line break '\n')
    auto textOffsetX = 0.0f;       // Offset X to next character to draw

    const f32 scaleFactor = fontSize/static_cast<f32>(font.baseSize);     // Character rectangle scaling factor

    // Word/character wrapping mechanism variables
    enum { MEASURE_STATE = 0, DRAW_STATE = 1 };
    i32 state = wordWrap? MEASURE_STATE : DRAW_STATE;

    auto startLine = -1;         // Index where to begin drawing (where a line begins)
    auto endLine = -1;           // Index where to stop drawing (where a line ends)
    auto lastk = -1;             // Holds last value of the character position

    for (i32 i = 0, k = 0; i < length; i++, k++)
    {
        // Get next codepoint from byte string and glyph index in font
        auto codepointByteCount = 0;
        const auto codepoint = GetCodepoint(&text[i], &codepointByteCount);
        const auto index = GetGlyphIndex(font, codepoint);

        // NOTE: Normally we exit the decoding sequence as soon as a bad byte is found (and return 0x3f)
        // but we need to draw all of the bad bytes using the '?' symbol moving one byte
        if (codepoint == 0x3f) codepointByteCount = 1;
        i += (codepointByteCount - 1);

        f32 glyphWidth = 0;
        if (codepoint != '\n')
        {
            glyphWidth = (font.glyphs[index].advanceX == 0) ? font.recs[index].width*scaleFactor : font.glyphs[index].advanceX*scaleFactor;

            if (i + 1 < length) glyphWidth = glyphWidth + spacing;
        }

        // NOTE: When wordWrap is ON we first measure how much of the text we can draw before going outside of the rec container
        // We store this info in startLine and endLine, then we change states, draw the text between those two variables
        // and change states again and again recursively until the end of the text (or until we get outside of the container)
        // When wordWrap is OFF we don't need the measure state so we go to the drawing state immediately
        // and begin drawing on the next line before we can get outside the container
        if (state == MEASURE_STATE)
        {
            // TODO: There are multiple types of spaces in UNICODE, maybe it's a good idea to add support for more
            // Ref: http://jkorpela.fi/chars/spaces.html
            if ((codepoint == ' ') || (codepoint == '\t') || (codepoint == '\n')) endLine = i;

            if ((textOffsetX + glyphWidth) > rec.width)
            {
                endLine = (endLine < 1)? i : endLine;
                if (i == endLine) endLine -= codepointByteCount;
                if ((startLine + codepointByteCount) == endLine) endLine = (i - codepointByteCount);

                state = !state;
            }
            else if ((i + 1) == length)
            {
                endLine = i;
                state = !state;
            }
            else if (codepoint == '\n') state = !state;

            if (state == DRAW_STATE)
            {
                textOffsetX = 0;
                i = startLine;
                glyphWidth = 0;

                // Save character position when we switch states
                const i32 tmp = lastk;
                lastk = k - 1;
                k = tmp;
            }
        }
        else
        {
            if (codepoint == '\n')
            {
                if (!wordWrap)
                {
                    textOffsetY += (font.baseSize + static_cast<f32>(font.baseSize)/2)*scaleFactor;
                    textOffsetX = 0;
                }
            }
            else
            {
                if (!wordWrap && ((textOffsetX + glyphWidth) > rec.width))
                {
                    textOffsetY += (font.baseSize + static_cast<f32>(font.baseSize)/2)*scaleFactor;
                    textOffsetX = 0;
                }

                // When text overflows rectangle height limit, just stop drawing
                if ((textOffsetY + font.baseSize*scaleFactor) > rec.height) break;

                // Draw selection background
                auto isGlyphSelected = false;
                if ((selectStart >= 0) && (k >= selectStart) && (k < (selectStart + selectLength)))
                {
                    DrawRectangleRec(Rectangle{ rec.x + textOffsetX - 1, rec.y + textOffsetY, glyphWidth, static_cast<f32>(font.baseSize)*scaleFactor }, selectBackTint);
                    isGlyphSelected = true;
                }

                // Draw current character glyph
                if ((codepoint != ' ') && (codepoint != '\t'))
                {
                    DrawTextCodepoint(font, codepoint, vec2{ rec.x + textOffsetX, rec.y + textOffsetY }, fontSize, isGlyphSelected? selectTint : tint);
                }
            }

            if (wordWrap && (i == endLine))
            {
                textOffsetY += (font.baseSize + static_cast<f32>(font.baseSize)/2)*scaleFactor;
                textOffsetX = 0;
                startLine = endLine;
                endLine = -1;
                glyphWidth = 0;
                selectStart += lastk - k;
                k = lastk;

                state = !state;
            }
        }

        if ((textOffsetX != 0) || (codepoint != ' ')) textOffsetX += glyphWidth;  // avoid leading spaces
    }
}
