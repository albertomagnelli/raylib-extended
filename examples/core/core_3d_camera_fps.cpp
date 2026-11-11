/*******************************************************************************************
*
*   raylib [core] example - 3d camera fps
*
*   Example complexity rating: [★★★☆] 3/4
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

#include "raymath.h"

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
// Movement constants
#define GRAVITY         32.0f
#define MAX_SPEED       20.0f
#define CROUCH_SPEED     5.0f
#define JUMP_FORCE      12.0f
#define MAX_ACCEL      150.0f
// Grounded drag
#define FRICTION         0.86f
// Increasing air drag, increases strafing speed
#define AIR_DRAG         0.98f
// Responsiveness for turning movement direction to looked direction
#define CONTROL         15.0f
#define CROUCH_HEIGHT    0.0f
#define STAND_HEIGHT     1.0f
#define BOTTOM_HEIGHT    0.5f

#define NORMALIZE_INPUT  0

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// Body structure
typedef struct {
    vec3 position;
    vec3 velocity;
    vec3 dir;
    bool isGrounded;
} Body;

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
static auto sensitivity = vec2{ 0.001f, 0.001f };

static auto player = Body{};
static auto lookRotation = vec2{};
static auto headTimer = 0.0f;
static auto walkLerp = 0.0f;
static f32 headLerp = STAND_HEIGHT;
static auto lean = vec2{};

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
static void DrawLevel(void);
static void UpdateCameraFPS(Camera *camera);
static void UpdateBody(Body *body, f32 rot, char side, char forward, bool jumpPressed, bool crouchHold);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr auto screenWidth = 800;
    constexpr auto screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [core] example - 3d camera fps");

    // Initialize camera variables
    // NOTE: UpdateCameraFPS() takes care of the rest
    auto camera = Camera{};
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    camera.position = vec3{
        player.position.x,
        player.position.y + (BOTTOM_HEIGHT + headLerp),
        player.position.z,
    };

    UpdateCameraFPS(&camera); // Update camera parameters

    DisableCursor();        // Limit cursor to relative movement inside the window

    SetTargetFPS(60);       // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        const auto mouseDelta = GetMouseDelta();
        lookRotation.x -= mouseDelta.x*sensitivity.x;
        lookRotation.y += mouseDelta.y*sensitivity.y;

        const char sideway = (IsKeyDown(KEY_D) - IsKeyDown(KEY_A));
        const char forward = (IsKeyDown(KEY_W) - IsKeyDown(KEY_S));
        const auto crouching = IsKeyDown(KEY_LEFT_CONTROL);
        UpdateBody(&player, lookRotation.x, sideway, forward, IsKeyPressed(KEY_SPACE), crouching);

        const auto delta = GetFrameTime();
        headLerp = Lerp(headLerp, (crouching ? CROUCH_HEIGHT : STAND_HEIGHT), 20.0f*delta);
        camera.position = vec3{
            player.position.x,
            player.position.y + (BOTTOM_HEIGHT + headLerp),
            player.position.z,
        };

        if (player.isGrounded && ((forward != 0) || (sideway != 0)))
        {
            headTimer += delta*3.0f;
            walkLerp = Lerp(walkLerp, 1.0f, 10.0f*delta);
            camera.fovy = Lerp(camera.fovy, 55.0f, 5.0f*delta);
        }
        else
        {
            walkLerp = Lerp(walkLerp, 0.0f, 10.0f*delta);
            camera.fovy = Lerp(camera.fovy, 60.0f, 5.0f*delta);
        }

        lean.x = Lerp(lean.x, sideway*0.02f, 10.0f*delta);
        lean.y = Lerp(lean.y, forward*0.015f, 10.0f*delta);

        UpdateCameraFPS(&camera);
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            BeginMode3D(camera);
                DrawLevel();
            EndMode3D();

            // Draw info box
            DrawRectangle(5, 5, 330, 75, Fade(SKYBLUE, 0.5f));
            DrawRectangleLines(5, 5, 330, 75, BLUE);

            DrawText("Camera controls:", 15, 15, 10, BLACK);
            DrawText("- Move keys: W, A, S, D, Space, Left-Ctrl", 15, 30, 10, BLACK);
            DrawText("- Look around: arrow keys or mouse", 15, 45, 10, BLACK);
            DrawText(TextFormat("- Velocity Len: (%06.3f)", glm::length(vec2{ player.velocity.x, player.velocity.z })), 15, 60, 10, BLACK);

        EndDrawing();
        //----------------------------------------------------------------------------------
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
// Update body considering current world state
void UpdateBody(Body *body, f32 rot, char side, char forward, bool jumpPressed, bool crouchHold)
{
    auto input = vec2{ static_cast<f32>(side), static_cast<f32>(-forward) };

#if defined(NORMALIZE_INPUT)
    // Slow down diagonal movement
    if ((side != 0) && (forward != 0)) input = glm::normalize(input);
#endif

    const auto delta = GetFrameTime();

    if (!body->isGrounded) body->velocity.y -= GRAVITY*delta;

    if (body->isGrounded && jumpPressed)
    {
        body->velocity.y = JUMP_FORCE;
        body->isGrounded = false;

        // Sound can be played at this moment
        //SetSoundPitch(fxJump, 1.0f + (GetRandomValue(-100, 100)*0.001));
        //PlaySound(fxJump);
    }

    const auto front = vec3{ sinf(rot), 0.f, cosf(rot) };
    const auto right = vec3{ cosf(-rot), 0.f, sinf(-rot) };

    const auto desiredDir = vec3{ input.x*right.x + input.y*front.x, 0.0f, input.x*right.z + input.y*front.z, };
    body->dir = glm::mix(body->dir, desiredDir, CONTROL*delta);

    const f32 decel = (body->isGrounded ? FRICTION : AIR_DRAG);
    auto hvel = vec3{ body->velocity.x*decel, 0.0f, body->velocity.z*decel };

    const f32 hvelLength = glm::length(hvel); // Magnitude
    if (hvelLength < (MAX_SPEED*0.01f)) hvel = vec3{};

    // This is what creates strafing
    const f32 speed = glm::dot(hvel, body->dir);

    // Whenever the amount of acceleration to add is clamped by the maximum acceleration constant,
    // a Player can make the speed faster by bringing the direction closer to horizontal velocity angle
    // More info here: https://youtu.be/v3zT3Z5apaM?t=165
    const f32 maxSpeed = (crouchHold? CROUCH_SPEED : MAX_SPEED);
    const auto accel = Clamp(maxSpeed - speed, 0.f, MAX_ACCEL*delta);
    hvel.x += body->dir.x*accel;
    hvel.z += body->dir.z*accel;

    body->velocity.x = hvel.x;
    body->velocity.z = hvel.z;

    body->position.x += body->velocity.x*delta;
    body->position.y += body->velocity.y*delta;
    body->position.z += body->velocity.z*delta;

    // Fancy collision system against the floor
    if (body->position.y <= 0.0f)
    {
        body->position.y = 0.0f;
        body->velocity.y = 0.0f;
        body->isGrounded = true; // Enable jumping
    }
}

// Update camera for FPS behaviour
static void UpdateCameraFPS(Camera *camera)
{
    const auto up = vec3{ 0.0f, 1.0f, 0.0f };
    const auto targetOffset = vec3{ 0.0f, 0.0f, -1.0f };

    // Left and right
    const auto yaw = Vector3RotateByAxisAngle(targetOffset, up, lookRotation.x);

    // Clamp view up
    auto maxAngleUp = Vector3Angle(up, yaw);
    maxAngleUp -= 0.001f; // Avoid numerical errors
    if ( -(lookRotation.y) > maxAngleUp) { lookRotation.y = -maxAngleUp; }

    // Clamp view down
    auto maxAngleDown = Vector3Angle((-(up)), yaw);
    maxAngleDown *= -1.0f;  // Downwards angle is negative
    maxAngleDown += 0.001f; // Avoid numerical errors
    if ( -(lookRotation.y) < maxAngleDown) { lookRotation.y = -maxAngleDown; }

    // Up and down
    const vec3 right = glm::normalize(glm::cross(yaw, up));

    // Rotate view vector around right axis
    f32 pitchAngle = -lookRotation.y - lean.y;
    pitchAngle = Clamp(pitchAngle, -PI/2 + 0.0001f, PI/2 - 0.0001f); // Clamp angle so it doesn't go past straight up or straight down
    const auto pitch = Vector3RotateByAxisAngle(yaw, right, pitchAngle);

    // Head animation
    // Rotate up direction around forward axis
    const f32 headSin = sinf(headTimer*PI);
    const f32 headCos = cosf(headTimer*PI);
    constexpr auto stepRotation = 0.01f;
    camera->up = Vector3RotateByAxisAngle(up, pitch, headSin*stepRotation + lean.x);

    // Camera BOB
    constexpr auto bobSide = 0.1f;
    constexpr auto bobUp = 0.15f;
    vec3 bobbing = ((right) * (headSin*bobSide));
    bobbing.y = fabsf(headCos*bobUp);

    camera->position = ((camera->position) + (((bobbing) * (walkLerp))));
    camera->target = ((camera->position) + (pitch));
}

// Draw game level
static void DrawLevel(void)
{
    constexpr auto floorExtent = 25;
    constexpr auto tileSize = 5.0f;
    const auto tileColor1 = Color{ 150, 200, 200, 255 };

    // Floor tiles
    for (i32 y = -floorExtent; y < floorExtent; y++)
    {
        for (i32 x = -floorExtent; x < floorExtent; x++)
        {
            if ((y & 1) && (x & 1))
            {
                DrawPlane(vec3{ x*tileSize, 0.0f, y*tileSize}, vec2{ tileSize, tileSize }, tileColor1);
            }
            else if (!(y & 1) && !(x & 1))
            {
                DrawPlane(vec3{ x*tileSize, 0.0f, y*tileSize}, vec2{ tileSize, tileSize }, LIGHTGRAY);
            }
        }
    }

    const auto towerSize = vec3{ 16.0f, 32.0f, 16.0f };
    const auto towerColor = Color{ 150, 200, 200, 255 };

    auto towerPos = vec3{ 16.0f, 16.0f, 16.0f };
    DrawCubeV(towerPos, towerSize, towerColor);
    DrawCubeWiresV(towerPos, towerSize, DARKBLUE);

    towerPos.x *= -1;
    DrawCubeV(towerPos, towerSize, towerColor);
    DrawCubeWiresV(towerPos, towerSize, DARKBLUE);

    towerPos.z *= -1;
    DrawCubeV(towerPos, towerSize, towerColor);
    DrawCubeWiresV(towerPos, towerSize, DARKBLUE);

    towerPos.x *= -1;
    DrawCubeV(towerPos, towerSize, towerColor);
    DrawCubeWiresV(towerPos, towerSize, DARKBLUE);

    // Red sun
    DrawSphere(vec3{ 300.0f, 300.0f, 0.0f }, 100.0f, Color{ 255, 0, 0, 255 });
}
