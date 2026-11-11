/*******************************************************************************************
*
*   raylib [models] example - mesh picking
*
*   Example complexity rating: [★★★☆] 3/4
*
*   Example originally created with raylib 1.7, last time updated with raylib 4.0
*
*   Example contributed by Joel Davis (@joeld42) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2017-2025 Joel Davis (@joeld42) and Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"
#include "raymath.h"

#undef FLT_MAX
#define FLT_MAX     340282346638528859811704183484516925440.0f     // Maximum value of a float, from bit pattern 01111111011111111111111111111111

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr auto screenWidth = 800;
    constexpr auto screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [models] example - mesh picking");

    // Define the camera to look into our 3d world
    auto camera = Camera{};
    camera.position = vec3{ 20.0f, 20.0f, 20.0f };      // Camera position
    camera.target = vec3{ 0.0f, 8.0f, 0.0f };           // Camera looking at point
    camera.up = vec3{ 0.0f, 1.6f, 0.0f };               // Camera up vector (rotation towards target)
    camera.fovy = 45.0f;                                // Camera field-of-view Y
    camera.projection = CAMERA_PERSPECTIVE;             // Camera projection type

    auto ray = Ray{};        // Picking ray

    const Model tower = LoadModel("resources/models/obj/turret.obj");                 // Load OBJ model
    const Texture2D texture = LoadTexture("resources/models/obj/turret_diffuse.png"); // Load model texture
    tower.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture;                  // Set model diffuse texture

    const auto towerPos = vec3{ 0.0f, 0.0f, 0.0f };                           // Set model position
    const auto towerBBox = GetMeshBoundingBox(tower.meshes[0]);    // Get mesh bounding box

    // Ground quad
    const auto g0 = vec3{ -50.0f, 0.0f, -50.0f };
    const auto g1 = vec3{ -50.0f, 0.0f,  50.0f };
    const auto g2 = vec3{  50.0f, 0.0f,  50.0f };
    const auto g3 = vec3{  50.0f, 0.0f, -50.0f };

    // Test triangle
    const auto ta = vec3{ -25.0f, 0.5f, 0.0f };
    const auto tb = vec3{ -4.0f, 2.5f, 1.0f };
    const auto tc = vec3{ -8.0f, 6.5f, 0.0f };

    auto bary = vec3{ 0.0f, 0.0f, 0.0f };

    // Test sphere
    const auto sp = vec3{ -30.0f, 5.0f, 5.0f };
    constexpr auto sr = 4.0f;

    SetTargetFPS(60);                   // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------
    // Main game loop
    while (!WindowShouldClose())        // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        if (IsCursorHidden()) UpdateCamera(&camera, CAMERA_FIRST_PERSON);          // Update camera

        // Toggle camera controls
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        {
            if (IsCursorHidden()) EnableCursor();
            else DisableCursor();
        }

        // Display information about closest hit
        auto collision = RayCollision{};
        const char *hitObjectName = "None";
        collision.distance = FLT_MAX;
        collision.hit = false;
        Color cursorColor = WHITE;

        // Get ray and test against objects
        ray = GetScreenToWorldRay(GetMousePosition(), camera);

        // Check ray collision against ground quad
        const auto groundHitInfo = GetRayCollisionQuad(ray, g0, g1, g2, g3);

        if ((groundHitInfo.hit) && (groundHitInfo.distance < collision.distance))
        {
            collision = groundHitInfo;
            cursorColor = GREEN;
            hitObjectName = "Ground";
        }

        // Check ray collision against test triangle
        const auto triHitInfo = GetRayCollisionTriangle(ray, ta, tb, tc);

        if ((triHitInfo.hit) && (triHitInfo.distance < collision.distance))
        {
            collision = triHitInfo;
            cursorColor = PURPLE;
            hitObjectName = "Triangle";

            bary = Vector3Barycenter(collision.point, ta, tb, tc);
        }

        // Check ray collision against test sphere
        const auto sphereHitInfo = GetRayCollisionSphere(ray, sp, sr);

        if ((sphereHitInfo.hit) && (sphereHitInfo.distance < collision.distance))
        {
            collision = sphereHitInfo;
            cursorColor = ORANGE;
            hitObjectName = "Sphere";
        }

        // Check ray collision against bounding box first, before trying the full ray-mesh test
        const auto boxHitInfo = GetRayCollisionBox(ray, towerBBox);

        if ((boxHitInfo.hit) && (boxHitInfo.distance < collision.distance))
        {
            collision = boxHitInfo;
            cursorColor = ORANGE;
            hitObjectName = "Box";

            // Check ray collision against model meshes
            auto meshHitInfo = RayCollision{};
            for (i32 m = 0; m < tower.meshCount; m++)
            {
                // NOTE: We consider the model.transform for the collision check but
                // it can be checked against any transform mat4, used when checking against same
                // model drawn multiple times with multiple transforms
                meshHitInfo = GetRayCollisionMesh(ray, tower.meshes[m], tower.transform);
                if (meshHitInfo.hit)
                {
                    // Save the closest hit mesh
                    if ((!collision.hit) || (collision.distance > meshHitInfo.distance)) collision = meshHitInfo;

                    break;  // Stop once one mesh collision is detected, the colliding mesh is m
                }
            }

            if (meshHitInfo.hit)
            {
                collision = meshHitInfo;
                cursorColor = ORANGE;
                hitObjectName = "Mesh";
            }
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            BeginMode3D(camera);

                // Draw the tower
                // WARNING: If scale is different than 1.0f,
                // not considered by GetRayCollisionModel()
                DrawModel(tower, towerPos, 1.0f, WHITE);

                // Draw the test triangle
                DrawLine3D(ta, tb, PURPLE);
                DrawLine3D(tb, tc, PURPLE);
                DrawLine3D(tc, ta, PURPLE);

                // Draw the test sphere
                DrawSphereWires(sp, sr, 8, 8, PURPLE);

                // Draw the mesh bbox if we hit it
                if (boxHitInfo.hit) DrawBoundingBox(towerBBox, LIME);

                // If we hit something, draw the cursor at the hit point
                if (collision.hit)
                {
                    DrawCube(collision.point, 0.3f, 0.3f, 0.3f, cursorColor);
                    DrawCubeWires(collision.point, 0.3f, 0.3f, 0.3f, RED);

                    vec3 normalEnd;
                    normalEnd.x = collision.point.x + collision.normal.x;
                    normalEnd.y = collision.point.y + collision.normal.y;
                    normalEnd.z = collision.point.z + collision.normal.z;

                    DrawLine3D(collision.point, normalEnd, RED);
                }

                DrawRay(ray, MAROON);

                DrawGrid(10, 10.0f);

            EndMode3D();

            // Draw some debug GUI text
            DrawText(TextFormat("Hit Object: %s", hitObjectName), 10, 50, 10, BLACK);

            if (collision.hit)
            {
                constexpr auto ypos = 70;

                DrawText(TextFormat("Distance: %3.2f", collision.distance), 10, ypos, 10, BLACK);

                DrawText(TextFormat("Hit Pos: %3.2f %3.2f %3.2f",
                                    collision.point.x,
                                    collision.point.y,
                                    collision.point.z), 10, ypos + 15, 10, BLACK);

                DrawText(TextFormat("Hit Norm: %3.2f %3.2f %3.2f",
                                    collision.normal.x,
                                    collision.normal.y,
                                    collision.normal.z), 10, ypos + 30, 10, BLACK);

                if (triHitInfo.hit && TextIsEqual(hitObjectName, "Triangle"))
                    DrawText(TextFormat("Barycenter: %3.2f %3.2f %3.2f",  bary.x, bary.y, bary.z), 10, ypos + 45, 10, BLACK);
            }

            DrawText("Right click mouse to toggle camera controls", 10, 430, 10, GRAY);

            DrawText("(c) Turret 3D model by Alberto Cano", screenWidth - 200, screenHeight - 20, 10, GRAY);

            DrawFPS(10, 10);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadModel(tower);         // Unload model
    UnloadTexture(texture);     // Unload texture

    CloseWindow();              // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
