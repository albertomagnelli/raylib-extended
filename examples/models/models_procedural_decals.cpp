/*******************************************************************************************
*
*   raylib [models] example - procedural decals
*
*   Example demonstrates projecting decals onto a 3D mesh surface by transforming the
*   mesh's own triangles into the decal's local space and clipping them against a box,
*   using only procedurally generated geometry and textures (no external resource files)
*
*   NOTE: This is the same clip-space decal projection idea used by models_decals.c
*   (in turn based on three.js' DecalGeometry), applied here to a generated mesh and
*   generated textures instead of a loaded model, so the example is fully self-contained
*
*   Example complexity rating: [★★★★] 4/4
*
*   Example originally created with raylib 6.0, last time updated with raylib 6.0
*
*   Example contributed by PanicTitan (@PanicTitan) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2025 PanicTitan (@PanicTitan)
*
********************************************************************************************/

#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include "raymath.h"

#include <string.h>     // Required for: memcpy()

//--------------------------------------------------------------------------------------
// Global Definitions
//--------------------------------------------------------------------------------------
#define MAX_DECALS  256

//--------------------------------------------------------------------------------------
// Types and Structures Definition
//--------------------------------------------------------------------------------------

namespace
{
    // Growable triangle-soup buffer used while building a clipped decal mesh
    typedef struct MeshBuilder {
        i32 vertexCount;
        i32 vertexCapacity;
        vec3 *vertices;
        vec2 *uvs;
    } MeshBuilder;
}

//------------------------------------------------------------------------------------
// Module Functions Declaration
//------------------------------------------------------------------------------------
static void AddTriangleToMeshBuilder(MeshBuilder *mb, vec3 v0, vec3 v1, vec3 v2);
static void FreeMeshBuilder(MeshBuilder *mb);
static Mesh BuildMesh(const MeshBuilder *mb);
static vec3 ClipSegment(vec3 v0, vec3 v1, vec3 p, f32 s);
static Mesh GenMeshDecal(Model model, mat4 projection, f32 decalSize, f32 decalOffset);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr i32 screenWidth = 800;
    constexpr i32 screenHeight = 450;

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "raylib [models] example - procedural decals");

    Camera3D camera = {};
    camera.position = vec3{ 0.0f, 2.5f, 5.0f };
    camera.target = vec3{ 0.0f, 0.0f, 0.0f };
    camera.up = vec3{ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Target model: a procedurally generated knot with a checkerboard skin
    const Model model = LoadModelFromMesh(GenMeshTorus(0.8f, 1.8f, 32, 64));

    const Image checkerImage = GenImageChecked(512, 512, 32, 32, LIGHTGRAY, GRAY);
    const Texture2D modelTexture = LoadTextureFromImage(checkerImage);
    UnloadImage(checkerImage);
    SetTextureFilter(modelTexture, TEXTURE_FILTER_BILINEAR);
    model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = modelTexture;

    const BoundingBox modelBBox = GetMeshBoundingBox(model.meshes[0]);
    camera.target = glm::mix(modelBBox.min, modelBBox.max, 0.5f);

    const f32 modelSize = fminf(fminf(fabsf(modelBBox.max.x - modelBBox.min.x),
        fabsf(modelBBox.max.y - modelBBox.min.y)), fabsf(modelBBox.max.z - modelBBox.min.z));

    const f32 decalSize = modelSize*0.35f;

    // Translucent cube previewing where the next decal will land
    Model placementCube = LoadModelFromMesh(GenMeshCube(decalSize, decalSize, decalSize));
    placementCube.materials[0].maps[0].color = LIME;

    // Decal texture: a procedurally drawn target/bullseye, no image file needed
    Image decalImage = GenImageColor(128, 128, BLANK);
    ImageDrawCircle(&decalImage, 64, 64, 60, RED);
    ImageDrawCircle(&decalImage, 64, 64, 45, WHITE);
    ImageDrawCircle(&decalImage, 64, 64, 30, RED);
    ImageDrawCircle(&decalImage, 64, 64, 15, YELLOW);

    const Texture2D decalTexture = LoadTextureFromImage(decalImage);
    UnloadImage(decalImage);
    SetTextureFilter(decalTexture, TEXTURE_FILTER_BILINEAR);

    const Material decalMaterial = LoadMaterialDefault();
    decalMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = decalTexture;
    decalMaterial.maps[MATERIAL_MAP_DIFFUSE].color = RAYWHITE;

    bool showModel = true;
    Model decals[MAX_DECALS] = {};
    i32 decalCount = 0;

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) UpdateCamera(&camera, CAMERA_THIRD_PERSON);

        // Cast a ray from the mouse and keep the closest point it hits on the model
        const Ray ray = GetScreenToWorldRay(GetMousePosition(), camera);
        RayCollision collision = {};

        if (GetRayCollisionBox(ray, modelBBox).hit && (decalCount < MAX_DECALS))
        {
            for (i32 m = 0; m < model.meshCount; m++)
            {
                const RayCollision meshHit = GetRayCollisionMesh(ray, model.meshes[m], model.transform);
                if (meshHit.hit && (!collision.hit || (meshHit.distance < collision.distance))) collision = meshHit;
            }
        }

        // Project a new decal at the hit point, facing along the surface normal
        if (collision.hit && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && (decalCount < MAX_DECALS))
        {
            constexpr f32 decalOffset = 0.01f;
            const vec3 lookTarget = ((collision.point) + (collision.normal));
            mat4 splat = glm::lookAt(collision.point, lookTarget, vec3{ 0.0f, 1.0f, 0.0f });
            splat = ((glm::rotate(mat4(1.0f), DEG2RAD*static_cast<f32>(GetRandomValue(-180, 180)), vec3{0.0f, 0.0f, 1.0f})) * (splat));

            if (const Mesh decalMesh = GenMeshDecal(model, splat, decalSize, decalOffset); decalMesh.vertexCount > 0)
            {
                decals[decalCount] = LoadModelFromMesh(decalMesh);
                decals[decalCount].materials[0].maps[0] = decalMaterial.maps[0];
                decalCount++;
            }
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            BeginMode3D(camera);

                if (showModel) DrawModel(model, vec3{}, 1.0f, WHITE);

                for (i32 i = 0; i < decalCount; i++) DrawModel(decals[i], vec3{}, 1.0f, WHITE);

                // Preview cube at the surface point currently under the mouse
                if (collision.hit)
                {
                    const vec3 lookTarget = ((collision.point) + (collision.normal));
                    const mat4 splat = glm::lookAt(collision.point, lookTarget, vec3{ 0.0f, 1.0f, 0.0f });
                    placementCube.transform = glm::inverse(splat);
                    DrawModel(placementCube, vec3{}, 1.0f, Fade(WHITE, 0.5f));
                }

                DrawGrid(10, 1.0f);

            EndMode3D();

            // Vertex/triangle counts: base model vs total once decal geometry is added
            i32 baseVertices = 0, baseTriangles = 0;
            for (i32 i = 0; i < model.meshCount; i++)
            {
                baseVertices += model.meshes[i].vertexCount;
                baseTriangles += model.meshes[i].triangleCount;
            }

            i32 totalVertices = baseVertices, totalTriangles = baseTriangles;
            for (i32 i = 0; i < decalCount; i++)
            {
                totalVertices += decals[i].meshes[0].vertexCount;
                totalTriangles += decals[i].meshes[0].triangleCount;
            }

        constexpr i32 statX = screenWidth - 280;
            DrawText("Vertices", statX + 90, 10, 10, LIME);
            DrawText("Triangles", statX + 180, 10, 10, LIME);
            DrawText("Base Model", statX, 25, 10, LIME);
            DrawText(TextFormat("%d", baseVertices), statX + 90, 25, 10, LIME);
            DrawText(TextFormat("%d", baseTriangles), statX + 180, 25, 10, LIME);
            DrawText("TOTAL", statX, 40, 10, LIME);
            DrawText(TextFormat("%d", totalVertices), statX + 90, 40, 10, LIME);
            DrawText(TextFormat("%d", totalTriangles), statX + 180, 40, 10, LIME);

            if (GuiButton(Rectangle{ 10, screenHeight - 80.0f, 100, 40 }, showModel ? "Hide Model" : "Show Model"))
                showModel = !showModel;

            if (GuiButton(Rectangle{ 120, screenHeight - 80.0f, 100, 40 }, "Clear Decals"))
            {
                for (i32 i = 0; i < decalCount; i++) UnloadModel(decals[i]);
                decalCount = 0;
            }

            DrawText("Left click: place decal   |   Right drag: orbit camera", 10, screenHeight - 25, 10, DARKGRAY);
            DrawFPS(10, 10);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    for (i32 i = 0; i < decalCount; i++) UnloadModel(decals[i]);

    UnloadModel(placementCube);
    UnloadTexture(decalTexture);
    UnloadTexture(modelTexture);
    UnloadModel(model);

    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------

// Appends a triangle to a growable mesh builder buffer, growing it in fixed-size chunks
static void AddTriangleToMeshBuilder(MeshBuilder *mb, vec3 v0, vec3 v1, vec3 v2)
{
    if (mb->vertexCapacity <= (mb->vertexCount + 3))
    {
        const i32 newVertexCapacity = (1 + (mb->vertexCapacity/256))*256;
        auto *newVertices = static_cast<vec3 *>(MemAlloc(newVertexCapacity*sizeof(vec3)));

        if (mb->vertexCapacity > 0)
        {
            memcpy(newVertices, mb->vertices, mb->vertexCount*sizeof(vec3));
            MemFree(mb->vertices);
        }

        mb->vertices = newVertices;
        mb->vertexCapacity = newVertexCapacity;
    }

    const i32 index = mb->vertexCount;
    mb->vertexCount += 3;

    mb->vertices[index + 0] = v0;
    mb->vertices[index + 1] = v1;
    mb->vertices[index + 2] = v2;
}

// Frees a mesh builder's backing buffers
static void FreeMeshBuilder(MeshBuilder *mb)
{
    if (mb->vertices != nullptr) MemFree(mb->vertices);
    if (mb->uvs != nullptr) MemFree(mb->uvs);
    *mb = {};
}

// Converts a mesh builder's triangle soup into a standard uploaded raylib Mesh
static Mesh BuildMesh(const MeshBuilder *mb)
{
    Mesh outMesh = {};

    outMesh.vertexCount = mb->vertexCount;
    outMesh.triangleCount = mb->vertexCount/3;
    outMesh.vertices = static_cast<f32 *>(MemAlloc(outMesh.vertexCount*3*sizeof(f32)));
    if (mb->uvs != nullptr) outMesh.texcoords = static_cast<f32 *>(MemAlloc(outMesh.vertexCount*2*sizeof(f32)));

    for (i32 i = 0; i < mb->vertexCount; i++)
    {
        outMesh.vertices[3*i + 0] = mb->vertices[i].x;
        outMesh.vertices[3*i + 1] = mb->vertices[i].y;
        outMesh.vertices[3*i + 2] = mb->vertices[i].z;

        if (mb->uvs != nullptr)
        {
            outMesh.texcoords[2*i + 0] = mb->uvs[i].x;
            outMesh.texcoords[2*i + 1] = mb->uvs[i].y;
        }
    }

    UploadMesh(&outMesh, false);

    return outMesh;
}

// Clips the segment [v0, v1] against the plane p.x = s, returning the intersection point
static vec3 ClipSegment(vec3 v0, vec3 v1, vec3 p, f32 s)
{
    const f32 d0 = glm::dot(v0, p) - s;
    const f32 d1 = glm::dot(v1, p) - s;
    const f32 t = d0/(d0 - d1);

    return glm::mix(v0, v1, t);
}

// Builds a decal mesh: the model's triangles are transformed into the decal's local space
// (so the decal sits at the origin, facing +Z) and clipped against a decalSize-sided box,
// following the same clip-space projection idea used by engines' decal systems (and by
// three.js' DecalGeometry, which the technique is commonly traced back to). What's left
// after clipping becomes the decal's own small mesh, UV-mapped from its local coordinates
static Mesh GenMeshDecal(Model model, mat4 projection, f32 decalSize, f32 decalOffset)
{
    const mat4 invProj = glm::inverse(projection);
    MeshBuilder meshBuilders[2] = {};
    i32 mbIndex = 0;

    // Gather triangles that land anywhere near the decal box; this is a loose, cheap
    // pre-filter meant to skip most of the model, the precise clip happens below
    for (i32 meshIndex = 0; meshIndex < model.meshCount; meshIndex++)
    {
        const Mesh mesh = model.meshes[meshIndex];

        for (i32 tri = 0; tri < mesh.triangleCount; tri++)
        {
            vec3 vertices[3] = {};

            if (mesh.indices == nullptr)
            {
                for (i32 v = 0; v < 3; v++)
                {
                    vertices[v] = vec3{
                        mesh.vertices[3*3*tri + 3*v + 0],
                        mesh.vertices[3*3*tri + 3*v + 1],
                        mesh.vertices[3*3*tri + 3*v + 2]
                    };
                }
            }
            else
            {
                for (i32 v = 0; v < 3; v++)
                {
                    const i32 idx = mesh.indices[3*tri + v];
                    vertices[v] = vec3{
                        mesh.vertices[3*idx + 0],
                        mesh.vertices[3*idx + 1],
                        mesh.vertices[3*idx + 2]
                    };
                }
            }

            i32 insideCount = 0;
            for (auto & vertice : vertices)
            {
                const auto v = vec3((projection)*vec4(vertice, 1.0f));
                if ((fabsf(v.x) < decalSize) || (fabsf(v.y) <= decalSize) || (fabsf(v.z) <= decalSize)) insideCount++;
                vertice = v;
            }

            if (insideCount > 0) AddTriangleToMeshBuilder(&meshBuilders[mbIndex], vertices[0], vertices[1], vertices[2]);
        }
    }

    // Clip the surviving triangles against each of the decal box's 6 faces in turn

    for (i32 face = 0; face < 6; face++)
    {
        mbIndex = 1 - mbIndex;

        MeshBuilder *inMesh = &meshBuilders[1 - mbIndex];
        MeshBuilder *outMesh = &meshBuilders[mbIndex];

        outMesh->vertexCount = 0;
        const f32 s = 0.5f*decalSize;

        for (i32 i = 0; i < inMesh->vertexCount; i += 3)
        {
            const vec3 planes[6] = {
                {  1,  0,  0 }, { -1,  0,  0 },
                {  0,  1,  0 }, {  0, -1,  0 },
                {  0,  0,  1 }, {  0,  0, -1 }
            };
            vec3 nV1, nV2, nV3, nV4;

            const f32 d1 = glm::dot(inMesh->vertices[i + 0], planes[face]) - s;
            const f32 d2 = glm::dot(inMesh->vertices[i + 1], planes[face]) - s;
            const f32 d3 = glm::dot(inMesh->vertices[i + 2], planes[face]) - s;

            const i32 v1Out = (d1 > 0);
            const i32 v2Out = (d2 > 0);
            const i32 v3Out = (d3 > 0);

            switch (const i32 total = v1Out + v2Out + v3Out)
            {
                case 0:
                    // Whole triangle is inside this face, keep it as-is
                    AddTriangleToMeshBuilder(outMesh, inMesh->vertices[i], inMesh->vertices[i + 1], inMesh->vertices[i + 2]);
                    break;
                case 1:
                    // One corner is outside; clip it off, turning the triangle into a quad
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

                        AddTriangleToMeshBuilder(outMesh, nV3, nV2, nV1);
                        AddTriangleToMeshBuilder(outMesh, nV2, nV3, nV4);
                        break;
                    }

                    if (v3Out)
                    {
                        nV1 = inMesh->vertices[i];
                        nV2 = inMesh->vertices[i + 1];
                        nV3 = ClipSegment(inMesh->vertices[i + 2], nV1, planes[face], s);
                        nV4 = ClipSegment(inMesh->vertices[i + 2], nV2, planes[face], s);
                    }

                    AddTriangleToMeshBuilder(outMesh, nV1, nV2, nV3);
                    AddTriangleToMeshBuilder(outMesh, nV4, nV3, nV2);
                    break;
                case 2:
                    // Two corners are outside; only the small corner near the surviving vertex remains
                    if (!v1Out)
                    {
                        nV1 = inMesh->vertices[i];
                        nV2 = ClipSegment(nV1, inMesh->vertices[i + 1], planes[face], s);
                        nV3 = ClipSegment(nV1, inMesh->vertices[i + 2], planes[face], s);
                        AddTriangleToMeshBuilder(outMesh, nV1, nV2, nV3);
                    }

                    if (!v2Out)
                    {
                        nV1 = inMesh->vertices[i + 1];
                        nV2 = ClipSegment(nV1, inMesh->vertices[i + 2], planes[face], s);
                        nV3 = ClipSegment(nV1, inMesh->vertices[i], planes[face], s);
                        AddTriangleToMeshBuilder(outMesh, nV1, nV2, nV3);
                    }

                    if (!v3Out)
                    {
                        nV1 = inMesh->vertices[i + 2];
                        nV2 = ClipSegment(nV1, inMesh->vertices[i], planes[face], s);
                        nV3 = ClipSegment(nV1, inMesh->vertices[i + 1], planes[face], s);
                        AddTriangleToMeshBuilder(outMesh, nV1, nV2, nV3);
                    }
                    break;
                default:
                    // All 3 corners are outside this face, the triangle is fully clipped away
                    break;
            }
        }
    }

    MeshBuilder *finalMesh = &meshBuilders[mbIndex];
    Mesh decalMesh = {};

    if (finalMesh->vertexCount > 0)
    {
        finalMesh->uvs = static_cast<vec2 *>(MemAlloc(sizeof(vec2)*finalMesh->vertexCount));

        for (i32 i = 0; i < finalMesh->vertexCount; i++)
        {
            // Clipped coordinates run roughly (-decalSize/2 .. decalSize/2); remap to (0..1)
            finalMesh->uvs[i].x = (finalMesh->vertices[i].x/decalSize + 0.5f);
            finalMesh->uvs[i].y = (finalMesh->vertices[i].y/decalSize + 0.5f);

            // Nudge slightly along the normal so the decal doesn't z-fight with the surface
            finalMesh->vertices[i].z -= decalOffset;
            finalMesh->vertices[i] = vec3((invProj)*vec4(finalMesh->vertices[i], 1.0f));
        }

        decalMesh = BuildMesh(finalMesh);
    }

    FreeMeshBuilder(&meshBuilders[0]);
    FreeMeshBuilder(&meshBuilders[1]);

    return decalMesh;
}
