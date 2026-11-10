/*******************************************************************************************
*
*   raylib [shapes] example - pie chart
*
*   Example complexity rating: [★★★☆] 3/4
*
*   Example originally created with raylib 5.5, last time updated with raylib 5.6
*
*   Example contributed by Gideon Serfontein (@GideonSerf) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2025 Gideon Serfontein (@GideonSerf)
*
********************************************************************************************/

#include "raylib.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#include <math.h>
#include <stdio.h>

#define MAX_PIE_SLICES  10       // Max pie slices

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr int screenWidth = 800;
    constexpr int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [shapes] example - pie chart");

    int sliceCount = 7;
    float donutInnerRadius = 25.0f;
    float values[MAX_PIE_SLICES] = { 300.0f, 100.0f, 450.0f, 350.0f, 600.0f, 380.0f, 750.0f }; // Initial slice values
    bool editingLabel[MAX_PIE_SLICES] = {};
    char labels[MAX_PIE_SLICES][32] = {}; // Slice label texts, editable

    bool showValues = true;
    bool showPercentages = false;
    bool showDonut = false;
    int hoveredSlice = -1;
    Rectangle scrollPanelBounds = {};
    Vector2 scrollContentOffset = {};
    Rectangle view = {};

    // UI layout parameters
    constexpr int panelWidth = 270;
    constexpr int panelMargin = 5;

    // UI Panel top-left anchor
    const Vector2 panelPos = {
        static_cast<float>(screenWidth) - panelMargin - panelWidth,
        static_cast<float>(panelMargin)
    };

    // UI Panel rectangle
    const Rectangle panelRect = {
        panelPos.x, panelPos.y,
        static_cast<float>(panelWidth),
        static_cast<float>(screenHeight) - 2.0f*panelMargin
    };

    // Pie chart geometry
    const Rectangle canvas = { 0, 0, panelPos.x, static_cast<float>(screenHeight) };
    const Vector2 center = { canvas.width/2.0f, canvas.height/2.0f };
    constexpr float radius = 205.0f;

    // Total value for percentage calculations
    float totalValue = 0.0f;

    // Initialize slice labels
    for (int i = 0; i < MAX_PIE_SLICES; i++) snprintf(labels[i], 32, "Slice %02i", i + 1);

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())
    {
        // Update
        //----------------------------------------------------------------------------------
        // Calculate total value for percentage calculations
        totalValue = 0.0f;
        for (int i = 0; i < sliceCount; i++) totalValue += values[i];

        // Check for mouse hover over slices
        hoveredSlice = -1; // Reset hovered slice
        const Vector2 mousePos = GetMousePosition();

        if (CheckCollisionPointRec(mousePos, canvas)) // Only check if mouse is inside the canvas
        {
            const float dx = mousePos.x - center.x;
            const float dy = mousePos.y - center.y;
            const float distance = sqrtf(dx*dx + dy*dy);

            if ((distance <= radius) && (!showDonut || distance >= donutInnerRadius)) // Inside the pie, outside the donut hole
            {
                float angle = atan2f(dy, dx)*RAD2DEG;
                if (angle < 0) angle += 360;

                float currentAngle = 0.0f;
                for (int i = 0; i < sliceCount; i++)
                {
                    const float sweep = (totalValue > 0)? (values[i]/totalValue)*360.0f : 0.0f;

                    if ((angle >= currentAngle) && (angle < (currentAngle + sweep)))
                    {
                        hoveredSlice = i;
                        break;
                    }

                    currentAngle += sweep;
                }
            }
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();
            ClearBackground(RAYWHITE);

            // Draw the pie chart on the canvas
            float startAngle = 0.0f;
            for (int i = 0; i < sliceCount; i++)
            {
                const float sweepAngle = (totalValue > 0)? (values[i]/totalValue)*360.0f : 0.0f;
                const float midAngle = startAngle + sweepAngle/2.0f; // Middle angle for label positioning

                const Color color = ColorFromHSV(static_cast<float>(i)/sliceCount*360.0f, 0.75f, 0.9f);
                float currentRadius = radius;

                // Make the hovered slice pop out by adding 20 pixels to its radius
                if (i == hoveredSlice) currentRadius += 20.0f;

                // Draw the pie slice (DrawRing() for donut mode)
                if (showDonut) DrawRing(center, donutInnerRadius, currentRadius, startAngle, startAngle + sweepAngle, 120, color);
                else DrawCircleSector(center, currentRadius, startAngle, startAngle + sweepAngle, 120, color);

                // Draw the label for the current slice
                if (values[i] > 0)
                {
                    char labelText[64] = {};
                    if (showValues && showPercentages) snprintf(labelText, 64, "%.1f (%.0f%%)", values[i], (values[i]/totalValue)*100.0f);
                    else if (showValues) snprintf(labelText, 64, "%.1f", values[i]);
                    else if (showPercentages) snprintf(labelText, 64, "%.0f%%", (values[i]/totalValue)*100.0f);
                    else labelText[0] = '\0';

                    const Vector2 textSize = MeasureTextEx(GetFontDefault(), labelText, 20, 1);
                    const float labelRadius = showDonut? (donutInnerRadius + radius)/2.0f : radius*0.7f;
                    const Vector2 labelPos = { center.x + cosf(midAngle*DEG2RAD)*labelRadius - textSize.x/2.0f,
                        center.y + sinf(midAngle*DEG2RAD)*labelRadius - textSize.y/2.0f };
                    DrawText(labelText, static_cast<int>(labelPos.x), static_cast<int>(labelPos.y), 20, WHITE);
                }

                startAngle += sweepAngle;
            }

            // UI control panel
            DrawRectangleRec(panelRect, Fade(LIGHTGRAY, 0.5f));
            DrawRectangleLinesEx(panelRect, 1.0f, GRAY);

            GuiSpinner(Rectangle{ panelPos.x + 95, static_cast<float>(panelPos.y) + 12, 125, 25 }, "Slices ", &sliceCount, 1, MAX_PIE_SLICES, false);
            GuiCheckBox(Rectangle{ panelPos.x + 20, static_cast<float>(panelPos.y) + 12 + 40, 20, 20 }, "Show Values", &showValues);
            GuiCheckBox(Rectangle{ panelPos.x + 20, static_cast<float>(panelPos.y) + 12 + 70, 20, 20 }, "Show Percentages", &showPercentages);
            GuiCheckBox(Rectangle{ panelPos.x + 20, static_cast<float>(panelPos.y) + 12 + 100, 20, 20 }, "Make Donut", &showDonut);

            if (!showDonut) GuiDisable();
            GuiSliderBar(Rectangle{ panelPos.x + 80, static_cast<float>(panelPos.y) + 12 + 130, panelRect.width - 100, 30 },
                "Inner Radius", nullptr, & donutInnerRadius, 5.0f, radius - 10.0f);
            GuiEnable();

            GuiLine(Rectangle{ panelPos.x + 10, static_cast<float>(panelPos.y) + 12 + 170, panelRect.width - 20, 1 }, nullptr);

            // Scrollable area for slice editors
            scrollPanelBounds = Rectangle{
                panelPos.x + panelMargin,
                static_cast<float>(panelPos.y) + 12 + 190,
                panelRect.width - panelMargin*2,
                panelRect.y + panelRect.height - (panelPos.y + 12 + 190) - panelMargin
            };
            const int contentHeight = sliceCount*35;

            GuiScrollPanel(scrollPanelBounds, nullptr, Rectangle{ 0, 0, panelRect.width - 25, static_cast<float>(contentHeight) },
                &scrollContentOffset, &view);

            const float contentX = view.x + scrollContentOffset.x; // Left of content
            const float contentY = view.y + scrollContentOffset.y; // Top of content

            BeginScissorMode(static_cast<int>(view.x), static_cast<int>(view.y), static_cast<int>(view.width), static_cast<int>(view.height));

                for (int i = 0; i < sliceCount; i++)
                {
                    const int rowY = static_cast<int>(contentY + 5 + i*35);

                    // Color indicator
                    const Color color = ColorFromHSV(static_cast<float>(i)/sliceCount*360.0f, 0.75f, 0.9f);
                    DrawRectangle(static_cast<int>(contentX + 15), rowY + 5, 20, 20, color);

                    // Label textbox
                    if (GuiTextBox(Rectangle{ contentX + 45, static_cast<float>(rowY), 75, 30 }, labels[i], 32, editingLabel[i]))
                        editingLabel[i] = !editingLabel[i];

                    GuiSliderBar(Rectangle{ contentX + 130, static_cast<float>(rowY), 110, 30 }, nullptr, nullptr, &values[i], 0.0f, 1000.0f);
                }

            EndScissorMode();

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow(); // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}