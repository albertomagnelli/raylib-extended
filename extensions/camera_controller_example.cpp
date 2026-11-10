/*******************************************************************************************
*
*   raylib-extended [camera_controller] - example 01: smooth Camera3D movement and rotation
*
*   Controls:
*       MOUSE           : Look around (rotation is smoothed)
*       W-A-S-D         : Move (movement is accelerated)
*       LEFT_SHIFT      : Sprint
*       LEFT_ALT        : Move slowly
*       SPACE / CTRL    : Move up / down
*       ARROW KEYS      : Look around with the keyboard
*       MOUSE WHEEL     : Zoom, distance in third-person/orbit, field-of-view in first-person/free
*       Q / E           : Roll, only in FREE mode
*       TAB             : Change camera mode
*       R               : Reset the camera to the one it was created with
*       K               : Add a velocity kick to the camera (knockback), it is damped out smoothly
*
*   This example requires the CameraController class: extensions/camera_controller.hpp
*
*   License: zlib/libpng
**********************************************************************************************/

#include "raylib.h"
#include "raymath.h"

#include "camera_controller.hpp"

static void DrawScene(const CameraController &camera);
static void DrawHud(const CameraController &camera, const char **modeNames, int modeCount);

int main(void)
{
    const int screenWidth = 1280;
    const int screenHeight = 720;

    InitWindow(screenWidth, screenHeight, "raylib-extended [camera_controller] - smooth camera3d movement and rotation");

    SetTargetFPS(60);

    // Camera the controller starts with, it is also the camera restored by the R key
    Camera3D initialCamera = { { 12.0f, 8.0f, 12.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 45.0f, CAMERA_PERSPECTIVE };

    CameraController camera(initialCamera, CAMERA_CONTROLLER_THIRD_PERSON);
    camera.SetMoveSpeed(8.0f);
    camera.SetDistance(10.0f);
    camera.SetLookSensitivity(0.08f);
    camera.SetLookResponse(18.0f);     // Lower = smoother look, 0.0f = instant look
    camera.SetMoveResponse(12.0f);     // Lower = heavier camera, higher = snappier movement
    camera.SetGamepadEnabled(true);

    const char *modeNames[] = { "FIRST_PERSON", "THIRD_PERSON", "ORBIT", "FREE" };
    const int modeCount = 4;

    Color backgroundColor = { 24, 24, 32, 255 };

    while (!WindowShouldClose())
    {
        // Cycle the camera modes
        if (IsKeyPressed(KEY_TAB))
        {
            int nextMode = ((int)camera.GetMode() + 1)%modeCount;
            camera.SetMode((CameraControllerMode)nextMode);
        }

        // Camera knockback, the controller damps it out on its own
        if (IsKeyPressed(KEY_K)) camera.AddVelocity(Vector3Scale(camera.GetForward(), -14.0f));

        // Advance the camera, it must be called once per frame, before BeginMode3D()
        camera.Update();

        BeginDrawing();
            ClearBackground(backgroundColor);

            BeginMode3D(camera.GetCamera());
                DrawScene(camera);
            EndMode3D();

            DrawHud(camera, modeNames, modeCount);
        EndDrawing();
    }

    CloseWindow();

    return 0;
}

// Draws a small scene to move around in
static void DrawScene(const CameraController &camera)
{
    DrawGrid(40, 1.0f);

    // A few boxes to judge the movement smoothness
    Vector3 centerBox = { 0.0f, 0.5f, 0.0f };
    Vector3 blueBox = { -6.0f, 2.0f, 4.0f };
    Vector3 greenBox = { 7.0f, 1.0f, -5.0f };
    Vector3 goldBox = { 4.0f, 6.0f, 8.0f };

    DrawCube(centerBox, 2.0f, 1.0f, 2.0f, MAROON);
    DrawCubeWires(centerBox, 2.0f, 1.0f, 2.0f, ORANGE);

    DrawCube(blueBox, 3.0f, 4.0f, 3.0f, SKYBLUE);
    DrawCubeWires(blueBox, 3.0f, 4.0f, 3.0f, WHITE);

    DrawCube(greenBox, 2.0f, 2.0f, 6.0f, DARKGREEN);
    DrawCubeWires(greenBox, 2.0f, 2.0f, 6.0f, LIME);

    DrawCube(goldBox, 2.0f, 2.0f, 2.0f, GOLD);
    DrawCubeWires(goldBox, 2.0f, 2.0f, 2.0f, YELLOW);

    // The pivot the controller moves: the camera position in first-person/free, the look-at point in third-person/orbit
    Vector3 pivot = camera.GetTarget();
    Vector3 pivotOnGround = { pivot.x, 0.0f, pivot.z };

    DrawSphere(pivot, 0.25f, RED);
    DrawLine3D(pivot, pivotOnGround, RED);

    // Camera basis vectors: forward (red), right (green), up (blue)
    Vector3 position = camera.GetPosition();
    DrawLine3D(position, position + camera.GetForward()*2.0f, RED);
    DrawLine3D(position, position + camera.GetRight()*2.0f, GREEN);
    DrawLine3D(position, position + camera.GetUp()*2.0f, BLUE);
}

// Draws the controller state, useful to see what the smoothing is doing
static void DrawHud(const CameraController &camera, const char **modeNames, int modeCount)
{
    const char *mode = modeNames[(int)camera.GetMode()%modeCount];

    Vector3 position = camera.GetPosition();
    Vector3 velocity = camera.GetVelocity();

    DrawText(TextFormat("mode: %s", mode), 16, 12, 20, RAYWHITE);
    DrawText("TAB change mode | R reset | K camera kick", 16, 40, 16, GRAY);
    DrawText("WASD move | SHIFT sprint | ALT slow | SPACE/CTRL up/down | MOUSE look | WHEEL zoom", 16, 62, 16, GRAY);

    DrawText(TextFormat("position: %.2f, %.2f, %.2f", position.x, position.y, position.z), 16, 96, 16, RAYWHITE);
    DrawText(TextFormat("speed: %.2f u/s", camera.GetSpeed()), 16, 118, 16, RAYWHITE);
    DrawText(TextFormat("velocity: %.2f, %.2f, %.2f", velocity.x, velocity.y, velocity.z), 16, 140, 16, LIGHTGRAY);
    DrawText(TextFormat("yaw: %.1f  pitch: %.1f  roll: %.1f", camera.GetYaw(), camera.GetPitch(), camera.GetRoll()), 16, 162, 16, RAYWHITE);
    DrawText(TextFormat("distance: %.2f  fovy: %.1f", camera.GetDistance(), camera.GetFovy()), 16, 184, 16, LIGHTGRAY);
    DrawText(TextFormat("fps: %i", GetFPS()), 16, 206, 16, LIGHTGRAY);
}
