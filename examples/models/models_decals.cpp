/*******************************************************************************************
*
*   raylib [models] example - decals
*
*   Example complexity rating: [★★★★] 4/4
*
*   Example originally created with raylib 6.0, last time updated with raylib 6.0
*
*   Example contributed by JP Mortiboys (@themushroompirates) and reviewed by Ramon Santamaria (@raysan5)
*   Based on previous work by @mrdoob
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2025 JP Mortiboys (@themushroompirates) and Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"

#include "raymath.h"

#include <array>

#include <string.h>         // Required for: memcpy()

#undef FLT_MAX
#define FLT_MAX     340282346638528859811704183484516925440.0f     // Maximum value of a float, from bit pattern 01111111011111111111111111111111

#define MAX_DECALS 256

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct MeshBuilder {
    int vertexCount;
    int vertexCapacity;
    glm::vec3 *vertices;
    glm::vec2 *uvs;
} MeshBuilder;

//------------------------------------------------------------------------------------
// Module Functions Declaration
//------------------------------------------------------------------------------------
static void AddTriangleToMeshBuilder(MeshBuilder *mb, glm::vec3 vertices[3]);
static void FreeMeshBuilder(MeshBuilder *mb);
static Mesh BuildMesh(MeshBuilder *mb);
static Mesh GenMeshDecal(Model inputModel, Matrix projection, float decalSize, float decalOffset);
static glm::vec3 ClipSegment(glm::vec3 v0, glm::vec3 v1, glm::vec3 p, float s);
static void FreeDecalMeshData(void)
{
    Model sentinel{};
    sentinel.meshCount = -1;
    GenMeshDecal(sentinel, Matrix{}, 0.0f, 0.0f);
}
static bool GuiButton(Rectangle rec, const char *label);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr int screenWidth = 800;
    constexpr int screenHeight = 450;

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "raylib [models] example - decals");

    // Define the camera to look into our 3d world
    Camera camera = {};
    camera.position = glm::vec3{ 5.0f, 5.0f, 5.0f }; // Camera position
    camera.target = glm::vec3{ 0.0f, 1.0f, 0.0f };    // Camera looking at point
    camera.up = glm::vec3{ 0.0f, 1.6f, 0.0f };        // Camera up vector (rotation towards target)
    camera.fovy = 45.0f;                                // Camera field-of-view Y
    camera.projection = CAMERA_PERSPECTIVE;             // Camera projection type

    // Load character model
    const Model model = LoadModel("resources/models/obj/character.obj");

    // Apply character skin
    const Texture2D modelTexture = LoadTexture("resources/models/obj/character_diffuse.png");
    SetTextureFilter(modelTexture, TEXTURE_FILTER_BILINEAR);
    model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = modelTexture;

    const BoundingBox modelBBox = GetMeshBoundingBox(model.meshes[0]);    // Get mesh bounding box

    camera.target  = glm::mix(modelBBox.min, modelBBox.max, 0.5f);
    camera.position = ((modelBBox.max) * (1.0f));
    camera.position.x *= 0.1f;

    const float modelSize = fminf(
        fminf(fabsf(modelBBox.max.x - modelBBox.min.x), fabsf(modelBBox.max.y - modelBBox.min.y)),
        fabsf(modelBBox.max.z - modelBBox.min.z));

    camera.position = glm::vec3{ 0.0f, modelBBox.max.y*1.2f, modelSize*3.0f };

    const float decalSize = modelSize*0.25f;
    constexpr float decalOffset = 0.01f;

    Model placementCube = LoadModelFromMesh(GenMeshCube(decalSize, decalSize, decalSize));
    placementCube.materials[0].maps[0].color = LIME;

    const Material decalMaterial = LoadMaterialDefault();
    decalMaterial.maps[0].color = YELLOW;

    Image decalImage = LoadImage("resources/raylib_logo.png");
    ImageResizeNN(&decalImage, decalImage.width/4, decalImage.height/4);
    const Texture decalTexture = LoadTextureFromImage(decalImage);
    UnloadImage(decalImage);

    SetTextureFilter(decalTexture, TEXTURE_FILTER_BILINEAR);
    decalMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = decalTexture;
    decalMaterial.maps[MATERIAL_MAP_DIFFUSE].color = RAYWHITE;

    bool showModel = true;
    static Model decalModels[MAX_DECALS] = {};
    int decalCount = 0;

    SetTargetFPS(60);                   // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())        // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) UpdateCamera(&camera, CAMERA_THIRD_PERSON);

        // Display information about closest hit
        RayCollision collision = {};
        collision.distance = FLT_MAX;
        collision.hit = false;

        // Get mouse ray
        const Ray ray = GetScreenToWorldRay(GetMousePosition(), camera);

        // Check ray collision against bounding box first, before trying the full ray-mesh test
        const RayCollision boxHitInfo = GetRayCollisionBox(ray, modelBBox);

        if ((boxHitInfo.hit) && (decalCount < MAX_DECALS))
        {
            // Check ray collision against model meshes
            RayCollision meshHitInfo = {};
            for (int m = 0; m < model.meshCount; m++)
            {
                // NOTE: We consider the model.transform for the collision check but
                // it can be checked against any transform Matrix, used when checking against same
                // model drawn multiple times with multiple transforms
                meshHitInfo = GetRayCollisionMesh(ray, model.meshes[m], model.transform);
                if (meshHitInfo.hit)
                {
                    // Save the closest hit mesh
                    if (!collision.hit || (collision.distance > meshHitInfo.distance)) collision = meshHitInfo;
                }
            }

            if (meshHitInfo.hit) collision = meshHitInfo;
        }

        // Add decal to mesh on hit point
        if (collision.hit && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && (decalCount < MAX_DECALS))
        {
            // Create the transformation to project the decal
            const glm::vec3 origin = ((collision.point) + (((collision.normal) * (1.0f))));
            Matrix splat = glm::lookAt(collision.point, origin, glm::vec3{ 0.0f, 1.0f, 0.0f });

            // Spin the placement around a bit
            splat = ((glm::rotate(Matrix(1.0f), DEG2RAD*(static_cast<float>(GetRandomValue(-180, 180))), glm::vec3{0.0f, 0.0f, 1.0f})) * (splat));

            const Mesh decalMesh = GenMeshDecal(model, splat, decalSize, decalOffset);

            if (decalMesh.vertexCount > 0)
            {
                const int decalIndex = decalCount++;
                decalModels[decalIndex] = LoadModelFromMesh(decalMesh);
                decalModels[decalIndex].materials[0].maps[0] = decalMaterial.maps[0];
            }
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();
            ClearBackground(RAYWHITE);

            BeginMode3D(camera);
                // Draw the model at the origin and default scale
                if (showModel) DrawModel(model, glm::vec3{0.0f, 0.0f, 0.0f}, 1.0f, WHITE);

                // Draw the decal models
                for (int i = 0; i < decalCount; i++) DrawModel(decalModels[i], glm::vec3{}, 1.0f, WHITE);

                // If we hit the mesh, draw the box for the decal
                if (collision.hit)
                {
                    const glm::vec3 origin = ((collision.point) + (((collision.normal) * (1.0f))));
                    const Matrix splat = glm::lookAt(collision.point, origin, glm::vec3{0,1,0});
                    placementCube.transform = glm::inverse(splat);
                    DrawModel(placementCube, glm::vec3{}, 1.0f, Fade(WHITE, 0.5f));
                }

                DrawGrid(10, 10.0f);
            EndMode3D();

            float yPos = 10;
            const float x0 = GetScreenWidth() - 300.0f;
            const float x1 = x0 + 100;
            const float x2 = x1 + 100;

            DrawText("Vertices", static_cast<int>(x1), static_cast<int>(yPos), 10, LIME);
            DrawText("Triangles", static_cast<int>(x2), static_cast<int>(yPos), 10, LIME);
            yPos += 15;

            int vertexCount = 0;
            int triangleCount = 0;

            for (int i = 0; i < model.meshCount; i++)
            {
                vertexCount += model.meshes[i].vertexCount;
                triangleCount += model.meshes[i].triangleCount;
            }

            DrawText("Main model", static_cast<int>(x0), static_cast<int>(yPos), 10, LIME);
            DrawText(TextFormat("%d", vertexCount), static_cast<int>(x1), static_cast<int>(yPos), 10, LIME);
            DrawText(TextFormat("%d", triangleCount), static_cast<int>(x2), static_cast<int>(yPos), 10, LIME);
            yPos += 15;

            for (int i = 0; i < decalCount; i++)
            {
                if (i == 20)
                {
                    DrawText("...", static_cast<int>(x0), static_cast<int>(yPos), 10, LIME);
                    yPos += 15;
                }

                if (i < 20)
                {
                    DrawText(TextFormat("Decal #%d", i+1), static_cast<int>(x0), static_cast<int>(yPos), 10, LIME);
                    DrawText(TextFormat("%d", decalModels[i].meshes[0].vertexCount), static_cast<int>(x1), static_cast<int>(yPos), 10, LIME);
                    DrawText(TextFormat("%d", decalModels[i].meshes[0].triangleCount), static_cast<int>(x2), static_cast<int>(yPos), 10, LIME);
                    yPos += 15;
                }

                vertexCount += decalModels[i].meshes[0].vertexCount;
                triangleCount += decalModels[i].meshes[0].triangleCount;
            }

            DrawText("TOTAL", static_cast<int>(x0), static_cast<int>(yPos), 10, LIME);
            DrawText(TextFormat("%d", vertexCount), static_cast<int>(x1), static_cast<int>(yPos), 10, LIME);
            DrawText(TextFormat("%d", triangleCount), static_cast<int>(x2), static_cast<int>(yPos), 10, LIME);
            yPos += 15;

            DrawText("Hold RMB to move camera", 10, 430, 10, GRAY);
            DrawText("(c) Character model and texture from kenney.nl", screenWidth - 260, screenHeight - 20, 10, GRAY);

            // UI elements
            if (GuiButton(Rectangle{ 10, screenHeight - 100.0f, 100, 60 }, showModel ? "Hide Model" : "Show Model")) showModel = !showModel;

            if (GuiButton(Rectangle{ 10 + 110, screenHeight - 100.0f, 100, 60 }, "Clear Decals"))
            {
                // Clear decals, unload all decal models
                for (int i = 0; i < decalCount; i++) UnloadModel(decalModels[i]);
                decalCount = 0;
            }

            DrawFPS(10, 10);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadModel(model);
    UnloadModel(placementCube);
    UnloadTexture(modelTexture);

    // Unload decal models
    for (int i = 0; i < decalCount; i++) UnloadModel(decalModels[i]);

    UnloadTexture(decalTexture);

    FreeDecalMeshData();        // Free the data for decal generation

    CloseWindow();              // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------
// Add triangles to mesh builder (dynamic array manager)
static void AddTriangleToMeshBuilder(MeshBuilder *mb, glm::vec3 vertices[3])
{
    // Reallocate and copy if we need to
    if (mb->vertexCapacity <= (mb->vertexCount + 3))
    {
        const int newVertexCapacity = (1 + (mb->vertexCapacity/256))*256;
        glm::vec3 *newVertices = static_cast<glm::vec3 *>(MemAlloc(newVertexCapacity*sizeof(glm::vec3)));

        if (mb->vertexCapacity > 0)
        {
            memcpy(newVertices, mb->vertices, mb->vertexCount*sizeof(glm::vec3));
            MemFree(mb->vertices);
        }

        mb->vertices = newVertices;
        mb->vertexCapacity = newVertexCapacity;
    }

    // Add 3 vertices
    const int index = mb->vertexCount;
    mb->vertexCount += 3;

    for (int i = 0; i < 3; i++) mb->vertices[index+i] = vertices[i];
}

// Free mesh builder
static void FreeMeshBuilder(MeshBuilder *mb)
{
    MemFree(mb->vertices);
    if (mb->uvs != nullptr) MemFree(mb->uvs);
    *mb = {};
}

// Build a Mesh from MeshBuilder data
static Mesh BuildMesh(MeshBuilder *mb)
{
    Mesh outMesh = {};

    outMesh.vertexCount = mb->vertexCount;
    outMesh.triangleCount = mb->vertexCount/3;
    outMesh.vertices = static_cast<float *>(MemAlloc(outMesh.vertexCount*3*sizeof(float)));
    if (mb->uvs != nullptr) outMesh.texcoords = static_cast<float *>(MemAlloc(outMesh.vertexCount*2*sizeof(float)));

    for (int i = 0; i < mb->vertexCount; i++)
    {
        outMesh.vertices[3*i+0] = mb->vertices[i].x;
        outMesh.vertices[3*i+1] = mb->vertices[i].y;
        outMesh.vertices[3*i+2] = mb->vertices[i].z;

        if (mb->uvs != nullptr)
        {
            outMesh.texcoords[2*i+0] = mb->uvs[i].x;
            outMesh.texcoords[2*i+1] = mb->uvs[i].y;
        }
    }

    UploadMesh(&outMesh, false);

    return outMesh;
}

// Clip segment
static glm::vec3 ClipSegment(glm::vec3 v0, glm::vec3 v1, glm::vec3 p, float s)
{
    const float d0 = glm::dot(v0, p) - s;
    const float d1 = glm::dot(v1, p) - s;
    const float s0 = d0/(d0 - d1);

    const glm::vec3 position = glm::mix(v0, v1, s0);

    return position;
}

// Generate mesh decals for provided model
static Mesh GenMeshDecal(Model target, Matrix projection, float decalSize, float decalOffset)
{
    // We're going to use these to build up our decal meshes
    // They'll resize automatically as we go, we'll free them at the end
    static MeshBuilder meshBuilders[2] = {};

    // Ugly way of telling us to free the static MeshBuilder data
    if (target.meshCount == -1)
    {
        FreeMeshBuilder(&meshBuilders[0]);
        FreeMeshBuilder(&meshBuilders[1]);
        return {};
    }

    // We're going to need the inverse matrix
    const Matrix invProj = glm::inverse(projection);

    // Reset the mesh builders
    meshBuilders[0].vertexCount = 0;
    meshBuilders[1].vertexCount = 0;

    // We'll be flip-flopping between the two mesh builders
    // Reading from one and writing to the other, then swapping
    int mbIndex = 0;

    // First pass, just get any triangle inside the bounding box (for each mesh of the model)
    for (int meshIndex = 0; meshIndex < target.meshCount; meshIndex++)
    {
        const Mesh mesh = target.meshes[meshIndex];
        for (int tri = 0; tri < mesh.triangleCount; tri++)
        {
            glm::vec3 vertices[3] = {};

            // The way we calculate the vertices of the mesh triangle
            // depend on whether the mesh vertices are indexed or not
            if (mesh.indices == nullptr)
            {
                for (int v = 0; v < 3; v++)
                {
                    vertices[v] = glm::vec3{
                        mesh.vertices[3*3*tri + 3*v + 0],
                        mesh.vertices[3*3*tri + 3*v + 1],
                        mesh.vertices[3*3*tri + 3*v + 2]
                    };
                }
            }
            else
            {
                for (int v = 0; v < 3; v++)
                {
                    vertices[v] = glm::vec3{
                        mesh.vertices[ 3*mesh.indices[3*tri+0] + v],
                        mesh.vertices[ 3*mesh.indices[3*tri+1] + v],
                        mesh.vertices[ 3*mesh.indices[3*tri+2] + v]
                    };
                }
            }

            // Transform all 3 vertices of the triangle
            // and check if they are inside our decal box
            int insideCount = 0;
            for (int i = 0; i < 3; i++)
            {
                // To projection space
                const glm::vec3 v = glm::vec3((projection)*glm::vec4(vertices[i], 1.0f));

                if ((fabsf(v.x) < decalSize) || (fabsf(v.y) <= decalSize) || (fabsf(v.z) <= decalSize)) insideCount++;

                // We need to keep the transformed vertex
                vertices[i] = v;
            }

            // If any of them are inside, we add the triangle - we'll clip it later
            if (insideCount > 0) AddTriangleToMeshBuilder(&meshBuilders[mbIndex], vertices);
        }
    }

    // Clipping time! We need to clip against all 6 directions
    const glm::vec3 planes[6] = {
       {  1,  0,  0 },
       { -1,  0,  0 },
       {  0,  1,  0 },
       {  0, -1,  0 },
       {  0,  0,  1 },
       {  0,  0, -1 }
    };

    for (int face = 0; face < 6; face++)
    {
        // Swap current model builder (so we read from the one we just wrote to)
        mbIndex = 1 - mbIndex;

        MeshBuilder *inMesh = &meshBuilders[1 - mbIndex];
        MeshBuilder *outMesh = &meshBuilders[mbIndex];

        // Reset write builder
        outMesh->vertexCount = 0;

        const float s = 0.5f*decalSize;

        for (int i = 0; i < inMesh->vertexCount; i += 3)
        {
            glm::vec3 nV1, nV2, nV3, nV4;

            const float d1 = glm::dot(inMesh->vertices[ i + 0 ], planes[face]) - s;
            const float d2 = glm::dot(inMesh->vertices[ i + 1 ], planes[face]) - s;
            const float d3 = glm::dot(inMesh->vertices[ i + 2 ], planes[face]) - s;

            const int v1Out = (d1 > 0);
            const int v2Out = (d2 > 0);
            const int v3Out = (d3 > 0);

            // Calculate, how many vertices of the face lie outside of the clipping plane
            const int total = v1Out + v2Out + v3Out;

            switch (total)
            {
                case 0:
                {
                    // The entire face lies inside of the plane, no clipping needed
                    AddTriangleToMeshBuilder(outMesh, std::array<glm::vec3, 3>{inMesh->vertices[i], inMesh->vertices[i+1], inMesh->vertices[i+2]}.data());
                } break;
                case 1:
                {
                    // One vertex lies outside of the plane, perform clipping
                    if (v1Out)
                    {
                        nV1 = inMesh->vertices[i + 1];
                        nV2 = inMesh->vertices[i + 2];
                        nV3 = ClipSegment(inMesh->vertices[i], nV1, planes[face], s);
                        nV4 = ClipSegment(inMesh->vertices[i], nV2, planes[face], s);
                    }

                    if (v2Out)
                    {
                        nV1 = inMesh->vertices[i];
                        nV2 = inMesh->vertices[i + 2];
                        nV3 = ClipSegment(inMesh->vertices[i + 1], nV1, planes[face], s);
                        nV4 = ClipSegment(inMesh->vertices[i + 1], nV2, planes[face], s);

                        AddTriangleToMeshBuilder(outMesh, std::array<glm::vec3, 3>{nV3, nV2, nV1}.data());
                        AddTriangleToMeshBuilder(outMesh, std::array<glm::vec3, 3>{nV2, nV3, nV4}.data());
                        break;
                    }

                    if (v3Out)
                    {
                        nV1 = inMesh->vertices[i];
                        nV2 = inMesh->vertices[i + 1];
                        nV3 = ClipSegment(inMesh->vertices[i + 2], nV1, planes[face], s);
                        nV4 = ClipSegment(inMesh->vertices[i + 2], nV2, planes[face], s);
                    }

                    AddTriangleToMeshBuilder(outMesh, std::array<glm::vec3, 3>{nV1, nV2, nV3}.data());
                    AddTriangleToMeshBuilder(outMesh, std::array<glm::vec3, 3>{nV4, nV3, nV2}.data());
                } break;
                case 2:
                {
                    // Two vertices lies outside of the plane, perform clipping
                    if (!v1Out)
                    {
                        nV1 = inMesh->vertices[i];
                        nV2 = ClipSegment(nV1, inMesh->vertices[i + 1], planes[face], s);
                        nV3 = ClipSegment(nV1, inMesh->vertices[i + 2], planes[face], s);
                        AddTriangleToMeshBuilder(outMesh, std::array<glm::vec3, 3>{nV1, nV2, nV3}.data());
                    }

                    if (!v2Out)
                    {
                        nV1 = inMesh->vertices[i + 1];
                        nV2 = ClipSegment(nV1, inMesh->vertices[i + 2], planes[face], s);
                        nV3 = ClipSegment(nV1, inMesh->vertices[i], planes[face], s);
                        AddTriangleToMeshBuilder(outMesh, std::array<glm::vec3, 3>{nV1, nV2, nV3}.data());
                    }

                    if (!v3Out)
                    {
                        nV1 = inMesh->vertices[i + 2];
                        nV2 = ClipSegment(nV1, inMesh->vertices[i], planes[face], s);
                        nV3 = ClipSegment(nV1, inMesh->vertices[i + 1], planes[face], s);
                        AddTriangleToMeshBuilder(outMesh, std::array<glm::vec3, 3>{nV1, nV2, nV3}.data());
                    }
                } break;
                case 3: // The entire face lies outside of the plane, so let's discard the corresponding vertices
                default: break;
            }
        }
    }

    // Now we just need to re-transform the vertices
    MeshBuilder *theMesh = &meshBuilders[mbIndex];

    // Allocate room for UVs
    if (theMesh->vertexCount > 0)
    {
        theMesh->uvs = static_cast<glm::vec2 *>(MemAlloc(sizeof(glm::vec2)*theMesh->vertexCount));

        for (int i = 0; i < theMesh->vertexCount; i++)
        {
            // Calculate the UVs based on the projected coords
            // They are clipped to (-decalSize .. decalSize) and we want them (0..1)
            theMesh->uvs[i].x = (theMesh->vertices[i].x/decalSize + 0.5f);
            theMesh->uvs[i].y = (theMesh->vertices[i].y/decalSize + 0.5f);

            // Tiny nudge in the normal direction so it renders properly over the mesh
            theMesh->vertices[i].z -= decalOffset;

            // From projection space to world space
            theMesh->vertices[i] = glm::vec3((invProj)*glm::vec4(theMesh->vertices[i], 1.0f));
        }

        // Decal model data ready, create the mesh and return it
        return BuildMesh(theMesh);
    }
    else
    {
        // Return a blank mesh as there's nothing to add
        return {};
    }
}

// Button UI element
static bool GuiButton(Rectangle rec, const char *label)
{
    Color bgColor = GRAY;
    bool pressed = false;

    if (CheckCollisionPointRec(GetMousePosition(), rec))
    {
        bgColor = LIGHTGRAY;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) pressed = true;
    }

    DrawRectangleRec(rec, bgColor);
    DrawRectangleLinesEx(rec, 2.0f, DARKGRAY);

    constexpr int fontSize = 10;
    const int textWidth = MeasureText(label, fontSize);

    DrawText(label, static_cast<int>(rec.x + rec.width*0.5f - textWidth*0.5f), static_cast<int>(rec.y + rec.height*0.5f - fontSize*0.5f), fontSize, DARKGRAY);

    return pressed;
}
