/*******************************************************************************************
*
*   raylib [models] example - animation blend custom
*
*   Example complexity rating: [★★★★] 4/4
*
*   Example originally created with raylib 5.5, last time updated with raylib 6.0
*
*   Example contributed by dmitrii-brand (@dmitrii-brand) and reviewed by Ramon Santamaria (@raysan5)
*
*   DETAILS: Example demonstrates per-bone animation blending, allowing smooth transitions
*   between two animations by interpolating bone transforms. This is useful for:
*    - Blending movement animations (walk/run) with action animations (jump/attack)
*    - Creating smooth animation transitions
*    - Layering animations (e.g., upper body attack while lower body walks)
*
*   WARNING: GPU skinning must be enabled in raylib with a compilation flag,
*   if not enabled, CPU skinning will be used instead
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2026 dmitrii-brand (@dmitrii-brand)
*
********************************************************************************************/

#include "raylib.h"

#include "raymath.h"

#include "rlgl.h"       // Requried for: rlUpdateVertexBuffer() (CPU-skinning)

#include <string.h>     // Required for: memcpy()
#include <stdlib.h>     // Required for: NULL

#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION            330
#else   // PLATFORM_ANDROID, PLATFORM_WEB
    #define GLSL_VERSION            100
#endif

//------------------------------------------------------------------------------------
// Module Functions Declaration
//------------------------------------------------------------------------------------
static bool IsUpperBodyBone(const char *boneName);
static void UpdateModelAnimationBoneCustom(Model *model, ModelAnimation *anim1, int frame1,
    ModelAnimation *anim2, int frame2, float blend, bool upperBodyBlend);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr int screenWidth = 800;
    constexpr int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [models] example - animation blend custom");

    // Define the camera to look into our 3d world
    Camera camera = {};
    camera.position = Vector3{ 4.0f, 4.0f, 4.0f }; // Camera position
    camera.target = Vector3{ 0.0f, 1.0f, 0.0f };  // Camera looking at point
    camera.up = Vector3{ 0.0f, 1.0f, 0.0f };      // Camera up vector (rotation towards target)
    camera.fovy = 45.0f;                            // Camera field-of-view Y
    camera.projection = CAMERA_PERSPECTIVE;         // Camera projection type

    // Load gltf model
    Model model = LoadModel("resources/models/gltf/greenman.glb");
    const Vector3 position = { 0.0f, 0.0f, 0.0f }; // Set model position

    // Load skinning shader
    // WARNING: GPU skinning must be enabled in raylib with a compilation flag,
    // if not enabled, CPU skinning will be used instead
    const Shader skinningShader = LoadShader(TextFormat("resources/shaders/glsl%i/skinning.vs", GLSL_VERSION),
                                       TextFormat("resources/shaders/glsl%i/skinning.fs", GLSL_VERSION));
    model.materials[1].shader = skinningShader;

    // Load gltf model animations
    int animCount = 0;
    ModelAnimation *anims = LoadModelAnimations("resources/models/gltf/greenman.glb", &animCount);

    // Use specific animation indices: 2-walk/move, 3-attack
    int animIndex0 = 2; // Walk/Move animation (index 2)
    int animIndex1 = 3; // Attack animation (index 3)
    int animCurrentFrame0 = 0;
    int animCurrentFrame1 = 0;

    // Validate indices
    if (animIndex0 >= animCount) animIndex0 = 0;
    if (animIndex1 >= animCount) animIndex1 = (animCount > 1) ? 1 : 0;

    bool upperBodyBlend = true;     // Toggle: true = upper/lower body blending, false = uniform blending (50/50)

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())        // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        UpdateCamera(&camera, CAMERA_ORBITAL);

        // Toggle upper/lower body blending mode (SPACE key)
        if (IsKeyPressed(KEY_SPACE)) upperBodyBlend = !upperBodyBlend;

        // Update animation frames
        ModelAnimation anim0 = anims[animIndex0];
        ModelAnimation anim1 = anims[animIndex1];

        animCurrentFrame0 = (animCurrentFrame0 + 1)%anim0.keyframeCount;
        animCurrentFrame1 = (animCurrentFrame1 + 1)%anim1.keyframeCount;

        // Blend the two animations
        // When upperBodyBlend is ON: upper body = attack (1.0), lower body = walk (0.0)
        // When upperBodyBlend is OFF: uniform blend at 0.5 (50% walk, 50% attack)
        const float blendFactor = (upperBodyBlend? 1.0f : 0.5f);
        UpdateModelAnimationBoneCustom(&model, &anim0, animCurrentFrame0,
            &anim1, animCurrentFrame1, blendFactor, upperBodyBlend);

        // raylib provided animation blending function
        //UpdateModelAnimationEx(model, anim0, (float)animCurrentFrame0,
        //    anim1, (float)animCurrentFrame1, blendFactor);
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            BeginMode3D(camera);

                DrawModel(model, position, 1.0f, WHITE);

                DrawGrid(10, 1.0f);

            EndMode3D();

            // Draw UI
            DrawText(TextFormat("ANIM 0: %s", anim0.name), 10, 10, 20, GRAY);
            DrawText(TextFormat("ANIM 1: %s", anim1.name), 10, 40, 20, GRAY);
            DrawText(TextFormat("[SPACE] Toggle blending mode: %s",
                upperBodyBlend? "Upper/Lower Body Blending" : "Uniform Blending"),
                10, GetScreenHeight() - 30, 20, DARKGRAY);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadModelAnimations(anims, animCount); // Unload model animation
    UnloadModel(model);    // Unload model and meshes/material
    UnloadShader(skinningShader);   // Unload GPU skinning shader

    CloseWindow();                  // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------
// Check if a bone is part of upper body (for selective blending)
static bool IsUpperBodyBone(const char *boneName)
{
    // Common upper body bone names (adjust based on your model)
    if (TextIsEqual(boneName, "spine") || TextIsEqual(boneName, "spine1") || TextIsEqual(boneName, "spine2") ||
        TextIsEqual(boneName, "chest") || TextIsEqual(boneName, "upperChest") ||
        TextIsEqual(boneName, "neck") || TextIsEqual(boneName, "head") ||
        TextIsEqual(boneName, "shoulder") || TextIsEqual(boneName, "shoulder_L") || TextIsEqual(boneName, "shoulder_R") ||
        TextIsEqual(boneName, "upperArm") || TextIsEqual(boneName, "upperArm_L") || TextIsEqual(boneName, "upperArm_R") ||
        TextIsEqual(boneName, "lowerArm") || TextIsEqual(boneName, "lowerArm_L") || TextIsEqual(boneName, "lowerArm_R") ||
        TextIsEqual(boneName, "hand") || TextIsEqual(boneName, "hand_L") || TextIsEqual(boneName, "hand_R") ||
        TextIsEqual(boneName, "clavicle") || TextIsEqual(boneName, "clavicle_L") || TextIsEqual(boneName, "clavicle_R"))
    {
        return true;
    }

    // Check if bone name contains upper body keywords
    if (strstr(boneName, "spine") != nullptr || strstr(boneName, "chest") != nullptr ||
        strstr(boneName, "neck") != nullptr || strstr(boneName, "head") != nullptr ||
        strstr(boneName, "shoulder") != nullptr || strstr(boneName, "arm") != nullptr ||
        strstr(boneName, "hand") != nullptr || strstr(boneName, "clavicle") != nullptr)
    {
        return true;
    }

    return false;
}

// Blend two animations per-bone with selective upper/lower body blending
static void UpdateModelAnimationBoneCustom(Model *model, ModelAnimation *anim0, int frame0,
    ModelAnimation *anim1, int frame1, float blend, bool upperBodyBlend)
{
    // Validate inputs
    if ((anim0->boneCount != 0) && (anim0->keyframePoses != nullptr) &&
        (anim1->boneCount != 0) && (anim1->keyframePoses != nullptr) &&
        (model->skeleton.boneCount != 0) && (model->skeleton.bindPose != nullptr))
    {
        // Clamp blend factor to [0, 1]
        blend = fminf(1.0f, fmaxf(0.0f, blend));

        // Ensure frame indices are valid
        if (frame0 >= anim0->keyframeCount) frame0 = anim0->keyframeCount - 1;
        if (frame1 >= anim1->keyframeCount) frame1 = anim1->keyframeCount - 1;
        if (frame0 < 0) frame0 = 0;
        if (frame1 < 0) frame1 = 0;

        // Get bone count (use minimum of all to be safe)
        unsigned int boneCount = model->skeleton.boneCount;
        if (anim0->boneCount < boneCount) boneCount = anim0->boneCount;
        if (anim1->boneCount < boneCount) boneCount = anim1->boneCount;

        // Blend each bone
        for (unsigned int boneIndex = 0; boneIndex < boneCount; boneIndex++)
        {
            // Determine blend factor for this bone
            float boneBlendFactor = blend;

            // If upper body blending is enabled, use different blend factors for upper vs lower body
            if (upperBodyBlend)
            {
                const char *boneName = model->skeleton.bones[boneIndex].name;
                const bool isUpperBody = IsUpperBodyBone(boneName);

                // Upper body: use anim1 (attack), Lower body: use anim0 (walk)
                // blend = 0.0 means full anim0 (walk), 1.0 means full anim1 (attack)
                if (isUpperBody) boneBlendFactor = blend; // Upper body: blend towards anim1 (attack)
                else boneBlendFactor = 1.0f - blend; // Lower body: blend towards anim0 (walk) - invert the blend
            }

            // Get transforms from both animations
            Transform *bindTransform = &model->skeleton.bindPose[boneIndex];
            Transform *animTransform0 = &anim0->keyframePoses[frame0][boneIndex];
            Transform *animTransform1 = &anim1->keyframePoses[frame1][boneIndex];

            // Blend the transforms
            Transform blended = {};
            blended.translation = glm::mix(animTransform0->translation, animTransform1->translation, boneBlendFactor);
            blended.rotation = glm::slerp(animTransform0->rotation, animTransform1->rotation, boneBlendFactor);
            blended.scale = glm::mix(animTransform0->scale, animTransform1->scale, boneBlendFactor);

            // Convert bind pose to matrix
            const Matrix bindMatrix = ((glm::translate(Matrix(1.0f), Vector3{bindTransform->translation.x, bindTransform->translation.y, bindTransform->translation.z})) * (((glm::mat4_cast(bindTransform->rotation)) * (glm::scale(Matrix(1.0f), Vector3{bindTransform->scale.x, bindTransform->scale.y, bindTransform->scale.z})))));

            // Convert blended transform to matrix
            const Matrix blendedMatrix = ((glm::translate(Matrix(1.0f), Vector3{blended.translation.x, blended.translation.y, blended.translation.z})) * (((glm::mat4_cast(blended.rotation)) * (glm::scale(Matrix(1.0f), Vector3{blended.scale.x, blended.scale.y, blended.scale.z})))));

            // Calculate final bone matrix (similar to UpdateModelAnimationBones)
            model->boneMatrices[boneIndex] = ((blendedMatrix) * (glm::inverse(bindMatrix)));
        }

        // CPU skinning, updates CPU buffers and uploads them to GPU (if available)
        // NOTE: Fallback in case GPU skinning is not supported or enabled
        for (int m = 0; m < model->meshCount; m++)
        {
            const Mesh mesh = model->meshes[m];
            Vector3 animVertex = {};
            Vector3 animNormal = {};
            const int vertexValuesCount = mesh.vertexCount*3;

            int boneIndex = 0;
            int boneCounter = 0;
            float boneWeight = 0.0f;
            bool bufferUpdateRequired = false; // Flag to check when anim vertex information is updated

            // Skip if missing bone data or missing anim buffers initialization
            if ((mesh.boneWeights == nullptr) || (mesh.boneIndices == nullptr) ||
                (mesh.animVertices == nullptr) || (mesh.animNormals == nullptr)) continue;

            for (int vCounter = 0; vCounter < vertexValuesCount; vCounter += 3)
            {
                mesh.animVertices[vCounter] = 0;
                mesh.animVertices[vCounter + 1] = 0;
                mesh.animVertices[vCounter + 2] = 0;
                if (mesh.animNormals != nullptr)
                {
                    mesh.animNormals[vCounter] = 0;
                    mesh.animNormals[vCounter + 1] = 0;
                    mesh.animNormals[vCounter + 2] = 0;
                }

                // Iterates over 4 bones per vertex
                for (int j = 0; j < 4; j++, boneCounter++)
                {
                    boneWeight = mesh.boneWeights[boneCounter];
                    boneIndex = mesh.boneIndices[boneCounter];

                    // Early stop when no transformation will be applied
                    if (boneWeight == 0.0f) continue;
                    animVertex = Vector3{ mesh.vertices[vCounter], mesh.vertices[vCounter + 1], mesh.vertices[vCounter + 2] };
                    animVertex = Vector3((model->boneMatrices[boneIndex])*Vector4(animVertex, 1.0f));
                    mesh.animVertices[vCounter] += animVertex.x*boneWeight;
                    mesh.animVertices[vCounter + 1] += animVertex.y*boneWeight;
                    mesh.animVertices[vCounter + 2] += animVertex.z*boneWeight;
                    bufferUpdateRequired = true;

                    // Normals processing
                    // NOTE: We use meshes.baseNormals (default normal) to calculate meshes.normals (animated normals)
                    if ((mesh.normals != nullptr) && (mesh.animNormals != nullptr ))
                    {
                        animNormal = Vector3{ mesh.normals[vCounter], mesh.normals[vCounter + 1], mesh.normals[vCounter + 2] };
                        animNormal = Vector3((glm::transpose(glm::inverse(model->boneMatrices[boneIndex])))*Vector4(animNormal, 1.0f));
                        mesh.animNormals[vCounter] += animNormal.x*boneWeight;
                        mesh.animNormals[vCounter + 1] += animNormal.y*boneWeight;
                        mesh.animNormals[vCounter + 2] += animNormal.z*boneWeight;
                    }
                }
            }

            if (bufferUpdateRequired)
            {
                // Update GPU vertex buffers with updated data (position + normals)
                rlUpdateVertexBuffer(mesh.vboId[SHADER_LOC_VERTEX_POSITION], mesh.animVertices, mesh.vertexCount*3*sizeof(float), 0);
                if (mesh.normals != nullptr) rlUpdateVertexBuffer(mesh.vboId[SHADER_LOC_VERTEX_NORMAL], mesh.animNormals, mesh.vertexCount*3*sizeof(float), 0);
            }
        }
    }
}

