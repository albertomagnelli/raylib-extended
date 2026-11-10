/*******************************************************************************************
*
*   raylib [core] example - input gestures testbed
*
*   Example complexity rating: [★★★☆] 3/4
*
*   Example originally created with raylib 5.0, last time updated with raylib 6.0
*
*   Example contributed by ubkp (@ubkp) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2023-2025 ubkp (@ubkp)
*
********************************************************************************************/

#include "raylib.h"

#include <math.h>       // Required for the protractor angle graphic drawing

#define GESTURE_LOG_SIZE    20
#define MAX_TOUCH_COUNT     32

//------------------------------------------------------------------------------------
// Module Functions Declaration
//------------------------------------------------------------------------------------
static const char *GetGestureName(int gesture); // Get text string for gesture value
static Color GetGestureColor(int gesture); // Get color for gesture value

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr int screenWidth = 800;
    constexpr int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [core] example - input gestures testbed");

    const Vector2 messagePosition = { 160, 7 };

    // Last gesture variables definitions
    int lastGesture = 0;
    const Vector2 lastGesturePosition = { 165, 130 };

    // Gesture log variables definitions
    // NOTE: The gesture log uses an array (as an inverted circular queue) to store the performed gestures
    std::string gestureLog[GESTURE_LOG_SIZE];
    // NOTE: The index for the inverted circular queue (moving from last to first direction, then looping around)
    int gestureLogIndex = 0;
    int previousGesture = 0;

    // Log mode values:
    // - 0 shows repeated events
    // - 1 hides repeated events
    // - 2 shows repeated events but hide hold events
    // - 3 hides repeated events and hide hold events
    int logMode = 1;

    Color gestureColor = { 0, 0, 0, 255 };
    const Rectangle logButton1 = { 53, 7, 48, 26 };
    const Rectangle logButton2 = { 108, 7, 36, 26 };
    const Vector2 gestureLogPosition = { 10, 10 };

    // Protractor variables definitions
    constexpr float angleLength = 90.0f;
    float currentAngleDegrees = 0.0f;
    Vector2 finalVector = { 0.0f, 0.0f };
    const Vector2 protractorPosition = { 266.0f, 315.0f };

    SetTargetFPS(60);   // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //--------------------------------------------------------------------------------------
        // Handle common gestures data
        int i, ii; // Iterators that will be reused by all for loops
        const int currentGesture = GetGestureDetected();
        const float currentDragDegrees = GetGestureDragAngle();
        const float currentPitchDegrees = GetGesturePinchAngle();
        const int touchCount = GetTouchPointCount();

        // Handle last gesture
        if ((currentGesture != 0) && (currentGesture != 4) && (currentGesture != previousGesture))
            lastGesture = currentGesture; // Filter the meaningful gestures (1, 2, 8 to 512) for the display

        // Handle gesture log
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        {
            if (CheckCollisionPointRec(GetMousePosition(), logButton1))
            {
                switch (logMode)
                {
                    case 3: logMode = 2; break;
                    case 2: logMode = 3; break;
                    case 1: logMode = 0; break;
                    default: logMode = 1; break;
                }
            }
            else if (CheckCollisionPointRec(GetMousePosition(), logButton2))
            {
                switch (logMode)
                {
                    case 3: logMode = 1; break;
                    case 2: logMode = 0; break;
                    case 1: logMode = 3; break;
                    default: logMode = 2; break;
                }
            }
        }

        int fillLog = 0; // Gate variable to be used to allow or not the gesture log to be filled
        if (currentGesture !=0)
        {
            if (logMode == 3) // 3 hides repeated events and hide hold events
            {
                if (((currentGesture != 4) && (currentGesture != previousGesture)) || (currentGesture < 3)) fillLog = 1;
            }
            else if (logMode == 2) // 2 shows repeated events but hide hold events
            {
                if (currentGesture != 4) fillLog = 1;
            }
            else if (logMode == 1) // 1 hides repeated events
            {
                if (currentGesture != previousGesture) fillLog = 1;
            }
            else  // 0 shows repeated events
            {
                fillLog = 1;
            }
        }

        if (fillLog) // If one of the conditions from logMode was met, fill the gesture log
        {
            previousGesture = currentGesture;
            gestureColor = GetGestureColor(currentGesture);
            if (gestureLogIndex <= 0) gestureLogIndex = GESTURE_LOG_SIZE;
            gestureLogIndex--;

            // Copy the gesture respective name to the gesture log array
            gestureLog[gestureLogIndex] = GetGestureName(currentGesture);
        }

        // Handle protractor
        if (currentGesture > 255) currentAngleDegrees = currentPitchDegrees; // Pinch In and Pinch Out
        else if (currentGesture > 15) currentAngleDegrees = currentDragDegrees; // Swipe Right, Swipe Left, Swipe Up and Swipe Down
        else if (currentGesture > 0) currentAngleDegrees = 0.0f; // Tap, Doubletap, Hold and Grab

        const float currentAngleRadians = ((currentAngleDegrees + 90.0f)*PI/180); // Convert the current angle to Radians
        // Calculate the final vector for display
        finalVector = Vector2{ (angleLength*sinf(currentAngleRadians)) + protractorPosition.x,
            (angleLength*cosf(currentAngleRadians)) + protractorPosition.y };

        // Handle touch and mouse pointer points
        Vector2 touchPosition[MAX_TOUCH_COUNT] = {};
        Vector2 mousePosition = {};
        if (currentGesture != GESTURE_NONE)
        {
            if (touchCount != 0)
            {
                for (i = 0; i < touchCount; i++) touchPosition[i] = GetTouchPosition(i); // Fill the touch positions
            }
            else mousePosition = GetMousePosition();
        }
        //--------------------------------------------------------------------------------------

        // Draw
        //--------------------------------------------------------------------------------------
        BeginDrawing();
            ClearBackground(RAYWHITE);

            // Draw common elements
            DrawText("*", static_cast<int>(messagePosition.x) + 5, static_cast<int>(messagePosition.y) + 5, 10, BLACK);
            DrawText("Example optimized for Web/HTML5\non Smartphones with Touch Screen.", static_cast<int>(messagePosition.x) + 15, static_cast<int>(messagePosition.y) + 5, 10, BLACK);
            DrawText("*", static_cast<int>(messagePosition.x) + 5, static_cast<int>(messagePosition.y) + 35, 10, BLACK);
            DrawText("While running on Desktop Web Browsers,\ninspect and turn on Touch Emulation.", static_cast<int>(messagePosition.x) + 15, static_cast<int>(messagePosition.y) + 35, 10, BLACK);

            // Draw last gesture
            DrawText("Last gesture", static_cast<int>(lastGesturePosition.x) + 33, static_cast<int>(lastGesturePosition.y) - 47, 20, BLACK);
            DrawText("Swipe         Tap       Pinch  Touch", static_cast<int>(lastGesturePosition.x) + 17, static_cast<int>(lastGesturePosition.y) - 18, 10, BLACK);
            DrawRectangle(static_cast<int>(lastGesturePosition.x) + 20, static_cast<int>(lastGesturePosition.y), 20, 20, lastGesture == GESTURE_SWIPE_UP ? RED : LIGHTGRAY);
            DrawRectangle(static_cast<int>(lastGesturePosition.x), static_cast<int>(lastGesturePosition.y) + 20, 20, 20, lastGesture == GESTURE_SWIPE_LEFT ? RED : LIGHTGRAY);
            DrawRectangle(static_cast<int>(lastGesturePosition.x) + 40, static_cast<int>(lastGesturePosition.y) + 20, 20, 20, lastGesture == GESTURE_SWIPE_RIGHT ? RED : LIGHTGRAY);
            DrawRectangle(static_cast<int>(lastGesturePosition.x) + 20, static_cast<int>(lastGesturePosition.y) + 40, 20, 20, lastGesture == GESTURE_SWIPE_DOWN ? RED : LIGHTGRAY);
            DrawCircle(static_cast<int>(lastGesturePosition.x) + 80, static_cast<int>(lastGesturePosition.y) + 16, 10, lastGesture == GESTURE_TAP ? BLUE : LIGHTGRAY);
            DrawRing( Vector2{lastGesturePosition.x + 103, lastGesturePosition.y + 16}, 6.0f, 11.0f, 0.0f, 360.0f, 0, lastGesture == GESTURE_DRAG ? LIME : LIGHTGRAY);
            DrawCircle(static_cast<int>(lastGesturePosition.x) + 80, static_cast<int>(lastGesturePosition.y) + 43, 10, lastGesture == GESTURE_DOUBLETAP ? SKYBLUE : LIGHTGRAY);
            DrawCircle(static_cast<int>(lastGesturePosition.x) + 103, static_cast<int>(lastGesturePosition.y) + 43, 10, lastGesture == GESTURE_DOUBLETAP ? SKYBLUE : LIGHTGRAY);
            DrawTriangle(Vector2{ lastGesturePosition.x + 122, lastGesturePosition.y + 16 }, Vector2{ lastGesturePosition.x + 137, lastGesturePosition.y + 26 }, Vector2{lastGesturePosition.x + 137, lastGesturePosition.y + 6 }, lastGesture == GESTURE_PINCH_OUT? ORANGE : LIGHTGRAY);
            DrawTriangle(Vector2{ lastGesturePosition.x + 147, lastGesturePosition.y + 6 }, Vector2{ lastGesturePosition.x + 147, lastGesturePosition.y + 26 }, Vector2{ lastGesturePosition.x + 162, lastGesturePosition.y + 16 }, lastGesture == GESTURE_PINCH_OUT? ORANGE : LIGHTGRAY);
            DrawTriangle(Vector2{ lastGesturePosition.x + 125, lastGesturePosition.y + 33 }, Vector2{ lastGesturePosition.x + 125, lastGesturePosition.y + 53 }, Vector2{ lastGesturePosition.x + 140, lastGesturePosition.y + 43 }, lastGesture == GESTURE_PINCH_IN? VIOLET : LIGHTGRAY);
            DrawTriangle(Vector2{ lastGesturePosition.x + 144, lastGesturePosition.y + 43 }, Vector2{ lastGesturePosition.x + 159, lastGesturePosition.y + 53 }, Vector2{ lastGesturePosition.x + 159, lastGesturePosition.y + 33 }, lastGesture == GESTURE_PINCH_IN? VIOLET : LIGHTGRAY);
            for (i = 0; i < 4; i++) DrawCircle(static_cast<int>(lastGesturePosition.x) + 180, static_cast<int>(lastGesturePosition.y) + 7 + i*15, 5, touchCount <= i? LIGHTGRAY : gestureColor);

            // Draw gesture log
            DrawText("Log", static_cast<int>(gestureLogPosition.x), static_cast<int>(gestureLogPosition.y), 20, BLACK);

            // Loop in both directions to print the gesture log array in the inverted order (and looping around if the index started somewhere in the middle)
            for (i = 0, ii = gestureLogIndex; i < GESTURE_LOG_SIZE; i++, ii = (ii + 1)%GESTURE_LOG_SIZE) DrawText(gestureLog[ii], static_cast<int>(gestureLogPosition.x), static_cast<int>(gestureLogPosition.y) + 410 - i*20, 20, (i == 0 ? gestureColor : LIGHTGRAY));
            Color logButton1Color, logButton2Color;
            switch (logMode)
            {
                case 3:  logButton1Color=MAROON; logButton2Color=MAROON; break;
                case 2:  logButton1Color=GRAY;   logButton2Color=MAROON; break;
                case 1:  logButton1Color=MAROON; logButton2Color=GRAY;   break;
                default: logButton1Color=GRAY;   logButton2Color=GRAY;   break;
            }
            DrawRectangleRec(logButton1, logButton1Color);
            DrawText("Hide", static_cast<int>(logButton1.x) + 7, static_cast<int>(logButton1.y) + 3, 10, WHITE);
            DrawText("Repeat", static_cast<int>(logButton1.x) + 7, static_cast<int>(logButton1.y) + 13, 10, WHITE);
            DrawRectangleRec(logButton2, logButton2Color);
            DrawText("Hide", static_cast<int>(logButton1.x) + 62, static_cast<int>(logButton1.y) + 3, 10, WHITE);
            DrawText("Hold", static_cast<int>(logButton1.x) + 62, static_cast<int>(logButton1.y) + 13, 10, WHITE);

            // Draw protractor
            DrawText("Angle", static_cast<int>(protractorPosition.x) + 55, static_cast<int>(protractorPosition.y) + 76, 10, BLACK);
            const std::string angleString = TextFormat("%f", currentAngleDegrees);
            const int angleStringDot = TextFindIndex(angleString, ".");
            const std::string angleStringTrim = TextSubtext(angleString, 0, angleStringDot + 3);
            DrawText( angleStringTrim, static_cast<int>(protractorPosition.x) + 55, static_cast<int>(protractorPosition.y) + 92, 20, gestureColor);
            DrawCircleV(protractorPosition, 80.0f, WHITE);
            DrawLineEx(Vector2{ protractorPosition.x - 90, protractorPosition.y }, Vector2{ protractorPosition.x + 90, protractorPosition.y }, 3.0f, LIGHTGRAY);
            DrawLineEx(Vector2{ protractorPosition.x, protractorPosition.y - 90 }, Vector2{ protractorPosition.x, protractorPosition.y + 90 }, 3.0f, LIGHTGRAY);
            DrawLineEx(Vector2{ protractorPosition.x - 80, protractorPosition.y - 45 }, Vector2{ protractorPosition.x + 80, protractorPosition.y + 45 }, 3.0f, GREEN);
            DrawLineEx(Vector2{ protractorPosition.x - 80, protractorPosition.y + 45 }, Vector2{ protractorPosition.x + 80, protractorPosition.y - 45 }, 3.0f, GREEN);
            DrawText("0", static_cast<int>(protractorPosition.x) + 96, static_cast<int>(protractorPosition.y) - 9, 20, BLACK);
            DrawText("30", static_cast<int>(protractorPosition.x) + 74, static_cast<int>(protractorPosition.y) - 68, 20, BLACK);
            DrawText("90", static_cast<int>(protractorPosition.x) - 11, static_cast<int>(protractorPosition.y) - 110, 20, BLACK);
            DrawText("150", static_cast<int>(protractorPosition.x) - 100, static_cast<int>(protractorPosition.y) - 68, 20, BLACK);
            DrawText("180", static_cast<int>(protractorPosition.x) - 124, static_cast<int>(protractorPosition.y) - 9, 20, BLACK);
            DrawText("210", static_cast<int>(protractorPosition.x) - 100, static_cast<int>(protractorPosition.y) + 50, 20, BLACK);
            DrawText("270", static_cast<int>(protractorPosition.x) - 18, static_cast<int>(protractorPosition.y) + 92, 20, BLACK);
            DrawText("330", static_cast<int>(protractorPosition.x) + 72, static_cast<int>(protractorPosition.y) + 50, 20, BLACK);
            if (currentAngleDegrees != 0.0f) DrawLineEx(protractorPosition, finalVector, 3.0f, gestureColor);

            // Draw touch and mouse pointer points
            if (currentGesture != GESTURE_NONE)
            {
                if ( touchCount != 0 )
                {
                    for (i = 0; i < touchCount; i++)
                    {
                        DrawCircleV(touchPosition[i], 50.0f, Fade(gestureColor, 0.5f));
                        DrawCircleV(touchPosition[i], 5.0f, gestureColor);
                    }

                    if (touchCount == 2) DrawLineEx(touchPosition[0], touchPosition[1], ((currentGesture == 512)? 8.0f : 12.0f), gestureColor);
                }
                else
                {
                    DrawCircleV(mousePosition, 35.0f, Fade(gestureColor, 0.5f));
                    DrawCircleV(mousePosition, 5.0f, gestureColor);
                }
            }

        EndDrawing();
        //--------------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------
// Get text string for gesture value
static const char *GetGestureName(int gesture)
{
    switch (gesture)
    {
        case 0: return "None"; break;
        case 1: return "Tap"; break;
        case 2: return "Double Tap"; break;
        case 4: return "Hold"; break;
        case 8: return "Drag"; break;
        case 16: return "Swipe Right"; break;
        case 32: return "Swipe Left"; break;
        case 64: return "Swipe Up"; break;
        case 128: return "Swipe Down"; break;
        case 256: return "Pinch In"; break;
        case 512: return "Pinch Out"; break;
        default:  return "Unknown"; break;
    }
}

// Get color for gesture value
static Color GetGestureColor(int gesture)
{
    switch (gesture)
    {
        case 0: return BLACK; break;
        case 1: return BLUE; break;
        case 2: return SKYBLUE; break;
        case 4: return BLACK; break;
        case 8: return LIME; break;
        case 16: return RED; break;
        case 32: return RED; break;
        case 64: return RED; break;
        case 128: return RED; break;
        case 256: return VIOLET; break;
        case 512: return ORANGE; break;
        default: return BLACK; break;
    }
}
