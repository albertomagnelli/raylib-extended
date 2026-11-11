/*******************************************************************************************
*
*   raylib [core] example - 2d camera platformer
*
*   Example complexity rating: [★★★☆] 3/4
*
*   Example originally created with raylib 2.5, last time updated with raylib 3.0
*
*   Example contributed by arvyy (@arvyy) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2019-2025 arvyy (@arvyy)
*
********************************************************************************************/

#include "raylib.h"
#include "raymath.h"

#define G 400
#define PLAYER_JUMP_SPD 350.0f
#define PLAYER_HOR_SPD 200.0f

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct Player {
    vec2 position;
    f32 speed;
    bool canJump;
} Player;

typedef struct EnvItem {
    Rectangle rect;
    i32 blocking;
    Color color;
} EnvItem;

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
void UpdatePlayer(Player *player, EnvItem *envItems, i32 envItemsLength, f32 delta);
void UpdateCameraCenter(Camera2D *camera, Player *player, EnvItem *envItems, i32 envItemsLength, f32 delta, i32 width, i32 height);
void UpdateCameraCenterInsideMap(Camera2D *camera, Player *player, EnvItem *envItems, i32 envItemsLength, f32 delta, i32 width, i32 height);
void UpdateCameraCenterSmoothFollow(Camera2D *camera, Player *player, EnvItem *envItems, i32 envItemsLength, f32 delta, i32 width, i32 height);
void UpdateCameraEvenOutOnLanding(Camera2D *camera, Player *player, EnvItem *envItems, i32 envItemsLength, f32 delta, i32 width, i32 height);
void UpdateCameraPlayerBoundsPush(Camera2D *camera, Player *player, EnvItem *envItems, i32 envItemsLength, f32 delta, i32 width, i32 height);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr auto screenWidth = 800;
    constexpr auto screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [core] example - 2d camera platformer");

    auto player = Player{};
    player.position = vec2{ 400, 280 };
    player.speed = 0;
    player.canJump = false;
    EnvItem envItems[] = {
        {{ 0, 0, 1000, 400 }, 0, LIGHTGRAY },
        {{ 0, 400, 1000, 200 }, 1, GRAY },
        {{ 300, 200, 400, 10 }, 1, GRAY },
        {{ 250, 300, 100, 10 }, 1, GRAY },
        {{ 650, 300, 100, 10 }, 1, GRAY }
    };

    const i32 envItemsLength = sizeof(envItems)/sizeof(envItems[0]);

    auto camera = Camera2D{};
    camera.target = player.position;
    camera.offset = vec2{ GetRenderWidth()/2.0f, GetRenderHeight()/2.0f };
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    // Store pointers to the multiple update camera functions
    void (*cameraUpdaters[])(Camera2D*, Player*, EnvItem*, i32, f32, i32, i32) = {
        UpdateCameraCenter,
        UpdateCameraCenterInsideMap,
        UpdateCameraCenterSmoothFollow,
        UpdateCameraEvenOutOnLanding,
        UpdateCameraPlayerBoundsPush
    };

    auto cameraOption = 0;
    const i32 cameraUpdatersLength = sizeof(cameraUpdaters)/sizeof(cameraUpdaters[0]);

    const char *cameraDescriptions[] = {
        "Follow player center",
        "Follow player center, but clamp to map edges",
        "Follow player center; smoothed",
        "Follow player center horizontally; update player center vertically after landing",
        "Player push camera on getting too close to screen edge"
    };

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())
    {
        // Update
        //----------------------------------------------------------------------------------
        const auto deltaTime = GetFrameTime();

        UpdatePlayer(&player, envItems, envItemsLength, deltaTime);

        camera.zoom += (GetMouseWheelMove()*0.05f);

        if (camera.zoom > 3.0f) camera.zoom = 3.0f;
        else if (camera.zoom < 0.25f) camera.zoom = 0.25f;

        if (IsKeyPressed(KEY_R))
        {
            camera.zoom = 1.0f;
            player.position = vec2{ 400, 280 };
        }

        if (IsKeyPressed(KEY_C)) cameraOption = (cameraOption + 1)%cameraUpdatersLength;

        // Call update camera function by its pointer
        cameraUpdaters[cameraOption](&camera, &player, envItems, envItemsLength, deltaTime, screenWidth, screenHeight);
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(LIGHTGRAY);

            BeginMode2D(camera);

                for (i32 i = 0; i < envItemsLength; i++) DrawRectangleRec(envItems[i].rect, envItems[i].color);

                const auto playerRect = Rectangle{ player.position.x - 20, player.position.y - 40, 40.0f, 40.0f };
                DrawRectangleRec(playerRect, RED);

                DrawCircleV(player.position, 5.0f, GOLD);

            EndMode2D();

            DrawText("Controls:", 20, 20, 10, BLACK);
            DrawText("- Right/Left to move", 40, 40, 10, DARKGRAY);
            DrawText("- Space to jump", 40, 60, 10, DARKGRAY);
            DrawText("- Mouse Wheel to Zoom in-out", 40, 80, 10, DARKGRAY);
            DrawText("- R to reset position + zoom", 40, 100, 10, DARKGRAY);
            DrawText("- C to change camera mode", 40, 120, 10, DARKGRAY);
            DrawText("Current camera mode:", 20, 140, 10, BLACK);
            DrawText(cameraDescriptions[cameraOption], 40, 160, 10, DARKGRAY);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

void UpdatePlayer(Player *player, EnvItem *envItems, i32 envItemsLength, f32 delta)
{
    if (IsKeyDown(KEY_LEFT)) player->position.x -= PLAYER_HOR_SPD*delta;
    if (IsKeyDown(KEY_RIGHT)) player->position.x += PLAYER_HOR_SPD*delta;
    if (IsKeyDown(KEY_SPACE) && player->canJump)
    {
        player->speed = -PLAYER_JUMP_SPD;
        player->canJump = false;
    }

    auto hitObstacle = false;
    for (i32 i = 0; i < envItemsLength; i++)
    {
        EnvItem *ei = envItems + i;
        vec2 *p = &(player->position);
        if (ei->blocking &&
            ei->rect.x <= p->x &&
            ei->rect.x + ei->rect.width >= p->x &&
            ei->rect.y >= p->y &&
            ei->rect.y <= p->y + player->speed*delta)
        {
            hitObstacle = true;
            player->speed = 0.0f;
            p->y = ei->rect.y;
            break;
        }
    }

    if (!hitObstacle)
    {
        player->position.y += player->speed*delta;
        player->speed += G*delta;
        player->canJump = false;
    }
    else player->canJump = true;
}

void UpdateCameraCenter(Camera2D *camera, Player *player, EnvItem *envItems, i32 envItemsLength, f32 delta, i32 width, i32 height)
{
    camera->offset = vec2{ width/2.0f, height/2.0f };
    camera->target = player->position;
}

void UpdateCameraCenterInsideMap(Camera2D *camera, Player *player, EnvItem *envItems, i32 envItemsLength, f32 delta, i32 width, i32 height)
{
    camera->target = player->position;
    camera->offset = vec2{ width/2.0f, height/2.0f };
    f32 minX = 1000, minY = 1000, maxX = -1000, maxY = -1000;

    for (i32 i = 0; i < envItemsLength; i++)
    {
        EnvItem *ei = envItems + i;
        minX = fminf(ei->rect.x, minX);
        maxX = fmaxf(ei->rect.x + ei->rect.width, maxX);
        minY = fminf(ei->rect.y, minY);
        maxY = fmaxf(ei->rect.y + ei->rect.height, maxY);
    }

    const auto max = GetWorldToScreen2D(vec2{ maxX, maxY }, *camera);
    const auto min = GetWorldToScreen2D(vec2{ minX, minY }, *camera);

    if (max.x < width) camera->offset.x = width - (max.x - static_cast<f32>(width)/2);
    if (max.y < height) camera->offset.y = height - (max.y - static_cast<f32>(height)/2);
    if (min.x > 0) camera->offset.x = static_cast<f32>(width)/2 - min.x;
    if (min.y > 0) camera->offset.y = static_cast<f32>(height)/2 - min.y;
}

void UpdateCameraCenterSmoothFollow(Camera2D *camera, Player *player, EnvItem *envItems, i32 envItemsLength, f32 delta, i32 width, i32 height)
{
    static constexpr f32 minSpeed = 30;
    static constexpr f32 minEffectLength = 10;
    static constexpr auto fractionSpeed = 0.8f;

    camera->offset = vec2{ width/2.0f, height/2.0f };
    const vec2 diff = ((player->position) - (camera->target));
    const f32 length = glm::length(diff);

    if (length > minEffectLength)
    {
        const f32 speed = fmaxf(fractionSpeed*length, minSpeed);
        camera->target = ((camera->target) + (((diff) * (speed*delta/length))));
    }
}

void UpdateCameraEvenOutOnLanding(Camera2D *camera, Player *player, EnvItem *envItems, i32 envItemsLength, f32 delta, i32 width, i32 height)
{
    static constexpr f32 evenOutSpeed = 700;
    static i32 eveningOut = false;
    static f32 evenOutTarget;

    camera->offset = vec2{ width/2.0f, height/2.0f };
    camera->target.x = player->position.x;

    if (eveningOut)
    {
        if (evenOutTarget > camera->target.y)
        {
            camera->target.y += evenOutSpeed*delta;

            if (camera->target.y > evenOutTarget)
            {
                camera->target.y = evenOutTarget;
                eveningOut = 0;
            }
        }
        else
        {
            camera->target.y -= evenOutSpeed*delta;

            if (camera->target.y < evenOutTarget)
            {
                camera->target.y = evenOutTarget;
                eveningOut = 0;
            }
        }
    }
    else
    {
        if (player->canJump && (player->speed == 0) && (player->position.y != camera->target.y))
        {
            eveningOut = 1;
            evenOutTarget = player->position.y;
        }
    }
}

void UpdateCameraPlayerBoundsPush(Camera2D *camera, Player *player, EnvItem *envItems, i32 envItemsLength, f32 delta, i32 width, i32 height)
{
    static const auto bbox = vec2{ 0.2f, 0.2f };

    const auto bboxWorldMin = GetScreenToWorld2D(vec2{ (1 - bbox.x)*0.5f*width, (1 - bbox.y)*0.5f*height }, *camera);
    const auto bboxWorldMax = GetScreenToWorld2D(vec2{ (1 + bbox.x)*0.5f*width, (1 + bbox.y)*0.5f*height }, *camera);
    camera->offset = vec2{ (1 - bbox.x)*0.5f*width, (1 - bbox.y)*0.5f*height };

    if (player->position.x < bboxWorldMin.x) camera->target.x = player->position.x;
    if (player->position.y < bboxWorldMin.y) camera->target.y = player->position.y;
    if (player->position.x > bboxWorldMax.x) camera->target.x = bboxWorldMin.x + (player->position.x - bboxWorldMax.x);
    if (player->position.y > bboxWorldMax.y) camera->target.y = bboxWorldMin.y + (player->position.y - bboxWorldMax.y);
}
