/**********************************************************************************************
*
*   rmodels - Basic functions to draw 3d shapes and load and draw 3d models
*
*   CONFIGURATION:
*       #define SUPPORT_MODULE_RMODELS      1
*           rmodels module is included in the build
*
*       #define SUPPORT_FILEFORMAT_OBJ      1
*       #define SUPPORT_FILEFORMAT_MTL      1
*       #define SUPPORT_FILEFORMAT_IQM      1
*       #define SUPPORT_FILEFORMAT_GLTF     1
*       #define SUPPORT_FILEFORMAT_GLTF_WRITE   0
*       #define SUPPORT_FILEFORMAT_VOX      1
*       #define SUPPORT_FILEFORMAT_M3D      1
*           Selected desired fileformats to be supported for model data loading
*
*       #define SUPPORT_MESH_GENERATION     1
*           Support procedural mesh generation functions, uses external par_shapes.h library
*           NOTE: Some generated meshes DO NOT include generated texture coordinates
*
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2013-2026 Ramon Santamaria (@raysan5)
*
*   This software is provided "as-is", without any express or implied warranty. In no event
*   will the authors be held liable for any damages arising from the use of this software.
*
*   Permission is granted to anyone to use this software for any purpose, including commercial
*   applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*     1. The origin of this software must not be misrepresented; you must not claim that you
*     wrote the original software. If you use this software in a product, an acknowledgment
*     in the product documentation would be appreciated but is not required.
*
*     2. Altered source versions must be plainly marked as such, and must not be misrepresented
*     as being the original software.
*
*     3. This notice may not be removed or altered from any source distribution.
*
**********************************************************************************************/

#include "raylib.h"         // Declares module functions

#include "config.h"         // Defines module configuration flags

#include "logging.h"         // Levelbasiertes Logging (log(), logMessage())

#if SUPPORT_MODULE_RMODELS

#include "rlgl.h"           // OpenGL abstraction layer to OpenGL 1.1, 2.1, 3.3+ or ES2
#include "raymath.h"        // Required for: vec3, quat and mat4 functionality

#include <stdio.h>          // Required for: sprintf(), snprintf()
#include <stdlib.h>         // Required for: malloc(), calloc(), free()
#include <string.h>         // Required for: strlen(), memcmp()
#include <math.h>           // Required for: sinf(), cosf(), sqrtf(), fabsf()

#if SUPPORT_FILEFORMAT_OBJ || SUPPORT_FILEFORMAT_MTL
    #define TINYOBJ_MALLOC malloc
    #define TINYOBJ_CALLOC calloc
    #define TINYOBJ_REALLOC realloc
    #define TINYOBJ_FREE free

    #define TINYOBJ_LOADER_C_IMPLEMENTATION
    #include "external/tinyobj_loader_c.h"      // OBJ/MTL file formats loading
#endif

#if SUPPORT_FILEFORMAT_GLTF
    #define CGLTF_MALLOC malloc
    #define CGLTF_FREE free

    #define CGLTF_IMPLEMENTATION
    #include "external/cgltf.h"         // glTF file format loading
#endif
#if SUPPORT_FILEFORMAT_GLTF_WRITE
    // NOTE: No need for custom allocators, memory buffer provided
    #define CGLTF_WRITE_IMPLEMENTATION
    #include "external/cgltf_write.h"   // glTF file format writing
#endif

#if SUPPORT_FILEFORMAT_VOX
    #define VOX_MALLOC malloc
    #define VOX_CALLOC calloc
    #define VOX_REALLOC realloc
    #define VOX_FREE free

    #define VOX_LOADER_IMPLEMENTATION
    #include "external/vox_loader.h"    // VOX file format loading (MagicaVoxel)
#endif

#if SUPPORT_FILEFORMAT_M3D
    #define M3D_MALLOC malloc
    #define M3D_REALLOC realloc
    #define M3D_FREE free

    #define M3D_IMPLEMENTATION
    #include "external/m3d.h"           // Model3D file format loading
#endif

#if SUPPORT_MESH_GENERATION
    #define PAR_MALLOC(T, N) ((T *)malloc(N*sizeof(T)))
    #define PAR_CALLOC(T, N) ((T *)calloc(N*sizeof(T), 1))
    #define PAR_REALLOC(T, BUF, N) ((T *)realloc(BUF, sizeof(T)*(N)))
    #define PAR_FREE free

    #if defined(_MSC_VER)           // Disable some MSVC warning
        #pragma warning(push)
        #pragma warning(disable : 4244)
        #pragma warning(disable : 4305)
    #endif

    #define PAR_SHAPES_IMPLEMENTATION
    #include "external/par_shapes.h"    // Shapes 3d parametric generation

    #if defined(_MSC_VER)
        #pragma warning(pop)        // Disable MSVC warning suppression
    #endif
#endif

#if defined(_WIN32)
    #include <direct.h>     // Required for: _chdir() [Used in LoadOBJ()]
    #define CHDIR _chdir
#else
    #include <unistd.h>     // Required for: chdir() (POSIX) [Used in LoadOBJ()]
    #define CHDIR chdir
#endif

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
#ifndef MAX_MATERIAL_MAPS
    #define MAX_MATERIAL_MAPS       12      // Maximum number of maps supported
#endif
#ifndef MAX_MESH_VERTEX_BUFFERS
#if SUPPORT_GPU_SKINNING
    // NOTE: Two additional vertex buffers required to store bone indices and bone weights
    // WARNING: Some GPUs could not support more than 8 VBOs
    #define MAX_MESH_VERTEX_BUFFERS  9      // Maximum vertex buffers (VBO) per mesh
#else
    #define MAX_MESH_VERTEX_BUFFERS  7      // Maximum vertex buffers (VBO) per mesh
#endif
#endif
#ifndef MAX_FILEPATH_LENGTH
    #define MAX_FILEPATH_LENGTH   4096      // Maximum length for filepaths (Linux PATH_MAX default value)
#endif

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// ...

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
// ...

//----------------------------------------------------------------------------------
// Module Internal Functions Declaration
//----------------------------------------------------------------------------------
#if SUPPORT_FILEFORMAT_OBJ
static Model LoadOBJ(const char *fileName);     // Load OBJ mesh data
#endif
#if SUPPORT_FILEFORMAT_IQM
static Model LoadIQM(const char *fileName);                                            // Load IQM mesh data
static ModelAnimation *LoadModelAnimationsIQM(const char *fileName, i32 *animCount);   // Load IQM animation data
#endif
#if SUPPORT_FILEFORMAT_GLTF
static Model LoadGLTF(const char *fileName);                                           // Load GLTF mesh data
static ModelAnimation *LoadModelAnimationsGLTF(const char *fileName, i32 *animCount);  // Load GLTF animation data
#endif
#if SUPPORT_FILEFORMAT_VOX
static Model LoadVOX(const char *filename);     // Load VOX mesh data
#endif
#if SUPPORT_FILEFORMAT_M3D
static Model LoadM3D(const char *filename);                                            // Load M3D mesh data
static ModelAnimation *LoadModelAnimationsM3D(const char *fileName, i32 *animCount);   // Load M3D animation data
#endif
#if SUPPORT_FILEFORMAT_OBJ || SUPPORT_FILEFORMAT_MTL
static void ProcessMaterialsOBJ(Material *rayMaterials, tinyobj_material_t *materials, const i32 materialCount);  // Process obj materials
#endif

// Update model vertex data (positions and normals)
static void UpdateModelAnimationVertexBuffers(const Model model);

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------
// Draw a line in 3D world space
void DrawLine3D(const vec3 startPos, const vec3 endPos, const Color color)
{
    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex3f(startPos.x, startPos.y, startPos.z);
        rlVertex3f(endPos.x, endPos.y, endPos.z);
    rlEnd();
}

// Draw a point in 3D space, actually a small line
// WARNING: OpenGL ES 2.0 does not support point mode drawing
void DrawPoint3D(const vec3 position, const Color color)
{
    rlPushMatrix();
        rlTranslatef(position.x, position.y, position.z);
        rlBegin(RL_LINES);
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlVertex3f(0.0f, 0.0f, 0.0f);
            rlVertex3f(0.0f, 0.0f, 0.1f);
        rlEnd();
    rlPopMatrix();
}

// Draw a circle in 3D world space
void DrawCircle3D(const vec3 center, const f32 radius, const vec3 rotationAxis, const f32 rotationAngle, const Color color)
{
    rlPushMatrix();
        rlTranslatef(center.x, center.y, center.z);
        rlRotatef(rotationAngle, rotationAxis.x, rotationAxis.y, rotationAxis.z);

        rlBegin(RL_LINES);
            for (i32 i = 0; i < 360; i += 10)
            {
                rlColor4ub(color.r, color.g, color.b, color.a);

                rlVertex3f(sinf(DEG2RAD*static_cast<f32>(i))*radius, cosf(DEG2RAD*static_cast<f32>(i))*radius, 0.0f);
                rlVertex3f(sinf(DEG2RAD*static_cast<f32>((i + 10)))*radius, cosf(DEG2RAD*static_cast<f32>((i + 10)))*radius, 0.0f);
            }
        rlEnd();
    rlPopMatrix();
}

// Draw a color-filled triangle (vertex in counter-clockwise order!)
void DrawTriangle3D(const vec3 v1, const vec3 v2, const vec3 v3, const Color color)
{
    rlBegin(RL_TRIANGLES);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex3f(v1.x, v1.y, v1.z);
        rlVertex3f(v2.x, v2.y, v2.z);
        rlVertex3f(v3.x, v3.y, v3.z);
    rlEnd();
}

// Draw a triangle strip defined by points
void DrawTriangleStrip3D(const vec3 *points, const i32 pointCount, const Color color)
{
    if (pointCount < 3) return; // Security check

    rlBegin(RL_TRIANGLES);
        rlColor4ub(color.r, color.g, color.b, color.a);

        for (i32 i = 2; i < pointCount; i++)
        {
            if ((i%2) == 0)
            {
                rlVertex3f(points[i].x, points[i].y, points[i].z);
                rlVertex3f(points[i - 2].x, points[i - 2].y, points[i - 2].z);
                rlVertex3f(points[i - 1].x, points[i - 1].y, points[i - 1].z);
            }
            else
            {
                rlVertex3f(points[i].x, points[i].y, points[i].z);
                rlVertex3f(points[i - 1].x, points[i - 1].y, points[i - 1].z);
                rlVertex3f(points[i - 2].x, points[i - 2].y, points[i - 2].z);
            }
        }
    rlEnd();
}

// Draw cube
// NOTE: Cube position is the center position
void DrawCube(const vec3 position, const f32 width, const f32 height, const f32 length, const Color color)
{
    constexpr auto x = 0.0f;
    constexpr auto y = 0.0f;
    constexpr auto z = 0.0f;

    rlPushMatrix();
        // NOTE: Transformation is applied in inverse order (scale -> rotate -> translate)
        rlTranslatef(position.x, position.y, position.z);
        //rlRotatef(45, 0, 1, 0);
        //rlScalef(1.0f, 1.0f, 1.0f);   // NOTE: Vertices are directly scaled on definition

        rlBegin(RL_TRIANGLES);
            rlColor4ub(color.r, color.g, color.b, color.a);

            // Front face
            rlNormal3f(0.0f, 0.0f, 1.0f);
            rlVertex3f(x - width/2, y - height/2, z + length/2);  // Bottom Left
            rlVertex3f(x + width/2, y - height/2, z + length/2);  // Bottom Right
            rlVertex3f(x - width/2, y + height/2, z + length/2);  // Top Left

            rlVertex3f(x + width/2, y + height/2, z + length/2);  // Top Right
            rlVertex3f(x - width/2, y + height/2, z + length/2);  // Top Left
            rlVertex3f(x + width/2, y - height/2, z + length/2);  // Bottom Right

            // Back face
            rlNormal3f(0.0f, 0.0f, -1.0f);
            rlVertex3f(x - width/2, y - height/2, z - length/2);  // Bottom Left
            rlVertex3f(x - width/2, y + height/2, z - length/2);  // Top Left
            rlVertex3f(x + width/2, y - height/2, z - length/2);  // Bottom Right

            rlVertex3f(x + width/2, y + height/2, z - length/2);  // Top Right
            rlVertex3f(x + width/2, y - height/2, z - length/2);  // Bottom Right
            rlVertex3f(x - width/2, y + height/2, z - length/2);  // Top Left

            // Top face
            rlNormal3f(0.0f, 1.0f, 0.0f);
            rlVertex3f(x - width/2, y + height/2, z - length/2);  // Top Left
            rlVertex3f(x - width/2, y + height/2, z + length/2);  // Bottom Left
            rlVertex3f(x + width/2, y + height/2, z + length/2);  // Bottom Right

            rlVertex3f(x + width/2, y + height/2, z - length/2);  // Top Right
            rlVertex3f(x - width/2, y + height/2, z - length/2);  // Top Left
            rlVertex3f(x + width/2, y + height/2, z + length/2);  // Bottom Right

            // Bottom face
            rlNormal3f(0.0f, -1.0f, 0.0f);
            rlVertex3f(x - width/2, y - height/2, z - length/2);  // Top Left
            rlVertex3f(x + width/2, y - height/2, z + length/2);  // Bottom Right
            rlVertex3f(x - width/2, y - height/2, z + length/2);  // Bottom Left

            rlVertex3f(x + width/2, y - height/2, z - length/2);  // Top Right
            rlVertex3f(x + width/2, y - height/2, z + length/2);  // Bottom Right
            rlVertex3f(x - width/2, y - height/2, z - length/2);  // Top Left

            // Right face
            rlNormal3f(1.0f, 0.0f, 0.0f);
            rlVertex3f(x + width/2, y - height/2, z - length/2);  // Bottom Right
            rlVertex3f(x + width/2, y + height/2, z - length/2);  // Top Right
            rlVertex3f(x + width/2, y + height/2, z + length/2);  // Top Left

            rlVertex3f(x + width/2, y - height/2, z + length/2);  // Bottom Left
            rlVertex3f(x + width/2, y - height/2, z - length/2);  // Bottom Right
            rlVertex3f(x + width/2, y + height/2, z + length/2);  // Top Left

            // Left face
            rlNormal3f(-1.0f, 0.0f, 0.0f);
            rlVertex3f(x - width/2, y - height/2, z - length/2);  // Bottom Right
            rlVertex3f(x - width/2, y + height/2, z + length/2);  // Top Left
            rlVertex3f(x - width/2, y + height/2, z - length/2);  // Top Right

            rlVertex3f(x - width/2, y - height/2, z + length/2);  // Bottom Left
            rlVertex3f(x - width/2, y + height/2, z + length/2);  // Top Left
            rlVertex3f(x - width/2, y - height/2, z - length/2);  // Bottom Right
        rlEnd();
    rlPopMatrix();
}

// Draw cube (Vector version)
void DrawCubeV(const vec3 position, const vec3 size, const Color color)
{
    DrawCube(position, size.x, size.y, size.z, color);
}

// Draw cube wires
void DrawCubeWires(const vec3 position, const f32 width, const f32 height, const f32 length, const Color color)
{
    constexpr auto x = 0.0f;
    constexpr auto y = 0.0f;
    constexpr auto z = 0.0f;

    rlPushMatrix();
        rlTranslatef(position.x, position.y, position.z);

        rlBegin(RL_LINES);
            rlColor4ub(color.r, color.g, color.b, color.a);

            // Front face
            //------------------------------------------------------------------
            // Bottom line
            rlVertex3f(x - width/2, y - height/2, z + length/2);  // Bottom left
            rlVertex3f(x + width/2, y - height/2, z + length/2);  // Bottom right

            // Left line
            rlVertex3f(x + width/2, y - height/2, z + length/2);  // Bottom right
            rlVertex3f(x + width/2, y + height/2, z + length/2);  // Top right

            // Top line
            rlVertex3f(x + width/2, y + height/2, z + length/2);  // Top right
            rlVertex3f(x - width/2, y + height/2, z + length/2);  // Top left

            // Right line
            rlVertex3f(x - width/2, y + height/2, z + length/2);  // Top left
            rlVertex3f(x - width/2, y - height/2, z + length/2);  // Bottom left

            // Back face
            //------------------------------------------------------------------
            // Bottom line
            rlVertex3f(x - width/2, y - height/2, z - length/2);  // Bottom left
            rlVertex3f(x + width/2, y - height/2, z - length/2);  // Bottom right

            // Left line
            rlVertex3f(x + width/2, y - height/2, z - length/2);  // Bottom right
            rlVertex3f(x + width/2, y + height/2, z - length/2);  // Top right

            // Top line
            rlVertex3f(x + width/2, y + height/2, z - length/2);  // Top right
            rlVertex3f(x - width/2, y + height/2, z - length/2);  // Top left

            // Right line
            rlVertex3f(x - width/2, y + height/2, z - length/2);  // Top left
            rlVertex3f(x - width/2, y - height/2, z - length/2);  // Bottom left

            // Top face
            //------------------------------------------------------------------
            // Left line
            rlVertex3f(x - width/2, y + height/2, z + length/2);  // Top left front
            rlVertex3f(x - width/2, y + height/2, z - length/2);  // Top left back

            // Right line
            rlVertex3f(x + width/2, y + height/2, z + length/2);  // Top right front
            rlVertex3f(x + width/2, y + height/2, z - length/2);  // Top right back

            // Bottom face
            //------------------------------------------------------------------
            // Left line
            rlVertex3f(x - width/2, y - height/2, z + length/2);  // Top left front
            rlVertex3f(x - width/2, y - height/2, z - length/2);  // Top left back

            // Right line
            rlVertex3f(x + width/2, y - height/2, z + length/2);  // Top right front
            rlVertex3f(x + width/2, y - height/2, z - length/2);  // Top right back
        rlEnd();
    rlPopMatrix();
}

// Draw cube wires (vector version)
void DrawCubeWiresV(const vec3 position, const vec3 size, const Color color)
{
    DrawCubeWires(position, size.x, size.y, size.z, color);
}

// Draw sphere
void DrawSphere(const vec3 centerPos, const f32 radius, const Color color)
{
    DrawSphereEx(centerPos, radius, 16, 16, color);
}

// Draw sphere with defined rings and slices
void DrawSphereEx(const vec3 centerPos, const f32 radius, const i32 rings, const i32 slices, const Color color)
{
#if 0
    // Basic implementation, do not use it!
    // For a sphere with 16 rings and 16 slices it requires 8640 cos()/sin() function calls!
    // New optimized version below only requires 4 cos()/sin() calls

    rlPushMatrix();
        // NOTE: Transformation is applied in inverse order (scale -> translate)
        rlTranslatef(centerPos.x, centerPos.y, centerPos.z);
        rlScalef(radius, radius, radius);

        rlBegin(RL_TRIANGLES);
            rlColor4ub(color.r, color.g, color.b, color.a);

            for (i32 i = 0; i < (rings + 2); i++)
            {
                for (i32 j = 0; j < slices; j++)
                {
                    rlVertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*i))*sinf(DEG2RAD*(360.0f*j/slices)),
                               sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*i)),
                               cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*i))*cosf(DEG2RAD*(360.0f*j/slices)));
                    rlVertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*sinf(DEG2RAD*(360.0f*(j + 1)/slices)),
                               sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1))),
                               cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*cosf(DEG2RAD*(360.0f*(j + 1)/slices)));
                    rlVertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*sinf(DEG2RAD*(360.0f*j/slices)),
                               sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1))),
                               cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*cosf(DEG2RAD*(360.0f*j/slices)));

                    rlVertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*i))*sinf(DEG2RAD*(360.0f*j/slices)),
                               sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*i)),
                               cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*i))*cosf(DEG2RAD*(360.0f*j/slices)));
                    rlVertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i)))*sinf(DEG2RAD*(360.0f*(j + 1)/slices)),
                               sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i))),
                               cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i)))*cosf(DEG2RAD*(360.0f*(j + 1)/slices)));
                    rlVertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*sinf(DEG2RAD*(360.0f*(j + 1)/slices)),
                               sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1))),
                               cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*cosf(DEG2RAD*(360.0f*(j + 1)/slices)));
                }
            }
        rlEnd();
    rlPopMatrix();
#endif

    rlPushMatrix();
        // NOTE: Transformation is applied in inverse order (scale -> translate)
        rlTranslatef(centerPos.x, centerPos.y, centerPos.z);
        rlScalef(radius, radius, radius);

        rlBegin(RL_TRIANGLES);
            rlColor4ub(color.r, color.g, color.b, color.a);

            const f32 ringangle = DEG2RAD*(180.0f/static_cast<f32>(rings));   // Angle between latitudinal parallels
            const f32 sliceangle = DEG2RAD*(360.0f/static_cast<f32>(slices)); // Angle between longitudinal meridians

            const f32 cosring = cosf(ringangle);
            const f32 sinring = sinf(ringangle);
            const f32 cosslice = cosf(sliceangle);
            const f32 sinslice = sinf(sliceangle);

            vec3 vertices[4]{}; // Required to store face vertices
            vertices[2] = vec3{ 0, 1, 0 };
            vertices[3] = vec3{ sinring, cosring, 0 };

            for (i32 i = 0; i < rings; i++)
            {
                for (i32 j = 0; j < slices; j++)
                {
                    vertices[0] = vertices[2]; // Rotate around y axis to set up vertices for next face
                    vertices[1] = vertices[3];
                    vertices[2] = vec3{ cosslice*vertices[2].x - sinslice*vertices[2].z, vertices[2].y, sinslice*vertices[2].x + cosslice*vertices[2].z }; // Rotation matrix around y axis
                    vertices[3] = vec3{ cosslice*vertices[3].x - sinslice*vertices[3].z, vertices[3].y, sinslice*vertices[3].x + cosslice*vertices[3].z };

                    rlNormal3f(vertices[0].x, vertices[0].y, vertices[0].z);
                    rlVertex3f(vertices[0].x, vertices[0].y, vertices[0].z);
                    rlNormal3f(vertices[3].x, vertices[3].y, vertices[3].z);
                    rlVertex3f(vertices[3].x, vertices[3].y, vertices[3].z);
                    rlNormal3f(vertices[1].x, vertices[1].y, vertices[1].z);
                    rlVertex3f(vertices[1].x, vertices[1].y, vertices[1].z);

                    rlNormal3f(vertices[0].x, vertices[0].y, vertices[0].z);
                    rlVertex3f(vertices[0].x, vertices[0].y, vertices[0].z);
                    rlNormal3f(vertices[2].x, vertices[2].y, vertices[2].z);
                    rlVertex3f(vertices[2].x, vertices[2].y, vertices[2].z);
                    rlNormal3f(vertices[3].x, vertices[3].y, vertices[3].z);
                    rlVertex3f(vertices[3].x, vertices[3].y, vertices[3].z);
                }

                vertices[2] = vertices[3];                                                                                                          // Rotate around z axis to set up  starting vertices for next ring
                vertices[3] = vec3{ cosring*vertices[3].x + sinring*vertices[3].y, -sinring*vertices[3].x + cosring*vertices[3].y, vertices[3].z }; // Rotation matrix around z axis
            }
        rlEnd();
    rlPopMatrix();
}

// Draw sphere wires
void DrawSphereWires(const vec3 centerPos, const f32 radius, const i32 rings, const i32 slices, const Color color)
{
    rlPushMatrix();
        // NOTE: Transformation is applied in inverse order (scale -> translate)
        rlTranslatef(centerPos.x, centerPos.y, centerPos.z);
        rlScalef(radius, radius, radius);

        rlBegin(RL_LINES);
            rlColor4ub(color.r, color.g, color.b, color.a);

            const f32 ringangle = DEG2RAD*(180.0f/static_cast<f32>(rings));   // Angle between latitudinal parallels
            const f32 sliceangle = DEG2RAD*(360.0f/static_cast<f32>(slices)); // Angle between longitudinal meridians

            const f32 cosring = cosf(ringangle);
            const f32 sinring = sinf(ringangle);
            const f32 cosslice = cosf(sliceangle);
            const f32 sinslice = sinf(sliceangle);

            vec3 vertices[4]{}; // Required to store face vertices
            vertices[2] = vec3{ 0, 1, 0 };
            vertices[3] = vec3{ sinring, cosring, 0 };

            for (i32 i = 0; i < rings; i++)
            {
                for (i32 j = 0; j < slices; j++)
                {
                    vertices[0] = vertices[2]; // Rotate around y axis to set up vertices for next face
                    vertices[1] = vertices[3];
                    vertices[2] = vec3{ cosslice*vertices[2].x - sinslice*vertices[2].z, vertices[2].y, sinslice*vertices[2].x + cosslice*vertices[2].z }; // Rotation matrix around y axis
                    vertices[3] = vec3{ cosslice*vertices[3].x - sinslice*vertices[3].z, vertices[3].y, sinslice*vertices[3].x + cosslice*vertices[3].z };

                    // Longitude Lines
                    rlVertex3f(vertices[0].x, vertices[0].y, vertices[0].z);
                    rlVertex3f(vertices[1].x, vertices[1].y, vertices[1].z);

                    // Latitude Lines
                    rlVertex3f(vertices[0].x, vertices[0].y, vertices[0].z);
                    rlVertex3f(vertices[2].x, vertices[2].y, vertices[2].z);

                    // Diagonal Lines
                    rlVertex3f(vertices[0].x, vertices[0].y, vertices[0].z);
                    rlVertex3f(vertices[3].x, vertices[3].y, vertices[3].z);
                }

                vertices[2] = vertices[3];                                                                                                          // Rotate around z axis to set up  starting vertices for next ring
                vertices[3] = vec3{ cosring*vertices[3].x + sinring*vertices[3].y, -sinring*vertices[3].x + cosring*vertices[3].y, vertices[3].z }; // Rotation matrix around z axis
            }
        rlEnd();
    rlPopMatrix();
}

// Draw a cylinder
// NOTE: It could be also used for pyramid and cone
void DrawCylinder(const vec3 position, const f32 radiusTop, const f32 radiusBottom, const f32 height, i32 sides, const Color color)
{
    if (sides < 3) sides = 3;

    const f32 angleStep = 360.0f/static_cast<f32>(sides);

    rlPushMatrix();
        rlTranslatef(position.x, position.y, position.z);

        rlBegin(RL_TRIANGLES);
            rlColor4ub(color.r, color.g, color.b, color.a);

            if (radiusTop > 0)
            {
                // Draw Body -------------------------------------------------------------------------------------
                for (i32 i = 0; i < sides; i++)
                {
                    rlVertex3f(sinf(DEG2RAD*static_cast<f32>(i)*angleStep)*radiusBottom, 0, cosf(DEG2RAD*static_cast<f32>(i)*angleStep)*radiusBottom);         //Bottom Left
                    rlVertex3f(sinf(DEG2RAD*static_cast<f32>((i+1))*angleStep)*radiusBottom, 0, cosf(DEG2RAD*static_cast<f32>((i+1))*angleStep)*radiusBottom); //Bottom Right
                    rlVertex3f(sinf(DEG2RAD*static_cast<f32>((i+1))*angleStep)*radiusTop, height, cosf(DEG2RAD*static_cast<f32>((i+1))*angleStep)*radiusTop);  //Top Right

                    rlVertex3f(sinf(DEG2RAD*static_cast<f32>(i)*angleStep)*radiusTop, height, cosf(DEG2RAD*static_cast<f32>(i)*angleStep)*radiusTop);         //Top Left
                    rlVertex3f(sinf(DEG2RAD*static_cast<f32>(i)*angleStep)*radiusBottom, 0, cosf(DEG2RAD*static_cast<f32>(i)*angleStep)*radiusBottom);        //Bottom Left
                    rlVertex3f(sinf(DEG2RAD*static_cast<f32>((i+1))*angleStep)*radiusTop, height, cosf(DEG2RAD*static_cast<f32>((i+1))*angleStep)*radiusTop); //Top Right
                }

                // Draw Cap --------------------------------------------------------------------------------------
                for (i32 i = 0; i < sides; i++)
                {
                    rlVertex3f(0, height, 0);
                    rlVertex3f(sinf(DEG2RAD*static_cast<f32>(i)*angleStep)*radiusTop, height, cosf(DEG2RAD*static_cast<f32>(i)*angleStep)*radiusTop);
                    rlVertex3f(sinf(DEG2RAD*static_cast<f32>((i+1))*angleStep)*radiusTop, height, cosf(DEG2RAD*static_cast<f32>((i+1))*angleStep)*radiusTop);
                }
            }
            else
            {
                // Draw Cone -------------------------------------------------------------------------------------
                for (i32 i = 0; i < sides; i++)
                {
                    rlVertex3f(0, height, 0);
                    rlVertex3f(sinf(DEG2RAD*static_cast<f32>(i)*angleStep)*radiusBottom, 0, cosf(DEG2RAD*static_cast<f32>(i)*angleStep)*radiusBottom);
                    rlVertex3f(sinf(DEG2RAD*static_cast<f32>((i+1))*angleStep)*radiusBottom, 0, cosf(DEG2RAD*static_cast<f32>((i+1))*angleStep)*radiusBottom);
                }
            }

            // Draw Base -----------------------------------------------------------------------------------------
            for (i32 i = 0; i < sides; i++)
            {
                rlVertex3f(0, 0, 0);
                rlVertex3f(sinf(DEG2RAD*static_cast<f32>((i+1))*angleStep)*radiusBottom, 0, cosf(DEG2RAD*static_cast<f32>((i+1))*angleStep)*radiusBottom);
                rlVertex3f(sinf(DEG2RAD*static_cast<f32>(i)*angleStep)*radiusBottom, 0, cosf(DEG2RAD*static_cast<f32>(i)*angleStep)*radiusBottom);
            }

        rlEnd();
    rlPopMatrix();
}

// Draw a cylinder with base at startPos and top at endPos
// NOTE: It could be also used for pyramid and cone
void DrawCylinderEx(const vec3 startPos, const vec3 endPos, const f32 startRadius, const f32 endRadius, i32 sides, const Color color)
{
    if (sides < 3) sides = 3;

    const auto direction = vec3{ endPos.x - startPos.x, endPos.y - startPos.y, endPos.z - startPos.z };
    if ((direction.x == 0) && (direction.y == 0) && (direction.z == 0)) return; // Security check

    // Construct a basis of the base and the top face:
    const vec3 b1 = glm::normalize(Vector3Perpendicular(direction));
    const vec3 b2 = glm::normalize(glm::cross(b1, direction));

    const f32 baseAngle = (2.0f*PI)/static_cast<f32>(sides);

    rlBegin(RL_TRIANGLES);
        rlColor4ub(color.r, color.g, color.b, color.a);

        for (i32 i = 0; i < sides; i++)
        {
            // Compute the four vertices
            const f32 s1 = sinf(baseAngle*static_cast<f32>((i + 0)))*startRadius;
            const f32 c1 = cosf(baseAngle*static_cast<f32>((i + 0)))*startRadius;
            const auto w1 = vec3{ startPos.x + s1*b1.x + c1*b2.x, startPos.y + s1*b1.y + c1*b2.y, startPos.z + s1*b1.z + c1*b2.z };
            const f32 s2 = sinf(baseAngle*static_cast<f32>((i + 1)))*startRadius;
            const f32 c2 = cosf(baseAngle*static_cast<f32>((i + 1)))*startRadius;
            const auto w2 = vec3{ startPos.x + s2*b1.x + c2*b2.x, startPos.y + s2*b1.y + c2*b2.y, startPos.z + s2*b1.z + c2*b2.z };
            const f32 s3 = sinf(baseAngle*static_cast<f32>((i + 0)))*endRadius;
            const f32 c3 = cosf(baseAngle*static_cast<f32>((i + 0)))*endRadius;
            const auto w3 = vec3{ endPos.x + s3*b1.x + c3*b2.x, endPos.y + s3*b1.y + c3*b2.y, endPos.z + s3*b1.z + c3*b2.z };
            const f32 s4 = sinf(baseAngle*static_cast<f32>((i + 1)))*endRadius;
            const f32 c4 = cosf(baseAngle*static_cast<f32>((i + 1)))*endRadius;
            const auto w4 = vec3{ endPos.x + s4*b1.x + c4*b2.x, endPos.y + s4*b1.y + c4*b2.y, endPos.z + s4*b1.z + c4*b2.z };

            if (startRadius > 0)
            {
                rlVertex3f(startPos.x, startPos.y, startPos.z); // |
                rlVertex3f(w2.x, w2.y, w2.z);                   // T0
                rlVertex3f(w1.x, w1.y, w1.z);                   // |
            }
                                                                //          w2 x.-----------x startPos
            rlVertex3f(w1.x, w1.y, w1.z);                       // |           |\'.  T0    /
            rlVertex3f(w2.x, w2.y, w2.z);                       // T1          | \ '.     /
            rlVertex3f(w3.x, w3.y, w3.z);                       // |           |T \  '.  /
                                                                //             | 2 \ T 'x w1
            rlVertex3f(w2.x, w2.y, w2.z);                       // |        w4 x.---\-1-|---x endPos
            rlVertex3f(w4.x, w4.y, w4.z);                       // T2            '.  \  |T3/
            rlVertex3f(w3.x, w3.y, w3.z);                       // |               '. \ | /
                                                                //                   '.\|/
            if (endRadius > 0)                                  //                     'x w3
            {
                rlVertex3f(endPos.x, endPos.y, endPos.z);       // |
                rlVertex3f(w3.x, w3.y, w3.z);                   // T3
                rlVertex3f(w4.x, w4.y, w4.z);                   // |
            }                                                   //
        }
    rlEnd();
}

// Draw a wired cylinder
// NOTE: It could be also used for pyramid and cone
void DrawCylinderWires(const vec3 position, const f32 radiusTop, const f32 radiusBottom, const f32 height, i32 sides, const Color color)
{
    if (sides < 3) sides = 3;

    const f32 angleStep = 360.0f/static_cast<f32>(sides);

    rlPushMatrix();
        rlTranslatef(position.x, position.y, position.z);

        rlBegin(RL_LINES);
            rlColor4ub(color.r, color.g, color.b, color.a);

            for (i32 i = 0; i < sides; i++)
            {
                rlVertex3f(sinf(DEG2RAD*static_cast<f32>(i)*angleStep)*radiusBottom, 0, cosf(DEG2RAD*static_cast<f32>(i)*angleStep)*radiusBottom);
                rlVertex3f(sinf(DEG2RAD*static_cast<f32>((i+1))*angleStep)*radiusBottom, 0, cosf(DEG2RAD*static_cast<f32>((i+1))*angleStep)*radiusBottom);

                rlVertex3f(sinf(DEG2RAD*static_cast<f32>((i+1))*angleStep)*radiusBottom, 0, cosf(DEG2RAD*static_cast<f32>((i+1))*angleStep)*radiusBottom);
                rlVertex3f(sinf(DEG2RAD*static_cast<f32>((i+1))*angleStep)*radiusTop, height, cosf(DEG2RAD*static_cast<f32>((i+1))*angleStep)*radiusTop);

                rlVertex3f(sinf(DEG2RAD*static_cast<f32>((i+1))*angleStep)*radiusTop, height, cosf(DEG2RAD*static_cast<f32>((i+1))*angleStep)*radiusTop);
                rlVertex3f(sinf(DEG2RAD*static_cast<f32>(i)*angleStep)*radiusTop, height, cosf(DEG2RAD*static_cast<f32>(i)*angleStep)*radiusTop);

                rlVertex3f(sinf(DEG2RAD*static_cast<f32>(i)*angleStep)*radiusTop, height, cosf(DEG2RAD*static_cast<f32>(i)*angleStep)*radiusTop);
                rlVertex3f(sinf(DEG2RAD*static_cast<f32>(i)*angleStep)*radiusBottom, 0, cosf(DEG2RAD*static_cast<f32>(i)*angleStep)*radiusBottom);
            }
        rlEnd();
    rlPopMatrix();
}

// Draw a wired cylinder with base at startPos and top at endPos
// NOTE: It could be also used for pyramid and cone
void DrawCylinderWiresEx(const vec3 startPos, const vec3 endPos, const f32 startRadius, const f32 endRadius, i32 slices, const Color color)
{
    if (slices < 3) slices = 3;

    const auto direction = vec3{ endPos.x - startPos.x, endPos.y - startPos.y, endPos.z - startPos.z };
    if ((direction.x == 0) && (direction.y == 0) && (direction.z == 0)) return; // Security check

    // Construct a basis of the base and the top face:
    const vec3 b1 = glm::normalize(Vector3Perpendicular(direction));
    const vec3 b2 = glm::normalize(glm::cross(b1, direction));

    const f32 baseAngle = (2.0f*PI)/static_cast<f32>(slices);

    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);

        for (i32 i = 0; i < slices; i++)
        {
            // Compute the four vertices
            const f32 s1 = sinf(baseAngle*static_cast<f32>((i + 0)))*startRadius;
            const f32 c1 = cosf(baseAngle*static_cast<f32>((i + 0)))*startRadius;
            const auto w1 = vec3{ startPos.x + s1*b1.x + c1*b2.x, startPos.y + s1*b1.y + c1*b2.y, startPos.z + s1*b1.z + c1*b2.z };
            const f32 s2 = sinf(baseAngle*static_cast<f32>((i + 1)))*startRadius;
            const f32 c2 = cosf(baseAngle*static_cast<f32>((i + 1)))*startRadius;
            const auto w2 = vec3{ startPos.x + s2*b1.x + c2*b2.x, startPos.y + s2*b1.y + c2*b2.y, startPos.z + s2*b1.z + c2*b2.z };
            const f32 s3 = sinf(baseAngle*static_cast<f32>((i + 0)))*endRadius;
            const f32 c3 = cosf(baseAngle*static_cast<f32>((i + 0)))*endRadius;
            const auto w3 = vec3{ endPos.x + s3*b1.x + c3*b2.x, endPos.y + s3*b1.y + c3*b2.y, endPos.z + s3*b1.z + c3*b2.z };
            const f32 s4 = sinf(baseAngle*static_cast<f32>((i + 1)))*endRadius;
            const f32 c4 = cosf(baseAngle*static_cast<f32>((i + 1)))*endRadius;
            const auto w4 = vec3{ endPos.x + s4*b1.x + c4*b2.x, endPos.y + s4*b1.y + c4*b2.y, endPos.z + s4*b1.z + c4*b2.z };

            rlVertex3f(w1.x, w1.y, w1.z);
            rlVertex3f(w2.x, w2.y, w2.z);

            rlVertex3f(w1.x, w1.y, w1.z);
            rlVertex3f(w3.x, w3.y, w3.z);

            rlVertex3f(w3.x, w3.y, w3.z);
            rlVertex3f(w4.x, w4.y, w4.z);
        }
    rlEnd();
}

// Draw a capsule with the center of its sphere caps at startPos and endPos
void DrawCapsule(const vec3 startPos, const vec3 endPos, const f32 radius, i32 rings, i32 slices, const Color color)
{
    if (slices < 3) slices = 3;
    if (rings  < 1) rings  = 1;
    auto direction = vec3{ endPos.x - startPos.x, endPos.y - startPos.y, endPos.z - startPos.z };

    // draw a sphere if start and end points are the same
    const bool sphereCase = (direction.x == 0) && (direction.y == 0) && (direction.z == 0);
    if (sphereCase) direction = vec3{0.0f, 1.0f, 0.0f};

    // Construct a basis of the base and the caps:
    vec3 b0 = glm::normalize(direction);
    const vec3 b1 = glm::normalize(Vector3Perpendicular(direction));
    const vec3 b2 = glm::normalize(glm::cross(b1, direction));
    vec3 capCenter = endPos;

    const f32 baseSliceAngle = (2.0f*PI)/static_cast<f32>(slices);
    const f32 baseRingAngle  = PI*0.5f/static_cast<f32>(rings);

    rlBegin(RL_TRIANGLES);
        rlColor4ub(color.r, color.g, color.b, color.a);

        // render both caps
        for (i32 c = 0; c < 2; c++)
        {
            for (i32 i = 0; i < rings; i++)
            {
                for (i32 j = 0; j < slices; j++)
                {
                    // Building up the rings from capCenter in the direction of the 'direction' vector computed earlier

                    // Compute the four vertices
                    const f32 ringSin1 = sinf(baseSliceAngle*static_cast<f32>((j + 0)))*cosf(baseRingAngle*static_cast<f32>(( i + 0 )));
                    const f32 ringCos1 = cosf(baseSliceAngle*static_cast<f32>((j + 0)))*cosf(baseRingAngle*static_cast<f32>(( i + 0 )));
                    const vec3 w1 = vec3{
                        capCenter.x + (sinf(baseRingAngle*static_cast<f32>(( i + 0 )))*b0.x + ringSin1*b1.x + ringCos1*b2.x)*radius,
                        capCenter.y + (sinf(baseRingAngle*static_cast<f32>(( i + 0 )))*b0.y + ringSin1*b1.y + ringCos1*b2.y)*radius,
                        capCenter.z + (sinf(baseRingAngle*static_cast<f32>(( i + 0 )))*b0.z + ringSin1*b1.z + ringCos1*b2.z)*radius
                    };
                    const f32 ringSin2 = sinf(baseSliceAngle*static_cast<f32>((j + 1)))*cosf(baseRingAngle*static_cast<f32>(( i + 0 )));
                    const f32 ringCos2 = cosf(baseSliceAngle*static_cast<f32>((j + 1)))*cosf(baseRingAngle*static_cast<f32>(( i + 0 )));
                    const vec3 w2 = vec3{
                        capCenter.x + (sinf(baseRingAngle*static_cast<f32>(( i + 0 )))*b0.x + ringSin2*b1.x + ringCos2*b2.x)*radius,
                        capCenter.y + (sinf(baseRingAngle*static_cast<f32>(( i + 0 )))*b0.y + ringSin2*b1.y + ringCos2*b2.y)*radius,
                        capCenter.z + (sinf(baseRingAngle*static_cast<f32>(( i + 0 )))*b0.z + ringSin2*b1.z + ringCos2*b2.z)*radius
                    };

                    const f32 ringSin3 = sinf(baseSliceAngle*static_cast<f32>((j + 0)))*cosf(baseRingAngle*static_cast<f32>(( i + 1 )));
                    const f32 ringCos3 = cosf(baseSliceAngle*static_cast<f32>((j + 0)))*cosf(baseRingAngle*static_cast<f32>(( i + 1 )));
                    const vec3 w3 = vec3{
                        capCenter.x + (sinf(baseRingAngle*static_cast<f32>(( i + 1 )))*b0.x + ringSin3*b1.x + ringCos3*b2.x)*radius,
                        capCenter.y + (sinf(baseRingAngle*static_cast<f32>(( i + 1 )))*b0.y + ringSin3*b1.y + ringCos3*b2.y)*radius,
                        capCenter.z + (sinf(baseRingAngle*static_cast<f32>(( i + 1 )))*b0.z + ringSin3*b1.z + ringCos3*b2.z)*radius
                    };
                    const f32 ringSin4 = sinf(baseSliceAngle*static_cast<f32>((j + 1)))*cosf(baseRingAngle*static_cast<f32>(( i + 1 )));
                    const f32 ringCos4 = cosf(baseSliceAngle*static_cast<f32>((j + 1)))*cosf(baseRingAngle*static_cast<f32>(( i + 1 )));
                    const vec3 w4 = vec3{
                        capCenter.x + (sinf(baseRingAngle*static_cast<f32>(( i + 1 )))*b0.x + ringSin4*b1.x + ringCos4*b2.x)*radius,
                        capCenter.y + (sinf(baseRingAngle*static_cast<f32>(( i + 1 )))*b0.y + ringSin4*b1.y + ringCos4*b2.y)*radius,
                        capCenter.z + (sinf(baseRingAngle*static_cast<f32>(( i + 1 )))*b0.z + ringSin4*b1.z + ringCos4*b2.z)*radius
                    };

                    // Make sure cap triangle normals are facing outwards
                    if (c == 0)
                    {
                        rlVertex3f(w1.x, w1.y, w1.z);
                        rlVertex3f(w2.x, w2.y, w2.z);
                        rlVertex3f(w3.x, w3.y, w3.z);

                        rlVertex3f(w2.x, w2.y, w2.z);
                        rlVertex3f(w4.x, w4.y, w4.z);
                        rlVertex3f(w3.x, w3.y, w3.z);
                    }
                    else
                    {
                        rlVertex3f(w1.x, w1.y, w1.z);
                        rlVertex3f(w3.x, w3.y, w3.z);
                        rlVertex3f(w2.x, w2.y, w2.z);

                        rlVertex3f(w2.x, w2.y, w2.z);
                        rlVertex3f(w3.x, w3.y, w3.z);
                        rlVertex3f(w4.x, w4.y, w4.z);
                    }
                }
            }
            capCenter = startPos;
            b0 = ((b0) * (-1.0f));
        }
        // render middle
        if (!sphereCase)
        {
            for (i32 j = 0; j < slices; j++)
            {
                // compute the four vertices
                const f32 ringSin1 = sinf(baseSliceAngle*static_cast<f32>((j + 0)))*radius;
                const f32 ringCos1 = cosf(baseSliceAngle*static_cast<f32>((j + 0)))*radius;
                const vec3 w1 = {
                    startPos.x + ringSin1*b1.x + ringCos1*b2.x,
                    startPos.y + ringSin1*b1.y + ringCos1*b2.y,
                    startPos.z + ringSin1*b1.z + ringCos1*b2.z
                };
                const f32 ringSin2 = sinf(baseSliceAngle*static_cast<f32>((j + 1)))*radius;
                const f32 ringCos2 = cosf(baseSliceAngle*static_cast<f32>((j + 1)))*radius;
                const vec3 w2 = {
                    startPos.x + ringSin2*b1.x + ringCos2*b2.x,
                    startPos.y + ringSin2*b1.y + ringCos2*b2.y,
                    startPos.z + ringSin2*b1.z + ringCos2*b2.z
                };

                const f32 ringSin3 = sinf(baseSliceAngle*static_cast<f32>((j + 0)))*radius;
                const f32 ringCos3 = cosf(baseSliceAngle*static_cast<f32>((j + 0)))*radius;
                const vec3 w3 = {
                    endPos.x + ringSin3*b1.x + ringCos3*b2.x,
                    endPos.y + ringSin3*b1.y + ringCos3*b2.y,
                    endPos.z + ringSin3*b1.z + ringCos3*b2.z
                };
                const f32 ringSin4 = sinf(baseSliceAngle*static_cast<f32>((j + 1)))*radius;
                const f32 ringCos4 = cosf(baseSliceAngle*static_cast<f32>((j + 1)))*radius;
                const vec3 w4 = {
                    endPos.x + ringSin4*b1.x + ringCos4*b2.x,
                    endPos.y + ringSin4*b1.y + ringCos4*b2.y,
                    endPos.z + ringSin4*b1.z + ringCos4*b2.z
                };
                                                                        //          w2 x.-----------x startPos
                rlVertex3f(w1.x, w1.y, w1.z);                         // |           |\'.  T0    /
                rlVertex3f(w2.x, w2.y, w2.z);                         // T1          | \ '.     /
                rlVertex3f(w3.x, w3.y, w3.z);                         // |           |T \  '.  /
                                                                        //             | 2 \ T 'x w1
                rlVertex3f(w2.x, w2.y, w2.z);                         // |        w4 x.---\-1-|---x endPos
                rlVertex3f(w4.x, w4.y, w4.z);                         // T2            '.  \  |T3/
                rlVertex3f(w3.x, w3.y, w3.z);                         // |               '. \ | /
                                                                        //                   '.\|/
                                                                        //                   'x w3
            }
        }
    rlEnd();
}

// Draw capsule wires with the center of its sphere caps at startPos and endPos
void DrawCapsuleWires(const vec3 startPos, const vec3 endPos, const f32 radius, i32 rings, i32 slices, const Color color)
{
    if (slices < 3) slices = 3;
    if (rings  < 1) rings = 1;
    auto direction = vec3{ endPos.x - startPos.x, endPos.y - startPos.y, endPos.z - startPos.z };

    // draw a sphere if start and end points are the same
    const bool sphereCase = (direction.x == 0) && (direction.y == 0) && (direction.z == 0);
    if (sphereCase) direction = vec3{0.0f, 1.0f, 0.0f};

    // Construct a basis of the base and the caps:
    vec3 b0 = glm::normalize(direction);
    const vec3 b1 = glm::normalize(Vector3Perpendicular(direction));
    const vec3 b2 = glm::normalize(glm::cross(b1, direction));
    vec3 capCenter = endPos;

    const f32 baseSliceAngle = (2.0f*PI)/static_cast<f32>(slices);
    const f32 baseRingAngle  = PI*0.5f/static_cast<f32>(rings);

    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);

        // render both caps
        for (i32 c = 0; c < 2; c++)
        {
            for (i32 i = 0; i < rings; i++)
            {
                for (i32 j = 0; j < slices; j++)
                {
                    // Building up the rings from capCenter in the direction of the 'direction' vector computed earlier

                    // Compute the four vertices
                    const f32 ringSin1 = sinf(baseSliceAngle*static_cast<f32>((j + 0)))*cosf(baseRingAngle*static_cast<f32>(( i + 0 )));
                    const f32 ringCos1 = cosf(baseSliceAngle*static_cast<f32>((j + 0)))*cosf(baseRingAngle*static_cast<f32>(( i + 0 )));
                    const vec3 w1 = vec3{
                        capCenter.x + (sinf(baseRingAngle*static_cast<f32>(( i + 0 )))*b0.x + ringSin1*b1.x + ringCos1*b2.x)*radius,
                        capCenter.y + (sinf(baseRingAngle*static_cast<f32>(( i + 0 )))*b0.y + ringSin1*b1.y + ringCos1*b2.y)*radius,
                        capCenter.z + (sinf(baseRingAngle*static_cast<f32>(( i + 0 )))*b0.z + ringSin1*b1.z + ringCos1*b2.z)*radius
                    };
                    const f32 ringSin2 = sinf(baseSliceAngle*static_cast<f32>((j + 1)))*cosf(baseRingAngle*static_cast<f32>(( i + 0 )));
                    const f32 ringCos2 = cosf(baseSliceAngle*static_cast<f32>((j + 1)))*cosf(baseRingAngle*static_cast<f32>(( i + 0 )));
                    const vec3 w2 = vec3{
                        capCenter.x + (sinf(baseRingAngle*static_cast<f32>(( i + 0 )))*b0.x + ringSin2*b1.x + ringCos2*b2.x)*radius,
                        capCenter.y + (sinf(baseRingAngle*static_cast<f32>(( i + 0 )))*b0.y + ringSin2*b1.y + ringCos2*b2.y)*radius,
                        capCenter.z + (sinf(baseRingAngle*static_cast<f32>(( i + 0 )))*b0.z + ringSin2*b1.z + ringCos2*b2.z)*radius
                    };

                    const f32 ringSin3 = sinf(baseSliceAngle*static_cast<f32>((j + 0)))*cosf(baseRingAngle*static_cast<f32>(( i + 1 )));
                    const f32 ringCos3 = cosf(baseSliceAngle*static_cast<f32>((j + 0)))*cosf(baseRingAngle*static_cast<f32>(( i + 1 )));
                    const vec3 w3 = vec3{
                        capCenter.x + (sinf(baseRingAngle*static_cast<f32>(( i + 1 )))*b0.x + ringSin3*b1.x + ringCos3*b2.x)*radius,
                        capCenter.y + (sinf(baseRingAngle*static_cast<f32>(( i + 1 )))*b0.y + ringSin3*b1.y + ringCos3*b2.y)*radius,
                        capCenter.z + (sinf(baseRingAngle*static_cast<f32>(( i + 1 )))*b0.z + ringSin3*b1.z + ringCos3*b2.z)*radius
                    };
                    const f32 ringSin4 = sinf(baseSliceAngle*static_cast<f32>((j + 1)))*cosf(baseRingAngle*static_cast<f32>(( i + 1 )));
                    const f32 ringCos4 = cosf(baseSliceAngle*static_cast<f32>((j + 1)))*cosf(baseRingAngle*static_cast<f32>(( i + 1 )));
                    const vec3 w4 = vec3{
                        capCenter.x + (sinf(baseRingAngle*static_cast<f32>(( i + 1 )))*b0.x + ringSin4*b1.x + ringCos4*b2.x)*radius,
                        capCenter.y + (sinf(baseRingAngle*static_cast<f32>(( i + 1 )))*b0.y + ringSin4*b1.y + ringCos4*b2.y)*radius,
                        capCenter.z + (sinf(baseRingAngle*static_cast<f32>(( i + 1 )))*b0.z + ringSin4*b1.z + ringCos4*b2.z)*radius
                    };

                    rlVertex3f(w1.x, w1.y, w1.z);
                    rlVertex3f(w2.x, w2.y, w2.z);

                    rlVertex3f(w2.x, w2.y, w2.z);
                    rlVertex3f(w3.x, w3.y, w3.z);

                    rlVertex3f(w1.x, w1.y, w1.z);
                    rlVertex3f(w3.x, w3.y, w3.z);

                    rlVertex3f(w2.x, w2.y, w2.z);
                    rlVertex3f(w4.x, w4.y, w4.z);

                    rlVertex3f(w3.x, w3.y, w3.z);
                    rlVertex3f(w4.x, w4.y, w4.z);
                }
            }
            capCenter = startPos;
            b0 = ((b0) * (-1.0f));
        }
        // render middle
        if (!sphereCase)
        {
            for (i32 j = 0; j < slices; j++)
            {
                // compute the four vertices
                const f32 ringSin1 = sinf(baseSliceAngle*static_cast<f32>((j + 0)))*radius;
                const f32 ringCos1 = cosf(baseSliceAngle*static_cast<f32>((j + 0)))*radius;
                const vec3 w1 = {
                    startPos.x + ringSin1*b1.x + ringCos1*b2.x,
                    startPos.y + ringSin1*b1.y + ringCos1*b2.y,
                    startPos.z + ringSin1*b1.z + ringCos1*b2.z
                };
                const f32 ringSin2 = sinf(baseSliceAngle*static_cast<f32>((j + 1)))*radius;
                const f32 ringCos2 = cosf(baseSliceAngle*static_cast<f32>((j + 1)))*radius;
                const vec3 w2 = {
                    startPos.x + ringSin2*b1.x + ringCos2*b2.x,
                    startPos.y + ringSin2*b1.y + ringCos2*b2.y,
                    startPos.z + ringSin2*b1.z + ringCos2*b2.z
                };

                const f32 ringSin3 = sinf(baseSliceAngle*static_cast<f32>((j + 0)))*radius;
                const f32 ringCos3 = cosf(baseSliceAngle*static_cast<f32>((j + 0)))*radius;
                const vec3 w3 = {
                    endPos.x + ringSin3*b1.x + ringCos3*b2.x,
                    endPos.y + ringSin3*b1.y + ringCos3*b2.y,
                    endPos.z + ringSin3*b1.z + ringCos3*b2.z
                };
                const f32 ringSin4 = sinf(baseSliceAngle*static_cast<f32>((j + 1)))*radius;
                const f32 ringCos4 = cosf(baseSliceAngle*static_cast<f32>((j + 1)))*radius;
                const vec3 w4 = {
                    endPos.x + ringSin4*b1.x + ringCos4*b2.x,
                    endPos.y + ringSin4*b1.y + ringCos4*b2.y,
                    endPos.z + ringSin4*b1.z + ringCos4*b2.z
                };

                rlVertex3f(w1.x, w1.y, w1.z);
                rlVertex3f(w3.x, w3.y, w3.z);

                rlVertex3f(w2.x, w2.y, w2.z);
                rlVertex3f(w4.x, w4.y, w4.z);

                rlVertex3f(w2.x, w2.y, w2.z);
                rlVertex3f(w3.x, w3.y, w3.z);
            }
        }
    rlEnd();
}

// Draw a plane
void DrawPlane(const vec3 centerPos, const vec2 size, const Color color)
{
    // NOTE: Plane is always created on XZ ground
    rlPushMatrix();
        rlTranslatef(centerPos.x, centerPos.y, centerPos.z);
        rlScalef(size.x, 1.0f, size.y);

        rlBegin(RL_QUADS);
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlNormal3f(0.0f, 1.0f, 0.0f);

            rlVertex3f(-0.5f, 0.0f, -0.5f);
            rlVertex3f(-0.5f, 0.0f, 0.5f);
            rlVertex3f(0.5f, 0.0f, 0.5f);
            rlVertex3f(0.5f, 0.0f, -0.5f);
        rlEnd();
    rlPopMatrix();
}

// Draw a ray line
void DrawRay(const Ray ray, const Color color)
{
    constexpr f32 scale = 10000;

    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlColor4ub(color.r, color.g, color.b, color.a);

        rlVertex3f(ray.position.x, ray.position.y, ray.position.z);
        rlVertex3f(ray.position.x + ray.direction.x*scale, ray.position.y + ray.direction.y*scale, ray.position.z + ray.direction.z*scale);
    rlEnd();
}

// Draw a grid centered at (0, 0, 0)
void DrawGrid(const i32 slices, const f32 spacing)
{
    const i32 halfSlices = slices/2;

    rlBegin(RL_LINES);
        for (i32 i = -halfSlices; i <= halfSlices; i++)
        {
            if (i == 0)
            {
                rlColor3f(0.5f, 0.5f, 0.5f);
            }
            else
            {
                rlColor3f(0.75f, 0.75f, 0.75f);
            }

            rlVertex3f(static_cast<f32>(i)*spacing, 0.0f, static_cast<f32>(-halfSlices)*spacing);
            rlVertex3f(static_cast<f32>(i)*spacing, 0.0f, static_cast<f32>(halfSlices)*spacing);

            rlVertex3f(static_cast<f32>(-halfSlices)*spacing, 0.0f, static_cast<f32>(i)*spacing);
            rlVertex3f(static_cast<f32>(halfSlices)*spacing, 0.0f, static_cast<f32>(i)*spacing);
        }
    rlEnd();
}

// Load model from files (mesh and material)
Model LoadModel(std::string_view fileNameView)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();

    auto model = Model{ 0 };

#if SUPPORT_FILEFORMAT_OBJ
    if (IsFileExtension(fileName, ".obj")) model = LoadOBJ(fileName);
#endif
#if SUPPORT_FILEFORMAT_IQM
    if (IsFileExtension(fileName, ".iqm")) model = LoadIQM(fileName);
#endif
#if SUPPORT_FILEFORMAT_GLTF
    if (IsFileExtension(fileName, ".gltf") || IsFileExtension(fileName, ".glb")) model = LoadGLTF(fileName);
#endif
#if SUPPORT_FILEFORMAT_VOX
    if (IsFileExtension(fileName, ".vox")) model = LoadVOX(fileName);
#endif
#if SUPPORT_FILEFORMAT_M3D
    if (IsFileExtension(fileName, ".m3d")) model = LoadM3D(fileName);
#endif

    // Make sure model transform is set to identity matrix!
    model.transform = mat4(1.0f);

    if ((model.meshCount != 0) && (model.meshes != nullptr))
    {
        // Upload vertex data to GPU (static meshes)
        for (i32 i = 0; i < model.meshCount; i++) UploadMesh(&model.meshes[i], false);
    }
    else  LogWarning("MESH: [{}] Failed to load model mesh(es) data", fileName);

    if (model.materialCount == 0)
    {
        LogWarning("MATERIAL: [{}] Failed to load model material data, default to white material", fileName);

        model.materialCount = 1;
        model.materials = static_cast<Material *>(calloc(model.materialCount, sizeof(Material)));
        model.materials[0] = LoadMaterialDefault();

        if (model.meshMaterial == nullptr) model.meshMaterial = static_cast<i32 *>(calloc((model.meshCount > 0) ? model.meshCount : 1, sizeof(i32)));
    }

    return model;
}

// Load model from generated mesh
// WARNING: A shallow copy of mesh is generated, passed by value,
// as long as struct contains pointers to data and some values, get a copy
// of mesh pointing to same data as original version... be careful!
Model LoadModelFromMesh(const Mesh mesh)
{
    auto model = Model{ 0 };

    model.transform = mat4(1.0f);

    model.meshCount = 1;
    model.meshes = static_cast<Mesh *>(calloc(model.meshCount, sizeof(Mesh)));
    model.meshes[0] = mesh;

    model.materialCount = 1;
    model.materials = static_cast<Material *>(calloc(model.materialCount, sizeof(Material)));
    model.materials[0] = LoadMaterialDefault();

    model.meshMaterial = static_cast<i32 *>(calloc(model.meshCount, sizeof(i32)));
    model.meshMaterial[0] = 0;  // First material index

    return model;
}

// Check if model is valid (loaded in GPU, VAO/VBOs)
bool IsModelValid(const Model model)
{
    auto result = false;

    if ((model.meshes != nullptr) &&           // Validate model contains some mesh
        (model.materials != nullptr) &&        // Validate model contains some material (at least default one)
        (model.meshMaterial != nullptr) &&     // Validate mesh-material linkage
        (model.meshCount > 0) &&            // Validate mesh count
        (model.materialCount > 0)) result = true; // Validate material count

    // NOTE: Many elements could be validated from a model, including every model mesh VAO/VBOs
    // but some VBOs could not be used, it depends on Mesh vertex data
    // NOTE: Guard the loop with 'model.meshes != nullptr': a model with meshCount > 0
    // but a null meshes array (failed load) would dereference a null pointer here
    for (i32 i = 0; (model.meshes != nullptr) && (i < model.meshCount); i++)
    {
        if ((model.meshes[i].vertices != nullptr) && (model.meshes[i].vboId[0] == 0)) { result = false; break; }   // Vertex position buffer not uploaded to GPU
        if ((model.meshes[i].texcoords != nullptr) && (model.meshes[i].vboId[1] == 0)) { result = false; break; }  // Vertex textcoords buffer not uploaded to GPU
        if ((model.meshes[i].normals != nullptr) && (model.meshes[i].vboId[2] == 0)) { result = false; break; }    // Vertex normals buffer not uploaded to GPU
        if ((model.meshes[i].colors != nullptr) && (model.meshes[i].vboId[3] == 0)) { result = false; break; }     // Vertex colors buffer not uploaded to GPU
        if ((model.meshes[i].tangents != nullptr) && (model.meshes[i].vboId[4] == 0)) { result = false; break; }   // Vertex tangents buffer not uploaded to GPU
        if ((model.meshes[i].texcoords2 != nullptr) && (model.meshes[i].vboId[5] == 0)) { result = false; break; } // Vertex texcoords2 buffer not uploaded to GPU
        if ((model.meshes[i].indices != nullptr) && (model.meshes[i].vboId[6] == 0)) { result = false; break; }    // Vertex indices buffer not uploaded to GPU
#if SUPPORT_GPU_SKINNING
        if ((model.meshes[i].boneIndices != nullptr) && (model.meshes[i].vboId[7] == 0)) { result = false; break; } // Vertex boneIndices buffer not uploaded to GPU
        if ((model.meshes[i].boneWeights != nullptr) && (model.meshes[i].vboId[8] == 0)) { result = false; break; } // Vertex boneWeights buffer not uploaded to GPU
#endif
        // NOTE: Some OpenGL versions do not support VAO, so avoid below check
        //if (model.meshes[i].vaoId == 0) { result = false; break }
    }

    return result;
}

// Unload model (meshes/materials) from memory (RAM and/or VRAM)
// NOTE: This function takes care of all model elements, for a detailed control
// over them, use UnloadMesh() and UnloadMaterial()
void UnloadModel(Model model)
{
    // Unload meshes
    for (i32 i = 0; i < model.meshCount; i++) UnloadMesh(model.meshes[i]);

    // Unload materials maps
    // NOTE: As the user could be sharing shaders and textures between models,
    // don't unload the material but free its maps,
    // the user is responsible for freeing models shaders and textures
    for (i32 i = 0; i < model.materialCount; i++) free(model.materials[i].maps);

    // Unload arrays
    free(model.meshes);
    free(model.materials);
    free(model.meshMaterial);

    // Unload animation data
    free(model.skeleton.bones);
    free(model.skeleton.bindPose);
    free(model.currentPose);
    free(model.boneMatrices);

    LogInfo("MODEL: Unloaded model (and meshes) from RAM and VRAM");
}

// Compute model bounding box limits (considers all meshes)
BoundingBox GetModelBoundingBox(const Model model)
{
    BoundingBox bounds{};

    if (model.meshCount > 0)
    {
        vec3 temp{};
        bounds = GetMeshBoundingBox(model.meshes[0]);

        for (i32 i = 1; i < model.meshCount; i++)
        {
            const auto tempBounds = GetMeshBoundingBox(model.meshes[i]);

            temp.x = (bounds.min.x < tempBounds.min.x)? bounds.min.x : tempBounds.min.x;
            temp.y = (bounds.min.y < tempBounds.min.y)? bounds.min.y : tempBounds.min.y;
            temp.z = (bounds.min.z < tempBounds.min.z)? bounds.min.z : tempBounds.min.z;
            bounds.min = temp;

            temp.x = (bounds.max.x > tempBounds.max.x)? bounds.max.x : tempBounds.max.x;
            temp.y = (bounds.max.y > tempBounds.max.y)? bounds.max.y : tempBounds.max.y;
            temp.z = (bounds.max.z > tempBounds.max.z)? bounds.max.z : tempBounds.max.z;
            bounds.max = temp;
        }
    }

    // Apply model.transform to bounding box
    // WARNING: Current BoundingBox structure design does not support rotation transformations,
    // in those cases is up to the user to calculate the proper box bounds (8 vertices transformed)
    bounds.min = vec3((model.transform)*vec4(bounds.min, 1.0f));
    bounds.max = vec3((model.transform)*vec4(bounds.max, 1.0f));

    return bounds;
}

// Upload vertex data into a VAO (if supported) and VBO
void UploadMesh(Mesh *mesh, const bool dynamic)
{
    if (mesh->vaoId > 0)
    {
        // Check if mesh has already been loaded in GPU
        LogWarning("VAO: [ID {}] Trying to re-load an already loaded mesh", mesh->vaoId);
        return;
    }

    mesh->vboId = static_cast<u32 *>(calloc(MAX_MESH_VERTEX_BUFFERS, sizeof(u32)));

    mesh->vaoId = 0;                                                 // Vertex Array Object
    mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION] = 0;     // Vertex buffer: positions
    mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD] = 0;     // Vertex buffer: texcoords
    mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL] = 0;       // Vertex buffer: normals
    mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR] = 0;        // Vertex buffer: colors
    mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT] = 0;      // Vertex buffer: tangents
    mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD2] = 0;    // Vertex buffer: texcoords2
    mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES] = 0;      // Vertex buffer: indices
#if SUPPORT_GPU_SKINNING
    mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEINDICES] = 0;  // Vertex buffer: boneIndices
    mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEWEIGHTS] = 0;  // Vertex buffer: boneWeights
#endif

#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
    mesh->vaoId = rlLoadVertexArray();

    rlEnableVertexArray(mesh->vaoId);

    // NOTE: Vertex attributes must be uploaded considering default locations points and available vertex data

    // Enable vertex attributes: position (shader-location = 0)
    void *vertices = (mesh->animVertices != nullptr)? mesh->animVertices : mesh->vertices;
    mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION] = rlLoadVertexBuffer(vertices, static_cast<i32>(mesh->vertexCount*3*sizeof(f32)), dynamic);
    rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION, 3, RL_FLOAT, 0, 0, 0);
    rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION);

    // Enable vertex attributes: texcoords (shader-location = 1)

    if (mesh->texcoords != nullptr)
    {
        mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD] = rlLoadVertexBuffer(mesh->texcoords, static_cast<i32>(mesh->vertexCount*2*sizeof(f32)), dynamic);
        rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD, 2, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD);
    }
    else
    {
        f32 value[2] = { 0.0f, 0.0f };
        rlSetVertexAttributeDefault(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD, value, SHADER_ATTRIB_VEC2, 2);
        rlDisableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD);
    }
    // WARNING: When setting default vertex attribute values, the values for each generic vertex attribute
    // is part of current state, and it is maintained even if a different program object is used

    if (mesh->normals != nullptr)
    {
        // Enable vertex attributes: normals (shader-location = 2)
        void *normals = (mesh->animNormals != nullptr)? mesh->animNormals : mesh->normals;
        mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL] = rlLoadVertexBuffer(normals, static_cast<i32>(mesh->vertexCount*3*sizeof(f32)), dynamic);
        rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL, 3, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL);
    }
    else
    {
        // Default vertex attribute: normal
        // WARNING: Default value provided to shader if location available
        f32 value[3] = { 0.0f, 0.0f, 1.0f };
        rlSetVertexAttributeDefault(RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL, value, SHADER_ATTRIB_VEC3, 3);
        rlDisableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL);
    }

    if (mesh->colors != nullptr)
    {
        // Enable vertex attribute: color (shader-location = 3)
        mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR] = rlLoadVertexBuffer(mesh->colors, static_cast<i32>(mesh->vertexCount*4*sizeof(u8)), dynamic);
        rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR, 4, RL_UNSIGNED_BYTE, 1, 0, 0);
        rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR);
    }
    else
    {
        // Default vertex attribute: color
        // WARNING: Default value provided to shader if location available
        f32 value[4] = { 1.0f, 1.0f, 1.0f, 1.0f };    // WHITE
        rlSetVertexAttributeDefault(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR, value, SHADER_ATTRIB_VEC4, 4);
        rlDisableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR);
    }

    if (mesh->tangents != nullptr)
    {
        // Enable vertex attribute: tangent (shader-location = 4)
        mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT] = rlLoadVertexBuffer(mesh->tangents, static_cast<i32>(mesh->vertexCount*4*sizeof(f32)), dynamic);
        rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT, 4, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT);
    }
    else
    {
        // Default vertex attribute: tangent
        // WARNING: Default value provided to shader if location available
        f32 value[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
        rlSetVertexAttributeDefault(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT, value, SHADER_ATTRIB_VEC4, 4);
        rlDisableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT);
    }

    if (mesh->texcoords2 != nullptr)
    {
        // Enable vertex attribute: texcoord2 (shader-location = 5)
        mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD2] = rlLoadVertexBuffer(mesh->texcoords2, static_cast<i32>(mesh->vertexCount*2*sizeof(f32)), dynamic);
        rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD2, 2, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD2);
    }
    else
    {
        // Default vertex attribute: texcoord2
        // WARNING: Default value provided to shader if location available
        f32 value[2] = { 0.0f, 0.0f };
        rlSetVertexAttributeDefault(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD2, value, SHADER_ATTRIB_VEC2, 2);
        rlDisableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD2);
    }

#if SUPPORT_GPU_SKINNING
    if (mesh->boneIndices != nullptr)
    {
        // Enable vertex attribute: boneIndices (shader-location = 7)
        mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEINDICES] = rlLoadVertexBuffer(mesh->boneIndices, static_cast<i32>(mesh->vertexCount*4*sizeof(u8)), dynamic);
        rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEINDICES, 4, RL_UNSIGNED_BYTE, 0, 0, 0);
        rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEINDICES);
    }
    else
    {
        // Default vertex attribute: boneIndices
        // WARNING: Default value provided to shader if location available
        f32 value[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        rlSetVertexAttributeDefault(RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEINDICES, value, SHADER_ATTRIB_VEC4, 4);
        rlDisableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEINDICES);
    }

    if (mesh->boneWeights != nullptr)
    {
        // Enable vertex attribute: boneWeights (shader-location = 8)
        mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEWEIGHTS] = rlLoadVertexBuffer(mesh->boneWeights, static_cast<i32>(mesh->vertexCount*4*sizeof(f32)), dynamic);
        rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEWEIGHTS, 4, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEWEIGHTS);
    }
    else
    {
        // Default vertex attribute: boneWeights
        // WARNING: Default value provided to shader if location available
        f32 value[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        rlSetVertexAttributeDefault(RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEWEIGHTS, value, SHADER_ATTRIB_VEC4, 2);
        rlDisableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEWEIGHTS);
    }
#endif

    if (mesh->indices != nullptr)
    {
        mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES] = rlLoadVertexBufferElement(mesh->indices, static_cast<i32>(mesh->triangleCount*3*sizeof(u16)), dynamic);
    }

    if (mesh->vaoId > 0) LogInfo("VAO: [ID {}] Mesh uploaded successfully to VRAM (GPU)", mesh->vaoId);
    else LogInfo("VBO: Mesh uploaded successfully to VRAM (GPU)");

    rlDisableVertexArray();
#endif
}

// Update mesh vertex data in GPU for a specific buffer index
void UpdateMeshBuffer(const Mesh mesh, const i32 index, void *data, const i32 dataSize, const i32 offset)
{
    rlUpdateVertexBuffer(mesh.vboId[index], data, dataSize, offset);
}

// Draw a 3d mesh with material and transform
void DrawMesh(const Mesh& mesh, const Material& material, const mat4& transform)
{
#if defined(GRAPHICS_API_OPENGL_11) || defined(GRAPHICS_API_OPENGL_SOFTWARE)
    #define GL_VERTEX_ARRAY         0x8074
    #define GL_NORMAL_ARRAY         0x8075
    #define GL_COLOR_ARRAY          0x8076
    #define GL_TEXTURE_COORD_ARRAY  0x8078

    if ((mesh.texcoords != nullptr) && (material.maps[MATERIAL_MAP_DIFFUSE].texture.id > 0)) rlEnableTexture(material.maps[MATERIAL_MAP_DIFFUSE].texture.id);

    if (mesh.animVertices != nullptr) rlEnableStatePointer(GL_VERTEX_ARRAY, mesh.animVertices);
    else rlEnableStatePointer(GL_VERTEX_ARRAY, mesh.vertices);

    if (mesh.texcoords) rlEnableStatePointer(GL_TEXTURE_COORD_ARRAY, mesh.texcoords);

    if (mesh.animNormals != nullptr) rlEnableStatePointer(GL_NORMAL_ARRAY, mesh.animNormals);
    else if (mesh.normals != nullptr) rlEnableStatePointer(GL_NORMAL_ARRAY, mesh.normals);

    if (mesh.colors != nullptr) rlEnableStatePointer(GL_COLOR_ARRAY, mesh.colors);

    rlPushMatrix();
        rlMultMatrixf(MatrixToFloat(transform));
        rlColor4ub(material.maps[MATERIAL_MAP_DIFFUSE].color.r,
                   material.maps[MATERIAL_MAP_DIFFUSE].color.g,
                   material.maps[MATERIAL_MAP_DIFFUSE].color.b,
                   material.maps[MATERIAL_MAP_DIFFUSE].color.a);

        if (mesh.indices != nullptr) rlDrawVertexArrayElements(0, mesh.triangleCount*3, mesh.indices);
        else rlDrawVertexArray(0, mesh.vertexCount);
    rlPopMatrix();

    rlDisableStatePointer(GL_VERTEX_ARRAY);
    rlDisableStatePointer(GL_TEXTURE_COORD_ARRAY);
    rlDisableStatePointer(GL_NORMAL_ARRAY);
    rlDisableStatePointer(GL_COLOR_ARRAY);

    rlDisableTexture();
#endif

#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
    // Bind shader program
    rlEnableShader(material.shader.id);

    if (material.shader.locs == nullptr) return;

    // Send required data to shader (matrices, values)
    //-----------------------------------------------------
    // Upload to shader material.colDiffuse
    if (material.shader.locs[SHADER_LOC_COLOR_DIFFUSE] != -1)
    {
        f32 values[4] = {
            static_cast<f32>(material.maps[MATERIAL_MAP_DIFFUSE].color.r)/255.0f,
            static_cast<f32>(material.maps[MATERIAL_MAP_DIFFUSE].color.g)/255.0f,
            static_cast<f32>(material.maps[MATERIAL_MAP_DIFFUSE].color.b)/255.0f,
            static_cast<f32>(material.maps[MATERIAL_MAP_DIFFUSE].color.a)/255.0f
        };

        rlSetUniform(material.shader.locs[SHADER_LOC_COLOR_DIFFUSE], values, SHADER_UNIFORM_VEC4, 1);
    }

    // Upload to shader material.colSpecular (if location available)
    if (material.shader.locs[SHADER_LOC_COLOR_SPECULAR] != -1)
    {
        f32 values[4] = {
            static_cast<f32>(material.maps[MATERIAL_MAP_SPECULAR].color.r)/255.0f,
            static_cast<f32>(material.maps[MATERIAL_MAP_SPECULAR].color.g)/255.0f,
            static_cast<f32>(material.maps[MATERIAL_MAP_SPECULAR].color.b)/255.0f,
            static_cast<f32>(material.maps[MATERIAL_MAP_SPECULAR].color.a)/255.0f
        };

        rlSetUniform(material.shader.locs[SHADER_LOC_COLOR_SPECULAR], values, SHADER_UNIFORM_VEC4, 1);
    }

    // Get a copy of current matrices to work with,
    // in case stereo render is required, and they need to be modified
    // NOTE: At this point the modelview matrix contains the view matrix (camera)
    // That's because BeginMode3D() sets it and there is no model-drawing function
    // that modifies it, all use rlPushMatrix() and rlPopMatrix()
    auto matModel = mat4(1.0f);
    const auto matView = rlGetMatrixModelview();
    auto matModelView = mat4(1.0f);
    const auto matProjection = rlGetMatrixProjection();

    // Upload view and projection matrices (if locations available)
    if (material.shader.locs[SHADER_LOC_MATRIX_VIEW] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_VIEW], matView);
    if (material.shader.locs[SHADER_LOC_MATRIX_PROJECTION] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_PROJECTION], matProjection);

    // Accumulate several model transformations:
    //    transform: model transformation provided (includes DrawModel() params combined with model.transform)
    //    rlGetMatrixTransform(): rlgl internal transform matrix due to push/pop matrix stack
    matModel = ((rlGetMatrixTransform()) * (transform));

    // Model transformation matrix is sent to shader uniform location: SHADER_LOC_MATRIX_MODEL
    if (material.shader.locs[SHADER_LOC_MATRIX_MODEL] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_MODEL], matModel);

    // Get model-view matrix
    matModelView = ((matView) * (matModel));

    // Upload model normal matrix (if locations available)
    if (material.shader.locs[SHADER_LOC_MATRIX_NORMAL] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_NORMAL], glm::transpose(glm::inverse(matModel)));
    //-----------------------------------------------------

    // Bind active texture maps (if available)
    for (i32 i = 0; i < MAX_MATERIAL_MAPS; i++)
    {
        if (material.maps[i].texture.id > 0)
        {
            // Select current shader texture slot
            rlActiveTextureSlot(i);

            // Enable texture for active slot
            if ((i == MATERIAL_MAP_IRRADIANCE) ||
                (i == MATERIAL_MAP_PREFILTER) ||
                (i == MATERIAL_MAP_CUBEMAP)) rlEnableTextureCubemap(material.maps[i].texture.id);
            else rlEnableTexture(material.maps[i].texture.id);

            rlSetUniform(material.shader.locs[SHADER_LOC_MAP_DIFFUSE + i], &i, SHADER_UNIFORM_INT, 1);
        }
    }

    // Try binding vertex array objects (VAO) or use VBOs if not possible
    // WARNING: UploadMesh() enables all vertex attributes available in mesh and sets default attribute values
    // for shader expected vertex attributes that are not provided by the mesh (i.e. colors)
    // This could be a dangerous approach because different meshes with different shaders can enable/disable some attributes
    if (!rlEnableVertexArray(mesh.vaoId))
    {
        // Bind mesh VBO data: vertex position (shader-location = 0)
        rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION]);
        rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_POSITION], 3, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_POSITION]);

        // Bind mesh VBO data: vertex texcoords (shader-location = 1)
        rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD]);
        rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01], 2, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01]);

        if (material.shader.locs[SHADER_LOC_VERTEX_NORMAL] != -1)
        {
            // Bind mesh VBO data: vertex normals (shader-location = 2)
            rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_NORMAL], 3, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_NORMAL]);
        }

        // Bind mesh VBO data: vertex colors (shader-location = 3, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_COLOR] != -1)
        {
            if (mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR] != 0)
            {
                rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR]);
                rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_COLOR], 4, RL_UNSIGNED_BYTE, 1, 0, 0);
                rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_COLOR]);
            }
            else
            {
                // Set default value for defined vertex attribute in shader but not provided by mesh
                // WARNING: It could result in GPU undefined behaviour
                f32 value[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
                rlSetVertexAttributeDefault(material.shader.locs[SHADER_LOC_VERTEX_COLOR], value, SHADER_ATTRIB_VEC4, 4);
                rlDisableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_COLOR]);
            }
        }

        // Bind mesh VBO data: vertex tangents (shader-location = 4, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_TANGENT] != -1)
        {
            rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TANGENT], 4, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TANGENT]);
        }

        // Bind mesh VBO data: vertex texcoords2 (shader-location = 5, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD02] != -1)
        {
            rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD2]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD02], 2, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD02]);
        }

#if SUPPORT_GPU_SKINNING
        // Bind mesh VBO data: vertex bone ids (shader-location = 6, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_BONEIDS] != -1)
        {
            rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEINDICES]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_BONEIDS], 4, RL_UNSIGNED_BYTE, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_BONEIDS]);
        }

        // Bind mesh VBO data: vertex bone weights (shader-location = 7, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_BONEWEIGHTS] != -1)
        {
            rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEWEIGHTS]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_BONEWEIGHTS], 4, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_BONEWEIGHTS]);
        }
#endif

        if (mesh.indices != nullptr) rlEnableVertexBufferElement(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES]);
    }

    auto eyeCount = 1;
    if (rlIsStereoRenderEnabled()) eyeCount = 2;

    for (i32 eye = 0; eye < eyeCount; eye++)
    {
        // Calculate model-view-projection matrix (MVP)
        auto matModelViewProjection = mat4(1.0f);
        if (eyeCount == 1) matModelViewProjection = ((matProjection) * (matModelView));
        else
        {
            // Setup current eye viewport (half screen width)
            rlViewport(eye*rlGetFramebufferWidth()/2, 0, rlGetFramebufferWidth()/2, rlGetFramebufferHeight());
            matModelViewProjection = ((rlGetMatrixProjectionStereo(eye)) * (((rlGetMatrixViewOffsetStereo(eye)) * (matModelView))));
        }

        // Send combined model-view-projection matrix to shader
        rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_MVP], matModelViewProjection);

        // Draw mesh
        if (mesh.indices != nullptr) rlDrawVertexArrayElements(0, mesh.triangleCount*3, nullptr);
        else rlDrawVertexArray(0, mesh.vertexCount);
    }

    // Unbind all bound texture maps
    for (i32 i = 0; i < MAX_MATERIAL_MAPS; i++)
    {
        if (material.maps[i].texture.id > 0)
        {
            // Select current shader texture slot
            rlActiveTextureSlot(i);

            // Disable texture for active slot
            if ((i == MATERIAL_MAP_IRRADIANCE) ||
                (i == MATERIAL_MAP_PREFILTER) ||
                (i == MATERIAL_MAP_CUBEMAP)) rlDisableTextureCubemap();
            else rlDisableTexture();
        }
    }

    // Disable all possible vertex array objects (or VBOs)
    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();

    // Disable shader program
    rlDisableShader();

    // Restore rlgl internal modelview and projection matrices
    rlSetMatrixModelview(matView);
    rlSetMatrixProjection(matProjection);
#endif
}

// Draw multiple mesh instances with material and different transforms
void DrawMeshInstanced(const Mesh mesh, const Material material, const mat4 *transforms, const i32 instances)
{
#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
    // Instancing required variables
    float16 *instanceTransform = nullptr;
    u32 instancesVboId = 0;

    // Bind shader program
    rlEnableShader(material.shader.id);

    // Send required data to shader (matrices, values)
    //-----------------------------------------------------
    // Upload to shader material.colDiffuse
    if (material.shader.locs[SHADER_LOC_COLOR_DIFFUSE] != -1)
    {
        f32 values[4] = {
            static_cast<f32>(material.maps[MATERIAL_MAP_DIFFUSE].color.r)/255.0f,
            static_cast<f32>(material.maps[MATERIAL_MAP_DIFFUSE].color.g)/255.0f,
            static_cast<f32>(material.maps[MATERIAL_MAP_DIFFUSE].color.b)/255.0f,
            static_cast<f32>(material.maps[MATERIAL_MAP_DIFFUSE].color.a)/255.0f
        };

        rlSetUniform(material.shader.locs[SHADER_LOC_COLOR_DIFFUSE], values, SHADER_UNIFORM_VEC4, 1);
    }

    // Upload to shader material.colSpecular (if location available)
    if (material.shader.locs[SHADER_LOC_COLOR_SPECULAR] != -1)
    {
        f32 values[4] = {
            static_cast<f32>(material.maps[MATERIAL_MAP_SPECULAR].color.r)/255.0f,
            static_cast<f32>(material.maps[MATERIAL_MAP_SPECULAR].color.g)/255.0f,
            static_cast<f32>(material.maps[MATERIAL_MAP_SPECULAR].color.b)/255.0f,
            static_cast<f32>(material.maps[MATERIAL_MAP_SPECULAR].color.a)/255.0f
        };

        rlSetUniform(material.shader.locs[SHADER_LOC_COLOR_SPECULAR], values, SHADER_UNIFORM_VEC4, 1);
    }

    // Get a copy of current matrices to work with,
    // in case stereo render is required, and they need to be modified
    // NOTE: At this point the modelview matrix contains the view matrix (camera)
    // That's because BeginMode3D() sets it and there is no model-drawing function
    // that modifies it, all use rlPushMatrix() and rlPopMatrix()
    const auto matModel = mat4(1.0f);
    const auto matView = rlGetMatrixModelview();
    auto matModelView = mat4(1.0f);
    const auto matProjection = rlGetMatrixProjection();

    // Upload view and projection matrices (if locations available)
    if (material.shader.locs[SHADER_LOC_MATRIX_VIEW] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_VIEW], matView);
    if (material.shader.locs[SHADER_LOC_MATRIX_PROJECTION] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_PROJECTION], matProjection);

    // Create instances buffer
    instanceTransform = static_cast<float16 *>(calloc(instances, sizeof(float16)));

    // Fill buffer with instances transformations as float16 arrays
    for (i32 i = 0; i < instances; i++) instanceTransform[i] = MatrixToFloatV(transforms[i]);

    // Enable mesh VAO to attach new buffer
    rlEnableVertexArray(mesh.vaoId);

    // This could alternatively use a static VBO and either glMapBuffer() or glBufferSubData()
    // It isn't clear which would be reliably faster in all cases and on all platforms,
    // anecdotally glMapBuffer() seems quite slow (syncs) while glBufferSubData() seems
    // no faster, since all the transform matrices are transferred anyway
    instancesVboId = rlLoadVertexBuffer(instanceTransform, static_cast<i32>(instances*sizeof(float16)), false);

    // Instances transformation matrices are sent to shader attribute location: SHADER_LOC_VERTEX_INSTANCETRANSFORM
    if (material.shader.locs[SHADER_LOC_VERTEX_INSTANCETRANSFORM] != -1)
    {
        for (u32 i = 0; i < 4; i++)
        {
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_INSTANCETRANSFORM] + i);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_INSTANCETRANSFORM] + i, 4, RL_FLOAT, 0, sizeof(mat4), static_cast<i32>(i*sizeof(vec4)));
            rlSetVertexAttributeDivisor(material.shader.locs[SHADER_LOC_VERTEX_INSTANCETRANSFORM] + i, 1);
        }
    }

    rlDisableVertexBuffer();
    rlDisableVertexArray();

    // Accumulate internal matrix transform (push/pop) and view matrix
    // NOTE: In this case, model instance transformation must be computed in the shader
    matModelView = ((matView) * (rlGetMatrixTransform()));

    // Upload model normal matrix (if locations available)
    if (material.shader.locs[SHADER_LOC_MATRIX_NORMAL] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_NORMAL], glm::transpose(glm::inverse(matModel)));
    //-----------------------------------------------------

    // Bind active texture maps (if available)
    for (i32 i = 0; i < MAX_MATERIAL_MAPS; i++)
    {
        if (material.maps[i].texture.id > 0)
        {
            // Select current shader texture slot
            rlActiveTextureSlot(i);

            // Enable texture for active slot
            if ((i == MATERIAL_MAP_IRRADIANCE) ||
                (i == MATERIAL_MAP_PREFILTER) ||
                (i == MATERIAL_MAP_CUBEMAP)) rlEnableTextureCubemap(material.maps[i].texture.id);
            else rlEnableTexture(material.maps[i].texture.id);

            rlSetUniform(material.shader.locs[SHADER_LOC_MAP_DIFFUSE + i], &i, SHADER_UNIFORM_INT, 1);
        }
    }

    // Try binding vertex array objects (VAO)
    // or use VBOs if not possible
    if (!rlEnableVertexArray(mesh.vaoId))
    {
        // Bind mesh VBO data: vertex position (shader-location = 0)
        rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION]);
        rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_POSITION], 3, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_POSITION]);

        // Bind mesh VBO data: vertex texcoords (shader-location = 1)
        rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD]);
        rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01], 2, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01]);

        if (material.shader.locs[SHADER_LOC_VERTEX_NORMAL] != -1)
        {
            // Bind mesh VBO data: vertex normals (shader-location = 2)
            rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_NORMAL], 3, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_NORMAL]);
        }

        // Bind mesh VBO data: vertex colors (shader-location = 3, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_COLOR] != -1)
        {
            if (mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR] != 0)
            {
                rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR]);
                rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_COLOR], 4, RL_UNSIGNED_BYTE, 1, 0, 0);
                rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_COLOR]);
            }
            else
            {
                // Set default value for unused attribute
                // NOTE: Required when using default shader and no VAO support
                f32 value[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
                rlSetVertexAttributeDefault(material.shader.locs[SHADER_LOC_VERTEX_COLOR], value, SHADER_ATTRIB_VEC4, 4);
                rlDisableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_COLOR]);
            }
        }

        // Bind mesh VBO data: vertex tangents (shader-location = 4, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_TANGENT] != -1)
        {
            rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TANGENT], 4, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TANGENT]);
        }

        // Bind mesh VBO data: vertex texcoords2 (shader-location = 5, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD02] != -1)
        {
            rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD2]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD02], 2, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD02]);
        }

#if SUPPORT_GPU_SKINNING
        // Bind mesh VBO data: vertex bone ids (shader-location = 6, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_BONEIDS] != -1)
        {
            rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEINDICES]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_BONEIDS], 4, RL_UNSIGNED_BYTE, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_BONEIDS]);
        }

        // Bind mesh VBO data: vertex bone weights (shader-location = 7, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_BONEWEIGHTS] != -1)
        {
            rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEWEIGHTS]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_BONEWEIGHTS], 4, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_BONEWEIGHTS]);
        }
#endif

        if (mesh.indices != nullptr) rlEnableVertexBufferElement(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES]);
    }

    auto eyeCount = 1;
    if (rlIsStereoRenderEnabled()) eyeCount = 2;

    for (i32 eye = 0; eye < eyeCount; eye++)
    {
        // Calculate model-view-projection matrix (MVP)
        auto matModelViewProjection = mat4(1.0f);
        if (eyeCount == 1) matModelViewProjection = ((matProjection) * (matModelView));
        else
        {
            // Setup current eye viewport (half screen width)
            rlViewport(eye*rlGetFramebufferWidth()/2, 0, rlGetFramebufferWidth()/2, rlGetFramebufferHeight());
            matModelViewProjection = ((rlGetMatrixProjectionStereo(eye)) * (((rlGetMatrixViewOffsetStereo(eye)) * (matModelView))));
        }

        // Send combined model-view-projection matrix to shader
        rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_MVP], matModelViewProjection);

        // Draw mesh instanced
        if (mesh.indices != nullptr) rlDrawVertexArrayElementsInstanced(0, mesh.triangleCount*3, nullptr, instances);
        else rlDrawVertexArrayInstanced(0, mesh.vertexCount, instances);
    }

    // Unbind all bound texture maps
    for (i32 i = 0; i < MAX_MATERIAL_MAPS; i++)
    {
        if (material.maps[i].texture.id > 0)
        {
            // Select current shader texture slot
            rlActiveTextureSlot(i);

            // Disable texture for active slot
            if ((i == MATERIAL_MAP_IRRADIANCE) ||
                (i == MATERIAL_MAP_PREFILTER) ||
                (i == MATERIAL_MAP_CUBEMAP)) rlDisableTextureCubemap();
            else rlDisableTexture();
        }
    }

    // Disable all possible vertex array objects (or VBOs)
    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();

    // Disable shader program
    rlDisableShader();

    // Remove instance transforms buffer
    rlUnloadVertexBuffer(instancesVboId);
    free(instanceTransform);
#endif
}

// Unload mesh from memory (RAM and VRAM)
void UnloadMesh(Mesh mesh)
{
    // Unload rlgl mesh vboId data
    rlUnloadVertexArray(mesh.vaoId);

    if (mesh.vboId != nullptr) for (i32 i = 0; i < MAX_MESH_VERTEX_BUFFERS; i++) rlUnloadVertexBuffer(mesh.vboId[i]);
    free(mesh.vboId);

    // Unload mesh vertex buffers
    free(mesh.vertices);
    free(mesh.texcoords);
    free(mesh.normals);
    free(mesh.colors);
    free(mesh.tangents);
    free(mesh.texcoords2);
    free(mesh.indices);

    // Unload mesh skin animation data
    free(mesh.boneWeights);
    free(mesh.boneIndices);

    // Unload mesh runtime CPU skinning data
    free(mesh.animVertices);
    free(mesh.animNormals);
}

// Export mesh data to file
bool ExportMesh(const Mesh mesh, std::string_view fileNameView)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();

    auto result = false;

    if (IsFileExtension(fileName, ".obj"))
    {
        // Estimated data size, it should be enough...
        const i32 vc = mesh.vertexCount;
        const i32 dataSize = vc*static_cast<i32>(strlen("v -0000.000000f -0000.000000f -0000.000000f\n")) +
                       vc*static_cast<i32>(strlen("vt -0.000000f -0.000000f\n")) +
                       vc*static_cast<i32>(strlen("vn -0.0000f -0.0000f -0.0000f\n")) +
                       mesh.triangleCount*snprintf(nullptr, 0, "f %i/%i/%i %i/%i/%i %i/%i/%i\n", vc, vc, vc, vc, vc, vc, vc, vc, vc);

        // NOTE: Text data buffer size is estimated considering mesh data size
        auto txtData = static_cast<char *>(calloc(dataSize + 1000, sizeof(char)));

        auto byteCount = 0;
        byteCount += sprintf(txtData + byteCount, "# //////////////////////////////////////////////////////////////////////////////////\n");
        byteCount += sprintf(txtData + byteCount, "# //                                                                              //\n");
        byteCount += sprintf(txtData + byteCount, "# // rMeshOBJ exporter v1.0 - Mesh exported as triangle faces and not optimized   //\n");
        byteCount += sprintf(txtData + byteCount, "# //                                                                              //\n");
        byteCount += sprintf(txtData + byteCount, "# // more info and bugs-report:  github.com/raysan5/raylib                        //\n");
        byteCount += sprintf(txtData + byteCount, "# // feedback and support:       ray[at]raylib.com                                //\n");
        byteCount += sprintf(txtData + byteCount, "# //                                                                              //\n");
        byteCount += sprintf(txtData + byteCount, "# // Copyright (c) 2018-2026 Ramon Santamaria (@raysan5)                          //\n");
        byteCount += sprintf(txtData + byteCount, "# //                                                                              //\n");
        byteCount += sprintf(txtData + byteCount, "# //////////////////////////////////////////////////////////////////////////////////\n\n");
        byteCount += sprintf(txtData + byteCount, "# Vertex Count:     %i\n", mesh.vertexCount);
        byteCount += sprintf(txtData + byteCount, "# Triangle Count:   %i\n\n", mesh.triangleCount);

        byteCount += sprintf(txtData + byteCount, "g mesh\n");

        for (i32 i = 0, v = 0; i < mesh.vertexCount; i++, v += 3)
        {
            byteCount += sprintf(txtData + byteCount, "v %.6f %.6f %.6f\n", mesh.vertices[v], mesh.vertices[v + 1], mesh.vertices[v + 2]);
        }

        for (i32 i = 0, v = 0; i < mesh.vertexCount; i++, v += 2)
        {
            byteCount += sprintf(txtData + byteCount, "vt %.6f %.6f\n", mesh.texcoords[v], mesh.texcoords[v + 1]);
        }

        for (i32 i = 0, v = 0; i < mesh.vertexCount; i++, v += 3)
        {
            byteCount += sprintf(txtData + byteCount, "vn %.4f %.4f %.4f\n", mesh.normals[v], mesh.normals[v + 1], mesh.normals[v + 2]);
        }

        if (mesh.indices != nullptr)
        {
            for (i32 i = 0, v = 0; i < mesh.triangleCount; i++, v += 3)
            {
                byteCount += sprintf(txtData + byteCount, "f %i/%i/%i %i/%i/%i %i/%i/%i\n",
                    mesh.indices[v] + 1, mesh.indices[v] + 1, mesh.indices[v] + 1,
                    mesh.indices[v + 1] + 1, mesh.indices[v + 1] + 1, mesh.indices[v + 1] + 1,
                    mesh.indices[v + 2] + 1, mesh.indices[v + 2] + 1, mesh.indices[v + 2] + 1);
            }
        }
        else
        {
            for (i32 i = 0, v = 1; i < mesh.triangleCount; i++, v += 3)
            {
                byteCount += sprintf(txtData + byteCount, "f %i/%i/%i %i/%i/%i %i/%i/%i\n", v, v, v, v + 1, v + 1, v + 1, v + 2, v + 2, v + 2);
            }
        }

        // NOTE: Text data length exported is determined by '\0' (NULL) character
        result = SaveFileText(fileName, txtData);

        free(txtData);
    }
    else if (IsFileExtension(fileName, ".gltf")) // Or .glb
    {
        // TODO: Implement gltf/glb support
        /*
        cgltf_size expectedSize = cgltf_write(options, NULL, 0, data);
        char *buffer = (char *)calloc(expectedSize, 0);
        cgltf_size actualSize = cgltf_write(options, buffer, expectedSize, data);

        // NOTE: cgltf_write() includes a NULL terminator that should be ommited in case of a .glb
        if (options->type == cgltf_file_type_glb) cgltf_write_glb(fileName, buffer, actual - 1, data->bin, data->bin_size);
        else SaveFileText(fileName, buffer); // Write a plain JSON file
        */
    }
    else if (IsFileExtension(fileName, ".raw"))
    {
        // TODO: Support additional file formats to export mesh vertex data
    }

    return result;
}

// Export mesh as code file (.h) defining multiple arrays of vertex attributes
bool ExportMeshAsCode(const Mesh mesh, std::string_view fileNameView)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();

    auto result = false;

#ifndef TEXT_BYTES_PER_LINE
    #define TEXT_BYTES_PER_LINE     20
#endif

    // NOTE: Text data buffer size is fixed to 64MB
    auto txtData = static_cast<char *>(calloc(64*1024*1024, sizeof(char)));  // 64 MB

    auto byteCount = 0;
    byteCount += sprintf(txtData + byteCount, "////////////////////////////////////////////////////////////////////////////////////////\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// MeshAsCode exporter v1.0 - Mesh vertex data exported as arrays                     //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// more info and bugs-report:  github.com/raysan5/raylib                              //\n");
    byteCount += sprintf(txtData + byteCount, "// feedback and support:       ray[at]raylib.com                                      //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// Copyright (c) 2023 Ramon Santamaria (@raysan5)                                     //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "////////////////////////////////////////////////////////////////////////////////////////\n\n");

    // Get file name from path and convert variable name to uppercase
    char varFileName[256] = { 0 };
    snprintf(varFileName, 256, "%s", GetFileNameWithoutExt(fileName).c_str()); // NOTE: Using function provided by [rcore] module
    for (i32 i = 0; varFileName[i] != '\0'; i++) if ((varFileName[i] >= 'a') && (varFileName[i] <= 'z')) { varFileName[i] = static_cast<char>(varFileName[i] - 32); }

    // Add image information
    byteCount += sprintf(txtData + byteCount, "// Mesh basic information\n");
    byteCount += sprintf(txtData + byteCount, "#define %s_VERTEX_COUNT    %i\n", varFileName, mesh.vertexCount);
    byteCount += sprintf(txtData + byteCount, "#define %s_TRIANGLE_COUNT   %i\n\n", varFileName, mesh.triangleCount);

    // Define vertex attributes data as separate arrays
    //-----------------------------------------------------------------------------------------
    if (mesh.vertices != nullptr)      // Vertex position (XYZ - 3 components per vertex - float)
    {
        byteCount += sprintf(txtData + byteCount, "static float %s_VERTEX_DATA[%i] = { ", varFileName, mesh.vertexCount*3);
        for (i32 i = 0; i < mesh.vertexCount*3 - 1; i++) byteCount += sprintf(txtData + byteCount, ((i%TEXT_BYTES_PER_LINE == 0)? "%.3ff,\n" : "%.3ff, "), mesh.vertices[i]);
        byteCount += sprintf(txtData + byteCount, "%.3ff };\n\n", mesh.vertices[mesh.vertexCount*3 - 1]);
    }

    if (mesh.texcoords != nullptr)      // Vertex texture coordinates (UV - 2 components per vertex - float)
    {
        byteCount += sprintf(txtData + byteCount, "static float %s_TEXCOORD_DATA[%i] = { ", varFileName, mesh.vertexCount*2);
        for (i32 i = 0; i < mesh.vertexCount*2 - 1; i++) byteCount += sprintf(txtData + byteCount, ((i%TEXT_BYTES_PER_LINE == 0)? "%.3ff,\n" : "%.3ff, "), mesh.texcoords[i]);
        byteCount += sprintf(txtData + byteCount, "%.3ff };\n\n", mesh.texcoords[mesh.vertexCount*2 - 1]);
    }

    if (mesh.texcoords2 != nullptr)      // Vertex texture coordinates (UV - 2 components per vertex - float)
    {
        byteCount += sprintf(txtData + byteCount, "static float %s_TEXCOORD2_DATA[%i] = { ", varFileName, mesh.vertexCount*2);
        for (i32 i = 0; i < mesh.vertexCount*2 - 1; i++) byteCount += sprintf(txtData + byteCount, ((i%TEXT_BYTES_PER_LINE == 0)? "%.3ff,\n" : "%.3ff, "), mesh.texcoords2[i]);
        byteCount += sprintf(txtData + byteCount, "%.3ff };\n\n", mesh.texcoords2[mesh.vertexCount*2 - 1]);
    }

    if (mesh.normals != nullptr)      // Vertex normals (XYZ - 3 components per vertex - float)
    {
        byteCount += sprintf(txtData + byteCount, "static float %s_NORMAL_DATA[%i] = { ", varFileName, mesh.vertexCount*3);
        for (i32 i = 0; i < mesh.vertexCount*3 - 1; i++) byteCount += sprintf(txtData + byteCount, ((i%TEXT_BYTES_PER_LINE == 0)? "%.3ff,\n" : "%.3ff, "), mesh.normals[i]);
        byteCount += sprintf(txtData + byteCount, "%.3ff };\n\n", mesh.normals[mesh.vertexCount*3 - 1]);
    }

    if (mesh.tangents != nullptr)      // Vertex tangents (XYZW - 4 components per vertex - float)
    {
        byteCount += sprintf(txtData + byteCount, "static float %s_TANGENT_DATA[%i] = { ", varFileName, mesh.vertexCount*4);
        for (i32 i = 0; i < mesh.vertexCount*4 - 1; i++) byteCount += sprintf(txtData + byteCount, ((i%TEXT_BYTES_PER_LINE == 0)? "%.3ff,\n" : "%.3ff, "), mesh.tangents[i]);
        byteCount += sprintf(txtData + byteCount, "%.3ff };\n\n", mesh.tangents[mesh.vertexCount*4 - 1]);
    }

    if (mesh.colors != nullptr)        // Vertex colors (RGBA - 4 components per vertex - unsigned char)
    {
        byteCount += sprintf(txtData + byteCount, "static unsigned char %s_COLOR_DATA[%i] = { ", varFileName, mesh.vertexCount*4);
        for (i32 i = 0; i < mesh.vertexCount*4 - 1; i++) byteCount += sprintf(txtData + byteCount, ((i%TEXT_BYTES_PER_LINE == 0)? "0x%x,\n" : "0x%x, "), mesh.colors[i]);
        byteCount += sprintf(txtData + byteCount, "0x%x };\n\n", mesh.colors[mesh.vertexCount*4 - 1]);
    }

    if (mesh.indices != nullptr)       // Vertex indices (3 index per triangle - unsigned short)
    {
        byteCount += sprintf(txtData + byteCount, "static unsigned short %s_INDEX_DATA[%i] = { ", varFileName, mesh.triangleCount*3);
        for (i32 i = 0; i < mesh.triangleCount*3 - 1; i++) byteCount += sprintf(txtData + byteCount, ((i%TEXT_BYTES_PER_LINE == 0)? "%i,\n" : "%i, "), mesh.indices[i]);
        byteCount += sprintf(txtData + byteCount, "%i };\n", mesh.indices[mesh.triangleCount*3 - 1]);
    }
    //-----------------------------------------------------------------------------------------

    // NOTE: Text data size exported is determined by '\0' (NULL) character
    result = SaveFileText(fileName, txtData);

    free(txtData);

    //if (result != 0) LogInfo("FILEIO: [{}] Image as code exported successfully", fileName);
    //else LogWarning("FILEIO: [{}] Failed to export image as code", fileName);

    return result;
}

#if SUPPORT_FILEFORMAT_OBJ || SUPPORT_FILEFORMAT_MTL
// Process obj materials
static void ProcessMaterialsOBJ(Material *materials, tinyobj_material_t *mats, const i32 materialCount)
{
    // Init model mats
    for (i32 m = 0; m < materialCount; m++)
    {
        // Init material to default
        // NOTE: Uses default shader, which only supports MATERIAL_MAP_DIFFUSE
        materials[m] = LoadMaterialDefault();

        if (mats == nullptr) continue;

        // Get default texture, in case no texture is defined
        // NOTE: rlgl default texture is a 1x1 pixel UNCOMPRESSED_R8G8B8A8
        materials[m].maps[MATERIAL_MAP_DIFFUSE].texture = Texture2D{ rlGetTextureIdDefault(), 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };

        if (mats[m].diffuse_texname != nullptr) materials[m].maps[MATERIAL_MAP_DIFFUSE].texture = LoadTexture(mats[m].diffuse_texname);                                                                                                         //char *diffuse_texname; // map_Kd
        else materials[m].maps[MATERIAL_MAP_DIFFUSE].color = Color{ static_cast<u8>(mats[m].diffuse[0]*255.0f), static_cast<u8>(mats[m].diffuse[1]*255.0f), static_cast<u8>(mats[m].diffuse[2]*255.0f), 255 }; //float diffuse[3];
        materials[m].maps[MATERIAL_MAP_DIFFUSE].value = 0.0f;

        if (mats[m].specular_texname != nullptr) materials[m].maps[MATERIAL_MAP_SPECULAR].texture = LoadTexture(mats[m].specular_texname);                                                                                                     //char *specular_texname; // map_Ks
        materials[m].maps[MATERIAL_MAP_SPECULAR].color = Color{ static_cast<u8>(mats[m].specular[0]*255.0f), static_cast<u8>(mats[m].specular[1]*255.0f), static_cast<u8>(mats[m].specular[2]*255.0f), 255 }; //float specular[3];
        materials[m].maps[MATERIAL_MAP_SPECULAR].value = 0.0f;

        if (mats[m].bump_texname != nullptr) materials[m].maps[MATERIAL_MAP_NORMAL].texture = LoadTexture(mats[m].bump_texname);  //char *bump_texname; // map_bump, bump
        materials[m].maps[MATERIAL_MAP_NORMAL].color = WHITE;
        materials[m].maps[MATERIAL_MAP_NORMAL].value = mats[m].shininess;

        materials[m].maps[MATERIAL_MAP_EMISSION].color = Color{ static_cast<u8>(mats[m].emission[0]*255.0f), static_cast<u8>(mats[m].emission[1]*255.0f), static_cast<u8>(mats[m].emission[2]*255.0f), 255 }; //float emission[3];

        if (mats[m].displacement_texname != nullptr) materials[m].maps[MATERIAL_MAP_HEIGHT].texture = LoadTexture(mats[m].displacement_texname);  //char *displacement_texname; // disp
    }
}
#endif

// Load materials from model file
Material * LoadMaterials(std::string_view fileNameView, i32 *materialCount)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();

    Material *materials = nullptr;
    u32 count = 0;

#if SUPPORT_FILEFORMAT_MTL
    if (IsFileExtension(fileName, ".mtl"))
    {
        tinyobj_material_t *mats = nullptr;

        const i32 result = tinyobj_parse_mtl_file(&mats, &count, fileName);
        if (result != TINYOBJ_SUCCESS) LogWarning("MATERIAL: [{}] Failed to parse materials file", fileName);

        materials = static_cast<Material *>(calloc(count, sizeof(Material)));
        ProcessMaterialsOBJ(materials, mats, static_cast<i32>(count));

        tinyobj_materials_free(mats, count);
    }
#else
    LogWarning("FILEIO: [{}] Failed to load material file", fileName);
#endif

    *materialCount = static_cast<i32>(count);
    return materials;
}

// Load default material (Supports: DIFFUSE, SPECULAR, NORMAL maps)
Material LoadMaterialDefault()
{
    auto material = Material{ 0 };
    material.maps = static_cast<MaterialMap *>(calloc(MAX_MATERIAL_MAPS, sizeof(MaterialMap)));

    // Using rlgl default shader
    material.shader.id = rlGetShaderIdDefault();
    material.shader.locs = rlGetShaderLocsDefault();

    // Using rlgl default texture (1x1 pixel, UNCOMPRESSED_R8G8B8A8, 1 mipmap)
    material.maps[MATERIAL_MAP_DIFFUSE].texture = Texture2D{ rlGetTextureIdDefault(), 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    //material.maps[MATERIAL_MAP_NORMAL].texture;         // NOTE: By default, not set
    //material.maps[MATERIAL_MAP_SPECULAR].texture;       // NOTE: By default, not set

    material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;    // Diffuse color
    material.maps[MATERIAL_MAP_SPECULAR].color = WHITE;   // Specular color

    return material;
}

// Check if material is valid (map textures loaded in GPU)
bool IsMaterialValid(const Material material)
{
    auto result = false;

    if ((material.maps != nullptr) && // Validate material contain some map
        (material.shader.id > 0)) result = true; // Validate material shader is valid

    // NOTE: Checking if available maps contain loaded textures does not determine if
    // a material is valid, there can be maps without a texture assigned, only properties

    return result;
}

// Unload material from memory
void UnloadMaterial(Material material)
{
    // Unload material shader (avoid unloading default shader, managed by raylib)
    if (material.shader.id != rlGetShaderIdDefault()) UnloadShader(material.shader);

    // Unload loaded texture maps (avoid unloading default texture, managed by raylib)
    if (material.maps != nullptr)
    {
        for (i32 i = 0; i < MAX_MATERIAL_MAPS; i++)
        {
            if (material.maps[i].texture.id != rlGetTextureIdDefault()) rlUnloadTexture(material.maps[i].texture.id);
        }
    }

    free(material.maps);
}

// Set texture for a material map type (MATERIAL_MAP_DIFFUSE, MATERIAL_MAP_SPECULAR...)
// NOTE: Previous texture should be manually unloaded
void SetMaterialTexture(const Material *material, const i32 mapType, Texture2D texture)
{
    material->maps[mapType].texture = texture;
}

// Set the material for a mesh
void SetModelMeshMaterial(Model *model, const i32 meshId, const i32 materialId)
{
    if (meshId >= model->meshCount) LogWarning("MESH: Id greater than mesh count");
    else if (materialId >= model->materialCount) LogWarning("MATERIAL: Id greater than material count");
    else  model->meshMaterial[meshId] = materialId;
}

// Load model animations from file
ModelAnimation * LoadModelAnimations(std::string_view fileNameView, i32 *animCount)
{
    const std::string fileNameStr(fileNameView);
    const char *fileName = fileNameStr.c_str();

    ModelAnimation *animations = nullptr;

#if SUPPORT_FILEFORMAT_IQM
    if (IsFileExtension(fileName, ".iqm")) animations = LoadModelAnimationsIQM(fileName, animCount);
#endif
#if SUPPORT_FILEFORMAT_M3D
    if (IsFileExtension(fileName, ".m3d")) animations = LoadModelAnimationsM3D(fileName, animCount);
#endif
#if SUPPORT_FILEFORMAT_GLTF
    if (IsFileExtension(fileName, ".gltf;.glb")) animations = LoadModelAnimationsGLTF(fileName, animCount);
#endif

    return animations;
}

// Update model animation data (bone matrices) for a specific pose
// NOTE 1: Request frame could be fractional, using a lerp interpolation between two frames
void UpdateModelAnimationBones(Model model, const ModelAnimation anim, const f32 frame)
{
    if (model.boneMatrices == nullptr) return;

    //UpdateModelAnimationEx(model, anim, frame, anim, frame, 0.0f);

    // Update model animated bones transform matrices for a given frame
    if ((anim.keyframeCount > 0) && (model.skeleton.bones != nullptr) && (anim.keyframePoses != nullptr))
    {
        // Get frame and blending from frame factor required
        auto currentFrame = static_cast<i32>(frame);
        i32 nextFrame = currentFrame + 1;
        f32 blend = frame - static_cast<f32>(currentFrame);
        blend = Clamp(blend, 0.0f, 1.0f);
        if (currentFrame >= anim.keyframeCount) currentFrame = currentFrame%anim.keyframeCount;
        if (nextFrame >= anim.keyframeCount) nextFrame = nextFrame%anim.keyframeCount;

        mat4 bindPoseMatrix{};
        mat4 currentPoseMatrix{};

        // Update all bones and bone matrices of model
        for (u32 boneIndex = 0; boneIndex < model.skeleton.boneCount; boneIndex++)
        {
            // Compute interpolated pose between current and next frame
            // NOTE: Storing animation frame data in model.currentPose
            model.currentPose[boneIndex].translation = glm::mix(anim.keyframePoses[currentFrame][boneIndex].translation, anim.keyframePoses[nextFrame][boneIndex].translation, blend);
            model.currentPose[boneIndex].rotation = glm::slerp(anim.keyframePoses[currentFrame][boneIndex].rotation, anim.keyframePoses[nextFrame][boneIndex].rotation, blend);
            model.currentPose[boneIndex].scale = glm::mix(anim.keyframePoses[currentFrame][boneIndex].scale, anim.keyframePoses[nextFrame][boneIndex].scale, blend);

            // Compute runtime bone matrix from model current pose
            //-----------------------------------------------------------------------------------
            Transform *bindPoseTransform = &model.skeleton.bindPose[boneIndex];
            bindPoseMatrix = ((glm::translate(mat4(1.0f), vec3{bindPoseTransform->translation.x, bindPoseTransform->translation.y, bindPoseTransform->translation.z})) * (((glm::mat4_cast(bindPoseTransform->rotation)) * (glm::scale(mat4(1.0f), vec3{bindPoseTransform->scale.x, bindPoseTransform->scale.y, bindPoseTransform->scale.z})))));

            Transform *currentPoseTransform = &model.currentPose[boneIndex];
            currentPoseMatrix = ((glm::translate(mat4(1.0f), vec3{currentPoseTransform->translation.x, currentPoseTransform->translation.y, currentPoseTransform->translation.z})) * (((glm::mat4_cast(currentPoseTransform->rotation)) * (glm::scale(mat4(1.0f), vec3{currentPoseTransform->scale.x, currentPoseTransform->scale.y, currentPoseTransform->scale.z})))));

            model.boneMatrices[boneIndex] = ((currentPoseMatrix) * (glm::inverse(bindPoseMatrix)));
            //-----------------------------------------------------------------------------------
        }
    }
}

// Update model animation data (vertex buffers / bone matrices) for a specific pose
// NOTE 1: Request frame could be fractional, using a lerp interpolation between two frames
// NOTE 2: Updated vertex animation data is uploaded to GPU in case of CPU skinning,
// for GPU skinning, bone matrices are uploaded to shader on DrawModelEx()
void UpdateModelAnimation(const Model model, const ModelAnimation anim, const f32 frame)
{
    if ((anim.keyframeCount > 0) && (model.skeleton.bones != nullptr) && (anim.keyframePoses != nullptr))
    {
        UpdateModelAnimationBones(model, anim, frame);

        // CPU skinning, updates CPU buffers and uploads them to GPU
        // NOTE: On GPU skinning not supported, use CPU skinning
        UpdateModelAnimationVertexBuffers(model);
    }
}

// Update model animation data (bone matrices) for a specific pose,
// defined by two different animations at specific frames blended together
// NOTE 1: Request frames could be fractional, using a lerp interpolation between two frames
void UpdateModelAnimationBonesEx(Model model, const ModelAnimation animA, const f32 frameA, const ModelAnimation animB, const f32 frameB, const f32 blend)
{
    if (model.boneMatrices == nullptr) return;

    if ((animA.keyframeCount > 0) && (animA.keyframePoses != nullptr) &&
        (animB.keyframeCount > 0) && (animB.keyframePoses != nullptr) &&
        (blend >= 0.0f) && (blend <= 1.0f))
    {
        // Inter-frame interpolation values for first animation
        i32 currentFrameA = static_cast<i32>(frameA)%animA.keyframeCount;
        i32 nextFrameA = currentFrameA + 1;
        f32 blendA = frameA - static_cast<f32>(currentFrameA);
        blendA = Clamp(blendA, 0.0f, 1.0f);
        if (currentFrameA >= animA.keyframeCount) currentFrameA = currentFrameA%animA.keyframeCount;
        if (nextFrameA >= animA.keyframeCount) nextFrameA = nextFrameA%animA.keyframeCount;

        // Inter-frame interpolation values for second animation
        i32 currentFrameB = static_cast<i32>(frameB)%animB.keyframeCount;
        i32 nextFrameB = currentFrameB + 1;
        f32 blendB = frameB - static_cast<f32>(currentFrameB);
        blendB = Clamp(blendB, 0.0f, 1.0f);
        if (currentFrameB >= animB.keyframeCount) currentFrameB = currentFrameB%animB.keyframeCount;
        if (nextFrameB >= animB.keyframeCount) nextFrameB = nextFrameB%animB.keyframeCount;

        mat4 bindPoseMatrix{};
        mat4 currentPoseMatrix{};

        for (u32 boneIndex = 0; boneIndex < model.skeleton.boneCount; boneIndex++)
        {
            // Get frame-interpolation for first animation
            const vec3 frameATranslation = glm::mix(animA.keyframePoses[currentFrameA][boneIndex].translation, animA.keyframePoses[nextFrameA][boneIndex].translation, blendA);
            const quat frameARotation = glm::slerp(animA.keyframePoses[currentFrameA][boneIndex].rotation, animA.keyframePoses[nextFrameA][boneIndex].rotation, blendA);
            const vec3 frameAScale = glm::mix(animA.keyframePoses[currentFrameA][boneIndex].scale, animA.keyframePoses[nextFrameA][boneIndex].scale, blendA);

            // Get frame-interpolation for second animation
            const vec3 frameBTranslation = glm::mix(animB.keyframePoses[currentFrameB][boneIndex].translation, animB.keyframePoses[nextFrameB][boneIndex].translation, blendB);
            const quat frameBRotation = glm::slerp(animB.keyframePoses[currentFrameB][boneIndex].rotation, animB.keyframePoses[nextFrameB][boneIndex].rotation, blendB);
            const vec3 frameBScale = glm::mix(animB.keyframePoses[currentFrameB][boneIndex].scale, animB.keyframePoses[nextFrameB][boneIndex].scale, blendB);

            // Compute interpolated pose between both animations frames
            // NOTE: Storing animation frame data in model.currentPose
            model.currentPose[boneIndex].translation = glm::mix(frameATranslation, frameBTranslation, blend);
            model.currentPose[boneIndex].rotation = glm::slerp(frameARotation, frameBRotation, blend);
            model.currentPose[boneIndex].scale = glm::mix(frameAScale, frameBScale, blend);

            // Compute runtime bone matrix from model current pose
            //-----------------------------------------------------------------------------------
            Transform *bindPoseTransform = &model.skeleton.bindPose[boneIndex];
            bindPoseMatrix = ((glm::translate(mat4(1.0f), vec3{bindPoseTransform->translation.x, bindPoseTransform->translation.y, bindPoseTransform->translation.z})) * (((glm::mat4_cast(bindPoseTransform->rotation)) * (glm::scale(mat4(1.0f), vec3{bindPoseTransform->scale.x, bindPoseTransform->scale.y, bindPoseTransform->scale.z})))));

            Transform *currentPoseTransform = &model.currentPose[boneIndex];
            currentPoseMatrix = ((glm::translate(mat4(1.0f), vec3{currentPoseTransform->translation.x, currentPoseTransform->translation.y, currentPoseTransform->translation.z})) * (((glm::mat4_cast(currentPoseTransform->rotation)) * (glm::scale(mat4(1.0f), vec3{currentPoseTransform->scale.x, currentPoseTransform->scale.y, currentPoseTransform->scale.z})))));

            model.boneMatrices[boneIndex] = ((currentPoseMatrix) * (glm::inverse(bindPoseMatrix)));
            //-----------------------------------------------------------------------------------

            /*
            vec3 outATranslation = animA.keyframePoses[currentFrameA][boneIndex].translation;
            quat outARotation = animA.keyframePoses[currentFrameA][boneIndex].rotation;
            vec3 outAScale = animA.keyframePoses[currentFrameA][boneIndex].scale;

            vec3 outBTranslation = animB.keyframePoses[currentFrameB][boneIndex].translation;
            quat outBRotation = animB.keyframePoses[currentFrameB][boneIndex].rotation;
            vec3 outBScale = animB.keyframePoses[currentFrameB][boneIndex].scale;

            // Invert bind pose transformation
            vec3 invBindTranslation = Vector3RotateByQuaternion(
                Vector3Negate(model.skeleton.bindPose[boneIndex].translation),
                QuaternionInvert(model.skeleton.bindPose[boneIndex].rotation));
            quat invBindRotation = QuaternionInvert(model.skeleton.bindPose[boneIndex].rotation);
            vec3 invBindScale = Vector3Divide(vec3{ 1.0f, 1.0f, 1.0f }, model.skeleton.bindPose[boneIndex].scale);

            vec3 boneTranslation = Vector3Add(Vector3RotateByQuaternion(
                Vector3Multiply(model.currentPose[boneIndex].scale, invBindTranslation),
                    model.currentPose[boneIndex].rotation),
                    model.currentPose[boneIndex].translation);
            quat boneRotation = QuaternionMultiply(model.currentPose[boneIndex].rotation, invBindRotation);
            vec3 boneScale = Vector3Multiply(model.currentPose[boneIndex].scale, invBindScale);

            model.boneMatrices[boneIndex] = MatrixMultiply(
                MatrixMultiply(QuaternionToMatrix(boneRotation),
                    MatrixTranslate(boneTranslation.x, boneTranslation.y, boneTranslation.z)),
                MatrixScale(boneScale.x, boneScale.y, boneScale.z));
            */
        }
    }
}

// Update model animation data (vertex buffers / bone matrices) for a specific pose,
// defined by two different animations at specific frames blended together
// NOTE 1: Request frames could be fractional, using a lerp interpolation between two frames
// NOTE 2: Updated vertex animation data is uploaded to GPU in case of CPU skinning,
// for GPU skinning, bone matrices are uploaded to shader on DrawModelEx()
void UpdateModelAnimationEx(const Model model, const ModelAnimation animA, const f32 frameA, const ModelAnimation animB, const f32 frameB, const f32 blend)
{
    if (model.boneMatrices == nullptr) return;

    if ((animA.keyframeCount > 0) && (animA.keyframePoses != nullptr) &&
        (animB.keyframeCount > 0) && (animB.keyframePoses != nullptr) &&
        (blend >= 0.0f) && (blend <= 1.0f))
    {
        UpdateModelAnimationBonesEx(model, animA, frameA, animB, frameB, blend);

        // CPU skinning, updates CPU buffers and uploads them to GPU (if available)
        // NOTE: Fallback in case GPU skinning is not supported or enabled
        UpdateModelAnimationVertexBuffers(model);
    }
}

// Update model vertex animation buffers (positions and normals)
// NOTE: Required for CPU skinning, uploads animated vertex buffers to GPU
static void UpdateModelAnimationVertexBuffers(const Model model)
{
    static mat4 boneNormalMatrices[256]{}; // Normal matrix per bone, bone indices are unsigned char
    auto boneNormalsReady = false;

    for (i32 m = 0; m < model.meshCount; m++)
    {
        Mesh *mesh = model.meshes + m; // don't copy the mesh, we may need to allocate buffers
        vec3 animVertex{};
        vec3 animNormal{};
        const i32 vertexValuesCount = mesh->vertexCount*3;

        auto boneIndex = 0;
        auto boneCounter = 0;
        auto boneWeight = 0.0f;
        auto bufferUpdateRequired = false; // Flag to check when anim vertex information is updated

        // Skip if missing bone data or missing anim buffers initialization
        if ((mesh->boneWeights == nullptr) || (mesh->boneIndices == nullptr)) continue;

        // Skip if the shader is using the bones directly
#if SUPPORT_GPU_SKINNING
        const Material material = model.materials[model.meshMaterial[m]];
        if ((material.shader.locs != nullptr) && (material.shader.locs[SHADER_LOC_VERTEX_BONEIDS] != -1)) continue;
#endif
        // Lazy allocation of CPU animation buffers for vertex positions and normals, 
        // to be used on CPU software skinning
        if (mesh->animVertices == nullptr)
        {
            mesh->animVertices = static_cast<f32 *>(calloc(mesh->vertexCount*3, sizeof(f32)));
            memcpy(mesh->animVertices, mesh->vertices, mesh->vertexCount*3*sizeof(f32));
        }
        if ((mesh->normals != nullptr) && (mesh->animNormals == nullptr))
        {
            mesh->animNormals = static_cast<f32 *>(calloc(mesh->vertexCount*3, sizeof(f32)));
            memcpy(mesh->animNormals, mesh->normals, mesh->vertexCount*3*sizeof(f32));
        }

        // Normal matrix only depends on the bone, compute it once per bone instead of per vertex
        if ((mesh->normals != nullptr) && !boneNormalsReady)
        {
            memset(boneNormalMatrices, 0, 256*sizeof(mat4));
            for (u32 b = 0; (b < model.skeleton.boneCount) && (b < 256); b++) boneNormalMatrices[b] = glm::transpose(glm::inverse(model.boneMatrices[b]));
            boneNormalsReady = true;
        }

        for (i32 vCounter = 0; vCounter < vertexValuesCount; vCounter += 3)
        {
            mesh->animVertices[vCounter] = 0;
            mesh->animVertices[vCounter + 1] = 0;
            mesh->animVertices[vCounter + 2] = 0;
            if (mesh->animNormals != nullptr)
            {
                mesh->animNormals[vCounter] = 0;
                mesh->animNormals[vCounter + 1] = 0;
                mesh->animNormals[vCounter + 2] = 0;
            }

            // Iterates over 4 bones per vertex
            for (i32 j = 0; j < 4; j++, boneCounter++)
            {
                boneWeight = mesh->boneWeights[boneCounter];
                boneIndex = mesh->boneIndices[boneCounter];

                // Early stop when no transformation will be applied
                if (boneWeight == 0.0f) continue;
                animVertex = vec3{ mesh->vertices[vCounter], mesh->vertices[vCounter + 1], mesh->vertices[vCounter + 2] };
                animVertex = vec3((model.boneMatrices[boneIndex])*vec4(animVertex, 1.0f));
                mesh->animVertices[vCounter] += animVertex.x*boneWeight;
                mesh->animVertices[vCounter + 1] += animVertex.y*boneWeight;
                mesh->animVertices[vCounter + 2] += animVertex.z*boneWeight;
                bufferUpdateRequired = true;

                // Normals processing
                // NOTE: Using meshes.baseNormals (default normal) to calculate meshes.normals (animated normals)
                if ((mesh->normals != nullptr) && (mesh->animNormals != nullptr ))
                {
                    animNormal = vec3{ mesh->normals[vCounter], mesh->normals[vCounter + 1], mesh->normals[vCounter + 2] };
                    animNormal = vec3((glm::transpose(glm::inverse(model.boneMatrices[boneIndex])))*vec4(animNormal, 1.0f));
                    mesh->animNormals[vCounter] += animNormal.x*boneWeight;
                    mesh->animNormals[vCounter + 1] += animNormal.y*boneWeight;
                    mesh->animNormals[vCounter + 2] += animNormal.z*boneWeight;
                }
            }
        }

        if (bufferUpdateRequired)
        {
            // Update GPU vertex buffers with updated data (position + normals)
            rlUpdateVertexBuffer(mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION], mesh->animVertices, static_cast<i32>(mesh->vertexCount*3*sizeof(f32)), 0);
            if (mesh->normals != nullptr) rlUpdateVertexBuffer(mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL], mesh->animNormals, static_cast<i32>(mesh->vertexCount*3*sizeof(f32)), 0);
        }
    }
}

// Unload animation array data
void UnloadModelAnimations(ModelAnimation *animations, const i32 animCount)
{
    for (i32 a = 0; a < animCount; a++)
    {
        for (i32 i = 0; i < animations[a].keyframeCount; i++)
            free(animations[a].keyframePoses[i]);

        free(animations[a].keyframePoses);
    }

    free(animations);
}

// Check model animation skeleton match
// NOTE: Only number of bones and parent connections are checked
bool IsModelAnimationValid(const Model model, const ModelAnimation anim)
{
    i32 result = true;

    if (model.skeleton.boneCount != anim.boneCount) result = false;

    return result;
}

#if SUPPORT_MESH_GENERATION
// Generate polygonal mesh
Mesh GenMeshPoly(const i32 sides, const f32 radius)
{
    auto mesh = Mesh{ 0 };

    if (sides < 3) return mesh; // Security check

    const i32 vertexCount = sides*3;

    // Vertices definition
    auto vertices = static_cast<vec3 *>(calloc(vertexCount, sizeof(vec3)));

    f32 d = 0.0f, dStep = 360.0f/static_cast<f32>(sides);
    for (i32 v = 0; v < vertexCount - 2; v += 3)
    {
        vertices[v] = vec3{ 0.0f, 0.0f, 0.0f };
        vertices[v + 1] = vec3{ sinf(DEG2RAD*d)*radius, 0.0f, cosf(DEG2RAD*d)*radius };
        vertices[v + 2] = vec3{ sinf(DEG2RAD*(d+dStep))*radius, 0.0f, cosf(DEG2RAD*(d+dStep))*radius };
        d += dStep;
    }

    // Normals definition
    auto normals = static_cast<vec3 *>(calloc(vertexCount, sizeof(vec3)));
    for (i32 n = 0; n < vertexCount; n++) normals[n] = vec3{ 0.0f, 1.0f, 0.0f }; // vec3.up;

    // TexCoords definition
    auto texcoords = static_cast<vec2 *>(calloc(vertexCount, sizeof(vec2)));
    for (i32 n = 0; n < vertexCount; n++) texcoords[n] = vec2{ 0.0f, 0.0f };

    mesh.vertexCount = vertexCount;
    mesh.triangleCount = sides;
    mesh.vertices = static_cast<f32 *>(calloc(mesh.vertexCount*3, sizeof(f32)));
    mesh.texcoords = static_cast<f32 *>(calloc(mesh.vertexCount*2, sizeof(f32)));
    mesh.normals = static_cast<f32 *>(calloc(mesh.vertexCount*3, sizeof(f32)));

    // Mesh vertices position array
    for (i32 i = 0; i < mesh.vertexCount; i++)
    {
        mesh.vertices[3*i] = vertices[i].x;
        mesh.vertices[3*i + 1] = vertices[i].y;
        mesh.vertices[3*i + 2] = vertices[i].z;
    }

    // Mesh texcoords array
    for (i32 i = 0; i < mesh.vertexCount; i++)
    {
        mesh.texcoords[2*i] = texcoords[i].x;
        mesh.texcoords[2*i + 1] = texcoords[i].y;
    }

    // Mesh normals array
    for (i32 i = 0; i < mesh.vertexCount; i++)
    {
        mesh.normals[3*i] = normals[i].x;
        mesh.normals[3*i + 1] = normals[i].y;
        mesh.normals[3*i + 2] = normals[i].z;
    }

    free(vertices);
    free(normals);
    free(texcoords);

    // Upload vertex data to GPU (static mesh)
    // NOTE: mesh.vboId array is allocated inside UploadMesh()
    UploadMesh(&mesh, false);

    return mesh;
}

// Generate plane mesh (with subdivisions)
Mesh GenMeshPlane(const f32 width, const f32 length, i32 resX, i32 resZ)
{
    auto mesh = Mesh{ 0 };

#define CUSTOM_MESH_GEN_PLANE
#if defined(CUSTOM_MESH_GEN_PLANE)
    resX++;
    resZ++;

    // Vertices definition
    const i32 vertexCount = resX*resZ; // vertices get reused for the faces

    auto vertices = static_cast<vec3 *>(calloc(vertexCount, sizeof(vec3)));
    for (i32 z = 0; z < resZ; z++)
    {
        // [-length/2, length/2]
        const f32 zPos = (static_cast<f32>(z)/static_cast<f32>((resZ - 1)) - 0.5f)*length;
        for (i32 x = 0; x < resX; x++)
        {
            // [-width/2, width/2]
            const f32 xPos = (static_cast<f32>(x)/static_cast<f32>((resX - 1)) - 0.5f)*width;
            vertices[x + z*resX] = vec3{ xPos, 0.0f, zPos };
        }
    }

    // Normals definition
    auto normals = static_cast<vec3 *>(calloc(vertexCount, sizeof(vec3)));
    for (i32 n = 0; n < vertexCount; n++) normals[n] = vec3{ 0.0f, 1.0f, 0.0f }; // vec3.up;

    // TexCoords definition
    auto texcoords = static_cast<vec2 *>(calloc(vertexCount, sizeof(vec2)));
    for (i32 v = 0; v < resZ; v++)
    {
        for (i32 u = 0; u < resX; u++)
        {
            texcoords[u + v*resX] = vec2{ static_cast<f32>(u)/static_cast<f32>((resX - 1)), static_cast<f32>(v)/static_cast<f32>((resZ - 1)) };
        }
    }

    // Triangles definition (indices)
    const i32 numFaces = (resX - 1)*(resZ - 1);
    auto triangles = static_cast<i32 *>(calloc((numFaces > 0) ? numFaces*6 : 1, sizeof(i32)));
    auto t = 0;
    for (i32 face = 0; face < numFaces; face++)
    {
        // Retrieve lower left corner from face ind
        const i32 i = face + face/(resX - 1);

        triangles[t++] = i + resX;
        triangles[t++] = i + 1;
        triangles[t++] = i;

        triangles[t++] = i + resX;
        triangles[t++] = i + resX + 1;
        triangles[t++] = i + 1;
    }

    mesh.vertexCount = vertexCount;
    mesh.triangleCount = numFaces*2;
    mesh.vertices = static_cast<f32 *>(calloc(mesh.vertexCount*3, sizeof(f32)));
    mesh.texcoords = static_cast<f32 *>(calloc(mesh.vertexCount*2, sizeof(f32)));
    mesh.normals = static_cast<f32 *>(calloc(mesh.vertexCount*3, sizeof(f32)));
    mesh.indices = static_cast<u16 *>(calloc((mesh.triangleCount > 0) ? mesh.triangleCount*3 : 1, sizeof(u16)));

    // Mesh vertices position array
    for (i32 i = 0; i < mesh.vertexCount; i++)
    {
        mesh.vertices[3*i] = vertices[i].x;
        mesh.vertices[3*i + 1] = vertices[i].y;
        mesh.vertices[3*i + 2] = vertices[i].z;
    }

    // Mesh texcoords array
    for (i32 i = 0; i < mesh.vertexCount; i++)
    {
        mesh.texcoords[2*i] = texcoords[i].x;
        mesh.texcoords[2*i + 1] = texcoords[i].y;
    }

    // Mesh normals array
    for (i32 i = 0; i < mesh.vertexCount; i++)
    {
        mesh.normals[3*i] = normals[i].x;
        mesh.normals[3*i + 1] = normals[i].y;
        mesh.normals[3*i + 2] = normals[i].z;
    }

    // Mesh indices array initialization
    for (i32 i = 0; i < mesh.triangleCount*3; i++) mesh.indices[i] = triangles[i];

    free(vertices);
    free(normals);
    free(texcoords);
    free(triangles);

#else       // Use par_shapes library to generate plane mesh

    par_shapes_mesh *plane = par_shapes_create_plane(resX, resZ);   // No normals/texcoords generated!!!
    par_shapes_scale(plane, width, length, 1.0f);
    par_shapes_rotate(plane, -PI/2.0f, (f32[]){ 1, 0, 0 });
    par_shapes_translate(plane, -width/2, 0.0f, length/2);

    mesh.vertices = static_cast<f32 *>(calloc(plane->ntriangles*3*3, sizeof(f32)));
    mesh.texcoords = static_cast<f32 *>(calloc(plane->ntriangles*3*2, sizeof(f32)));
    mesh.normals = static_cast<f32 *>(calloc(plane->ntriangles*3*3, sizeof(f32)));

    mesh.vertexCount = plane->ntriangles*3;
    mesh.triangleCount = plane->ntriangles;

    for (i32 k = 0; k < mesh.vertexCount; k++)
    {
        mesh.vertices[k*3] = plane->points[plane->triangles[k]*3];
        mesh.vertices[k*3 + 1] = plane->points[plane->triangles[k]*3 + 1];
        mesh.vertices[k*3 + 2] = plane->points[plane->triangles[k]*3 + 2];

        mesh.normals[k*3] = plane->normals[plane->triangles[k]*3];
        mesh.normals[k*3 + 1] = plane->normals[plane->triangles[k]*3 + 1];
        mesh.normals[k*3 + 2] = plane->normals[plane->triangles[k]*3 + 2];

        mesh.texcoords[k*2] = plane->tcoords[plane->triangles[k]*2];
        mesh.texcoords[k*2 + 1] = plane->tcoords[plane->triangles[k]*2 + 1];
    }

    par_shapes_free_mesh(plane);
#endif

    // Upload vertex data to GPU (static mesh)
    UploadMesh(&mesh, false);

    return mesh;
}

// Generated cuboid mesh
Mesh GenMeshCube(const f32 width, const f32 height, const f32 length)
{
    auto mesh = Mesh{ 0 };

#define CUSTOM_MESH_GEN_CUBE
#if defined(CUSTOM_MESH_GEN_CUBE)
    f32 vertices[] = {
        -width/2, -height/2, length/2,
        width/2, -height/2, length/2,
        width/2, height/2, length/2,
        -width/2, height/2, length/2,
        -width/2, -height/2, -length/2,
        -width/2, height/2, -length/2,
        width/2, height/2, -length/2,
        width/2, -height/2, -length/2,
        -width/2, height/2, -length/2,
        -width/2, height/2, length/2,
        width/2, height/2, length/2,
        width/2, height/2, -length/2,
        -width/2, -height/2, -length/2,
        width/2, -height/2, -length/2,
        width/2, -height/2, length/2,
        -width/2, -height/2, length/2,
        width/2, -height/2, -length/2,
        width/2, height/2, -length/2,
        width/2, height/2, length/2,
        width/2, -height/2, length/2,
        -width/2, -height/2, -length/2,
        -width/2, -height/2, length/2,
        -width/2, height/2, length/2,
        -width/2, height/2, -length/2
    };

    f32 texcoords[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f,
        0.0f, 0.0f,
        0.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f,
        0.0f, 0.0f,
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f
    };

    f32 normals[] = {
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f,-1.0f,
        0.0f, 0.0f,-1.0f,
        0.0f, 0.0f,-1.0f,
        0.0f, 0.0f,-1.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f,-1.0f, 0.0f,
        0.0f,-1.0f, 0.0f,
        0.0f,-1.0f, 0.0f,
        0.0f,-1.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        -1.0f, 0.0f, 0.0f,
        -1.0f, 0.0f, 0.0f,
        -1.0f, 0.0f, 0.0f,
        -1.0f, 0.0f, 0.0f
    };

    mesh.vertices = static_cast<f32 *>(calloc(24*3, sizeof(f32)));
    memcpy(mesh.vertices, vertices, 24*3*sizeof(f32));

    mesh.texcoords = static_cast<f32 *>(calloc(24*2, sizeof(f32)));
    memcpy(mesh.texcoords, texcoords, 24*2*sizeof(f32));

    mesh.normals = static_cast<f32 *>(calloc(24*3, sizeof(f32)));
    memcpy(mesh.normals, normals, 24*3*sizeof(f32));

    mesh.indices = static_cast<u16 *>(calloc(36, sizeof(u16)));

    auto k = 0;

    // Indices can be initialized right now
    for (i32 i = 0; i < 36; i += 6)
    {
        mesh.indices[i] = 4*k;
        mesh.indices[i + 1] = 4*k + 1;
        mesh.indices[i + 2] = 4*k + 2;
        mesh.indices[i + 3] = 4*k;
        mesh.indices[i + 4] = 4*k + 2;
        mesh.indices[i + 5] = 4*k + 3;

        k++;
    }

    mesh.vertexCount = 24;
    mesh.triangleCount = 12;

#else               // Use par_shapes library to generate cube mesh
/*
// Platonic solids:
par_shapes_mesh *par_shapes_create_tetrahedron();       // 4 sides polyhedron (pyramid)
par_shapes_mesh *par_shapes_create_cube();              // 6 sides polyhedron (cube)
par_shapes_mesh *par_shapes_create_octahedron();        // 8 sides polyhedron (diamond)
par_shapes_mesh *par_shapes_create_dodecahedron();      // 12 sides polyhedron
par_shapes_mesh *par_shapes_create_icosahedron();       // 20 sides polyhedron
*/
    // Platonic solid generation: cube (6 sides)
    // NOTE: No normals/texcoords generated by default
    auto cube = par_shapes_create_cube();
    cube->tcoords = PAR_MALLOC(f32, 2*cube->npoints);
    for (i32 i = 0; i < 2*cube->npoints; i++) cube->tcoords[i] = 0.0f;
    par_shapes_scale(cube, width, height, length);
    par_shapes_translate(cube, -width/2, 0.0f, -length/2);
    par_shapes_compute_normals(cube);

    mesh.vertices = static_cast<f32 *>(calloc(cube->ntriangles*3*3, sizeof(f32)));
    mesh.texcoords = static_cast<f32 *>(calloc(cube->ntriangles*3*2, sizeof(f32)));
    mesh.normals = static_cast<f32 *>(calloc(cube->ntriangles*3*3, sizeof(f32)));

    mesh.vertexCount = cube->ntriangles*3;
    mesh.triangleCount = cube->ntriangles;

    for (i32 k = 0; k < mesh.vertexCount; k++)
    {
        mesh.vertices[k*3] = cube->points[cube->triangles[k]*3];
        mesh.vertices[k*3 + 1] = cube->points[cube->triangles[k]*3 + 1];
        mesh.vertices[k*3 + 2] = cube->points[cube->triangles[k]*3 + 2];

        mesh.normals[k*3] = cube->normals[cube->triangles[k]*3];
        mesh.normals[k*3 + 1] = cube->normals[cube->triangles[k]*3 + 1];
        mesh.normals[k*3 + 2] = cube->normals[cube->triangles[k]*3 + 2];

        mesh.texcoords[k*2] = cube->tcoords[cube->triangles[k]*2];
        mesh.texcoords[k*2 + 1] = cube->tcoords[cube->triangles[k]*2 + 1];
    }

    par_shapes_free_mesh(cube);
#endif

    // Upload vertex data to GPU (static mesh)
    UploadMesh(&mesh, false);

    return mesh;
}

// Generate sphere mesh (standard sphere)
Mesh GenMeshSphere(const f32 radius, const i32 rings, const i32 slices)
{
    auto mesh = Mesh{ 0 };

    if ((rings >= 3) && (slices >= 3))
    {
        par_shapes_set_epsilon_degenerate_sphere(0.0);
        par_shapes_mesh *sphere = par_shapes_create_parametric_sphere(slices, rings);
        par_shapes_scale(sphere, radius, radius, radius);
        // NOTE: Soft normals are computed internally

        mesh.vertices = static_cast<f32 *>(calloc((sphere->ntriangles > 0) ? sphere->ntriangles*3*3 : 1, sizeof(f32)));
        mesh.texcoords = static_cast<f32 *>(calloc((sphere->ntriangles > 0) ? sphere->ntriangles*3*2 : 1, sizeof(f32)));
        mesh.normals = static_cast<f32 *>(calloc((sphere->ntriangles > 0) ? sphere->ntriangles*3*3 : 1, sizeof(f32)));

        mesh.vertexCount = sphere->ntriangles*3;
        mesh.triangleCount = sphere->ntriangles;

        for (i32 k = 0; k < mesh.vertexCount; k++)
        {
            mesh.vertices[k*3] = sphere->points[sphere->triangles[k]*3];
            mesh.vertices[k*3 + 1] = sphere->points[sphere->triangles[k]*3 + 1];
            mesh.vertices[k*3 + 2] = sphere->points[sphere->triangles[k]*3 + 2];

            mesh.normals[k*3] = sphere->normals[sphere->triangles[k]*3];
            mesh.normals[k*3 + 1] = sphere->normals[sphere->triangles[k]*3 + 1];
            mesh.normals[k*3 + 2] = sphere->normals[sphere->triangles[k]*3 + 2];

            mesh.texcoords[k*2] = sphere->tcoords[sphere->triangles[k]*2];
            mesh.texcoords[k*2 + 1] = sphere->tcoords[sphere->triangles[k]*2 + 1];
        }

        par_shapes_free_mesh(sphere);

        // Upload vertex data to GPU (static mesh)
        UploadMesh(&mesh, false);
    }
    else LogWarning("MESH: Failed to generate mesh: sphere");

    return mesh;
}

// Generate hemisphere mesh (half sphere, no bottom cap)
Mesh GenMeshHemiSphere(f32 radius, const i32 rings, const i32 slices)
{
    auto mesh = Mesh{ 0 };

    if ((rings >= 3) && (slices >= 3))
    {
        if (radius < 0.0f) radius = 0.0f;

        par_shapes_mesh *sphere = par_shapes_create_hemisphere(slices, rings);
        par_shapes_scale(sphere, radius, radius, radius);
        // NOTE: Soft normals are computed internally

        mesh.vertices = static_cast<f32 *>(calloc((sphere->ntriangles > 0) ? sphere->ntriangles*3*3 : 1, sizeof(f32)));
        mesh.texcoords = static_cast<f32 *>(calloc((sphere->ntriangles > 0) ? sphere->ntriangles*3*2 : 1, sizeof(f32)));
        mesh.normals = static_cast<f32 *>(calloc((sphere->ntriangles > 0) ? sphere->ntriangles*3*3 : 1, sizeof(f32)));

        mesh.vertexCount = sphere->ntriangles*3;
        mesh.triangleCount = sphere->ntriangles;

        for (i32 k = 0; k < mesh.vertexCount; k++)
        {
            mesh.vertices[k*3] = sphere->points[sphere->triangles[k]*3];
            mesh.vertices[k*3 + 1] = sphere->points[sphere->triangles[k]*3 + 1];
            mesh.vertices[k*3 + 2] = sphere->points[sphere->triangles[k]*3 + 2];

            mesh.normals[k*3] = sphere->normals[sphere->triangles[k]*3];
            mesh.normals[k*3 + 1] = sphere->normals[sphere->triangles[k]*3 + 1];
            mesh.normals[k*3 + 2] = sphere->normals[sphere->triangles[k]*3 + 2];

            mesh.texcoords[k*2] = sphere->tcoords[sphere->triangles[k]*2];
            mesh.texcoords[k*2 + 1] = sphere->tcoords[sphere->triangles[k]*2 + 1];
        }

        par_shapes_free_mesh(sphere);

        // Upload vertex data to GPU (static mesh)
        UploadMesh(&mesh, false);
    }
    else LogWarning("MESH: Failed to generate mesh: hemisphere");

    return mesh;
}

// Generate cylinder mesh
Mesh GenMeshCylinder(const f32 radius, const f32 height, const i32 slices)
{
    auto mesh = Mesh{ 0 };

    if (slices >= 3)
    {
        // Instance a cylinder that sits on the Z=0 plane using the given tessellation
        // levels across the UV domain.  Think of "slices" like a number of pizza
        // slices, and "stacks" like a number of stacked rings
        // Height and radius are both 1.0, but they can easily be changed with par_shapes_scale
        par_shapes_mesh *cylinder = par_shapes_create_cylinder(slices, 8);
        par_shapes_scale(cylinder, radius, radius, height);
        par_shapes_rotate(cylinder, -PI/2.0f, (f32[]){ 1, 0, 0 });

        // Generate an orientable disk shape (top cap)
        par_shapes_mesh *capTop = par_shapes_create_disk(radius, slices, (f32[]){ 0, 0, 0 }, (f32[]){ 0, 0, 1 });
        capTop->tcoords = PAR_MALLOC(f32, 2*capTop->npoints);
        for (i32 i = 0; i < 2*capTop->npoints; i++) capTop->tcoords[i] = 0.0f;
        par_shapes_rotate(capTop, -PI/2.0f, (f32[]){ 1, 0, 0 });
        par_shapes_rotate(capTop, 90*DEG2RAD, (f32[]){ 0, 1, 0 });
        par_shapes_translate(capTop, 0, height, 0);

        // Generate an orientable disk shape (bottom cap)
        par_shapes_mesh *capBottom = par_shapes_create_disk(radius, slices, (f32[]){ 0, 0, 0 }, (f32[]){ 0, 0, -1 });
        capBottom->tcoords = PAR_MALLOC(f32, 2*capBottom->npoints);
        for (i32 i = 0; i < 2*capBottom->npoints; i++) capBottom->tcoords[i] = 0.95f;
        par_shapes_rotate(capBottom, PI/2.0f, (f32[]){ 1, 0, 0 });
        par_shapes_rotate(capBottom, -90*DEG2RAD, (f32[]){ 0, 1, 0 });

        par_shapes_merge_and_free(cylinder, capTop);
        par_shapes_merge_and_free(cylinder, capBottom);

        mesh.vertices = static_cast<f32 *>(calloc(cylinder->ntriangles*3*3, sizeof(f32)));
        mesh.texcoords = static_cast<f32 *>(calloc(cylinder->ntriangles*3*2, sizeof(f32)));
        mesh.normals = static_cast<f32 *>(calloc(cylinder->ntriangles*3*3, sizeof(f32)));

        mesh.vertexCount = cylinder->ntriangles*3;
        mesh.triangleCount = cylinder->ntriangles;

        for (i32 k = 0; k < mesh.vertexCount; k++)
        {
            mesh.vertices[k*3] = cylinder->points[cylinder->triangles[k]*3];
            mesh.vertices[k*3 + 1] = cylinder->points[cylinder->triangles[k]*3 + 1];
            mesh.vertices[k*3 + 2] = cylinder->points[cylinder->triangles[k]*3 + 2];

            mesh.normals[k*3] = cylinder->normals[cylinder->triangles[k]*3];
            mesh.normals[k*3 + 1] = cylinder->normals[cylinder->triangles[k]*3 + 1];
            mesh.normals[k*3 + 2] = cylinder->normals[cylinder->triangles[k]*3 + 2];

            mesh.texcoords[k*2] = cylinder->tcoords[cylinder->triangles[k]*2];
            mesh.texcoords[k*2 + 1] = cylinder->tcoords[cylinder->triangles[k]*2 + 1];
        }

        par_shapes_free_mesh(cylinder);

        // Upload vertex data to GPU (static mesh)
        UploadMesh(&mesh, false);
    }
    else LogWarning("MESH: Failed to generate mesh: cylinder");

    return mesh;
}

// Generate cone/pyramid mesh
Mesh GenMeshCone(const f32 radius, const f32 height, const i32 slices)
{
    auto mesh = Mesh{ 0 };

    if (slices >= 3)
    {
        // Instance a cone that sits on the Z=0 plane using the given tessellation
        // levels across the UV domain.  Think of "slices" like a number of pizza
        // slices, and "stacks" like a number of stacked rings
        // Height and radius are both 1.0, but they can easily be changed with par_shapes_scale
        par_shapes_mesh *cone = par_shapes_create_cone(slices, 8);
        par_shapes_scale(cone, radius, radius, height);
        par_shapes_rotate(cone, -PI/2.0f, (f32[]){ 1, 0, 0 });
        par_shapes_rotate(cone, PI/2.0f, (f32[]){ 0, 1, 0 });

        // Generate an orientable disk shape (bottom cap)
        par_shapes_mesh *capBottom = par_shapes_create_disk(radius, slices, (f32[]){ 0, 0, 0 }, (f32[]){ 0, 0, -1 });
        capBottom->tcoords = PAR_MALLOC(f32, 2*capBottom->npoints);
        for (i32 i = 0; i < 2*capBottom->npoints; i++) capBottom->tcoords[i] = 0.95f;
        par_shapes_rotate(capBottom, PI/2.0f, (f32[]){ 1, 0, 0 });

        par_shapes_merge_and_free(cone, capBottom);

        mesh.vertices = static_cast<f32 *>(calloc(cone->ntriangles*3*3, sizeof(f32)));
        mesh.texcoords = static_cast<f32 *>(calloc(cone->ntriangles*3*2, sizeof(f32)));
        mesh.normals = static_cast<f32 *>(calloc(cone->ntriangles*3*3, sizeof(f32)));

        mesh.vertexCount = cone->ntriangles*3;
        mesh.triangleCount = cone->ntriangles;

        for (i32 k = 0; k < mesh.vertexCount; k++)
        {
            mesh.vertices[k*3] = cone->points[cone->triangles[k]*3];
            mesh.vertices[k*3 + 1] = cone->points[cone->triangles[k]*3 + 1];
            mesh.vertices[k*3 + 2] = cone->points[cone->triangles[k]*3 + 2];

            mesh.normals[k*3] = cone->normals[cone->triangles[k]*3];
            mesh.normals[k*3 + 1] = cone->normals[cone->triangles[k]*3 + 1];
            mesh.normals[k*3 + 2] = cone->normals[cone->triangles[k]*3 + 2];

            mesh.texcoords[k*2] = cone->tcoords[cone->triangles[k]*2];
            mesh.texcoords[k*2 + 1] = cone->tcoords[cone->triangles[k]*2 + 1];
        }

        par_shapes_free_mesh(cone);

        // Upload vertex data to GPU (static mesh)
        UploadMesh(&mesh, false);
    }
    else LogWarning("MESH: Failed to generate mesh: cone");

    return mesh;
}

// Generate torus mesh
// NOTE: The distance between the center of the hole and the center of the
// tube is half size of the radius of the tube (radius*size/2)
Mesh GenMeshTorus(f32 radius, const f32 size, const i32 radSeg, const i32 sides)
{
    auto mesh = Mesh{ 0 };

    if ((sides >= 3) && (radSeg >= 3))
    {
        if (radius > 1.0f) radius = 1.0f;
        else if (radius < 0.1f) radius = 0.1f;

        // Create a donut that sits on the Z=0 plane with the specified inner radius
        // The outer radius can be controlled with par_shapes_scale
        par_shapes_mesh *torus = par_shapes_create_torus(radSeg, sides, radius);
        par_shapes_scale(torus, size/2, size/2, size/2);

        mesh.vertices = static_cast<f32 *>(calloc(torus->ntriangles*3*3, sizeof(f32)));
        mesh.texcoords = static_cast<f32 *>(calloc(torus->ntriangles*3*2, sizeof(f32)));
        mesh.normals = static_cast<f32 *>(calloc(torus->ntriangles*3*3, sizeof(f32)));

        mesh.vertexCount = torus->ntriangles*3;
        mesh.triangleCount = torus->ntriangles;

        for (i32 k = 0; k < mesh.vertexCount; k++)
        {
            mesh.vertices[k*3] = torus->points[torus->triangles[k]*3];
            mesh.vertices[k*3 + 1] = torus->points[torus->triangles[k]*3 + 1];
            mesh.vertices[k*3 + 2] = torus->points[torus->triangles[k]*3 + 2];

            mesh.normals[k*3] = torus->normals[torus->triangles[k]*3];
            mesh.normals[k*3 + 1] = torus->normals[torus->triangles[k]*3 + 1];
            mesh.normals[k*3 + 2] = torus->normals[torus->triangles[k]*3 + 2];

            mesh.texcoords[k*2] = torus->tcoords[torus->triangles[k]*2];
            mesh.texcoords[k*2 + 1] = torus->tcoords[torus->triangles[k]*2 + 1];
        }

        par_shapes_free_mesh(torus);

        // Upload vertex data to GPU (static mesh)
        UploadMesh(&mesh, false);
    }
    else  LogWarning("MESH: Failed to generate mesh: torus");

    return mesh;
}

// Generate trefoil knot mesh
Mesh GenMeshKnot(f32 radius, const f32 size, const i32 radSeg, const i32 sides)
{
    auto mesh = Mesh{ 0 };

    if ((sides >= 3) && (radSeg >= 3))
    {
        if (radius > 3.0f) radius = 3.0f;
        else if (radius < 0.5f) radius = 0.5f;

        par_shapes_mesh *knot = par_shapes_create_trefoil_knot(radSeg, sides, radius);
        par_shapes_scale(knot, size, size, size);

        mesh.vertices = static_cast<f32 *>(calloc(knot->ntriangles*3*3, sizeof(f32)));
        mesh.texcoords = static_cast<f32 *>(calloc(knot->ntriangles*3*2, sizeof(f32)));
        mesh.normals = static_cast<f32 *>(calloc(knot->ntriangles*3*3, sizeof(f32)));

        mesh.vertexCount = knot->ntriangles*3;
        mesh.triangleCount = knot->ntriangles;

        for (i32 k = 0; k < mesh.vertexCount; k++)
        {
            mesh.vertices[k*3] = knot->points[knot->triangles[k]*3];
            mesh.vertices[k*3 + 1] = knot->points[knot->triangles[k]*3 + 1];
            mesh.vertices[k*3 + 2] = knot->points[knot->triangles[k]*3 + 2];

            mesh.normals[k*3] = knot->normals[knot->triangles[k]*3];
            mesh.normals[k*3 + 1] = knot->normals[knot->triangles[k]*3 + 1];
            mesh.normals[k*3 + 2] = knot->normals[knot->triangles[k]*3 + 2];

            mesh.texcoords[k*2] = knot->tcoords[knot->triangles[k]*2];
            mesh.texcoords[k*2 + 1] = knot->tcoords[knot->triangles[k]*2 + 1];
        }

        par_shapes_free_mesh(knot);

        // Upload vertex data to GPU (static mesh)
        UploadMesh(&mesh, false);
    }
    else LogWarning("MESH: Failed to generate mesh: knot");

    return mesh;
}

// Generate a mesh from heightmap
// NOTE: Vertex data is uploaded to GPU
Mesh GenMeshHeightmap(const Image heightmap, const vec3 size)
{
    #define GRAY_VALUE(c) (static_cast<f32>((c.r + c.g + c.b))/3.0f)

    auto mesh = Mesh{ 0 };

    const i32 mapX = heightmap.width;
    const i32 mapZ = heightmap.height;

    auto pixels = LoadImageColors(heightmap);

    // NOTE: One vertex per pixel
    mesh.triangleCount = (mapX - 1)*(mapZ - 1)*2;    // One quad every four pixels

    mesh.vertexCount = mesh.triangleCount*3;

    mesh.vertices = static_cast<f32 *>(calloc((mesh.vertexCount > 0) ? mesh.vertexCount*3 : 1, sizeof(f32)));
    mesh.normals = static_cast<f32 *>(calloc((mesh.vertexCount > 0) ? mesh.vertexCount*3 : 1, sizeof(f32)));
    mesh.texcoords = static_cast<f32 *>(calloc((mesh.vertexCount > 0) ? mesh.vertexCount*2 : 1, sizeof(f32)));
    mesh.colors = nullptr;

    auto vCounter = 0;       // Used to count vertices float by float
    auto tcCounter = 0;      // Used to count texcoords float by float
    auto nCounter = 0;       // Used to count normals float by float

    const auto scaleFactor = vec3{ size.x/static_cast<f32>((mapX - 1)), size.y/255.0f, size.z/static_cast<f32>((mapZ - 1)) };

    vec3 vA{};
    vec3 vB{};
    vec3 vC{};
    vec3 vN{};

    for (i32 z = 0; z < mapZ-1; z++)
    {
        for (i32 x = 0; x < mapX-1; x++)
        {
            // Fill vertices array with data
            //----------------------------------------------------------

            // one triangle - 3 vertex
            mesh.vertices[vCounter] = static_cast<f32>(x)*scaleFactor.x;
            mesh.vertices[vCounter + 1] = GRAY_VALUE(pixels[x + z*mapX])*scaleFactor.y;
            mesh.vertices[vCounter + 2] = static_cast<f32>(z)*scaleFactor.z;

            mesh.vertices[vCounter + 3] = static_cast<f32>(x)*scaleFactor.x;
            mesh.vertices[vCounter + 4] = GRAY_VALUE(pixels[x + (z + 1)*mapX])*scaleFactor.y;
            mesh.vertices[vCounter + 5] = static_cast<f32>(z + 1)*scaleFactor.z;

            mesh.vertices[vCounter + 6] = static_cast<f32>(x + 1)*scaleFactor.x;
            mesh.vertices[vCounter + 7] = GRAY_VALUE(pixels[(x + 1) + z*mapX])*scaleFactor.y;
            mesh.vertices[vCounter + 8] = static_cast<f32>(z)*scaleFactor.z;

            // Another triangle - 3 vertex
            mesh.vertices[vCounter + 9] = mesh.vertices[vCounter + 6];
            mesh.vertices[vCounter + 10] = mesh.vertices[vCounter + 7];
            mesh.vertices[vCounter + 11] = mesh.vertices[vCounter + 8];

            mesh.vertices[vCounter + 12] = mesh.vertices[vCounter + 3];
            mesh.vertices[vCounter + 13] = mesh.vertices[vCounter + 4];
            mesh.vertices[vCounter + 14] = mesh.vertices[vCounter + 5];

            mesh.vertices[vCounter + 15] = static_cast<f32>(x + 1)*scaleFactor.x;
            mesh.vertices[vCounter + 16] = GRAY_VALUE(pixels[(x + 1) + (z + 1)*mapX])*scaleFactor.y;
            mesh.vertices[vCounter + 17] = static_cast<f32>(z + 1)*scaleFactor.z;
            vCounter += 18;     // 6 vertex, 18 floats

            // Fill texcoords array with data
            //--------------------------------------------------------------
            mesh.texcoords[tcCounter] = static_cast<f32>(x)/static_cast<f32>((mapX - 1));
            mesh.texcoords[tcCounter + 1] = static_cast<f32>(z)/static_cast<f32>((mapZ - 1));

            mesh.texcoords[tcCounter + 2] = static_cast<f32>(x)/static_cast<f32>((mapX - 1));
            mesh.texcoords[tcCounter + 3] = static_cast<f32>(z + 1)/static_cast<f32>((mapZ - 1));

            mesh.texcoords[tcCounter + 4] = static_cast<f32>(x + 1)/static_cast<f32>((mapX - 1));
            mesh.texcoords[tcCounter + 5] = static_cast<f32>(z)/static_cast<f32>((mapZ - 1));

            mesh.texcoords[tcCounter + 6] = mesh.texcoords[tcCounter + 4];
            mesh.texcoords[tcCounter + 7] = mesh.texcoords[tcCounter + 5];

            mesh.texcoords[tcCounter + 8] = mesh.texcoords[tcCounter + 2];
            mesh.texcoords[tcCounter + 9] = mesh.texcoords[tcCounter + 3];

            mesh.texcoords[tcCounter + 10] = static_cast<f32>(x + 1)/static_cast<f32>((mapX - 1));
            mesh.texcoords[tcCounter + 11] = static_cast<f32>(z + 1)/static_cast<f32>((mapZ - 1));
            tcCounter += 12;    // 6 texcoords, 12 floats

            // Fill normals array with data
            //--------------------------------------------------------------
            for (i32 i = 0; i < 18; i += 9)
            {
                vA.x = mesh.vertices[nCounter + i];
                vA.y = mesh.vertices[nCounter + i + 1];
                vA.z = mesh.vertices[nCounter + i + 2];

                vB.x = mesh.vertices[nCounter + i + 3];
                vB.y = mesh.vertices[nCounter + i + 4];
                vB.z = mesh.vertices[nCounter + i + 5];

                vC.x = mesh.vertices[nCounter + i + 6];
                vC.y = mesh.vertices[nCounter + i + 7];
                vC.z = mesh.vertices[nCounter + i + 8];

                vN = glm::normalize(glm::cross(((vB) - (vA)), ((vC) - (vA))));

                mesh.normals[nCounter + i] = vN.x;
                mesh.normals[nCounter + i + 1] = vN.y;
                mesh.normals[nCounter + i + 2] = vN.z;

                mesh.normals[nCounter + i + 3] = vN.x;
                mesh.normals[nCounter + i + 4] = vN.y;
                mesh.normals[nCounter + i + 5] = vN.z;

                mesh.normals[nCounter + i + 6] = vN.x;
                mesh.normals[nCounter + i + 7] = vN.y;
                mesh.normals[nCounter + i + 8] = vN.z;
            }

            nCounter += 18;     // 6 vertex, 18 floats
        }
    }

    UnloadImageColors(pixels);  // Unload pixels color data

    // Upload vertex data to GPU (static mesh)
    UploadMesh(&mesh, false);

    return mesh;
}

// Generate a cubes mesh from pixel data
// NOTE: Vertex data is uploaded to GPU
Mesh GenMeshCubicmap(const Image cubicmap, const vec3 cubeSize)
{
    #define COLOR_EQUAL(col1, col2) ((col1.r == col2.r)&&(col1.g == col2.g)&&(col1.b == col2.b)&&(col1.a == col2.a))

    auto mesh = Mesh{ 0 };

    auto pixels = LoadImageColors(cubicmap);

    // NOTE: Max possible number of triangles numCubes*(12 triangles by cube)
    const i32 maxTriangles = cubicmap.width*cubicmap.height*12;

    auto vCounter = 0;       // Used to count vertices
    auto tcCounter = 0;      // Used to count texcoords
    auto nCounter = 0;       // Used to count normals

    const f32 w = cubeSize.x;
    const f32 h = cubeSize.z;
    const f32 h2 = cubeSize.y;

    auto mapVertices = static_cast<vec3 *>(calloc(maxTriangles*3, sizeof(vec3)));
    auto mapTexcoords = static_cast<vec2 *>(calloc(maxTriangles*3, sizeof(vec2)));
    auto mapNormals = static_cast<vec3 *>(calloc(maxTriangles*3, sizeof(vec3)));

    // Define the 6 normals of the cube, combined accordingly later
    const auto n1 = vec3{ 1.0f, 0.0f, 0.0f };
    const auto n2 = vec3{ -1.0f, 0.0f, 0.0f };
    const auto n3 = vec3{ 0.0f, 1.0f, 0.0f };
    const auto n4 = vec3{ 0.0f, -1.0f, 0.0f };
    const auto n5 = vec3{ 0.0f, 0.0f, -1.0f };
    const auto n6 = vec3{ 0.0f, 0.0f, 1.0f };

    // NOTE: Using texture rectangles to define different
    // textures for top-bottom-front-back-right-left (6)
    typedef struct RectangleF {
        f32 x;
        f32 y;
        f32 width;
        f32 height;
    } RectangleF;

    const auto rightTexUV = RectangleF{ 0.0f, 0.0f, 0.5f, 0.5f };
    const auto leftTexUV = RectangleF{ 0.5f, 0.0f, 0.5f, 0.5f };
    const auto frontTexUV = RectangleF{ 0.0f, 0.0f, 0.5f, 0.5f };
    const auto backTexUV = RectangleF{ 0.5f, 0.0f, 0.5f, 0.5f };
    const auto topTexUV = RectangleF{ 0.0f, 0.5f, 0.5f, 0.5f };
    const auto bottomTexUV = RectangleF{ 0.5f, 0.5f, 0.5f, 0.5f };

    for (i32 z = 0; z < cubicmap.height; ++z)
    {
        for (i32 x = 0; x < cubicmap.width; x++)
        {
            // Define the 8 vertex of the cube, to be combined accordingly later
            const auto v1 = vec3{ w*(static_cast<f32>(x) - 0.5f), h2, h*(static_cast<f32>(z) - 0.5f) };
            const auto v2 = vec3{ w*(static_cast<f32>(x) - 0.5f), h2, h*(static_cast<f32>(z) + 0.5f) };
            const auto v3 = vec3{ w*(static_cast<f32>(x) + 0.5f), h2, h*(static_cast<f32>(z) + 0.5f) };
            const auto v4 = vec3{ w*(static_cast<f32>(x) + 0.5f), h2, h*(static_cast<f32>(z) - 0.5f) };
            const auto v5 = vec3{ w*(static_cast<f32>(x) + 0.5f), 0, h*(static_cast<f32>(z) - 0.5f) };
            const auto v6 = vec3{ w*(static_cast<f32>(x) - 0.5f), 0, h*(static_cast<f32>(z) - 0.5f) };
            const auto v7 = vec3{ w*(static_cast<f32>(x) - 0.5f), 0, h*(static_cast<f32>(z) + 0.5f) };
            const auto v8 = vec3{ w*(static_cast<f32>(x) + 0.5f), 0, h*(static_cast<f32>(z) + 0.5f) };

            // Check pixel color to be WHITE -> draw full cube
            if (COLOR_EQUAL(pixels[z*cubicmap.width + x], WHITE))
            {
                // Define triangles and checking collateral cubes
                //------------------------------------------------

                // Define top triangles (2 tris, 6 vertex --> v1-v2-v3, v1-v3-v4)
                // WARNING: Not required for a WHITE cubes, created to allow seeing the map from outside
                mapVertices[vCounter] = v1;
                mapVertices[vCounter + 1] = v2;
                mapVertices[vCounter + 2] = v3;
                mapVertices[vCounter + 3] = v1;
                mapVertices[vCounter + 4] = v3;
                mapVertices[vCounter + 5] = v4;
                vCounter += 6;

                mapNormals[nCounter] = n3;
                mapNormals[nCounter + 1] = n3;
                mapNormals[nCounter + 2] = n3;
                mapNormals[nCounter + 3] = n3;
                mapNormals[nCounter + 4] = n3;
                mapNormals[nCounter + 5] = n3;
                nCounter += 6;

                mapTexcoords[tcCounter] = vec2{ topTexUV.x, topTexUV.y };
                mapTexcoords[tcCounter + 1] = vec2{ topTexUV.x, topTexUV.y + topTexUV.height };
                mapTexcoords[tcCounter + 2] = vec2{ topTexUV.x + topTexUV.width, topTexUV.y + topTexUV.height };
                mapTexcoords[tcCounter + 3] = vec2{ topTexUV.x, topTexUV.y };
                mapTexcoords[tcCounter + 4] = vec2{ topTexUV.x + topTexUV.width, topTexUV.y + topTexUV.height };
                mapTexcoords[tcCounter + 5] = vec2{ topTexUV.x + topTexUV.width, topTexUV.y };
                tcCounter += 6;

                // Define bottom triangles (2 tris, 6 vertex --> v6-v8-v7, v6-v5-v8)
                mapVertices[vCounter] = v6;
                mapVertices[vCounter + 1] = v8;
                mapVertices[vCounter + 2] = v7;
                mapVertices[vCounter + 3] = v6;
                mapVertices[vCounter + 4] = v5;
                mapVertices[vCounter + 5] = v8;
                vCounter += 6;

                mapNormals[nCounter] = n4;
                mapNormals[nCounter + 1] = n4;
                mapNormals[nCounter + 2] = n4;
                mapNormals[nCounter + 3] = n4;
                mapNormals[nCounter + 4] = n4;
                mapNormals[nCounter + 5] = n4;
                nCounter += 6;

                mapTexcoords[tcCounter] = vec2{ bottomTexUV.x + bottomTexUV.width, bottomTexUV.y };
                mapTexcoords[tcCounter + 1] = vec2{ bottomTexUV.x, bottomTexUV.y + bottomTexUV.height };
                mapTexcoords[tcCounter + 2] = vec2{ bottomTexUV.x + bottomTexUV.width, bottomTexUV.y + bottomTexUV.height };
                mapTexcoords[tcCounter + 3] = vec2{ bottomTexUV.x + bottomTexUV.width, bottomTexUV.y };
                mapTexcoords[tcCounter + 4] = vec2{ bottomTexUV.x, bottomTexUV.y };
                mapTexcoords[tcCounter + 5] = vec2{ bottomTexUV.x, bottomTexUV.y + bottomTexUV.height };
                tcCounter += 6;

                // Checking cube on bottom of current cube
                if (((z < cubicmap.height - 1) && COLOR_EQUAL(pixels[(z + 1)*cubicmap.width + x], BLACK)) || (z == cubicmap.height - 1))
                {
                    // Define front triangles (2 tris, 6 vertex) --> v2 v7 v3, v3 v7 v8
                    // NOTE: Collateral occluded faces are not generated
                    mapVertices[vCounter] = v2;
                    mapVertices[vCounter + 1] = v7;
                    mapVertices[vCounter + 2] = v3;
                    mapVertices[vCounter + 3] = v3;
                    mapVertices[vCounter + 4] = v7;
                    mapVertices[vCounter + 5] = v8;
                    vCounter += 6;

                    mapNormals[nCounter] = n6;
                    mapNormals[nCounter + 1] = n6;
                    mapNormals[nCounter + 2] = n6;
                    mapNormals[nCounter + 3] = n6;
                    mapNormals[nCounter + 4] = n6;
                    mapNormals[nCounter + 5] = n6;
                    nCounter += 6;

                    mapTexcoords[tcCounter] = vec2{ frontTexUV.x, frontTexUV.y };
                    mapTexcoords[tcCounter + 1] = vec2{ frontTexUV.x, frontTexUV.y + frontTexUV.height };
                    mapTexcoords[tcCounter + 2] = vec2{ frontTexUV.x + frontTexUV.width, frontTexUV.y };
                    mapTexcoords[tcCounter + 3] = vec2{ frontTexUV.x + frontTexUV.width, frontTexUV.y };
                    mapTexcoords[tcCounter + 4] = vec2{ frontTexUV.x, frontTexUV.y + frontTexUV.height };
                    mapTexcoords[tcCounter + 5] = vec2{ frontTexUV.x + frontTexUV.width, frontTexUV.y + frontTexUV.height };
                    tcCounter += 6;
                }

                // Checking cube on top of current cube
                if (((z > 0) && COLOR_EQUAL(pixels[(z - 1)*cubicmap.width + x], BLACK)) || (z == 0))
                {
                    // Define back triangles (2 tris, 6 vertex) --> v1 v5 v6, v1 v4 v5
                    // NOTE: Collateral occluded faces are not generated
                    mapVertices[vCounter] = v1;
                    mapVertices[vCounter + 1] = v5;
                    mapVertices[vCounter + 2] = v6;
                    mapVertices[vCounter + 3] = v1;
                    mapVertices[vCounter + 4] = v4;
                    mapVertices[vCounter + 5] = v5;
                    vCounter += 6;

                    mapNormals[nCounter] = n5;
                    mapNormals[nCounter + 1] = n5;
                    mapNormals[nCounter + 2] = n5;
                    mapNormals[nCounter + 3] = n5;
                    mapNormals[nCounter + 4] = n5;
                    mapNormals[nCounter + 5] = n5;
                    nCounter += 6;

                    mapTexcoords[tcCounter] = vec2{ backTexUV.x + backTexUV.width, backTexUV.y };
                    mapTexcoords[tcCounter + 1] = vec2{ backTexUV.x, backTexUV.y + backTexUV.height };
                    mapTexcoords[tcCounter + 2] = vec2{ backTexUV.x + backTexUV.width, backTexUV.y + backTexUV.height };
                    mapTexcoords[tcCounter + 3] = vec2{ backTexUV.x + backTexUV.width, backTexUV.y };
                    mapTexcoords[tcCounter + 4] = vec2{ backTexUV.x, backTexUV.y };
                    mapTexcoords[tcCounter + 5] = vec2{ backTexUV.x, backTexUV.y + backTexUV.height };
                    tcCounter += 6;
                }

                // Checking cube on right of current cube
                if (((x < cubicmap.width - 1) && COLOR_EQUAL(pixels[z*cubicmap.width + (x + 1)], BLACK)) || (x == cubicmap.width - 1))
                {
                    // Define right triangles (2 tris, 6 vertex) --> v3 v8 v4, v4 v8 v5
                    // NOTE: Collateral occluded faces are not generated
                    mapVertices[vCounter] = v3;
                    mapVertices[vCounter + 1] = v8;
                    mapVertices[vCounter + 2] = v4;
                    mapVertices[vCounter + 3] = v4;
                    mapVertices[vCounter + 4] = v8;
                    mapVertices[vCounter + 5] = v5;
                    vCounter += 6;

                    mapNormals[nCounter] = n1;
                    mapNormals[nCounter + 1] = n1;
                    mapNormals[nCounter + 2] = n1;
                    mapNormals[nCounter + 3] = n1;
                    mapNormals[nCounter + 4] = n1;
                    mapNormals[nCounter + 5] = n1;
                    nCounter += 6;

                    mapTexcoords[tcCounter] = vec2{ rightTexUV.x, rightTexUV.y };
                    mapTexcoords[tcCounter + 1] = vec2{ rightTexUV.x, rightTexUV.y + rightTexUV.height };
                    mapTexcoords[tcCounter + 2] = vec2{ rightTexUV.x + rightTexUV.width, rightTexUV.y };
                    mapTexcoords[tcCounter + 3] = vec2{ rightTexUV.x + rightTexUV.width, rightTexUV.y };
                    mapTexcoords[tcCounter + 4] = vec2{ rightTexUV.x, rightTexUV.y + rightTexUV.height };
                    mapTexcoords[tcCounter + 5] = vec2{ rightTexUV.x + rightTexUV.width, rightTexUV.y + rightTexUV.height };
                    tcCounter += 6;
                }

                // Checking cube on left of current cube
                if (((x > 0) && COLOR_EQUAL(pixels[z*cubicmap.width + (x - 1)], BLACK)) || (x == 0))
                {
                    // Define left triangles (2 tris, 6 vertex) --> v1 v7 v2, v1 v6 v7
                    // NOTE: Collateral occluded faces are not generated
                    mapVertices[vCounter] = v1;
                    mapVertices[vCounter + 1] = v7;
                    mapVertices[vCounter + 2] = v2;
                    mapVertices[vCounter + 3] = v1;
                    mapVertices[vCounter + 4] = v6;
                    mapVertices[vCounter + 5] = v7;
                    vCounter += 6;

                    mapNormals[nCounter] = n2;
                    mapNormals[nCounter + 1] = n2;
                    mapNormals[nCounter + 2] = n2;
                    mapNormals[nCounter + 3] = n2;
                    mapNormals[nCounter + 4] = n2;
                    mapNormals[nCounter + 5] = n2;
                    nCounter += 6;

                    mapTexcoords[tcCounter] = vec2{ leftTexUV.x, leftTexUV.y };
                    mapTexcoords[tcCounter + 1] = vec2{ leftTexUV.x + leftTexUV.width, leftTexUV.y + leftTexUV.height };
                    mapTexcoords[tcCounter + 2] = vec2{ leftTexUV.x + leftTexUV.width, leftTexUV.y };
                    mapTexcoords[tcCounter + 3] = vec2{ leftTexUV.x, leftTexUV.y };
                    mapTexcoords[tcCounter + 4] = vec2{ leftTexUV.x, leftTexUV.y + leftTexUV.height };
                    mapTexcoords[tcCounter + 5] = vec2{ leftTexUV.x + leftTexUV.width, leftTexUV.y + leftTexUV.height };
                    tcCounter += 6;
                }
            }
            // Check pixel color to be BLACK, in that case only drawing floor and roof
            else if (COLOR_EQUAL(pixels[z*cubicmap.width + x], BLACK))
            {
                // Define top triangles (2 tris, 6 vertex --> v1-v2-v3, v1-v3-v4)
                mapVertices[vCounter] = v1;
                mapVertices[vCounter + 1] = v3;
                mapVertices[vCounter + 2] = v2;
                mapVertices[vCounter + 3] = v1;
                mapVertices[vCounter + 4] = v4;
                mapVertices[vCounter + 5] = v3;
                vCounter += 6;

                mapNormals[nCounter] = n4;
                mapNormals[nCounter + 1] = n4;
                mapNormals[nCounter + 2] = n4;
                mapNormals[nCounter + 3] = n4;
                mapNormals[nCounter + 4] = n4;
                mapNormals[nCounter + 5] = n4;
                nCounter += 6;

                mapTexcoords[tcCounter] = vec2{ topTexUV.x, topTexUV.y };
                mapTexcoords[tcCounter + 1] = vec2{ topTexUV.x + topTexUV.width, topTexUV.y + topTexUV.height };
                mapTexcoords[tcCounter + 2] = vec2{ topTexUV.x, topTexUV.y + topTexUV.height };
                mapTexcoords[tcCounter + 3] = vec2{ topTexUV.x, topTexUV.y };
                mapTexcoords[tcCounter + 4] = vec2{ topTexUV.x + topTexUV.width, topTexUV.y };
                mapTexcoords[tcCounter + 5] = vec2{ topTexUV.x + topTexUV.width, topTexUV.y + topTexUV.height };
                tcCounter += 6;

                // Define bottom triangles (2 tris, 6 vertex --> v6-v8-v7, v6-v5-v8)
                mapVertices[vCounter] = v6;
                mapVertices[vCounter + 1] = v7;
                mapVertices[vCounter + 2] = v8;
                mapVertices[vCounter + 3] = v6;
                mapVertices[vCounter + 4] = v8;
                mapVertices[vCounter + 5] = v5;
                vCounter += 6;

                mapNormals[nCounter] = n3;
                mapNormals[nCounter + 1] = n3;
                mapNormals[nCounter + 2] = n3;
                mapNormals[nCounter + 3] = n3;
                mapNormals[nCounter + 4] = n3;
                mapNormals[nCounter + 5] = n3;
                nCounter += 6;

                mapTexcoords[tcCounter] = vec2{ bottomTexUV.x + bottomTexUV.width, bottomTexUV.y };
                mapTexcoords[tcCounter + 1] = vec2{ bottomTexUV.x + bottomTexUV.width, bottomTexUV.y + bottomTexUV.height };
                mapTexcoords[tcCounter + 2] = vec2{ bottomTexUV.x, bottomTexUV.y + bottomTexUV.height };
                mapTexcoords[tcCounter + 3] = vec2{ bottomTexUV.x + bottomTexUV.width, bottomTexUV.y };
                mapTexcoords[tcCounter + 4] = vec2{ bottomTexUV.x, bottomTexUV.y + bottomTexUV.height };
                mapTexcoords[tcCounter + 5] = vec2{ bottomTexUV.x, bottomTexUV.y };
                tcCounter += 6;
            }
        }
    }

    // Move data from mapVertices temp arrays to vertices float array
    mesh.vertexCount = vCounter;
    mesh.triangleCount = vCounter/3;

    mesh.vertices = static_cast<f32 *>(calloc((mesh.vertexCount > 0) ? mesh.vertexCount*3 : 1, sizeof(f32)));
    mesh.normals = static_cast<f32 *>(calloc((mesh.vertexCount > 0) ? mesh.vertexCount*3 : 1, sizeof(f32)));
    mesh.texcoords = static_cast<f32 *>(calloc((mesh.vertexCount > 0) ? mesh.vertexCount*2 : 1, sizeof(f32)));
    mesh.colors = nullptr;

    auto fCounter = 0;

    // Move vertices data
    for (i32 i = 0; i < vCounter; i++)
    {
        mesh.vertices[fCounter] = mapVertices[i].x;
        mesh.vertices[fCounter + 1] = mapVertices[i].y;
        mesh.vertices[fCounter + 2] = mapVertices[i].z;
        fCounter += 3;
    }

    fCounter = 0;

    // Move normals data
    for (i32 i = 0; i < nCounter; i++)
    {
        mesh.normals[fCounter] = mapNormals[i].x;
        mesh.normals[fCounter + 1] = mapNormals[i].y;
        mesh.normals[fCounter + 2] = mapNormals[i].z;
        fCounter += 3;
    }

    fCounter = 0;

    // Move texcoords data
    for (i32 i = 0; i < tcCounter; i++)
    {
        mesh.texcoords[fCounter] = mapTexcoords[i].x;
        mesh.texcoords[fCounter + 1] = mapTexcoords[i].y;
        fCounter += 2;
    }

    free(mapVertices);
    free(mapNormals);
    free(mapTexcoords);

    UnloadImageColors(pixels);   // Unload pixels color data

    // Upload vertex data to GPU (static mesh)
    UploadMesh(&mesh, false);

    return mesh;
}
#endif // SUPPORT_MESH_GENERATION

// Compute mesh bounding box limits
// NOTE: minVertex and maxVertex should be transformed by model transform matrix
BoundingBox GetMeshBoundingBox(const Mesh mesh)
{
    // Get min and max vertex to construct bounds (AABB)
    vec3 minVertex{};
    vec3 maxVertex{};

    if (mesh.vertices != nullptr)
    {
        minVertex = vec3{ mesh.vertices[0], mesh.vertices[1], mesh.vertices[2] };
        maxVertex = vec3{ mesh.vertices[0], mesh.vertices[1], mesh.vertices[2] };

        for (i32 i = 1; i < mesh.vertexCount; i++)
        {
            minVertex = glm::min(minVertex, vec3{ mesh.vertices[i*3], mesh.vertices[i*3 + 1], mesh.vertices[i*3 + 2] });
            maxVertex = glm::max(maxVertex, vec3{ mesh.vertices[i*3], mesh.vertices[i*3 + 1], mesh.vertices[i*3 + 2] });
        }
    }

    // Create the bounding box
    BoundingBox box{};
    box.min = minVertex;
    box.max = maxVertex;

    return box;
}

// Compute mesh tangents
void GenMeshTangents(Mesh *mesh)
{
    // Check if input mesh data is useful
    if ((mesh == nullptr) || (mesh->vertices == nullptr) || (mesh->texcoords == nullptr) || (mesh->normals == nullptr))
    {
        LogWarning("MESH: Tangents generation requires vertices, texcoords and normals vertex attribute data");
        return;
    }

    // Allocate or reallocate tangents data
    if (mesh->tangents == nullptr) mesh->tangents = static_cast<f32 *>(calloc(mesh->vertexCount*4, sizeof(f32)));
    else
    {
        free(mesh->tangents);
        mesh->tangents = static_cast<f32 *>(calloc(mesh->vertexCount*4, sizeof(f32)));
    }

    // Allocate temporary arrays for tangents calculation
    auto tan1 = static_cast<vec3 *>(calloc(mesh->vertexCount, sizeof(vec3)));
    auto tan2 = static_cast<vec3 *>(calloc(mesh->vertexCount, sizeof(vec3)));

    if (tan1 == nullptr || tan2 == nullptr)
    {
        LogWarning("MESH: Failed to allocate temporary memory for tangent calculation");
        if (tan1) free(tan1);
        if (tan2) free(tan2);
        return;
    }

    // Process all triangles of the mesh
    // 'triangleCount' must be always valid
    for (i32 t = 0; t < mesh->triangleCount; t++)
    {
        // Get triangle vertex indices
        i32 i0 = 0, i1 = 0, i2 = 0;

        if (mesh->indices != nullptr)
        {
            // Use indices if available
            i0 = mesh->indices[t*3 + 0];
            i1 = mesh->indices[t*3 + 1];
            i2 = mesh->indices[t*3 + 2];
        }
        else
        {
            // Sequential access for non-indexed mesh
            i0 = t*3 + 0;
            i1 = t*3 + 1;
            i2 = t*3 + 2;
        }

        // Get triangle vertices position
        const auto v1 = vec3{ mesh->vertices[i0*3 + 0], mesh->vertices[i0*3 + 1], mesh->vertices[i0*3 + 2] };
        const auto v2 = vec3{ mesh->vertices[i1*3 + 0], mesh->vertices[i1*3 + 1], mesh->vertices[i1*3 + 2] };
        const auto v3 = vec3{ mesh->vertices[i2*3 + 0], mesh->vertices[i2*3 + 1], mesh->vertices[i2*3 + 2] };

        // Get triangle texcoords
        const auto uv1 = vec2{ mesh->texcoords[i0*2 + 0], mesh->texcoords[i0*2 + 1] };
        const auto uv2 = vec2{ mesh->texcoords[i1*2 + 0], mesh->texcoords[i1*2 + 1] };
        const auto uv3 = vec2{ mesh->texcoords[i2*2 + 0], mesh->texcoords[i2*2 + 1] };

        // Calculate triangle edges
        const f32 x1 = v2.x - v1.x;
        const f32 y1 = v2.y - v1.y;
        const f32 z1 = v2.z - v1.z;
        const f32 x2 = v3.x - v1.x;
        const f32 y2 = v3.y - v1.y;
        const f32 z2 = v3.z - v1.z;

        // Calculate texture coordinate differences
        const f32 s1 = uv2.x - uv1.x;
        const f32 t1 = uv2.y - uv1.y;
        const f32 s2 = uv3.x - uv1.x;
        const f32 t2 = uv3.y - uv1.y;

        // Calculate denominator and check for degenerate UV
        const f32 div = s1*t2 - s2*t1;
        const f32 r = (fabsf(div) < 0.0001f)? 0.0f : 1.0f/div;

        // Calculate tangent and bitangent directions
        const auto sdir = vec3{ (t2*x1 - t1*x2)*r, (t2*y1 - t1*y2)*r, (t2*z1 - t1*z2)*r };
        const auto tdir = vec3{ (s1*x2 - s2*x1)*r, (s1*y2 - s2*y1)*r, (s1*z2 - s2*z1)*r };

        // Accumulate tangents and bitangents for each vertex of the triangle
        tan1[i0] = ((tan1[i0]) + (sdir));
        tan1[i1] = ((tan1[i1]) + (sdir));
        tan1[i2] = ((tan1[i2]) + (sdir));

        tan2[i0] = ((tan2[i0]) + (tdir));
        tan2[i1] = ((tan2[i1]) + (tdir));
        tan2[i2] = ((tan2[i2]) + (tdir));
    }

    // Calculate final tangents for each vertex
    for (i32 i = 0; i < mesh->vertexCount; i++)
    {
        const auto normal = vec3{ mesh->normals[i*3 + 0], mesh->normals[i*3 + 1], mesh->normals[i*3 + 2] };
        vec3 tangent = tan1[i];

        // Handle zero tangent (can happen with degenerate UVs)
        if (glm::length(tangent) < 0.0001f)
        {
            // Create a tangent perpendicular to the normal
            if (fabsf(normal.z) > 0.707f) tangent = vec3{ 1.0f, 0.0f, 0.0f };
            else tangent = glm::normalize(vec3{ -normal.y, normal.x, 0.0f });

            mesh->tangents[i*4 + 0] = tangent.x;
            mesh->tangents[i*4 + 1] = tangent.y;
            mesh->tangents[i*4 + 2] = tangent.z;
            mesh->tangents[i*4 + 3] = 1.0f;
            continue;
        }

        // Gram-Schmidt orthogonalization to make tangent orthogonal to normal
        // T_prime = T - N*dot(N, T)
        vec3 orthogonalized = ((tangent) - (((normal) * (glm::dot(normal, tangent)))));

        // Handle cases where orthogonalized vector is too small
        if (glm::length(orthogonalized) < 0.0001f)
        {
            // Create a tangent perpendicular to the normal
            if (fabsf(normal.z) > 0.707f) orthogonalized = vec3{ 1.0f, 0.0f, 0.0f };
            else orthogonalized = glm::normalize(vec3{ -normal.y, normal.x, 0.0f });
        }
        else
        {
            // Normalize the orthogonalized tangent
            orthogonalized = glm::normalize(orthogonalized);
        }

        // Store the calculated tangent
        mesh->tangents[i*4 + 0] = orthogonalized.x;
        mesh->tangents[i*4 + 1] = orthogonalized.y;
        mesh->tangents[i*4 + 2] = orthogonalized.z;

        // Calculate the handedness (w component)
        mesh->tangents[i*4 + 3] = (glm::dot(glm::cross(normal, orthogonalized), tan2[i]) < 0.0f)? -1.0f : 1.0f;
    }

    // Free temporary arrays
    free(tan1);
    free(tan2);

    // Update vertex buffers if available
    if (mesh->vboId != nullptr)
    {
        if (mesh->vboId[SHADER_LOC_VERTEX_TANGENT] != 0)
        {
            // Update existing tangent vertex buffer
            rlUpdateVertexBuffer(mesh->vboId[SHADER_LOC_VERTEX_TANGENT], mesh->tangents, static_cast<i32>(mesh->vertexCount*4*sizeof(f32)), 0);
        }
        else
        {
            // Create new tangent vertex buffer
            mesh->vboId[SHADER_LOC_VERTEX_TANGENT] = rlLoadVertexBuffer(mesh->tangents, static_cast<i32>(mesh->vertexCount*4*sizeof(f32)), false);
        }

        // Set up vertex attributes for shader
        rlEnableVertexArray(mesh->vaoId);
        rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT, 4, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT);
        rlDisableVertexArray();
    }

    LogInfo("MESH: Tangents data computed and uploaded for provided mesh");
}

// Draw a model (with texture if set)
void DrawModel(const Model model, const vec3 position, const f32 scale, const Color tint)
{
    const auto vScale = vec3{ scale, scale, scale };
    const auto rotationAxis = vec3{ 0.0f, 1.0f, 0.0f };

    DrawModelEx(model, position, rotationAxis, 0.0f, vScale, tint);
}

// Draw a model with custom transform
void DrawModelEx(Model model, const vec3 position, const vec3 rotationAxis, const f32 rotationAngle, const vec3 scale, const Color tint)
{
    // Calculate transformation matrix from function parameters
    // Get transform matrix (rotation -> scale -> translation)
    const mat4 matScale = glm::scale(mat4(1.0f), vec3{scale.x, scale.y, scale.z});
    const mat4 matRotation = glm::rotate(mat4(1.0f), rotationAngle*DEG2RAD, rotationAxis);
    const mat4 matTranslation = glm::translate(mat4(1.0f), vec3{position.x, position.y, position.z});

    const mat4 matTransform = ((matTranslation) * (((matRotation) * (matScale))));

    // Combine model transformation matrix (model.transform) with matrix generated by function parameters (matTransform)
    model.transform = ((matTransform) * (model.transform));

    for (i32 i = 0; i < model.meshCount; i++)
    {
        const Material mat = model.materials[model.meshMaterial[i]];
        const Color colDiffuse = mat.maps[MATERIAL_MAP_DIFFUSE].color;

        // Applying color tint directly to material diffuse map,
        // because it comes as an input parameter to the function
        auto colTinted = Color{ 0 };
        colTinted.r = static_cast<u8>((static_cast<i32>(colDiffuse.r)*static_cast<i32>(tint.r))/255);
        colTinted.g = static_cast<u8>((static_cast<i32>(colDiffuse.g)*static_cast<i32>(tint.g))/255);
        colTinted.b = static_cast<u8>((static_cast<i32>(colDiffuse.b)*static_cast<i32>(tint.b))/255);
        colTinted.a = static_cast<u8>((static_cast<i32>(colDiffuse.a)*static_cast<i32>(tint.a))/255);

        mat.maps[MATERIAL_MAP_DIFFUSE].color = colTinted;

        // Upload runtime bone transforms matrices, to compute skinning on the shader (GPU-skinning)
        // NOTE: Required location must be found and Mesh bones indices and weights must be also uploaded to shader
        if ((mat.shader.locs != nullptr) && (mat.shader.locs[SHADER_LOC_MATRIX_BONETRANSFORMS] != -1) && (model.boneMatrices != nullptr))
        {
            rlEnableShader(mat.shader.id); // Enable shader to set bone transform matrices
            rlSetUniformMatrices(mat.shader.locs[SHADER_LOC_MATRIX_BONETRANSFORMS], model.boneMatrices, static_cast<i32>(model.skeleton.boneCount));
        }

        DrawMesh(model.meshes[i], mat, model.transform);

        // Restore material diffuse map color (before tint applied)
        mat.maps[MATERIAL_MAP_DIFFUSE].color = colDiffuse;
    }
}

// Draw a model wires (with texture if set)
void DrawModelWires(const Model model, const vec3 position, const f32 scale, const Color tint)
{
    rlEnableWireMode();

    DrawModel(model, position, scale, tint);

    rlDisableWireMode();
}

// Draw a model wires with custom transform
void DrawModelWiresEx(const Model model, const vec3 position, const vec3 rotationAxis, const f32 rotationAngle, const vec3 scale, const Color tint)
{
    rlEnableWireMode();

    DrawModelEx(model, position, rotationAxis, rotationAngle, scale, tint);

    rlDisableWireMode();
}

// Draw a billboard
void DrawBillboard(const Camera camera, const Texture2D texture, const vec3 position, const f32 scale, const Color tint)
{
    const auto rec = Rectangle{ 0.0f, 0.0f, static_cast<f32>(texture.width), static_cast<f32>(texture.height) };

    DrawBillboardRec(camera, texture, rec, position, vec2{ scale*fabsf(rec.width/rec.height), scale }, tint);
}

// Draw a billboard (part of a texture defined by a rectangle)
void DrawBillboardRec(const Camera camera, const Texture2D texture, const Rectangle rec, const vec3 position, const vec2 size, const Color tint)
{
    // NOTE: Billboard locked on axis-Y
    const auto up = vec3{ 0.0f, 1.0f, 0.0f };

    DrawBillboardPro(camera, texture, rec, position, up, size, size*0.5f, 0.0f, tint);
}

// Draw a billboard texture defined by source rectangle with scaling and rotation
void DrawBillboardPro(const Camera camera, const Texture2D texture, Rectangle rec, const vec3 position, vec3 up, const vec2 size, vec2 origin, const f32 rotation, const Color tint)
{
    // Compute the up vector and the right vector
    mat4 matView = glm::lookAt(camera.position, camera.target, camera.up);
    auto right = vec3{ matView[0][0], matView[1][0], matView[2][0] };
    right = ((right) * (size.x));
    up = ((up) * (size.y));

    // Flip the content of the billboard while maintaining the counterclockwise edge rendering order
    if (size.x < 0.0f)
    {
        rec.x -= size.x;
        rec.width *= -1.0;
        right = (-(right));
        origin.x *= -1.0f;
    }
    if (size.y < 0.0f)
    {
        rec.y -= size.y;
        rec.height *= -1.0;
        up = (-(up));
        origin.y *= -1.0f;
    }

    // Draw the texture region described by rec on the following rectangle in 3D space:
    //
    //                size.x          <--.
    //  3 ^---------------------------+ 2 \ rotation
    //    |                           |   /
    //    |                           |
    //    |   origin.x   position     |
    // up |..............             | size.y
    //    |             .             |
    //    |             . origin.y    |
    //    |             .             |
    //  0 +---------------------------> 1
    //                right
    vec3 forward;
    if (rotation != 0.0) forward = glm::cross(right, up);

    const vec3 origin3D = ((((glm::normalize(right)) * (origin.x))) + (((glm::normalize(up)) * (origin.y))));

    vec3 points[4];
    points[0] = vec3{};
    points[1] = right;
    points[2] = ((up) + (right));
    points[3] = up;

    for (i32 i = 0; i < 4; i++)
    {
        points[i] = ((points[i]) - (origin3D));
        if (rotation != 0.0) points[i] = Vector3RotateByAxisAngle(points[i], forward, rotation*DEG2RAD);
        points[i] = ((points[i]) + (position));
    }

    vec2 texcoords[4];
    texcoords[0] = vec2{ rec.x/static_cast<f32>(texture.width), (rec.y + rec.height)/static_cast<f32>(texture.height) };
    texcoords[1] = vec2{ (rec.x + rec.width)/static_cast<f32>(texture.width), (rec.y + rec.height)/static_cast<f32>(texture.height) };
    texcoords[2] = vec2{ (rec.x + rec.width)/static_cast<f32>(texture.width), rec.y/static_cast<f32>(texture.height) };
    texcoords[3] = vec2{ rec.x/static_cast<f32>(texture.width), rec.y/static_cast<f32>(texture.height) };

    rlSetTexture(texture.id);
    rlBegin(RL_QUADS);

        rlColor4ub(tint.r, tint.g, tint.b, tint.a);
        for (i32 i = 0; i < 4; i++)
        {
            rlTexCoord2f(texcoords[i].x, texcoords[i].y);
            rlVertex3f(points[i].x, points[i].y, points[i].z);
        }

    rlEnd();
    rlSetTexture(0);
}

// Draw a bounding box with wires
void DrawBoundingBox(const BoundingBox box, const Color color)
{
    vec3 size{};

    size.x = fabsf(box.max.x - box.min.x);
    size.y = fabsf(box.max.y - box.min.y);
    size.z = fabsf(box.max.z - box.min.z);

    const auto center = vec3{ box.min.x + size.x/2.0f, box.min.y + size.y/2.0f, box.min.z + size.z/2.0f };

    DrawCubeWires(center, size.x, size.y, size.z, color);
}

// Check collision between two spheres
bool CheckCollisionSpheres(const vec3 center1, const f32 radius1, const vec3 center2, const f32 radius2)
{
    auto collision = false;

    // Simple way to check for collision, checking distance between two points
    // Unfortunately, sqrtf() is a costly operation, so avoid it with following solution
    /*
    float dx = center1.x - center2.x;      // X distance between centers
    float dy = center1.y - center2.y;      // Y distance between centers
    float dz = center1.z - center2.z;      // Z distance between centers

    float distance = sqrtf(dx*dx + dy*dy + dz*dz);  // Distance between centers

    if (distance <= (radius1 + radius2)) collision = true;
    */

    // Check for distances squared to avoid sqrtf()
    const f32 radSum = radius1 + radius2;
    if (glm::dot((center1) - (center2), (center1) - (center2)) <= (radSum*radSum)) collision = true;

    return collision;
}

// Check collision between two boxes
// NOTE: Boxes are defined by two points minimum and maximum
bool CheckCollisionBoxes(const BoundingBox box1, const BoundingBox box2)
{
    auto collision = true;

    if ((box1.max.x >= box2.min.x) && (box1.min.x <= box2.max.x))
    {
        if ((box1.max.y < box2.min.y) || (box1.min.y > box2.max.y)) collision = false;
        if ((box1.max.z < box2.min.z) || (box1.min.z > box2.max.z)) collision = false;
    }
    else collision = false;

    return collision;
}

// Check collision between box and sphere
bool CheckCollisionBoxSphere(const BoundingBox box, const vec3 center, const f32 radius)
{
    auto collision = false;

    const vec3 closestPoint = {
        Clamp(center.x, box.min.x, box.max.x),
        Clamp(center.y, box.min.y, box.max.y),
        Clamp(center.z, box.min.z, box.max.z)
    };

    if (glm::dot((center) - (closestPoint), (center) - (closestPoint)) <= (radius*radius)) collision = true;

    return collision;
}

// Get collision info between ray and sphere
RayCollision GetRayCollisionSphere(const Ray ray, const vec3 center, const f32 radius)
{
    auto collision = RayCollision{ 0 };

    const vec3 raySpherePos = ((center) - (ray.position));
    const f32 vector = glm::dot(raySpherePos, ray.direction);
    const f32 distanceToCenter = glm::length(raySpherePos);
    const f32 d = radius*radius - (distanceToCenter*distanceToCenter - vector*vector);

    // Check if the ray's line intersects with the sphere
    if (d >= 0.0f)
    {
        vec3 point{};
        vec3 normal{};
        auto hitDistance = 0.0f;

        // Check if ray origin is inside the sphere to calculate the correct collision point
        if (distanceToCenter < radius)
        {
            hitDistance = vector + sqrtf(d);

            // Calculate collision point
            point = ((ray.position) + (((ray.direction) * (hitDistance))));

            // Calculate collision normal (pointing outwards)
            normal = (-(glm::normalize(((point) - (center)))));
        }
        else
        {
            hitDistance = vector - sqrtf(d);

            // Calculate collision point
            point = ((ray.position) + (((ray.direction) * (hitDistance))));

            // Calculate collision normal (pointing inwards)
            normal = glm::normalize(((point) - (center)));
        }

        if (hitDistance >= 0.0f)
        {
            collision.hit = true;
            collision.point = point;
            collision.normal = normal;
            collision.distance = hitDistance;
        }
    }

    return collision;
}

// Get collision info between ray and box
RayCollision GetRayCollisionBox(Ray ray, const BoundingBox box)
{
    auto collision = RayCollision{ 0 };

    // NOTE: If ray.position is inside the box, the distance is negative (as if the ray was reversed)
    // Reversing ray.direction will give use the correct result
    const bool insideBox = (ray.position.x > box.min.x) && (ray.position.x < box.max.x) &&
                     (ray.position.y > box.min.y) && (ray.position.y < box.max.y) &&
                     (ray.position.z > box.min.z) && (ray.position.z < box.max.z);

    if (insideBox) ray.direction = (-(ray.direction));

    f32 t[11] = { 0 };

    t[8] = 1.0f/ray.direction.x;
    t[9] = 1.0f/ray.direction.y;
    t[10] = 1.0f/ray.direction.z;

    t[0] = (box.min.x - ray.position.x)*t[8];
    t[1] = (box.max.x - ray.position.x)*t[8];
    t[2] = (box.min.y - ray.position.y)*t[9];
    t[3] = (box.max.y - ray.position.y)*t[9];
    t[4] = (box.min.z - ray.position.z)*t[10];
    t[5] = (box.max.z - ray.position.z)*t[10];
    t[6] = fmax(fmax(fmin(t[0], t[1]), fmin(t[2], t[3])), fmin(t[4], t[5]));
    t[7] = fmin(fmin(fmax(t[0], t[1]), fmax(t[2], t[3])), fmax(t[4], t[5]));

    collision.hit = !((t[7] < 0) || (t[6] > t[7]));
    collision.distance = t[6];
    collision.point = ((ray.position) + (((ray.direction) * (collision.distance))));

    // Get box center point
    collision.normal = glm::mix(box.min, box.max, 0.5f);
    // Get vector center point->hit point
    collision.normal = ((collision.point) - (collision.normal));
    // Scale vector to unit cube
    // NOTE: Use an additional .01 to fix numerical errors
    collision.normal = ((collision.normal) * (2.01f));
    collision.normal = ((collision.normal) / (((box.max) - (box.min))));
    // The relevant elements of the vector are now slightly larger than 1.0f (or smaller than -1.0f)
    // and the others are somewhere between -1.0 and 1.0 casting to int is exactly our wanted normal!
    collision.normal.x = static_cast<f32>(static_cast<i32>(collision.normal.x));
    collision.normal.y = static_cast<f32>(static_cast<i32>(collision.normal.y));
    collision.normal.z = static_cast<f32>(static_cast<i32>(collision.normal.z));

    collision.normal = glm::normalize(collision.normal);

    if (insideBox)
    {
        // Reset ray.direction
        ray.direction = (-(ray.direction));
        // Fix result
        collision.distance *= -1.0f;
        collision.normal = (-(collision.normal));
    }

    return collision;
}

// Get collision info between ray and mesh
RayCollision GetRayCollisionMesh(Ray ray, const Mesh mesh, const mat4 transform)
{
    auto collision = RayCollision{ 0 };

    // Check if mesh vertex data on CPU for testing
    if (mesh.vertices != nullptr)
    {
        // Transform the ray into model space by the inverse of the transform, 
        // so it can be tested against the untransformed vertex data
        const mat4 inverseMatrix = glm::inverse(transform);
        ray.position = vec3((inverseMatrix)*vec4(ray.position, 1.0f));

        // Direction does not get translation, just rotation
        const quat invRot = glm::quat_cast(mat3(inverseMatrix));
        ray.direction = ((invRot)*(ray.direction));

        const i32 triangleCount = mesh.triangleCount;

        // Test against all triangles in mesh
        for (i32 i = 0; i < triangleCount; i++)
        {
            vec3 a{};
            vec3 b{};
            vec3 c{};
            auto vertdata = reinterpret_cast<vec3 *>(mesh.vertices);

            if (mesh.indices)
            {
                a = vertdata[mesh.indices[i*3 + 0]];
                b = vertdata[mesh.indices[i*3 + 1]];
                c = vertdata[mesh.indices[i*3 + 2]];
            }
            else
            {
                a = vertdata[i*3 + 0];
                b = vertdata[i*3 + 1];
                c = vertdata[i*3 + 2];
            }

            const auto triHitInfo = GetRayCollisionTriangle(ray, a, b, c);

            if (triHitInfo.hit)
            {
                // Save the closest hit triangle
                if ((!collision.hit) || (collision.distance > triHitInfo.distance)) collision = triHitInfo;
            }
        }
    }

    // Set the hit point and normal back in world space
    if (collision.hit)
    {
        collision.point = vec3((transform)*vec4(collision.point, 1.0f));
        collision.normal = ((glm::quat_cast(mat3(transform)))*(collision.normal));
    }

    return collision;
}

// Get collision info between ray and triangle
// NOTE: The points are expected to be in counter-clockwise winding
// NOTE: Based on https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm
RayCollision GetRayCollisionTriangle(const Ray ray, const vec3 p1, const vec3 p2, const vec3 p3)
{
    #define EPSILON 0.000001f        // A small number

    auto collision = RayCollision{ 0 };
    vec3 edge1{};
    vec3 edge2{};
    vec3 p{};
    vec3 q{};
    vec3 tv{};
    f32 det = 0.0f, invDet = 0.0f, u = 0.0f, v = 0.0f, t = 0.0f;

    // Find vectors for two edges sharing V1
    edge1 = ((p2) - (p1));
    edge2 = ((p3) - (p1));

    // Begin calculating determinant - also used to calculate u parameter
    p = glm::cross(ray.direction, edge2);

    // If determinant is near zero, ray lies in plane of triangle or ray is parallel to plane of triangle
    det = glm::dot(edge1, p);

    // Avoid culling!
    if ((det > -EPSILON) && (det < EPSILON)) return collision;

    invDet = 1.0f/det;

    // Calculate distance from V1 to ray origin
    tv = ((ray.position) - (p1));

    // Calculate u parameter and test bound
    u = glm::dot(tv, p)*invDet;

    // The intersection lies outside the triangle
    if ((u < 0.0f) || (u > 1.0f)) return collision;

    // Prepare to test v parameter
    q = glm::cross(tv, edge1);

    // Calculate V parameter and test bound
    v = glm::dot(ray.direction, q)*invDet;

    // The intersection lies outside the triangle
    if ((v < 0.0f) || ((u + v) > 1.0f)) return collision;

    t = glm::dot(edge2, q)*invDet;

    if (t > EPSILON)
    {
        // Ray hit, get hit point and normal
        collision.hit = true;
        collision.distance = t;
        collision.normal = glm::normalize(glm::cross(edge1, edge2));
        collision.point = ((ray.position) + (((ray.direction) * (t))));
    }

    return collision;
}

// Get collision info between ray and quad
// NOTE: The points are expected to be in counter-clockwise winding
RayCollision GetRayCollisionQuad(const Ray ray, const vec3 p1, const vec3 p2, const vec3 p3, const vec3 p4)
{
    auto collision = RayCollision{ 0 };

    collision = GetRayCollisionTriangle(ray, p1, p2, p4);

    if (!collision.hit) collision = GetRayCollisionTriangle(ray, p2, p3, p4);

    return collision;
}

//----------------------------------------------------------------------------------
// Module Internal Functions Definition
//----------------------------------------------------------------------------------
#if SUPPORT_FILEFORMAT_IQM || SUPPORT_FILEFORMAT_GLTF
// Build pose from parent joints
// NOTE: Required for animations loading (required by IQM and GLTF)
static void BuildPoseFromParentJoints(const BoneInfo *bones, const i32 boneCount, Transform *transforms)
{
    for (i32 i = 0; i < boneCount; i++)
    {
        if (bones[i].parent >= 0)
        {
            if (bones[i].parent > i)
            {
                LogWarning("Skipping bone not topologically sorted: Bone {} has parent {}", i, bones[i].parent);
                continue;
            }
            transforms[i].rotation = ((transforms[bones[i].parent].rotation) * (transforms[i].rotation));
            transforms[i].scale = ((transforms[i].scale) * (transforms[bones[i].parent].scale));
            transforms[i].translation = ((transforms[i].translation) * (transforms[bones[i].parent].scale));
            transforms[i].translation = ((transforms[bones[i].parent].rotation)*(transforms[i].translation));
            transforms[i].translation = ((transforms[i].translation) + (transforms[bones[i].parent].translation));
        }
    }
}
#endif

#if SUPPORT_FILEFORMAT_OBJ
// Load OBJ mesh data
// Notes to keep in mind:
//  - A mesh is created for every material present in the obj file
//  - The model.meshCount is therefore the materialCount returned from tinyobj
//  - The mesh is automatically triangulated by tinyobj
static Model LoadOBJ(const char *fileName)
{
    auto objAttributes = tinyobj_attrib_t{ 0 };
    tinyobj_shape_t *objShapes = nullptr;
    u32 objShapeCount = 0;

    tinyobj_material_t *objMaterials = nullptr;
    u32 objMaterialCount = 0;

    auto model = Model{ 0 };
    model.transform = mat4(1.0f);

    const auto fileText = LoadFileText(fileName);

    if (fileText.empty())
    {
        LogWarning("MODEL: [{}] Unable to read obj file", fileName);
        return model;
    }

    char currentDir[MAX_FILEPATH_LENGTH] = { 0 };
    const auto currentDirPath = GetWorkingDirectory();
    snprintf(currentDir, MAX_FILEPATH_LENGTH, "%s", currentDirPath.c_str()); // Save current working directory
    const auto workingDir = GetDirectoryPath(fileName);               // Switch to OBJ directory for material path correctness
    if (CHDIR(workingDir.c_str()) != 0) LogWarning("MODEL: [{}] Failed to change working directory", workingDir.c_str());

    const auto dataSize = static_cast<u32>(fileText.size());

    const u32 flags = TINYOBJ_FLAG_TRIANGULATE;
    const i32 ret = tinyobj_parse_obj(&objAttributes, &objShapes, &objShapeCount, &objMaterials, &objMaterialCount, fileText.c_str(), dataSize, flags);

    if (ret != TINYOBJ_SUCCESS)
    {
        LogWarning("MODEL: Unable to read obj data {}", fileName);
        return model;
    }


    u32 faceVertIndex = 0;
    u32 nextShape = 1;
    auto lastMaterial = -1;
    u32 meshIndex = 0;

    // Count meshes
    u32 nextShapeEnd = objAttributes.num_face_num_verts;

    // See how many verts till the next shape
    if (objShapeCount > 1) nextShapeEnd = objShapes[nextShape].face_offset;

    // Walk all the faces
    for (u32 faceId = 0; faceId < objAttributes.num_faces; faceId++)
    {
        if (faceId >= nextShapeEnd)
        {
            // Try to find the last vert in the next shape
            nextShape++;
            if (nextShape < objShapeCount) nextShapeEnd = objShapes[nextShape].face_offset;
            else nextShapeEnd = objAttributes.num_face_num_verts; // This is actually the total number of face verts in the file, not faces
            meshIndex++;
        }
        else if ((lastMaterial != -1) && (objAttributes.material_ids[faceId] != lastMaterial))
        {
            meshIndex++; // If this is a new material, a new mesh is allocated
        }

        lastMaterial = objAttributes.material_ids[faceId];
        faceVertIndex += objAttributes.face_num_verts[faceId];
    }

    // Allocate the base meshes and materials
    model.meshCount = static_cast<i32>(meshIndex) + 1;
    model.meshes = static_cast<Mesh *>(MemAlloc(sizeof(Mesh)*model.meshCount));

    if (objMaterialCount > 0)
    {
        model.materialCount = static_cast<i32>(objMaterialCount);
        model.materials = static_cast<Material *>(MemAlloc(sizeof(Material)*objMaterialCount));
    }
    else // Allocate at least one material
    {
        model.materialCount = 1;
        model.materials = static_cast<Material *>(MemAlloc(sizeof(Material)*1));
    }

    model.meshMaterial = static_cast<i32 *>(MemAlloc(sizeof(i32)*model.meshCount));

    // See how many verts are in each mesh
    auto localMeshVertexCounts = static_cast<u32 *>(MemAlloc(sizeof(u32)*model.meshCount));

    faceVertIndex = 0;
    nextShapeEnd = objAttributes.num_face_num_verts;
    lastMaterial = -1;
    meshIndex = 0;
    u32 localMeshVertexCount = 0;

    nextShape = 1;
    if (objShapeCount > 1) nextShapeEnd = objShapes[nextShape].face_offset;

    // Walk all the faces
    for (u32 faceId = 0; faceId < objAttributes.num_faces; faceId++)
    {
        auto newMesh = false; // Is a new mesh required?
        if (faceId >= nextShapeEnd)
        {
            // Try to find the last vert in the next shape
            nextShape++;
            if (nextShape < objShapeCount) nextShapeEnd = objShapes[nextShape].face_offset;
            else nextShapeEnd = objAttributes.num_face_num_verts; // this is actually the total number of face verts in the file, not faces

            newMesh = true;
        }
        else if ((lastMaterial != -1) && (objAttributes.material_ids[faceId] != lastMaterial))
        {
            newMesh = true;
        }

        lastMaterial = objAttributes.material_ids[faceId];

        if (newMesh)
        {
            localMeshVertexCounts[meshIndex] = localMeshVertexCount;

            localMeshVertexCount = 0;
            meshIndex++;
        }

        faceVertIndex += objAttributes.face_num_verts[faceId];
        localMeshVertexCount += objAttributes.face_num_verts[faceId];
    }

    localMeshVertexCounts[meshIndex] = localMeshVertexCount;

    for (i32 i = 0; i < model.meshCount; i++)
    {
        // Allocate the buffers for each mesh
        const u32 vertexCount = localMeshVertexCounts[i];

        model.meshes[i].vertexCount = static_cast<i32>(vertexCount);
        model.meshes[i].triangleCount = static_cast<i32>(vertexCount)/3;

        model.meshes[i].vertices = static_cast<f32 *>(MemAlloc(sizeof(f32)*vertexCount*3));
        model.meshes[i].normals = static_cast<f32 *>(MemAlloc(sizeof(f32)*vertexCount*3));
    #if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
        model.meshes[i].texcoords = static_cast<f32 *>(MemAlloc(sizeof(f32)*vertexCount*2));
        model.meshes[i].colors = static_cast<u8 *>(MemAlloc(sizeof(u8)*vertexCount*4));
    #else
        if (objAttributes.texcoords != nullptr && objAttributes.num_texcoords > 0)
            model.meshes[i].texcoords = static_cast<f32 *>(MemAlloc(sizeof(f32)*vertexCount*2));
        else model.meshes[i].texcoords = nullptr;
        model.meshes[i].colors = nullptr;
    #endif
    }

    MemFree(localMeshVertexCounts);
    localMeshVertexCounts = nullptr;

    // Fill meshes
    faceVertIndex = 0;

    nextShapeEnd = objAttributes.num_face_num_verts;

    // See how many verts till the next shape
    nextShape = 1;
    if (objShapeCount > 1) nextShapeEnd = objShapes[nextShape].face_offset;
    lastMaterial = -1;
    meshIndex = 0;
    localMeshVertexCount = 0;

    // Walk all the faces
    for (u32 faceId = 0; faceId < objAttributes.num_faces; faceId++)
    {
        auto newMesh = false; // Is a new mesh required?
        if (faceId >= nextShapeEnd)
        {
            // Try to find the last vert in the next shape
            nextShape++;
            if (nextShape < objShapeCount) nextShapeEnd = objShapes[nextShape].face_offset;
            else nextShapeEnd = objAttributes.num_face_num_verts; // This is actually the total number of face verts in the file, not faces
            newMesh = true;
        }

        // If this is a new material, a new mesh is allocated
        if (lastMaterial != -1 && objAttributes.material_ids[faceId] != lastMaterial) newMesh = true;
        lastMaterial = objAttributes.material_ids[faceId];

        if (newMesh)
        {
            localMeshVertexCount = 0;
            meshIndex++;
        }

        auto matId = 0;
        if ((lastMaterial >= 0) && (lastMaterial < static_cast<i32>(objMaterialCount))) matId = lastMaterial;

        model.meshMaterial[meshIndex] = matId;

        for (i32 f = 0; f < objAttributes.face_num_verts[faceId]; f++)
        {
            const i32 vertIndex = objAttributes.faces[faceVertIndex].v_idx;
            const i32 normalIndex = objAttributes.faces[faceVertIndex].vn_idx;
            const i32 texcordIndex = objAttributes.faces[faceVertIndex].vt_idx;

            // NOTE: Out-of-range indices from malformed files are skipped, keeping zeroed values
            if ((vertIndex >= 0) && (vertIndex < static_cast<i32>(objAttributes.num_vertices)))
            {
                for (i32 i = 0; i < 3; i++) model.meshes[meshIndex].vertices[localMeshVertexCount*3 + i] = objAttributes.vertices[vertIndex*3 + i];
            }

            if ((objAttributes.texcoords != nullptr) && (texcordIndex != TINYOBJ_INVALID_INDEX) && (texcordIndex >= 0) && (texcordIndex < static_cast<i32>(objAttributes.num_texcoords)) && (model.meshes[meshIndex].texcoords))
            {
                for (i32 i = 0; i < 2; i++) model.meshes[meshIndex].texcoords[localMeshVertexCount*2 + i] = objAttributes.texcoords[texcordIndex*2 + i];
                model.meshes[meshIndex].texcoords[localMeshVertexCount*2 + 1] = 1.0f - model.meshes[meshIndex].texcoords[localMeshVertexCount*2 + 1];
            }

            if ((objAttributes.normals != nullptr) && (normalIndex != TINYOBJ_INVALID_INDEX) && (normalIndex >= 0) && (normalIndex < static_cast<i32>(objAttributes.num_normals)))
            {
                for (i32 i = 0; i < 3; i++) model.meshes[meshIndex].normals[localMeshVertexCount*3 + i] = objAttributes.normals[normalIndex*3 + i];
            }
            else
            {
                model.meshes[meshIndex].normals[localMeshVertexCount*3 + 0] = 0.0f;
                model.meshes[meshIndex].normals[localMeshVertexCount*3 + 1] = 1.0f;
                model.meshes[meshIndex].normals[localMeshVertexCount*3 + 2] = 0.0f;
            }
        #if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
            for (i32 i = 0; i < 4; i++) model.meshes[meshIndex].colors[localMeshVertexCount*4 + i] = 255;
        #endif
            faceVertIndex++;
            localMeshVertexCount++;
        }
    }

    if (objMaterialCount > 0) ProcessMaterialsOBJ(model.materials, objMaterials, static_cast<i32>(objMaterialCount));
    else model.materials[0] = LoadMaterialDefault(); // Set default material for the mesh

    tinyobj_attrib_free(&objAttributes);
    tinyobj_shapes_free(objShapes, objShapeCount);
    tinyobj_materials_free(objMaterials, objMaterialCount);

    // Restore current working directory
    if (CHDIR(currentDir) != 0)
    {
        LogWarning("MODEL: [{}] Failed to change working directory", currentDir);
    }

    return model;
}
#endif

#if SUPPORT_FILEFORMAT_IQM
// Load IQM mesh data
static Model LoadIQM(const char *fileName)
{
    #define IQM_MAGIC        "INTERQUAKEMODEL"  // IQM file magic number
    #define IQM_VERSION              2          // Only IQM version 2 supported

    #define BONE_NAME_LENGTH        32          // BoneInfo name string length
    #define MESH_NAME_LENGTH        32          // Mesh name string length
    #define MATERIAL_NAME_LENGTH    32          // Material name string length

    auto dataSize = 0;
    auto fileData = LoadFileData(fileName, &dataSize);
    u8 *fileDataPtr = fileData;

    // IQM file structs
    //-----------------------------------------------------------------------------------
    typedef struct IQMHeader {
        char magic[16];
        u32 version;
        u32 dataSize;
        u32 flags;
        u32 num_text, ofs_text;
        u32 num_meshes, ofs_meshes;
        u32 num_vertexarrays, num_vertexes, ofs_vertexarrays;
        u32 num_triangles, ofs_triangles, ofs_adjacency;
        u32 num_joints, ofs_joints;
        u32 num_poses, ofs_poses;
        u32 num_anims, ofs_anims;
        u32 num_frames, num_framechannels, ofs_frames, ofs_bounds;
        u32 num_comment, ofs_comment;
        u32 num_extensions, ofs_extensions;
    } IQMHeader;

    typedef struct IQMMesh {
        u32 name;
        u32 material;
        u32 first_vertex, num_vertexes;
        u32 first_triangle, num_triangles;
    } IQMMesh;

    typedef struct IQMTriangle {
        u32 vertex[3];
    } IQMTriangle;

    typedef struct IQMJoint {
        u32 name;
        i32 parent;
        f32 translate[3], rotate[4], scale[3];
    } IQMJoint;

    typedef struct IQMVertexArray {
        u32 type;
        u32 flags;
        u32 format;
        u32 size;
        u32 offset;
    } IQMVertexArray;

    // NOTE: Below IQM structures are not used but listed for reference
    /*
    typedef struct IQMAdjacency {
        unsigned int triangle[3];
    } IQMAdjacency;

    typedef struct IQMPose {
        int parent;
        unsigned int mask;
        float channeloffset[10];
        float channelscale[10];
    } IQMPose;

    typedef struct IQMAnim {
        unsigned int name;
        unsigned int first_frame, num_frames;
        float framerate;
        unsigned int flags;
    } IQMAnim;

    typedef struct IQMBounds {
        float bbmin[3], bbmax[3];
        float xyradius, radius;
    } IQMBounds;
    */
    //-----------------------------------------------------------------------------------

    // IQM vertex data types
    enum {
        IQM_POSITION     = 0,
        IQM_TEXCOORD     = 1,
        IQM_NORMAL       = 2,
        IQM_TANGENT      = 3,       // NOTE: Tangents unused by default
        IQM_BLENDINDEXES = 4,
        IQM_BLENDWEIGHTS = 5,
        IQM_COLOR        = 6,
        IQM_CUSTOM       = 0x10     // NOTE: Custom vertex values unused by default
    };

    auto model = Model{ 0 };

    IQMMesh *imesh = nullptr;
    IQMTriangle *tri = nullptr;
    IQMVertexArray *va = nullptr;
    IQMJoint *ijoint = nullptr;

    f32 *vertex = nullptr;
    f32 *normal = nullptr;
    f32 *text = nullptr;
    char *blendi = nullptr;
    u8 *blendw = nullptr;
    u8 *color = nullptr;

    // In case file can not be read, return an empty model
    if (fileDataPtr == nullptr) return model;

    const auto basePath = GetDirectoryPath(fileName);

    // Read IQM header
    auto iqmHeader = reinterpret_cast<IQMHeader *>(fileDataPtr);

    if (memcmp(iqmHeader->magic, IQM_MAGIC, sizeof(IQM_MAGIC)) != 0)
    {
        LogWarning("MODEL: [{}] IQM file is not a valid model", fileName);
        UnloadFileData(fileData);
        return model;
    }

    if (iqmHeader->version != IQM_VERSION)
    {
        LogWarning("MODEL: [{}] IQM file version not supported ({})", fileName, iqmHeader->version);
        UnloadFileData(fileData);
        return model;
    }

    //fileDataPtr += sizeof(IQMHeader);       // Move file data pointer

    // Meshes data processing
    imesh = static_cast<IQMMesh *>(calloc(iqmHeader->num_meshes, sizeof(IQMMesh)));
    //fseek(iqmFile, iqmHeader->ofs_meshes, SEEK_SET);
    //fread(imesh, sizeof(IQMMesh)*iqmHeader->num_meshes, 1, iqmFile);
    memcpy(imesh, fileDataPtr + iqmHeader->ofs_meshes, iqmHeader->num_meshes*sizeof(IQMMesh));

    model.meshCount = static_cast<i32>(iqmHeader->num_meshes);
    model.meshes = static_cast<Mesh *>(calloc(model.meshCount, sizeof(Mesh)));

    model.materialCount = model.meshCount;
    model.materials = static_cast<Material *>(calloc(model.materialCount, sizeof(Material)));
    model.meshMaterial = static_cast<i32 *>(calloc(model.meshCount, sizeof(i32)));

    char name[MESH_NAME_LENGTH] = { 0 };
    char material[MATERIAL_NAME_LENGTH] = { 0 };

    for (i32 i = 0; i < model.meshCount; i++)
    {
        //fseek(iqmFile, iqmHeader->ofs_text + imesh[a].name, SEEK_SET);
        //fread(name, sizeof(char), MESH_NAME_LENGTH, iqmFile);
        memcpy(name, fileDataPtr + iqmHeader->ofs_text + imesh[i].name, MESH_NAME_LENGTH*sizeof(char));

        //fseek(iqmFile, iqmHeader->ofs_text + imesh[a].material, SEEK_SET);
        //fread(material, sizeof(char), MATERIAL_NAME_LENGTH, iqmFile);
        memcpy(material, fileDataPtr + iqmHeader->ofs_text + imesh[i].material, MATERIAL_NAME_LENGTH*sizeof(char));

        model.materials[i] = LoadMaterialDefault();
        if (TextLength(material) > 0) model.materials[i].maps[MATERIAL_MAP_ALBEDO].texture = LoadTexture(TextFormat("%s/%s", basePath.c_str(), material));

        model.meshMaterial[i] = i;

        LogDebug("MODEL: [{}] mesh name ({}), material ({})", fileName, name, material);

        model.meshes[i].vertexCount = static_cast<i32>(imesh[i].num_vertexes);

        model.meshes[i].vertices = static_cast<f32 *>(calloc(model.meshes[i].vertexCount*3, sizeof(f32)));       // Default vertex positions
        model.meshes[i].normals = static_cast<f32 *>(calloc(model.meshes[i].vertexCount*3, sizeof(f32)));        // Default vertex normals
        model.meshes[i].texcoords = static_cast<f32 *>(calloc(model.meshes[i].vertexCount*2, sizeof(f32)));      // Default vertex texcoords

        model.meshes[i].boneIndices = static_cast<u8 *>(calloc(model.meshes[i].vertexCount*4, sizeof(u8)));  // Up-to 4 bones supported!
        model.meshes[i].boneWeights = static_cast<f32 *>(calloc(model.meshes[i].vertexCount*4, sizeof(f32)));                  // Up-to 4 bones supported!

        model.meshes[i].triangleCount = static_cast<i32>(imesh[i].num_triangles);
        model.meshes[i].indices = static_cast<u16 *>(calloc(model.meshes[i].triangleCount*3, sizeof(u16)));
    }

    // Triangles data processing
    tri = static_cast<IQMTriangle *>(calloc(iqmHeader->num_triangles, sizeof(IQMTriangle)));
    //fseek(iqmFile, iqmHeader->ofs_triangles, SEEK_SET);
    //fread(tri, sizeof(IQMTriangle), iqmHeader->num_triangles, iqmFile);
    memcpy(tri, fileDataPtr + iqmHeader->ofs_triangles, iqmHeader->num_triangles*sizeof(IQMTriangle));

    for (i32 m = 0; m < model.meshCount; m++)
    {
        auto tcounter = 0;

        for (u32 i = imesh[m].first_triangle; i < (imesh[m].first_triangle + imesh[m].num_triangles); i++)
        {
            // IQM triangles indexes are stored in counter-clockwise, but raylib processes the index in linear order,
            // expecting they point to the counter-clockwise vertex triangle, so triangle indexes need to be reversed
            // NOTE: raylib renders vertex data in counter-clockwise order (standard convention) by default
            model.meshes[m].indices[tcounter + 2] = tri[i].vertex[0] - imesh[m].first_vertex;
            model.meshes[m].indices[tcounter + 1] = tri[i].vertex[1] - imesh[m].first_vertex;
            model.meshes[m].indices[tcounter] = tri[i].vertex[2] - imesh[m].first_vertex;
            tcounter += 3;
        }
    }

    // Vertex arrays data processing
    va = static_cast<IQMVertexArray *>(calloc(iqmHeader->num_vertexarrays, sizeof(IQMVertexArray)));
    //fseek(iqmFile, iqmHeader->ofs_vertexarrays, SEEK_SET);
    //fread(va, sizeof(IQMVertexArray), iqmHeader->num_vertexarrays, iqmFile);
    memcpy(va, fileDataPtr + iqmHeader->ofs_vertexarrays, iqmHeader->num_vertexarrays*sizeof(IQMVertexArray));

    for (u32 i = 0; i < iqmHeader->num_vertexarrays; i++)
    {
        switch (va[i].type)
        {
            case IQM_POSITION:
            {
                vertex = static_cast<f32 *>(calloc(iqmHeader->num_vertexes*3, sizeof(f32)));
                //fseek(iqmFile, va[a].offset, SEEK_SET);
                //fread(vertex, iqmHeader->num_vertexes*3*sizeof(float), 1, iqmFile);
                memcpy(vertex, fileDataPtr + va[i].offset, iqmHeader->num_vertexes*3*sizeof(f32));

                for (u32 m = 0; m < iqmHeader->num_meshes; m++)
                {
                    auto vCounter = 0;
                    for (u32 i = imesh[m].first_vertex*3; i < (imesh[m].first_vertex + imesh[m].num_vertexes)*3; i++)
                    {
                        model.meshes[m].vertices[vCounter] = vertex[i];
                        if (model.meshes[m].animVertices != nullptr) model.meshes[m].animVertices[vCounter] = vertex[i];
                        vCounter++;
                    }
                }
            } break;
            case IQM_NORMAL:
            {
                normal = static_cast<f32 *>(calloc(iqmHeader->num_vertexes*3, sizeof(f32)));
                //fseek(iqmFile, va[a].offset, SEEK_SET);
                //fread(normal, iqmHeader->num_vertexes*3*sizeof(float), 1, iqmFile);
                memcpy(normal, fileDataPtr + va[i].offset, iqmHeader->num_vertexes*3*sizeof(f32));

                for (u32 m = 0; m < iqmHeader->num_meshes; m++)
                {
                    auto vCounter = 0;
                    for (u32 i = imesh[m].first_vertex*3; i < (imesh[m].first_vertex + imesh[m].num_vertexes)*3; i++)
                    {
                        model.meshes[m].normals[vCounter] = normal[i];
                        if (model.meshes[m].animNormals != nullptr) model.meshes[m].animNormals[vCounter] = normal[i];
                        vCounter++;
                    }
                }
            } break;
            case IQM_TEXCOORD:
            {
                text = static_cast<f32 *>(calloc(iqmHeader->num_vertexes*2, sizeof(f32)));
                //fseek(iqmFile, va[a].offset, SEEK_SET);
                //fread(text, iqmHeader->num_vertexes*2*sizeof(float), 1, iqmFile);
                memcpy(text, fileDataPtr + va[i].offset, iqmHeader->num_vertexes*2*sizeof(f32));

                for (u32 m = 0; m < iqmHeader->num_meshes; m++)
                {
                    auto vCounter = 0;
                    for (u32 i = imesh[m].first_vertex*2; i < (imesh[m].first_vertex + imesh[m].num_vertexes)*2; i++)
                    {
                        model.meshes[m].texcoords[vCounter] = text[i];
                        vCounter++;
                    }
                }
            } break;
            case IQM_BLENDINDEXES:
            {
                blendi = static_cast<char *>(calloc(iqmHeader->num_vertexes*4, sizeof(char)));
                //fseek(iqmFile, va[a].offset, SEEK_SET);
                //fread(blendi, iqmHeader->num_vertexes*4*sizeof(char), 1, iqmFile);
                memcpy(blendi, fileDataPtr + va[i].offset, iqmHeader->num_vertexes*4*sizeof(char));

                for (u32 m = 0; m < iqmHeader->num_meshes; m++)
                {
                    auto boneCounter = 0;
                    for (u32 i = imesh[m].first_vertex*4; i < (imesh[m].first_vertex + imesh[m].num_vertexes)*4; i++)
                    {
                        model.meshes[m].boneIndices[boneCounter] = blendi[i];
                        boneCounter++;
                    }
                }
            } break;
            case IQM_BLENDWEIGHTS:
            {
                blendw = static_cast<u8 *>(calloc(iqmHeader->num_vertexes*4, sizeof(u8)));
                //fseek(iqmFile, va[a].offset, SEEK_SET);
                //fread(blendw, iqmHeader->num_vertexes*4*sizeof(unsigned char), 1, iqmFile);
                memcpy(blendw, fileDataPtr + va[i].offset, iqmHeader->num_vertexes*4*sizeof(u8));

                for (u32 m = 0; m < iqmHeader->num_meshes; m++)
                {
                    auto boneCounter = 0;
                    for (u32 i = imesh[m].first_vertex*4; i < (imesh[m].first_vertex + imesh[m].num_vertexes)*4; i++)
                    {
                        model.meshes[m].boneWeights[boneCounter] = static_cast<f32>(blendw[i])/255.0f;
                        boneCounter++;
                    }
                }
            } break;
            case IQM_COLOR:
            {
                color = static_cast<u8 *>(calloc(iqmHeader->num_vertexes*4, sizeof(u8)));
                //fseek(iqmFile, va[a].offset, SEEK_SET);
                //fread(blendw, iqmHeader->num_vertexes*4*sizeof(unsigned char), 1, iqmFile);
                memcpy(color, fileDataPtr + va[i].offset, iqmHeader->num_vertexes*4*sizeof(u8));

                for (u32 m = 0; m < iqmHeader->num_meshes; m++)
                {
                    model.meshes[m].colors = static_cast<u8 *>(calloc(model.meshes[m].vertexCount*4, sizeof(u8)));

                    auto vCounter = 0;
                    for (u32 i = imesh[m].first_vertex*4; i < (imesh[m].first_vertex + imesh[m].num_vertexes)*4; i++)
                    {
                        model.meshes[m].colors[vCounter] = color[i];
                        vCounter++;
                    }
                }
            } break;
        }
    }

    // Bones (joints) data processing
    ijoint = static_cast<IQMJoint *>(calloc(iqmHeader->num_joints, sizeof(IQMJoint)));
    //fseek(iqmFile, iqmHeader->ofs_joints, SEEK_SET);
    //fread(ijoint, sizeof(IQMJoint), iqmHeader->num_joints, iqmFile);
    memcpy(ijoint, fileDataPtr + iqmHeader->ofs_joints, iqmHeader->num_joints*sizeof(IQMJoint));

    model.skeleton.boneCount = iqmHeader->num_joints;
    model.skeleton.bones = static_cast<BoneInfo *>(calloc(iqmHeader->num_joints, sizeof(BoneInfo)));
    model.skeleton.bindPose = static_cast<Transform *>(calloc(iqmHeader->num_joints, sizeof(Transform)));

    for (u32 i = 0; i < iqmHeader->num_joints; i++)
    {
        // Bones
        model.skeleton.bones[i].parent = ijoint[i].parent;
        //fseek(iqmFile, iqmHeader->ofs_text + ijoint[a].name, SEEK_SET);
        //fread(model.bones[a].name, sizeof(char), BONE_NAME_LENGTH, iqmFile);
        memcpy(model.skeleton.bones[i].name, fileDataPtr + iqmHeader->ofs_text + ijoint[i].name, BONE_NAME_LENGTH*sizeof(char));

        // Bind pose (base pose)
        model.skeleton.bindPose[i].translation.x = ijoint[i].translate[0];
        model.skeleton.bindPose[i].translation.y = ijoint[i].translate[1];
        model.skeleton.bindPose[i].translation.z = ijoint[i].translate[2];

        model.skeleton.bindPose[i].rotation.x = ijoint[i].rotate[0];
        model.skeleton.bindPose[i].rotation.y = ijoint[i].rotate[1];
        model.skeleton.bindPose[i].rotation.z = ijoint[i].rotate[2];
        model.skeleton.bindPose[i].rotation.w = ijoint[i].rotate[3];

        model.skeleton.bindPose[i].scale.x = ijoint[i].scale[0];
        model.skeleton.bindPose[i].scale.y = ijoint[i].scale[1];
        model.skeleton.bindPose[i].scale.z = ijoint[i].scale[2];
    }

    BuildPoseFromParentJoints(model.skeleton.bones, static_cast<i32>(model.skeleton.boneCount), model.skeleton.bindPose);

    // Initialize runtime animation data: current pose and bone matrices
    model.currentPose = static_cast<Transform *>(calloc((model.skeleton.boneCount > 0) ? model.skeleton.boneCount : 1, sizeof(Transform)));
    model.boneMatrices = static_cast<mat4 *>(calloc((model.skeleton.boneCount > 0) ? model.skeleton.boneCount : 1, sizeof(mat4)));
    for (u32 j = 0; j < model.skeleton.boneCount; j++) model.boneMatrices[j] = mat4(1.0f);

    UnloadFileData(fileData);

    free(imesh);
    free(tri);
    free(va);
    free(vertex);
    free(normal);
    free(text);
    free(blendi);
    free(blendw);
    free(ijoint);
    free(color);

    return model;
}

// Load IQM animation data
static ModelAnimation *LoadModelAnimationsIQM(const char *fileName, i32 *animCount)
{
    #define IQM_MAGIC       "INTERQUAKEMODEL"   // IQM file magic number
    #define IQM_VERSION     2                   // only IQM version 2 supported

    auto dataSize = 0;
    auto fileData = LoadFileData(fileName, &dataSize);
    u8 *fileDataPtr = fileData;

    typedef struct IQMHeader {
        char magic[16];
        u32 version;
        u32 dataSize;
        u32 flags;
        u32 num_text, ofs_text;
        u32 num_meshes, ofs_meshes;
        u32 num_vertexarrays, num_vertexes, ofs_vertexarrays;
        u32 num_triangles, ofs_triangles, ofs_adjacency;
        u32 num_joints, ofs_joints;
        u32 num_poses, ofs_poses;
        u32 num_anims, ofs_anims;
        u32 num_frames, num_framechannels, ofs_frames, ofs_bounds;
        u32 num_comment, ofs_comment;
        u32 num_extensions, ofs_extensions;
    } IQMHeader;

    typedef struct IQMJoint {
        u32 name;
        i32 parent;
        f32 translate[3], rotate[4], scale[3];
    } IQMJoint;

    typedef struct IQMPose {
        i32 parent;
        u32 mask;
        f32 channeloffset[10];
        f32 channelscale[10];
    } IQMPose;

    typedef struct IQMAnim {
        u32 name;
        u32 first_frame, num_frames;
        f32 framerate;
        u32 flags;
    } IQMAnim;

    // In case file can not be read, return an empty model
    if (fileDataPtr == nullptr) return nullptr;

    // Read IQM header
    auto iqmHeader = reinterpret_cast<IQMHeader *>(fileDataPtr);

    if (memcmp(iqmHeader->magic, IQM_MAGIC, sizeof(IQM_MAGIC)) != 0)
    {
        LogWarning("MODEL: [{}] IQM file is not a valid model", fileName);
        UnloadFileData(fileData);
        return nullptr;
    }

    if (iqmHeader->version != IQM_VERSION)
    {
        LogWarning("MODEL: [{}] IQM file version not supported ({})", fileName, iqmHeader->version);
        UnloadFileData(fileData);
        return nullptr;
    }

    // Get bones data
    auto poses = static_cast<IQMPose *>(calloc(iqmHeader->num_poses, sizeof(IQMPose)));
    //fseek(iqmFile, iqmHeader->ofs_poses, SEEK_SET);
    //fread(poses, sizeof(IQMPose), iqmHeader->num_poses, iqmFile);
    memcpy(poses, fileDataPtr + iqmHeader->ofs_poses, iqmHeader->num_poses*sizeof(IQMPose));

    // Get animations data
    *animCount = static_cast<i32>(iqmHeader->num_anims);
    auto anim = static_cast<IQMAnim *>(calloc(iqmHeader->num_anims, sizeof(IQMAnim)));
    //fseek(iqmFile, iqmHeader->ofs_anims, SEEK_SET);
    //fread(anim, sizeof(IQMAnim), iqmHeader->num_anims, iqmFile);
    memcpy(anim, fileDataPtr + iqmHeader->ofs_anims, iqmHeader->num_anims*sizeof(IQMAnim));

    auto animations = static_cast<ModelAnimation *>(calloc(iqmHeader->num_anims, sizeof(ModelAnimation)));

    // frameposes
    auto framedata = static_cast<u16 *>(calloc(iqmHeader->num_frames*iqmHeader->num_framechannels, sizeof(u16)));
    //fseek(iqmFile, iqmHeader->ofs_frames, SEEK_SET);
    //fread(framedata, sizeof(unsigned short), iqmHeader->num_frames*iqmHeader->num_framechannels, iqmFile);
    memcpy(framedata, fileDataPtr + iqmHeader->ofs_frames, iqmHeader->num_frames*iqmHeader->num_framechannels*sizeof(u16));

    // joints
    auto joints = static_cast<IQMJoint *>(calloc(iqmHeader->num_joints, sizeof(IQMJoint)));
    memcpy(joints, fileDataPtr + iqmHeader->ofs_joints, iqmHeader->num_joints*sizeof(IQMJoint));

    for (u32 a = 0; a < iqmHeader->num_anims; a++)
    {
        animations[a].boneCount = iqmHeader->num_poses;
        auto bones = static_cast<BoneInfo *>(calloc(iqmHeader->num_poses, sizeof(BoneInfo)));

        animations[a].keyframeCount = static_cast<i32>(anim[a].num_frames);
        animations[a].keyframePoses = static_cast<Transform * *>(calloc(anim[a].num_frames, sizeof(Transform *)));
        memcpy(animations[a].name, fileDataPtr + iqmHeader->ofs_text + anim[a].name, 32);
        // TODO: Store animation framerate data?
        //animations[a].framerate = anim.framerate;

        LogInfo("MODEL: [{}] Loaded animation: {} | Frames: {} | Framerate: {:.6f}", fileName, animations[a].name, animations[a].keyframeCount, anim[a].framerate);

        for (u32 j = 0; j < iqmHeader->num_poses; j++)
        {
            bones[j].parent = poses[j].parent;

            // NOTE: No need to store bones names, bones only required to generate keyframe poses
            //if (iqmHeader->num_joints > 0) memcpy(bones[j].name, fileDataPtr + iqmHeader->ofs_text + joints[j].name, BONE_NAME_LENGTH*sizeof(char));
            //else memcpy(bones[j].name, "ANIMJOINTNAME", 13); // Default bone name otherwise
        }

        for (u32 j = 0; j < anim[a].num_frames; j++)
            animations[a].keyframePoses[j] = static_cast<Transform *>(calloc(iqmHeader->num_poses, sizeof(Transform)));

        i32 dcounter = static_cast<i32>(anim[a].first_frame*iqmHeader->num_framechannels);

        for (u32 frame = 0; frame < anim[a].num_frames; frame++)
        {
            for (u32 i = 0; i < iqmHeader->num_poses; i++)
            {
                animations[a].keyframePoses[frame][i].translation.x = poses[i].channeloffset[0];

                if (poses[i].mask & 0x01)
                {
                    animations[a].keyframePoses[frame][i].translation.x += static_cast<f32>(framedata[dcounter])*poses[i].channelscale[0];
                    dcounter++;
                }

                animations[a].keyframePoses[frame][i].translation.y = poses[i].channeloffset[1];

                if (poses[i].mask & 0x02)
                {
                    animations[a].keyframePoses[frame][i].translation.y += static_cast<f32>(framedata[dcounter])*poses[i].channelscale[1];
                    dcounter++;
                }

                animations[a].keyframePoses[frame][i].translation.z = poses[i].channeloffset[2];

                if (poses[i].mask & 0x04)
                {
                    animations[a].keyframePoses[frame][i].translation.z += static_cast<f32>(framedata[dcounter])*poses[i].channelscale[2];
                    dcounter++;
                }

                animations[a].keyframePoses[frame][i].rotation.x = poses[i].channeloffset[3];

                if (poses[i].mask & 0x08)
                {
                    animations[a].keyframePoses[frame][i].rotation.x += static_cast<f32>(framedata[dcounter])*poses[i].channelscale[3];
                    dcounter++;
                }

                animations[a].keyframePoses[frame][i].rotation.y = poses[i].channeloffset[4];

                if (poses[i].mask & 0x10)
                {
                    animations[a].keyframePoses[frame][i].rotation.y += static_cast<f32>(framedata[dcounter])*poses[i].channelscale[4];
                    dcounter++;
                }

                animations[a].keyframePoses[frame][i].rotation.z = poses[i].channeloffset[5];

                if (poses[i].mask & 0x20)
                {
                    animations[a].keyframePoses[frame][i].rotation.z += static_cast<f32>(framedata[dcounter])*poses[i].channelscale[5];
                    dcounter++;
                }

                animations[a].keyframePoses[frame][i].rotation.w = poses[i].channeloffset[6];

                if (poses[i].mask & 0x40)
                {
                    animations[a].keyframePoses[frame][i].rotation.w += static_cast<f32>(framedata[dcounter])*poses[i].channelscale[6];
                    dcounter++;
                }

                animations[a].keyframePoses[frame][i].scale.x = poses[i].channeloffset[7];

                if (poses[i].mask & 0x80)
                {
                    animations[a].keyframePoses[frame][i].scale.x += static_cast<f32>(framedata[dcounter])*poses[i].channelscale[7];
                    dcounter++;
                }

                animations[a].keyframePoses[frame][i].scale.y = poses[i].channeloffset[8];

                if (poses[i].mask & 0x100)
                {
                    animations[a].keyframePoses[frame][i].scale.y += static_cast<f32>(framedata[dcounter])*poses[i].channelscale[8];
                    dcounter++;
                }

                animations[a].keyframePoses[frame][i].scale.z = poses[i].channeloffset[9];

                if (poses[i].mask & 0x200)
                {
                    animations[a].keyframePoses[frame][i].scale.z += static_cast<f32>(framedata[dcounter])*poses[i].channelscale[9];
                    dcounter++;
                }

                animations[a].keyframePoses[frame][i].rotation = glm::normalize(animations[a].keyframePoses[frame][i].rotation);
            }
        }

        // Build frameposes
        for (u32 frame = 0; frame < anim[a].num_frames; frame++)
        {
            for (u32 i = 0; i < animations[a].boneCount; i++)
            {
                if (bones[i].parent >= 0)
                {
                    animations[a].keyframePoses[frame][i].rotation = ((animations[a].keyframePoses[frame][bones[i].parent].rotation) * (animations[a].keyframePoses[frame][i].rotation));
                    animations[a].keyframePoses[frame][i].translation = ((animations[a].keyframePoses[frame][bones[i].parent].rotation)*(animations[a].keyframePoses[frame][i].translation));
                    animations[a].keyframePoses[frame][i].translation = ((animations[a].keyframePoses[frame][i].translation) + (animations[a].keyframePoses[frame][bones[i].parent].translation));
                    animations[a].keyframePoses[frame][i].scale = ((animations[a].keyframePoses[frame][i].scale) * (animations[a].keyframePoses[frame][bones[i].parent].scale));
                }
            }
        }

        free(bones);
    }

    UnloadFileData(fileData);

    free(joints);
    free(framedata);
    free(poses);
    free(anim);

    return animations;
}

#endif

#if SUPPORT_FILEFORMAT_GLTF
// Load file data callback for cgltf
static cgltf_result LoadFileGLTFCallback(const struct cgltf_memory_options *memoryOptions, const struct cgltf_file_options *fileOptions, const char *path, cgltf_size *size, void **data)
{
    i32 filesize;
    auto filedata = LoadFileData(path, &filesize);

    if (filedata == nullptr) return cgltf_result_io_error;

    *size = filesize;
    *data = filedata;

    return cgltf_result_success;
}

// Release file data callback for cgltf
static void ReleaseFileGLTFCallback(const struct cgltf_memory_options *memoryOptions, const struct cgltf_file_options *fileOptions, void *data, const cgltf_size size)
{
    UnloadFileData(static_cast<u8 *>(data));
}

// Load image from different glTF provided methods (uri, path, buffer_view)
static Image LoadImageFromCgltfImage(const cgltf_image *cgltfImage, const char *texPath)
{
    auto image = Image{ nullptr };

    if (cgltfImage == nullptr) return image;

    if (cgltfImage->uri != nullptr)     // Check if image data is provided as an uri (base64 or path)
    {
        if ((strlen(cgltfImage->uri) > 5) &&
            (cgltfImage->uri[0] == 'd') &&
            (cgltfImage->uri[1] == 'a') &&
            (cgltfImage->uri[2] == 't') &&
            (cgltfImage->uri[3] == 'a') &&
            (cgltfImage->uri[4] == ':'))     // Check if image is provided as base64 text data
        {
            // Data URI Format: data:<mediatype>;base64,<data>

            // Find the comma
            auto i = 0;
            while ((cgltfImage->uri[i] != ',') && (cgltfImage->uri[i] != 0)) i++;

            if (cgltfImage->uri[i] == 0) LogWarning("IMAGE: glTF data URI is not a valid image");
            else
            {
                auto base64Size = static_cast<i32>(strlen(cgltfImage->uri + i + 1));
                while (cgltfImage->uri[i + base64Size] == '=') base64Size--;          // Ignore optional paddings
                const i32 numberOfEncodedBits = base64Size*6 - (base64Size*6)%8;      // Encoded bits minus extra bits, so it becomes a multiple of 8 bits
                const i32 outSize = numberOfEncodedBits >> 3;                         // Actual encoded bytes
                void *data = nullptr;

                auto options = cgltf_options{};
                options.file.read = LoadFileGLTFCallback;
                options.file.release = ReleaseFileGLTFCallback;
                const cgltf_result result = cgltf_load_buffer_base64(&options, outSize, cgltfImage->uri + i + 1, &data);

                if (result == cgltf_result_success)
                {
                    image = LoadImageFromMemory(".png", static_cast<u8 *>(data), outSize);
                    free(data);
                }
            }
        }
        else     // Check if image is provided as image path
        {
            image = LoadImage(TextFormat("%s/%s", texPath, cgltfImage->uri));
        }
    }
    else if ((cgltfImage->buffer_view != nullptr) && (cgltfImage->buffer_view->buffer->data != nullptr)) // Check if image is provided as data buffer
    {
        auto data = static_cast<u8 *>(calloc(cgltfImage->buffer_view->size, sizeof(u8)));
        auto offset = static_cast<i32>(cgltfImage->buffer_view->offset);
        const i32 stride = static_cast<i32>(cgltfImage->buffer_view->stride)? static_cast<i32>(cgltfImage->buffer_view->stride) : 1;

        // Copy buffer data to memory for loading
        for (u32 i = 0; i < cgltfImage->buffer_view->size; i++)
        {
            data[i] = (static_cast<u8 *>(cgltfImage->buffer_view->buffer->data))[offset];
            offset += stride;
        }

        // Check mime_type for image: (cgltfImage->mime_type == "image/png")
        // NOTE: Detected that some models define mime_type as "image\\/png"
        if ((strcmp(cgltfImage->mime_type, "image\\/png") == 0) || (strcmp(cgltfImage->mime_type, "image/png") == 0))
        {
            image = LoadImageFromMemory(".png", data, static_cast<i32>(cgltfImage->buffer_view->size));
        }
        else if ((strcmp(cgltfImage->mime_type, "image\\/jpeg") == 0) || (strcmp(cgltfImage->mime_type, "image/jpeg") == 0))
        {
            image = LoadImageFromMemory(".jpg", data, static_cast<i32>(cgltfImage->buffer_view->size));
        }
        else LogWarning("MODEL: glTF image data MIME type not recognized ({})", TextFormat("%s/%s", texPath, cgltfImage->uri).c_str());

        free(data);
    }

    return image;
}

// Load bone info from GLTF skin data
static BoneInfo *LoadBoneInfoGLTF(const cgltf_skin skin, u32 *boneCount)
{
    *boneCount = static_cast<u32>(skin.joints_count);
    auto bones = static_cast<BoneInfo *>(calloc(skin.joints_count, sizeof(BoneInfo)));

    for (u32 i = 0; i < skin.joints_count; i++)
    {
        const cgltf_node node = *skin.joints[i];
        if (node.name != nullptr) snprintf(bones[i].name, sizeof(bones[i].name), "%s", node.name);

        // Find parent bone index by walking up the node tree past any
        // non-joint ancestors (intermediate transform nodes used by some
        // DCC exporters), until we hit a node that is also in skin.joints.
        auto parentIndex = -1;
        cgltf_node *ancestor = node.parent;
        while (ancestor != nullptr && parentIndex == -1)
        {
            for (u32 j = 0; j < skin.joints_count; j++)
            {
                if (skin.joints[j] == ancestor) { parentIndex = static_cast<i32>(j); break; }
            }
            if (parentIndex == -1) ancestor = ancestor->parent;
        }

        bones[i].parent = parentIndex;
    }

    return bones;
}

// Load glTF file into model struct, .gltf and .glb supported
static Model LoadGLTF(const char *fileName)
{
    /*********************************************************************************************

        Function implemented by Wilhem Barbier(@wbrbr), with modifications by Tyler Bezera(@gamerfiend)
        Transform handling implemented by Paul Melis (@paulmelis)
        Reviewed by Ramon Santamaria (@raysan5)

        FEATURES:
          - Supports .gltf and .glb files
          - Supports embedded (base64) or external textures
          - Supports PBR metallic/roughness flow, loads material textures, values and colors
                     PBR specular/glossiness flow and extended texture flows not supported
          - Supports multiple meshes per model (every primitives is loaded as a separate mesh)
          - Supports basic animations
          - Transforms, including parent-child relations, are applied on the mesh data,
            but the hierarchy is not kept (as it can't be represented)
          - Mesh instances in the glTF file (a.e. same mesh linked from multiple nodes)
            are turned into separate raylib Meshes

        RESTRICTIONS:
          - Only triangle meshes supported
          - Vertex attribute types and formats supported:
              > Vertices (position): vec3: float
              > Normals: vec3: float
              > Texcoords: vec2: float
              > Colors: vec4: u8, u16, f32 (normalized)
              > Indices: u16, u32 (truncated to u16)
          - Scenes defined in the glTF file are ignored. All nodes in the file are used

    ***********************************************************************************************/

    // Macro to simplify attributes loading code
    #define LOAD_ATTRIBUTE(accesor, numComp, srcType, dstPtr) LOAD_ATTRIBUTE_CAST(accesor, numComp, srcType, dstPtr, srcType)

    #define LOAD_ATTRIBUTE_CAST(accesor, numComp, srcType, dstPtr, dstType) \
    { \
        i32 n = 0; \
        srcType *buffer = (srcType *)accesor->buffer_view->buffer->data + accesor->buffer_view->offset/sizeof(srcType) + accesor->offset/sizeof(srcType); \
        for (u32 k = 0; k < accesor->count; k++) \
        {\
            for (i32 l = 0; l < numComp; l++) \
            {\
                dstPtr[numComp*k + l] = (dstType)buffer[n + l];\
            }\
            n += static_cast<i32>((accesor->stride/sizeof(srcType)));\
        }\
    }

    auto model = Model{ 0 };

    // glTF file loading
    auto dataSize = 0;
    auto fileData = LoadFileData(fileName, &dataSize);

    if (fileData == nullptr) return model;

    // glTF data loading
    auto options = cgltf_options{};
    options.file.read = LoadFileGLTFCallback;
    options.file.release = ReleaseFileGLTFCallback;
    cgltf_data *data = nullptr;
    cgltf_result result = cgltf_parse(&options, fileData, dataSize, &data);

    if (result == cgltf_result_success)
    {
        if (data->file_type == cgltf_file_type_glb) LogInfo("MODEL: [{}] Model basic data (glb) loaded successfully", fileName);
        else if (data->file_type == cgltf_file_type_gltf) LogInfo("MODEL: [{}] Model basic data (glTF) loaded successfully", fileName);
        else LogWarning("MODEL: [{}] Model format not recognized", fileName);

        LogInfo("    > Meshes count: {}", data->meshes_count);
        LogInfo("    > Materials count: {} (+1 default)", data->materials_count);
        LogDebug("    > Buffers count: {}", data->buffers_count);
        LogDebug("    > Images count: {}", data->images_count);
        LogDebug("    > Textures count: {}", data->textures_count);

        // Force reading data buffers (fills buffer_view->buffer->data)
        // NOTE: If an uri is defined to base64 data or external path, it's automatically loaded
        result = cgltf_load_buffers(&options, data, fileName);
        if (result != cgltf_result_success) LogInfo("MODEL: [{}] Failed to load mesh/material buffers", fileName);

        auto primitivesCount = 0;
        auto dracoCompression = false;

        // NOTE: Load every primitive in the glTF as a separate raylib Mesh
        // Determine total number of meshes needed from the node hierarchy
        for (u32 i = 0; i < data->nodes_count; i++)
        {
            cgltf_node *node = &(data->nodes[i]);
            cgltf_mesh *mesh = node->mesh;
            if (!mesh) continue;

            for (u32 p = 0; p < mesh->primitives_count; p++)
            {
                if (mesh->primitives[p].has_draco_mesh_compression)
                {
                    dracoCompression = true;
                    LogWarning("MODEL: [{}] Failed to load mesh data, Draco compression not supported", fileName);
                    break;
                }
                else if (mesh->primitives[p].type == cgltf_primitive_type_triangles) primitivesCount++;
            }
        }

        if (dracoCompression)
        {
            LogWarning("MODEL: [{}] Failed to load glTF data", fileName);
            cgltf_free(data);
            UnloadFileData(fileData);
            return model;
        }

        LogDebug("    > Primitives (triangles only) count based on hierarchy : {}", primitivesCount);

        // Load our model data: meshes and materials
        model.meshCount = primitivesCount;
        model.meshes = static_cast<Mesh *>(calloc((model.meshCount > 0) ? model.meshCount : 1, sizeof(Mesh)));

        // NOTE: Keep an extra slot for default material, in case some mesh requires it
        model.materialCount = static_cast<i32>(data->materials_count) + 1;
        model.materials = static_cast<Material *>(calloc(model.materialCount, sizeof(Material)));
        model.materials[0] = LoadMaterialDefault();     // Load default material (index: 0)

        // Load mesh-material indices, by default all meshes are mapped to material index: 0
        model.meshMaterial = static_cast<i32 *>(calloc((model.meshCount > 0) ? model.meshCount : 1, sizeof(i32)));

        // Load materials data
        //----------------------------------------------------------------------------------------------------
        for (u32 i = 0, j = 1; i < data->materials_count; i++, j++)
        {
            model.materials[j] = LoadMaterialDefault();
            const auto texPath = GetDirectoryPath(fileName);

            // Check glTF material flow: PBR metallic/roughness flow
            // NOTE: Alternatively, materials can follow PBR specular/glossiness flow
            if (data->materials[i].has_pbr_metallic_roughness)
            {
                // Load base color texture (albedo)
                if (data->materials[i].pbr_metallic_roughness.base_color_texture.texture)
                {
                    const auto imAlbedo = LoadImageFromCgltfImage(data->materials[i].pbr_metallic_roughness.base_color_texture.texture->image, texPath.c_str());
                    if (imAlbedo.data != nullptr)
                    {
                        model.materials[j].maps[MATERIAL_MAP_ALBEDO].texture = LoadTextureFromImage(imAlbedo);
                        UnloadImage(imAlbedo);
                    }
                }
                // Load base color factor (tint)
                model.materials[j].maps[MATERIAL_MAP_ALBEDO].color.r = static_cast<u8>(data->materials[i].pbr_metallic_roughness.base_color_factor[0]*255);
                model.materials[j].maps[MATERIAL_MAP_ALBEDO].color.g = static_cast<u8>(data->materials[i].pbr_metallic_roughness.base_color_factor[1]*255);
                model.materials[j].maps[MATERIAL_MAP_ALBEDO].color.b = static_cast<u8>(data->materials[i].pbr_metallic_roughness.base_color_factor[2]*255);
                model.materials[j].maps[MATERIAL_MAP_ALBEDO].color.a = static_cast<u8>(data->materials[i].pbr_metallic_roughness.base_color_factor[3]*255);

                // Load metallic/roughness texture
                if (data->materials[i].pbr_metallic_roughness.metallic_roughness_texture.texture)
                {
                    const auto imMetallicRoughness = LoadImageFromCgltfImage(data->materials[i].pbr_metallic_roughness.metallic_roughness_texture.texture->image, texPath.c_str());
                    if (imMetallicRoughness.data != nullptr)
                    {
                        auto imMetallic = Image{ nullptr };
                        auto imRoughness = Image{ nullptr };

                        imMetallic.data = calloc(imMetallicRoughness.width*imMetallicRoughness.height, 1);
                        imRoughness.data = calloc(imMetallicRoughness.width*imMetallicRoughness.height, 1);

                        imMetallic.width = imRoughness.width = imMetallicRoughness.width;
                        imMetallic.height = imRoughness.height = imMetallicRoughness.height;

                        imMetallic.format = imRoughness.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;
                        imMetallic.mipmaps = imRoughness.mipmaps = 1;

                        for (i32 x = 0; x < imRoughness.width; x++)
                        {
                            for (i32 y = 0; y < imRoughness.height; y++)
                            {
                                const auto color = GetImageColor(imMetallicRoughness, x, y);

                                (static_cast<u8 *>(imRoughness.data))[y*imRoughness.width + x] = color.g; // Roughness color channel
                                (static_cast<u8 *>(imMetallic.data))[y*imMetallic.width + x] = color.b;   // Metallic color channel
                            }
                        }

                        model.materials[j].maps[MATERIAL_MAP_ROUGHNESS].texture = LoadTextureFromImage(imRoughness);
                        model.materials[j].maps[MATERIAL_MAP_METALNESS].texture = LoadTextureFromImage(imMetallic);

                        UnloadImage(imRoughness);
                        UnloadImage(imMetallic);
                        UnloadImage(imMetallicRoughness);
                    }

                    // Load metallic/roughness material properties
                    const f32 roughness = data->materials[i].pbr_metallic_roughness.roughness_factor;
                    model.materials[j].maps[MATERIAL_MAP_ROUGHNESS].value = roughness;

                    const f32 metallic = data->materials[i].pbr_metallic_roughness.metallic_factor;
                    model.materials[j].maps[MATERIAL_MAP_METALNESS].value = metallic;
                }

                // Load normal texture
                if (data->materials[i].normal_texture.texture)
                {
                    const auto imNormal = LoadImageFromCgltfImage(data->materials[i].normal_texture.texture->image, texPath.c_str());
                    if (imNormal.data != nullptr)
                    {
                        model.materials[j].maps[MATERIAL_MAP_NORMAL].texture = LoadTextureFromImage(imNormal);
                        UnloadImage(imNormal);
                    }
                }

                // Load ambient occlusion texture
                if (data->materials[i].occlusion_texture.texture)
                {
                    const auto imOcclusion = LoadImageFromCgltfImage(data->materials[i].occlusion_texture.texture->image, texPath.c_str());
                    if (imOcclusion.data != nullptr)
                    {
                        model.materials[j].maps[MATERIAL_MAP_OCCLUSION].texture = LoadTextureFromImage(imOcclusion);
                        UnloadImage(imOcclusion);
                    }
                }

                // Load emissive texture
                if (data->materials[i].emissive_texture.texture)
                {
                    const auto imEmissive = LoadImageFromCgltfImage(data->materials[i].emissive_texture.texture->image, texPath.c_str());
                    if (imEmissive.data != nullptr)
                    {
                        model.materials[j].maps[MATERIAL_MAP_EMISSION].texture = LoadTextureFromImage(imEmissive);
                        UnloadImage(imEmissive);
                    }

                    // Load emissive color factor
                    model.materials[j].maps[MATERIAL_MAP_EMISSION].color.r = static_cast<u8>(data->materials[i].emissive_factor[0]*255);
                    model.materials[j].maps[MATERIAL_MAP_EMISSION].color.g = static_cast<u8>(data->materials[i].emissive_factor[1]*255);
                    model.materials[j].maps[MATERIAL_MAP_EMISSION].color.b = static_cast<u8>(data->materials[i].emissive_factor[2]*255);
                    model.materials[j].maps[MATERIAL_MAP_EMISSION].color.a = 255;
                }
            }

            // Other possible materials not supported by raylib pipeline:
            // has_clearcoat, has_transmission, has_volume, has_ior, has specular, has_sheen
        }
        //----------------------------------------------------------------------------------------------------

        // Load meshes data
        //
        // NOTE: Visit each node in the hierarchy and process any mesh linked from it
        //  - Each primitive within a glTF node becomes a raylib Mesh
        //  - The local-to-world transform of each node is used to transform the points/normals/tangents of the created Mesh(es)
        //  - Any glTF mesh linked from more than one Node (a.e. instancing) is turned into multiple Mesh's, as each Node will have its own transform applied
        //
        // WARNING: The code below disregards the scenes defined in the file, all nodes are used
        //----------------------------------------------------------------------------------------------------
        auto meshIndex = 0;
        for (u32 i = 0; i < data->nodes_count; i++)
        {
            cgltf_node *node = &(data->nodes[i]);

            cgltf_mesh *mesh = node->mesh;
            if (!mesh) continue;

            cgltf_float worldTransform[16];
            cgltf_node_transform_world(node, worldTransform);

            const mat4 worldMatrix = {
                worldTransform[0], worldTransform[1], worldTransform[2], worldTransform[3],
                worldTransform[4], worldTransform[5], worldTransform[6], worldTransform[7],
                worldTransform[8], worldTransform[9], worldTransform[10], worldTransform[11],
                worldTransform[12], worldTransform[13], worldTransform[14], worldTransform[15]
            };

            const mat4 worldMatrixNormals = glm::transpose(glm::inverse(worldMatrix));

            for (u32 p = 0; p < mesh->primitives_count; p++)
            {
                // NOTE: Only support primitives defined by triangles
                // Other alternatives: points, lines, line_strip, triangle_strip
                if (mesh->primitives[p].type != cgltf_primitive_type_triangles) continue;

                // NOTE: Attributes data could be provided in several data formats (8, 8u, 16u, 32...),
                // Only some formats for each attribute type are supported, read info at the top of this function!

                for (u32 j = 0; j < mesh->primitives[p].attributes_count; j++)
                {
                    // Check the different attributes for every primitive
                    if (mesh->primitives[p].attributes[j].type == cgltf_attribute_type_position)      // POSITION, vec3, float
                    {
                        cgltf_accessor *attribute = mesh->primitives[p].attributes[j].data;

                        // WARNING: SPECS: POSITION accessor MUST have its min and max properties defined

                        if (model.meshes[meshIndex].vertices != nullptr) LogWarning("MODEL: [{}] Vertices attribute data already loaded", fileName);
                        else
                        {
                            if ((attribute->type == cgltf_type_vec3) && (attribute->component_type == cgltf_component_type_r_32f))
                            {
                                // Init raylib mesh vertices to copy glTF attribute data
                                model.meshes[meshIndex].vertexCount = static_cast<i32>(attribute->count);
                                model.meshes[meshIndex].vertices = static_cast<f32 *>(calloc(attribute->count*3, sizeof(f32)));

                                // Load 3 components of float data type into mesh.vertices
                                LOAD_ATTRIBUTE(attribute, 3, f32, model.meshes[meshIndex].vertices)

                                // Transform the vertices
                                f32 *vertices = model.meshes[meshIndex].vertices;
                                for (u32 k = 0; k < attribute->count; k++)
                                {
                                    const auto vt = vec3((worldMatrix)*vec4(vec3{ vertices[3*k], vertices[3*k+1], vertices[3*k+2] }, 1.0f));
                                    vertices[3*k] = vt.x;
                                    vertices[3*k+1] = vt.y;
                                    vertices[3*k+2] = vt.z;
                                }
                            }
                            else if ((attribute->type == cgltf_type_vec3) && (attribute->component_type == cgltf_component_type_r_16u))
                            {
                                // Init raylib mesh vertices to copy glTF attribute data
                                model.meshes[meshIndex].vertexCount = static_cast<i32>(attribute->count);
                                model.meshes[meshIndex].vertices = static_cast<f32 *>(calloc(attribute->count*3, sizeof(f32)));

                                // Load data into a temp buffer to be converted to raylib data type
                                auto temp = static_cast<u16 *>(calloc(attribute->count*3, sizeof(u16)));
                                LOAD_ATTRIBUTE(attribute, 3, u16, temp);

                                // Convert data to raylib vertex data type (float) the matrix will scale it to the correct size as a float
                                for (u32 t = 0; t < attribute->count*3; t++) model.meshes[meshIndex].vertices[t] = static_cast<f32>(temp[t]);

                                free(temp);

                                // Transform the vertices
                                f32 *vertices = model.meshes[meshIndex].vertices;
                                for (u32 k = 0; k < attribute->count; k++)
                                {
                                    const auto vt = vec3((worldMatrix)*vec4(vec3{ vertices[3*k], vertices[3*k + 1], vertices[3*k + 2] }, 1.0f));
                                    vertices[3*k] = vt.x;
                                    vertices[3*k + 1] = vt.y;
                                    vertices[3*k + 2] = vt.z;
                                }
                            }
                            else if ((attribute->type == cgltf_type_vec3) && (attribute->component_type == cgltf_component_type_r_16))
                            {
                                // Init raylib mesh vertices to copy glTF attribute data
                                model.meshes[meshIndex].vertexCount = static_cast<i32>(attribute->count);
                                model.meshes[meshIndex].vertices = static_cast<f32 *>(calloc(attribute->count*3, sizeof(f32)));

                                // Load data into a temp buffer to be converted to raylib data type
                                auto temp = static_cast<short *>(calloc(attribute->count*3, sizeof(short)));
                                LOAD_ATTRIBUTE(attribute, 3, short, temp);

                                // Convert data to raylib vertex data type (float) the matrix will scale it to the correct size as a float
                                for (u32 t = 0; t < attribute->count*3; t++) model.meshes[meshIndex].vertices[t] = static_cast<f32>(temp[t]);

                                free(temp);

                                // Transform the vertices
                                f32 *vertices = model.meshes[meshIndex].vertices;
                                for (u32 k = 0; k < attribute->count; k++)
                                {
                                    const auto vt = vec3((worldMatrix)*vec4(vec3{ vertices[3*k], vertices[3*k + 1], vertices[3*k + 2] }, 1.0f));
                                    vertices[3*k] = vt.x;
                                    vertices[3*k + 1] = vt.y;
                                    vertices[3*k + 2] = vt.z;
                                }
                            }
                            else LogWarning("MODEL: [{}] Vertices attribute data format not supported, use vec3 float", fileName);
                        }
                    }
                    else if (mesh->primitives[p].attributes[j].type == cgltf_attribute_type_normal) // NORMAL, vec3, float
                    {
                        cgltf_accessor *attribute = mesh->primitives[p].attributes[j].data;

                        if (model.meshes[meshIndex].normals != nullptr) LogWarning("MODEL: [{}] Normals attribute data already loaded", fileName);
                        else
                        {
                            if ((attribute->type == cgltf_type_vec3) && (attribute->component_type == cgltf_component_type_r_32f))
                            {
                                // Init raylib mesh normals to copy glTF attribute data
                                model.meshes[meshIndex].normals = static_cast<f32 *>(calloc(attribute->count*3, sizeof(f32)));

                                // Load 3 components of float data type into mesh.normals
                                LOAD_ATTRIBUTE(attribute, 3, f32, model.meshes[meshIndex].normals)

                                // Transform the normals
                                f32 *normals = model.meshes[meshIndex].normals;
                                for (u32 k = 0; k < attribute->count; k++)
                                {
                                    const auto nt = vec3((worldMatrixNormals)*vec4(vec3{ normals[3*k], normals[3*k+1], normals[3*k+2] }, 1.0f));
                                    normals[3*k] = nt.x;
                                    normals[3*k+1] = nt.y;
                                    normals[3*k+2] = nt.z;
                                }
                            }
                            else if ((attribute->type == cgltf_type_vec3) && (attribute->component_type == cgltf_component_type_r_16))
                            {
                                // Init raylib mesh normals to copy glTF attribute data
                                model.meshes[meshIndex].normals = static_cast<f32 *>(calloc(attribute->count*3, sizeof(f32)));

                                // Load data into a temp buffer to be converted to raylib data type
                                auto temp = static_cast<short *>(calloc(attribute->count*3, sizeof(short)));
                                LOAD_ATTRIBUTE(attribute, 3, short, temp);

                                // Convert data to raylib normal data type (float)
                                for (u32 t = 0; t < attribute->count*3; t++) model.meshes[meshIndex].normals[t] = static_cast<f32>(temp[t]);

                                free(temp);

                                // Transform the normals
                                f32 *normals = model.meshes[meshIndex].normals;
                                for (u32 k = 0; k < attribute->count; k++)
                                {
                                    const vec3 nt = glm::normalize(vec3((worldMatrixNormals)*vec4(vec3{ normals[3*k], normals[3*k + 1], normals[3*k + 2] }, 1.0f)));
                                    normals[3*k] = nt.x;
                                    normals[3*k + 1] = nt.y;
                                    normals[3*k + 2] = nt.z;
                                }
                            }
                            else if ((attribute->type == cgltf_type_vec3) && (attribute->component_type == cgltf_component_type_r_8u))
                            {
                                // Init raylib mesh normals to copy glTF attribute data
                                model.meshes[meshIndex].normals = static_cast<f32 *>(calloc(attribute->count*3, sizeof(f32)));

                                // Load data into a temp buffer to be converted to raylib data type
                                auto temp = static_cast<u8 *>(calloc(attribute->count*3, sizeof(u8)));
                                LOAD_ATTRIBUTE(attribute, 3, u8, temp);

                                // Convert data to raylib normal data type (float)
                                for (u32 t = 0; t < attribute->count*3; t++) model.meshes[meshIndex].normals[t] = static_cast<f32>(temp[t]);

                                free(temp);

                                // Transform the normals
                                f32 *normals = model.meshes[meshIndex].normals;
                                for (u32 k = 0; k < attribute->count; k++)
                                {
                                    const vec3 nt = glm::normalize(vec3((worldMatrixNormals)*vec4(vec3{ normals[3*k], normals[3*k + 1], normals[3*k + 2] }, 1.0f)));
                                    normals[3*k] = nt.x;
                                    normals[3*k + 1] = nt.y;
                                    normals[3*k + 2] = nt.z;
                                }
                            }
                            else if ((attribute->type == cgltf_type_vec3) && (attribute->component_type == cgltf_component_type_r_8))
                            {
                                // Init raylib mesh normals to copy glTF attribute data
                                model.meshes[meshIndex].normals = static_cast<f32 *>(calloc(attribute->count*3, sizeof(f32)));

                                // Load data into a temp buffer to be converted to raylib data type
                                auto temp = static_cast<char *>(calloc(attribute->count*3, sizeof(char)));
                                LOAD_ATTRIBUTE(attribute, 3, char, temp);

                                // Convert data to raylib normal data type (float)
                                for (u32 t = 0; t < attribute->count*3; t++) model.meshes[meshIndex].normals[t] = static_cast<f32>(temp[t]);

                                free(temp);

                                // Transform the normals
                                f32 *normals = model.meshes[meshIndex].normals;
                                for (u32 k = 0; k < attribute->count; k++)
                                {
                                    const vec3 nt = glm::normalize(vec3((worldMatrixNormals)*vec4(vec3{ normals[3*k], normals[3*k + 1], normals[3*k + 2] }, 1.0f)));
                                    normals[3*k] = nt.x;
                                    normals[3*k + 1] = nt.y;
                                    normals[3*k + 2] = nt.z;
                                }
                            }
                            else LogWarning("MODEL: [{}] Normals attribute data format not supported, use vec3 float", fileName);
                        }
                    }
                    else if (mesh->primitives[p].attributes[j].type == cgltf_attribute_type_tangent) // TANGENT, vec4, float, w is tangent basis sign
                    {
                        cgltf_accessor *attribute = mesh->primitives[p].attributes[j].data;

                        if (model.meshes[meshIndex].tangents != nullptr) LogWarning("MODEL: [{}] Tangents attribute data already loaded", fileName);
                        else
                        {
                            if ((attribute->type == cgltf_type_vec4) && (attribute->component_type == cgltf_component_type_r_32f))
                            {
                                // Init raylib mesh tangent to copy glTF attribute data
                                model.meshes[meshIndex].tangents = static_cast<f32 *>(calloc(attribute->count*4, sizeof(f32)));

                                // Load 4 components of float data type into mesh.tangents
                                LOAD_ATTRIBUTE(attribute, 4, f32, model.meshes[meshIndex].tangents)

                                // Transform the tangents
                                f32 *tangents = model.meshes[meshIndex].tangents;
                                for (u32 k = 0; k < attribute->count; k++)
                                {
                                    const auto tt = vec3((worldMatrix)*vec4(vec3{ tangents[4*k], tangents[4*k+1], tangents[4*k+2] }, 1.0f));
                                    tangents[4*k] = tt.x;
                                    tangents[4*k+1] = tt.y;
                                    tangents[4*k+2] = tt.z;
                                }
                            }
                            else LogWarning("MODEL: [{}] Tangents attribute data format not supported, use vec4 float", fileName);
                        }
                    }
                    else if (mesh->primitives[p].attributes[j].type == cgltf_attribute_type_texcoord) // TEXCOORD_n, vec2, float/u8n/u16n
                    {
                        // Support up to 2 texture coordinates attributes
                        f32 *texcoordPtr = nullptr;

                        cgltf_accessor *attribute = mesh->primitives[p].attributes[j].data;

                        if (attribute->type == cgltf_type_vec2)
                        {
                            if (attribute->component_type == cgltf_component_type_r_32f) // vec2, float
                            {
                                // Init raylib mesh texcoords to copy glTF attribute data
                                texcoordPtr = static_cast<f32 *>(calloc(attribute->count*2, sizeof(f32)));

                                // Load 3 components of float data type into mesh.texcoords
                                LOAD_ATTRIBUTE(attribute, 2, f32, texcoordPtr)
                            }
                            else if (attribute->component_type == cgltf_component_type_r_8u) // vec2, u8n
                            {
                                // Init raylib mesh texcoords to copy glTF attribute data
                                texcoordPtr = static_cast<f32 *>(calloc(attribute->count*2, sizeof(f32)));

                                // Load data into a temp buffer to be converted to raylib data type
                                auto temp = static_cast<u8 *>(calloc(attribute->count*2, sizeof(u8)));
                                LOAD_ATTRIBUTE(attribute, 2, u8, temp);

                                // Convert data to raylib texcoord data type (float)
                                for (u32 t = 0; t < attribute->count*2; t++) texcoordPtr[t] = static_cast<f32>(temp[t])/255.0f;

                                free(temp);
                            }
                            else if (attribute->component_type == cgltf_component_type_r_16u) // vec2, u16n
                            {
                                // Init raylib mesh texcoords to copy glTF attribute data
                                texcoordPtr = static_cast<f32 *>(calloc(attribute->count*2, sizeof(f32)));

                                // Load data into a temp buffer to be converted to raylib data type
                                auto temp = static_cast<u16 *>(calloc(attribute->count*2, sizeof(u16)));
                                LOAD_ATTRIBUTE(attribute, 2, u16, temp);

                                // Convert data to raylib texcoord data type (float)
                                for (u32 t = 0; t < attribute->count*2; t++) texcoordPtr[t] = static_cast<f32>(temp[t])/65535.0f;

                                free(temp);
                            }
                            else LogWarning("MODEL: [{}] Texcoords attribute data format not supported", fileName);
                        }
                        else LogWarning("MODEL: [{}] Texcoords attribute data format not supported, use vec2 float", fileName);

                        const i32 index = mesh->primitives[p].attributes[j].index;
                        if (index == 0) model.meshes[meshIndex].texcoords = texcoordPtr;
                        else if (index == 1) model.meshes[meshIndex].texcoords2 = texcoordPtr;
                        else
                        {
                            LogWarning("MODEL: [{}] No more than 2 texture coordinates attributes supported", fileName);
                            if (texcoordPtr != nullptr) free(texcoordPtr);
                        }
                    }
                    else if (mesh->primitives[p].attributes[j].type == cgltf_attribute_type_color) // COLOR_n, vec3/vec4, float/u8n/u16n
                    {
                        cgltf_accessor *attribute = mesh->primitives[p].attributes[j].data;

                        // WARNING: SPECS: All components of each COLOR_n accessor element MUST be clamped to [0.0, 1.0] range

                        if (model.meshes[meshIndex].colors != nullptr) LogWarning("MODEL: [{}] Colors attribute data already loaded", fileName);
                        else
                        {
                            if (attribute->type == cgltf_type_vec3) // RGB
                            {
                                if (attribute->component_type == cgltf_component_type_r_8u)
                                {
                                    // Init raylib mesh color to copy glTF attribute data
                                    model.meshes[meshIndex].colors = static_cast<u8 *>(calloc(attribute->count*4, sizeof(u8)));

                                    // Load data into a temp buffer to be converted to raylib data type
                                    auto temp = static_cast<u8 *>(calloc(attribute->count*3, sizeof(u8)));
                                    LOAD_ATTRIBUTE(attribute, 3, u8, temp);

                                    // Convert data to raylib color data type (4 bytes)
                                    for (u32 c = 0, k = 0; c < (attribute->count*4 - 3); c += 4, k += 3)
                                    {
                                        model.meshes[meshIndex].colors[c] = temp[k];
                                        model.meshes[meshIndex].colors[c + 1] = temp[k + 1];
                                        model.meshes[meshIndex].colors[c + 2] = temp[k + 2];
                                        model.meshes[meshIndex].colors[c + 3] = 255;
                                    }

                                    free(temp);
                                }
                                else if (attribute->component_type == cgltf_component_type_r_16u)
                                {
                                    // Init raylib mesh color to copy glTF attribute data
                                    model.meshes[meshIndex].colors = static_cast<u8 *>(calloc(attribute->count*4, sizeof(u8)));

                                    // Load data into a temp buffer to be converted to raylib data type
                                    auto temp = static_cast<u16 *>(calloc(attribute->count*3, sizeof(u16)));
                                    LOAD_ATTRIBUTE(attribute, 3, u16, temp);

                                    // Convert data to raylib color data type (4 bytes)
                                    for (u32 c = 0, k = 0; c < (attribute->count*4 - 3); c += 4, k += 3)
                                    {
                                        model.meshes[meshIndex].colors[c] = static_cast<u8>((static_cast<f32>(temp[k])/65535.0f)*255.0f);
                                        model.meshes[meshIndex].colors[c + 1] = static_cast<u8>((static_cast<f32>(temp[k + 1])/65535.0f)*255.0f);
                                        model.meshes[meshIndex].colors[c + 2] = static_cast<u8>((static_cast<f32>(temp[k + 2])/65535.0f)*255.0f);
                                        model.meshes[meshIndex].colors[c + 3] = 255;
                                    }

                                    free(temp);
                                }
                                else if (attribute->component_type == cgltf_component_type_r_32f)
                                {
                                    // Init raylib mesh color to copy glTF attribute data
                                    model.meshes[meshIndex].colors = static_cast<u8 *>(calloc(attribute->count*4, sizeof(u8)));

                                    // Load data into a temp buffer to be converted to raylib data type
                                    auto temp = static_cast<f32 *>(calloc(attribute->count*3, sizeof(f32)));
                                    LOAD_ATTRIBUTE(attribute, 3, f32, temp);

                                    // Convert data to raylib color data type (4 bytes)
                                    for (u32 c = 0, k = 0; c < (attribute->count*4 - 3); c += 4, k += 3)
                                    {
                                        model.meshes[meshIndex].colors[c] = static_cast<u8>(temp[k]*255.0f);
                                        model.meshes[meshIndex].colors[c + 1] = static_cast<u8>(temp[k + 1]*255.0f);
                                        model.meshes[meshIndex].colors[c + 2] = static_cast<u8>(temp[k + 2]*255.0f);
                                        model.meshes[meshIndex].colors[c + 3] = 255;
                                    }

                                    free(temp);
                                }
                                else LogWarning("MODEL: [{}] Color attribute data format not supported", fileName);
                            }
                            else if (attribute->type == cgltf_type_vec4) // RGBA
                            {
                                if (attribute->component_type == cgltf_component_type_r_8u)
                                {
                                    // Init raylib mesh color to copy glTF attribute data
                                    model.meshes[meshIndex].colors = static_cast<u8 *>(calloc(attribute->count*4, sizeof(u8)));

                                    // Load 4 components of unsigned char data type into mesh.colors
                                    LOAD_ATTRIBUTE(attribute, 4, u8, model.meshes[meshIndex].colors)
                                }
                                else if (attribute->component_type == cgltf_component_type_r_16u)
                                {
                                    // Init raylib mesh color to copy glTF attribute data
                                    model.meshes[meshIndex].colors = static_cast<u8 *>(calloc(attribute->count*4, sizeof(u8)));

                                    // Load data into a temp buffer to be converted to raylib data type
                                    auto temp = static_cast<u16 *>(calloc(attribute->count*4, sizeof(u16)));
                                    LOAD_ATTRIBUTE(attribute, 4, u16, temp);

                                    // Convert data to raylib color data type (4 bytes)
                                    for (u32 c = 0; c < attribute->count*4; c++) model.meshes[meshIndex].colors[c] = static_cast<u8>((static_cast<f32>(temp[c])/65535.0f)*255.0f);

                                    free(temp);
                                }
                                else if (attribute->component_type == cgltf_component_type_r_32f)
                                {
                                    // Init raylib mesh color to copy glTF attribute data
                                    model.meshes[meshIndex].colors = static_cast<u8 *>(calloc(attribute->count*4, sizeof(u8)));

                                    // Load data into a temp buffer to be converted to raylib data type
                                    auto temp = static_cast<f32 *>(calloc(attribute->count*4, sizeof(f32)));
                                    LOAD_ATTRIBUTE(attribute, 4, f32, temp);

                                    // Convert data to raylib color data type (4 bytes), color data must be normalized
                                    for (u32 c = 0; c < attribute->count*4; c++) model.meshes[meshIndex].colors[c] = static_cast<u8>(temp[c]*255.0f);

                                    free(temp);
                                }
                                else LogWarning("MODEL: [{}] Color attribute data format not supported", fileName);
                            }
                            else LogWarning("MODEL: [{}] Color attribute data format not supported", fileName);
                        }
                    }

                    // NOTE: Attributes related to animations data are processed after mesh data loading
                }

                // Load primitive indices data (if provided)
                if ((mesh->primitives[p].indices != nullptr) && (mesh->primitives[p].indices->buffer_view != nullptr))
                {
                    cgltf_accessor *attribute = mesh->primitives[p].indices;

                    model.meshes[meshIndex].triangleCount = static_cast<i32>(attribute->count)/3;

                    if (model.meshes[meshIndex].indices != nullptr) LogWarning("MODEL: [{}] Indices attribute data already loaded", fileName);
                    else
                    {
                        if (attribute->component_type == cgltf_component_type_r_16u)
                        {
                            // Init raylib mesh indices to copy glTF attribute data
                            model.meshes[meshIndex].indices = static_cast<u16 *>(calloc(attribute->count, sizeof(u16)));

                            // Load unsigned short data type into mesh.indices
                            LOAD_ATTRIBUTE(attribute, 1, u16, model.meshes[meshIndex].indices)
                        }
                        else if (attribute->component_type == cgltf_component_type_r_8u)
                        {
                            // Init raylib mesh indices to copy glTF attribute data
                            model.meshes[meshIndex].indices = static_cast<u16 *>(calloc(attribute->count, sizeof(u16)));
                            LOAD_ATTRIBUTE_CAST(attribute, 1, u8, model.meshes[meshIndex].indices, u16)

                        }
                        else if (attribute->component_type == cgltf_component_type_r_32u)
                        {
                            // Init raylib mesh indices to copy glTF attribute data
                            model.meshes[meshIndex].indices = static_cast<u16 *>(calloc(attribute->count, sizeof(u16)));
                            LOAD_ATTRIBUTE_CAST(attribute, 1, u32, model.meshes[meshIndex].indices, u16);

                            LogWarning("MODEL: [{}] Indices data converted from u32 to u16, possible loss of data", fileName);
                        }
                        else LogWarning("MODEL: [{}] Indices data format not supported, use u16", fileName);
                    }
                }
                else model.meshes[meshIndex].triangleCount = model.meshes[meshIndex].vertexCount/3;    // Unindexed mesh

                // Assign to the primitive mesh the corresponding material index
                // NOTE: If no material defined, mesh uses the already assigned default material (index: 0)
                for (u32 m = 0; m < data->materials_count; m++)
                {
                    // The primitive actually keeps the pointer to the corresponding material,
                    // raylib instead assigns to the mesh the by its index, as loaded in model.materials array
                    // To get the index, check if material pointers match, and assign the corresponding index,
                    // skipping index 0, the default material
                    if (&data->materials[m] == mesh->primitives[p].material)
                    {
                        model.meshMaterial[meshIndex] = static_cast<i32>(m) + 1;
                        break;
                    }
                }

                meshIndex++;       // Move to next mesh
            }
        }
        //----------------------------------------------------------------------------------------------------

        // Load animation data
        // REF: https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#skins
        // REF: https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#skinned-mesh-attributes
        //
        // LIMITATIONS:
        //  - Only supports 1 armature per file, and skips loading it if there are multiple armatures
        //  - Only supports linear interpolation (default method in Blender when checked "Always Sample Animations" when exporting a GLTF file)
        //  - Only supports translation/rotation/scale animation channel.path, weights not considered (a.e. morph targets)
        //----------------------------------------------------------------------------------------------------
        if (data->skins_count > 0)
        {
            const cgltf_skin skin = data->skins[0];
            model.skeleton.bones = LoadBoneInfoGLTF(skin, &model.skeleton.boneCount);
            model.skeleton.bindPose = static_cast<Transform *>(calloc(model.skeleton.boneCount, sizeof(Transform)));

            for (u32 i = 0; i < model.skeleton.boneCount; i++)
            {
                mat4 bindMatrix{};
                cgltf_float inverseBindTransform[16] = { 0 };

                if ((skin.inverse_bind_matrices != nullptr) &&
                    (skin.inverse_bind_matrices->count >= skin.joints_count) &&
                    cgltf_accessor_read_float(skin.inverse_bind_matrices, i, inverseBindTransform, 16))
                {
                    const mat4 inverseBindMatrix = {
                inverseBindTransform[0], inverseBindTransform[1], inverseBindTransform[2], inverseBindTransform[3],
                inverseBindTransform[4], inverseBindTransform[5], inverseBindTransform[6], inverseBindTransform[7],
                inverseBindTransform[8], inverseBindTransform[9], inverseBindTransform[10], inverseBindTransform[11],
                inverseBindTransform[12], inverseBindTransform[13], inverseBindTransform[14], inverseBindTransform[15]
            };
                    bindMatrix = glm::inverse(inverseBindMatrix);
                }
                else
                {
                    cgltf_float worldTransform[16] = { 0 };
                    cgltf_node_transform_world(skin.joints[i], worldTransform);

                    const mat4 worldMatrix = {
                worldTransform[0], worldTransform[1], worldTransform[2], worldTransform[3],
                worldTransform[4], worldTransform[5], worldTransform[6], worldTransform[7],
                worldTransform[8], worldTransform[9], worldTransform[10], worldTransform[11],
                worldTransform[12], worldTransform[13], worldTransform[14], worldTransform[15]
            };
                    bindMatrix = worldMatrix;
                }

                MatrixDecompose(bindMatrix,
                    &(model.skeleton.bindPose[i].translation),
                    &(model.skeleton.bindPose[i].rotation),
                    &(model.skeleton.bindPose[i].scale));
            }

            if (data->skins_count > 1) LogWarning("MODEL: [{}] can only load one skin (armature) per model, but gltf skins_count == {}", fileName, data->skins_count);
        }

        meshIndex = 0;
        for (u32 i = 0; i < data->nodes_count; i++)
        {
            cgltf_node *node = &(data->nodes[i]);

            cgltf_mesh *mesh = node->mesh;
            if (!mesh) continue;

            for (u32 p = 0; p < mesh->primitives_count; p++)
            {
                auto hasJoints = false;

                // NOTE: Only support primitives defined by triangles
                if (mesh->primitives[p].type != cgltf_primitive_type_triangles) continue;

                for (u32 j = 0; j < mesh->primitives[p].attributes_count; j++)
                {
                    // NOTE: JOINTS_1 + WEIGHT_1 will be used for +4 joints influencing a vertex -> Not supported by raylib
                    if (mesh->primitives[p].attributes[j].type == cgltf_attribute_type_joints) // JOINTS_n (vec4: 4 bones max per vertex / u8, u16)
                    {
                        hasJoints = true;
                        cgltf_accessor *attribute = mesh->primitives[p].attributes[j].data;

                        // NOTE: JOINTS_n can only be vec4 and u8/u16
                        // SPECS: https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes-overview

                        // WARNING: raylib only supports model.meshes[].boneIndices as u8 (unsigned char),
                        // if data is provided in any other format, it is converted to supported format but
                        // it could imply data loss (a warning message is issued in that case)

                        if (attribute->type == cgltf_type_vec4)
                        {
                            if (attribute->component_type == cgltf_component_type_r_8u)
                            {
                                // Init raylib mesh boneIndices to copy glTF attribute data
                                model.meshes[meshIndex].boneIndices = static_cast<u8 *>(calloc(model.meshes[meshIndex].vertexCount*4, sizeof(u8)));

                                // Load attribute: vec4, u8 (unsigned char)
                                LOAD_ATTRIBUTE(attribute, 4, u8, model.meshes[meshIndex].boneIndices)
                            }
                            else if (attribute->component_type == cgltf_component_type_r_16u)
                            {
                                // Init raylib mesh boneIndices to copy glTF attribute data
                                model.meshes[meshIndex].boneIndices = static_cast<u8 *>(calloc(model.meshes[meshIndex].vertexCount*4, sizeof(u8)));

                                // Load data into a temp buffer to be converted to raylib data type
                                auto temp = static_cast<u16 *>(calloc(model.meshes[meshIndex].vertexCount*4, sizeof(u16)));
                                LOAD_ATTRIBUTE(attribute, 4, u16, temp);

                                // Convert data to raylib color data type (4 bytes)
                                auto boneIdOverflowWarning = false;
                                for (i32 b = 0; b < model.meshes[meshIndex].vertexCount*4; b++)
                                {
                                    if ((temp[b] > 255) && !boneIdOverflowWarning)
                                    {
                                        LogWarning("MODEL: [{}] Joint attribute data format (u16) overflow", fileName);
                                        boneIdOverflowWarning = true;
                                    }

                                    // Despite the possible overflow, convert data to unsigned char
                                    model.meshes[meshIndex].boneIndices[b] = static_cast<u8>(temp[b]);
                                }

                                free(temp);
                            }
                            else LogWarning("MODEL: [{}] Joint attribute data format not supported", fileName);
                        }
                        else LogWarning("MODEL: [{}] Joint attribute data format not supported", fileName);
                    }
                    else if (mesh->primitives[p].attributes[j].type == cgltf_attribute_type_weights) // WEIGHTS_n (vec4, u8n/u16n/f32)
                    {
                        cgltf_accessor *attribute = mesh->primitives[p].attributes[j].data;

                        if (attribute->type == cgltf_type_vec4)
                        {
                            if (attribute->component_type == cgltf_component_type_r_8u)
                            {
                                // Init raylib mesh bone weight to copy glTF attribute data
                                model.meshes[meshIndex].boneWeights = static_cast<f32 *>(calloc(model.meshes[meshIndex].vertexCount*4, sizeof(f32)));

                                // Load data into a temp buffer to be converted to raylib data type
                                auto temp = static_cast<u8 *>(calloc(attribute->count*4, sizeof(u8)));
                                LOAD_ATTRIBUTE(attribute, 4, u8, temp);

                                // Convert data to raylib bone weight data type (4 bytes)
                                for (u32 b = 0; b < attribute->count*4; b++) model.meshes[meshIndex].boneWeights[b] = static_cast<f32>(temp[b])/255.0f;

                                free(temp);
                            }
                            else if (attribute->component_type == cgltf_component_type_r_16u)
                            {
                                // Init raylib mesh bone weight to copy glTF attribute data
                                model.meshes[meshIndex].boneWeights = static_cast<f32 *>(calloc(model.meshes[meshIndex].vertexCount*4, sizeof(f32)));

                                // Load data into a temp buffer to be converted to raylib data type
                                auto temp = static_cast<u16 *>(calloc(attribute->count*4, sizeof(u16)));
                                LOAD_ATTRIBUTE(attribute, 4, u16, temp);

                                // Convert data to raylib bone weight data type
                                for (u32 b = 0; b < attribute->count*4; b++) model.meshes[meshIndex].boneWeights[b] = static_cast<f32>(temp[b])/65535.0f;

                                free(temp);
                            }
                            else if (attribute->component_type == cgltf_component_type_r_32f)
                            {
                                // Init raylib mesh bone weight to copy glTF attribute data
                                model.meshes[meshIndex].boneWeights = static_cast<f32 *>(calloc(model.meshes[meshIndex].vertexCount*4, sizeof(f32)));

                                // Load 4 components of float data type into mesh.boneWeights
                                // for cgltf_attribute_type_weights:
                                //   - data.meshes[0] (256 vertices)
                                //   - 256 values, provided as cgltf_type_vec4 of float (4 byte per joint, stride 16)
                                LOAD_ATTRIBUTE(attribute, 4, f32, model.meshes[meshIndex].boneWeights)
                            }
                            else LogWarning("MODEL: [{}] Joint weight attribute data format not supported, use vec4 float", fileName);
                        }
                        else LogWarning("MODEL: [{}] Joint weight attribute data format not supported, use vec4 float", fileName);
                    }
                }

                // Check if animated, and the mesh was not given any bone assignments, but is the child of a bone node
                // in this case, all the verts need to be attached to the parent bone so it will animate with the bone
                if ((data->skins_count > 0) && !hasJoints && (node->parent != nullptr) && (node->parent->mesh == nullptr))
                {
                    auto parentBoneId = -1;
                    for (u32 joint = 0; joint < model.skeleton.boneCount; joint++)
                    {
                        if (data->skins[0].joints[joint] == node->parent)
                        {
                            parentBoneId = static_cast<i32>(joint);
                            break;
                        }
                    }

                    if (parentBoneId >= 0)
                    {
                        model.meshes[meshIndex].boneIndices = static_cast<u8 *>(calloc(model.meshes[meshIndex].vertexCount*4, sizeof(u8)));
                        model.meshes[meshIndex].boneWeights = static_cast<f32 *>(calloc(model.meshes[meshIndex].vertexCount*4, sizeof(f32)));

                        for (i32 vertexIndex = 0; vertexIndex < model.meshes[meshIndex].vertexCount*4; vertexIndex += 4)
                        {
                            model.meshes[meshIndex].boneIndices[vertexIndex] = static_cast<u8>(parentBoneId);
                            model.meshes[meshIndex].boneWeights[vertexIndex] = 1.0f;
                        }
                    }
                }

                model.meshes[meshIndex].boneCount = static_cast<i32>(model.skeleton.boneCount);

                meshIndex++; // Move to next mesh
            }
        }

        // Initialize runtime animation data: current pose and bone matrices
        model.currentPose = static_cast<Transform *>(calloc((model.skeleton.boneCount > 0) ? model.skeleton.boneCount : 1, sizeof(Transform)));
        model.boneMatrices = static_cast<mat4 *>(calloc((model.skeleton.boneCount > 0) ? model.skeleton.boneCount : 1, sizeof(mat4)));
        for (u32 j = 0; j < model.skeleton.boneCount; j++) model.boneMatrices[j] = mat4(1.0f);
        //----------------------------------------------------------------------------------------------------

        // Free unused allocated memory in case of no bones defined
        if (model.skeleton.boneCount == 0)
        {
            free(model.currentPose);
            free(model.boneMatrices);
            model.currentPose = nullptr;
            model.boneMatrices = nullptr;
        }

        // Free all cgltf loaded data
        cgltf_free(data);
    }
    else LogWarning("MODEL: [{}] Failed to load glTF data", fileName);

    // WARNING: cgltf requires the file pointer available while reading data
    UnloadFileData(fileData);

    return model;
}

// Get interpolated pose for bone sampler at a specific time. Returns true on success
static bool GetPoseAtTimeGLTF(cgltf_interpolation_type interpolationType, const cgltf_accessor *input, const cgltf_accessor *output, const f32 time, void *data)
{
    if (interpolationType >= cgltf_interpolation_type_max_enum) return false;

    // Input and output should have the same count
    auto tstart = 0.0f;
    auto tend = 0.0f;
    auto keyframe = 0;       // Defaults to first pose
    auto found = false;

    for (i32 i = 0; i < static_cast<i32>(input->count) - 1; i++)
    {
        const cgltf_bool r1 = cgltf_accessor_read_float(input, i, &tstart, 1);
        if (!r1) return false;

        const cgltf_bool r2 = cgltf_accessor_read_float(input, i + 1, &tend, 1);
        if (!r2) return false;

        if ((tstart <= time) && (time < tend))
        {
            keyframe = i;
            found = true;
            break;
        }
    }

    // No interval contains a time at (or past) the last keyframe, because the
    // search above requires time < tend: clamp to the edge interval instead of
    // falling back to keyframe 0, which returns a pose from the start
    if (!found && (static_cast<i32>(input->count) >= 2))
    {
        keyframe = static_cast<i32>(input->count) - 2;

        auto tfirst = 0.0f;
        if (!cgltf_accessor_read_float(input, 0, &tfirst, 1)) return false;
        if (time < tfirst) keyframe = 0;

        if (!cgltf_accessor_read_float(input, keyframe, &tstart, 1)) return false;
        if (!cgltf_accessor_read_float(input, keyframe + 1, &tend, 1)) return false;
    }

    // Constant animation, no need to interpolate
    if (FloatEquals(tend, tstart))
    {
        interpolationType = cgltf_interpolation_type_step;
    }

    const f32 duration = fmaxf((tend - tstart), EPSILON);
    f32 t = (time - tstart)/duration;
    t = (t < 0.0f)? 0.0f : t;
    t = (t > 1.0f)? 1.0f : t;

    if (output->component_type != cgltf_component_type_r_32f) return false;

    if (output->type == cgltf_type_vec3)
    {
        switch (interpolationType)
        {
            case cgltf_interpolation_type_step:
            {
                f32 tmp[3] = { 0.0f };
                cgltf_accessor_read_float(output, keyframe, tmp, 3);
                const auto v1 = vec3{tmp[0], tmp[1], tmp[2]};
                auto r = static_cast<vec3 *>(data);

                *r = v1;
            } break;
            case cgltf_interpolation_type_linear:
            {
                f32 tmp[3] = { 0.0f };
                cgltf_accessor_read_float(output, keyframe, tmp, 3);
                const auto v1 = vec3{tmp[0], tmp[1], tmp[2]};
                cgltf_accessor_read_float(output, keyframe+1, tmp, 3);
                const auto v2 = vec3{tmp[0], tmp[1], tmp[2]};
                auto r = static_cast<vec3 *>(data);

                *r = glm::mix(v1, v2, t);
            } break;
            case cgltf_interpolation_type_cubic_spline:
            {
                f32 tmp[3] = { 0.0f };
                cgltf_accessor_read_float(output, 3*keyframe+1, tmp, 3);
                const auto v1 = vec3{tmp[0], tmp[1], tmp[2]};
                cgltf_accessor_read_float(output, 3*keyframe+2, tmp, 3);
                const auto tangent1 = vec3{tmp[0], tmp[1], tmp[2]};
                cgltf_accessor_read_float(output, 3*(keyframe+1)+1, tmp, 3);
                const auto v2 = vec3{tmp[0], tmp[1], tmp[2]};
                cgltf_accessor_read_float(output, 3*(keyframe+1), tmp, 3);
                const auto tangent2 = vec3{tmp[0], tmp[1], tmp[2]};
                auto r = static_cast<vec3 *>(data);

                *r = Vector3CubicHermite(v1, tangent1, v2, tangent2, t);
            } break;
            default: break;
        }
    }
    else if (output->type == cgltf_type_vec4)
    {
        // Only v4 is for rotations, so it's a quaternion
        switch (interpolationType)
        {
            case cgltf_interpolation_type_step:
            {
                f32 tmp[4] = { 0.0f };
                cgltf_accessor_read_float(output, keyframe, tmp, 4);
                auto r = static_cast<quat *>(data);

                *r = quat{tmp[3], tmp[0], tmp[1], tmp[2]};
            } break;
            case cgltf_interpolation_type_linear:
            {
                f32 tmp[4] = { 0.0f };
                cgltf_accessor_read_float(output, keyframe, tmp, 4);
                const auto q1 = quat{tmp[3], tmp[0], tmp[1], tmp[2]};
                cgltf_accessor_read_float(output, keyframe+1, tmp, 4);
                const auto q2 = quat{tmp[3], tmp[0], tmp[1], tmp[2]};
                auto r = static_cast<quat *>(data);

                *r = glm::slerp(q1, q2, t);
            } break;
            case cgltf_interpolation_type_cubic_spline:
            {
                f32 tmp[4] = { 0.0f };
                cgltf_accessor_read_float(output, 3*keyframe+1, tmp, 4);
                auto q1 = quat{tmp[3], tmp[0], tmp[1], tmp[2]};
                cgltf_accessor_read_float(output, 3*keyframe+2, tmp, 4);
                auto outTangent1 = quat{0.0f, tmp[0], tmp[1], tmp[2]};
                cgltf_accessor_read_float(output, 3*(keyframe+1)+1, tmp, 4);
                auto q2 = quat{tmp[3], tmp[0], tmp[1], tmp[2]};
                cgltf_accessor_read_float(output, 3*(keyframe+1), tmp, 4);
                auto inTangent2 = quat{0.0f, tmp[0], tmp[1], tmp[2]};
                auto r = static_cast<quat *>(data);

                q1 = glm::normalize(q1);
                q2 = glm::normalize(q2);

                if (glm::dot(q1, q2) < 0.0f) q2 = -q2;

                outTangent1 = QuaternionScale(outTangent1, duration);
                inTangent2 = QuaternionScale(inTangent2, duration);

                *r = QuaternionCubicHermiteSpline(q1, outTangent1, q2, inTangent2, t);
            } break;
            default: break;
        }
    }

    return true;
}

#define GLTF_FRAMERATE 60.0f    // glTF animation framerate (frames per second)

static ModelAnimation *LoadModelAnimationsGLTF(const char *fileName, i32 *animCount)
{
    // glTF file loading
    auto dataSize = 0;
    auto fileData = LoadFileData(fileName, &dataSize);

    ModelAnimation *animations = nullptr;

    // glTF data loading
    auto options = cgltf_options{};
    options.file.read = LoadFileGLTFCallback;
    options.file.release = ReleaseFileGLTFCallback;
    cgltf_data *data = nullptr;
    cgltf_result result = cgltf_parse(&options, fileData, dataSize, &data);

    if (result != cgltf_result_success)
    {
        LogWarning("MODEL: [{}] Failed to load glTF data", fileName);
        *animCount = 0;
        return nullptr;
    }

    result = cgltf_load_buffers(&options, data, fileName);
    if (result != cgltf_result_success) LogInfo("MODEL: [{}] Failed to load animation buffers", fileName);

    if (result == cgltf_result_success)
    {
        if (data->skins_count > 0)
        {
            const cgltf_skin skin = data->skins[0];

            // Precompute, per joint, the static transform contributed by any
            // intermediate non-joint nodes between the joint and its nearest
            // joint ancestor. This handles exporters (e.g. wow.export) that
            // store bone offsets on dummy parent nodes rather than on the
            // joints themselves. Depends only on the skin, not the animation.
            const auto jointCount = static_cast<i32>(skin.joints_count);
            auto extOffset = static_cast<mat4 *>(calloc(jointCount, sizeof(mat4)));

            if (extOffset == nullptr)
            {
                // Allocation failed: abort animation loading at the source rather than
                // propagating a NULL pointer into the per-frame transform loop below
                LogWarning("MODEL: [{}] Failed to allocate joint offset buffer", fileName);
                cgltf_free(data);
                UnloadFileData(fileData);
                *animCount = 0;
                return nullptr;
            }

            *animCount = static_cast<i32>(data->animations_count);
            animations = static_cast<ModelAnimation *>(calloc(data->animations_count, sizeof(ModelAnimation)));

            for (i32 k = 0; k < jointCount; k++)
            {
                extOffset[k] = mat4(1.0f);
                cgltf_node *n = skin.joints[k]->parent;

                while (n != nullptr)
                {
                    auto isJoint = false;
                    for (i32 jj = 0; jj < jointCount; jj++)
                    {
                        if (skin.joints[jj] == n)
                        {
                            isJoint = true;
                            break;
                        }
                    }

                    if (isJoint) break;

                    // Compose the intermediate node's local TRS (scale, then rotation, then translation)
                    const mat4 nodeScale = glm::scale(mat4(1.0f), vec3{n->scale[0], n->scale[1], n->scale[2]});
                    const mat4 nodeRotation = glm::mat4_cast(quat{n->rotation[3], n->rotation[0], n->rotation[1], n->rotation[2]});
                    const mat4 nodeTranslation = glm::translate(mat4(1.0f), vec3{n->translation[0], n->translation[1], n->translation[2]});
                    const mat4 nodeTransform = ((nodeTranslation) * (((nodeRotation) * (nodeScale))));

                    extOffset[k] = ((nodeTransform) * (extOffset[k]));
                    n = n->parent;
                }
            }

            for (u32 a = 0; a < data->animations_count; a++)
            {
                auto bones = LoadBoneInfoGLTF(skin, &animations[a].boneCount);

                const cgltf_animation animData = data->animations[a];

                struct Channels {
                    cgltf_animation_channel *translate;
                    cgltf_animation_channel *rotate;
                    cgltf_animation_channel *scale;
                };

                auto boneChannels = static_cast<struct Channels *>(calloc(animations[a].boneCount, sizeof(struct Channels)));
                auto animDuration = 0.0f;

                for (u32 j = 0; j < animData.channels_count; j++)
                {
                    const cgltf_animation_channel channel = animData.channels[j];
                    auto boneIndex = -1;

                    for (u32 k = 0; k < skin.joints_count; k++)
                    {
                        if (animData.channels[j].target_node == skin.joints[k])
                        {
                            boneIndex = static_cast<i32>(k);
                            break;
                        }
                    }

                    if (boneIndex == -1) continue; // Animation channel for a node not in the skeleton

                    if (animData.channels[j].sampler->interpolation != cgltf_interpolation_type_max_enum)
                    {
                        if (channel.target_path == cgltf_animation_path_type_translation) boneChannels[boneIndex].translate = &animData.channels[j];
                        else if (channel.target_path == cgltf_animation_path_type_rotation) boneChannels[boneIndex].rotate = &animData.channels[j];
                        else if (channel.target_path == cgltf_animation_path_type_scale) boneChannels[boneIndex].scale = &animData.channels[j];
                        else LogWarning("MODEL: [{}] Unsupported target_path on channel {}'s sampler for animation {}. Skipping.", fileName, j, a);
                    }
                    else LogWarning("MODEL: [{}] Invalid interpolation curve encountered for GLTF animation.", fileName);

                    auto time = 0.0f;
                    result = static_cast<cgltf_result>(cgltf_accessor_read_float(channel.sampler->input, channel.sampler->input->count - 1, &time, 1));
                    if (!result)
                    {
                        LogWarning("MODEL: [{}] Failed to load input time", fileName);
                        continue;
                    }

                    animDuration = (time > animDuration)? time : animDuration;
                }

                if (animData.name != nullptr) snprintf(animations[a].name, sizeof(animations[a].name), "%s", animData.name);

                animations[a].keyframeCount = static_cast<i32>(animDuration*GLTF_FRAMERATE) + 1;
                animations[a].keyframePoses = static_cast<Transform * *>(calloc(animations[a].keyframeCount, sizeof(Transform *)));

                for (i32 j = 0; j < animations[a].keyframeCount; j++)
                {
                    animations[a].keyframePoses[j] = static_cast<Transform *>(calloc(animations[a].boneCount, sizeof(Transform)));
                    const f32 time = static_cast<f32>(j) / GLTF_FRAMERATE;

                    for (u32 k = 0; k < animations[a].boneCount; k++)
                    {
                        auto translation = vec3{skin.joints[k]->translation[0], skin.joints[k]->translation[1], skin.joints[k]->translation[2]};
                        auto rotation = quat{skin.joints[k]->rotation[3], skin.joints[k]->rotation[0], skin.joints[k]->rotation[1], skin.joints[k]->rotation[2]};
                        auto scale = vec3{skin.joints[k]->scale[0], skin.joints[k]->scale[1], skin.joints[k]->scale[2]};

                        if (boneChannels[k].translate)
                        {
                            if (!GetPoseAtTimeGLTF(boneChannels[k].translate->sampler->interpolation, boneChannels[k].translate->sampler->input, boneChannels[k].translate->sampler->output, time, &translation))
                            {
                                LogInfo("MODEL: [{}] Failed to load translate pose data for bone {}", fileName, bones[k].name);
                            }
                        }

                        if (boneChannels[k].rotate)
                        {
                            if (!GetPoseAtTimeGLTF(boneChannels[k].rotate->sampler->interpolation, boneChannels[k].rotate->sampler->input, boneChannels[k].rotate->sampler->output, time, &rotation))
                            {
                                LogInfo("MODEL: [{}] Failed to load rotate pose data for bone {}", fileName, bones[k].name);
                            }
                        }

                        if (boneChannels[k].scale)
                        {
                            if (!GetPoseAtTimeGLTF(boneChannels[k].scale->sampler->interpolation, boneChannels[k].scale->sampler->input, boneChannels[k].scale->sampler->output, time, &scale))
                            {
                                LogInfo("MODEL: [{}] Failed to load scale pose data for bone {}", fileName, bones[k].name);
                            }
                        }

                        // Compose joint local TRS, then prepend the static
                        // intermediate non-joint offsets so the final TRS is
                        // expressed relative to the joint's skeleton parent.
                        const mat4 S = glm::scale(mat4(1.0f), vec3{scale.x, scale.y, scale.z});
                        const mat4 R = glm::mat4_cast(rotation);
                        const mat4 T = glm::translate(mat4(1.0f), vec3{translation.x, translation.y, translation.z});
                        const mat4 jointLocal = ((T) * (((R) * (S))));
                        const mat4 combined = ((extOffset[k]) * (jointLocal));

                        Transform tr;
                        MatrixDecompose(combined, &tr.translation, &tr.rotation, &tr.scale);
                        animations[a].keyframePoses[j][k] = tr;
                    }

                    BuildPoseFromParentJoints(bones, static_cast<i32>(animations[a].boneCount), animations[a].keyframePoses[j]);
                }

                LogInfo("MODEL: [{}] Loaded animation: {} | Frames: {} | Duration: {:.6f}s", fileName, (animData.name != nullptr)? animData.name : "NULL", animations[a].keyframeCount, animDuration);
                free(boneChannels);
                free(bones);
            }

            free(extOffset);
        }

        if (data->skins_count > 1)
        {
            LogWarning("MODEL: [{}] Expected one unique skin to load animation data from, but found {}", fileName, data->skins_count);
        }

        cgltf_free(data);
    }

    UnloadFileData(fileData);

    return animations;
}
#endif

#if SUPPORT_FILEFORMAT_VOX
// Load VOX (MagicaVoxel) mesh data
static Model LoadVOX(const char *fileName)
{
    auto model = Model{ 0 };

    auto nbvertices = 0;
    auto meshescount = 0;

    // Read vox file into buffer
    auto dataSize = 0;
    auto fileData = LoadFileData(fileName, &dataSize);

    if (fileData == nullptr)
    {
        LogWarning("MODEL: [{}] Failed to load VOX file", fileName);
        return model;
    }

    // Read and build voxarray description
    auto voxarray = VoxArray3D{ 0 };
    const i32 ret = Vox_LoadFromMemory(fileData, dataSize, &voxarray);

    if (ret != VOX_SUCCESS)
    {
        // Error
        UnloadFileData(fileData);

        LogWarning("MODEL: [{}] Failed to load VOX data", fileName);
        return model;
    }
    else
    {
        // Success: Compute meshes count
        nbvertices = voxarray.vertices.used;
        meshescount = 1 + (nbvertices/65536);

        LogInfo("MODEL: [{}] VOX data loaded successfully : {} vertices/{} meshes", fileName, nbvertices, meshescount);
    }

    // Build models from meshes
    model.transform = mat4(1.0f);

    model.meshCount = meshescount;
    model.meshes = static_cast<Mesh *>(calloc(model.meshCount, sizeof(Mesh)));

    model.meshMaterial = static_cast<i32 *>(calloc(model.meshCount, sizeof(i32)));

    model.materialCount = 1;
    model.materials = static_cast<Material *>(calloc(model.materialCount, sizeof(Material)));
    model.materials[0] = LoadMaterialDefault();

    // Init model meshes
    i32 verticesRemain = voxarray.vertices.used;
    constexpr auto verticesMax = 65532; // 5461 voxels x 12 vertices per voxel -> 65532 (must be inf 65536)

    // 6*4 = 12 vertices per voxel
    auto pvertices = reinterpret_cast<vec3 *>(voxarray.vertices.array);
    auto pnormals = reinterpret_cast<vec3 *>(voxarray.normals.array);
    auto pcolors = reinterpret_cast<Color *>(voxarray.colors.array);

    u16 *pindices = voxarray.indices.array;    // 5461*6*6 = 196596 indices max per mesh

    for (i32 i = 0; i < meshescount; i++)
    {
        Mesh *pmesh = &model.meshes[i];
        memset(pmesh, 0, sizeof(Mesh));

        // Copy vertices
        pmesh->vertexCount = static_cast<i32>(fmin(verticesMax, verticesRemain));

        pmesh->vertices = static_cast<f32 *>(calloc(pmesh->vertexCount*3, sizeof(f32)));
        memcpy(pmesh->vertices, pvertices, pmesh->vertexCount*3*sizeof(f32));

        // Copy normals
        pmesh->normals = static_cast<f32 *>(calloc(pmesh->vertexCount*3, sizeof(f32)));
        memcpy(pmesh->normals, pnormals, pmesh->vertexCount*3*sizeof(f32));

        // Copy indices
        pmesh->indices = static_cast<u16 *>(calloc((voxarray.indices.used > 0) ? voxarray.indices.used : 1, sizeof(u16)));
        memcpy(pmesh->indices, pindices, voxarray.indices.used*sizeof(u16));

        pmesh->triangleCount = (pmesh->vertexCount/4)*2;

        // Copy colors
        pmesh->colors = static_cast<u8 *>(calloc(pmesh->vertexCount*4, sizeof(u8)));
        memcpy(pmesh->colors, pcolors, pmesh->vertexCount*4*sizeof(u8));

        // First material index
        model.meshMaterial[i] = 0;

        verticesRemain -= verticesMax;
        pvertices += verticesMax;
        pnormals += verticesMax;
        pcolors += verticesMax;
    }

    // Free buffers
    Vox_FreeArrays(&voxarray);
    UnloadFileData(fileData);

    return model;
}
#endif

#if SUPPORT_FILEFORMAT_M3D
// Hook LoadFileData()/UnloadFileData() calls to M3D loaders
u8 *m3d_loaderhook(char *fn, u32 *len) { return LoadFileData(static_cast<const char *>(fn), reinterpret_cast<i32 *>(len)); }
void m3d_freehook(void *data) { UnloadFileData(static_cast<u8 *>(data)); }

// Load M3D mesh data
static Model LoadM3D(const char *fileName)
{
    auto model = Model{ 0 };

    m3d_t *m3d = nullptr;
    m3dp_t *prop = nullptr;
    i32 i, j, k, l, n, mi = -2, vcolor = 0;

    auto dataSize = 0;
    auto fileData = LoadFileData(fileName, &dataSize);

    if (fileData != nullptr)
    {
        m3d = m3d_load(fileData, m3d_loaderhook, m3d_freehook, nullptr);

        if (!m3d || M3D_ERR_ISFATAL(m3d->errcode))
        {
            LogWarning("MODEL: [{}] Failed to load M3D data, error code {}", fileName, m3d? m3d->errcode : -2);
            if (m3d) m3d_free(m3d);
            UnloadFileData(fileData);
            return model;
        }
        else LogInfo("MODEL: [{}] M3D data loaded successfully: {} faces/{} materials", fileName, m3d->numface, m3d->nummaterial);

        // Check if face is found, if not, probably just a material library
        if (!m3d->numface)
        {
            m3d_free(m3d);
            UnloadFileData(fileData);
            return model;
        }

        if (m3d->nummaterial > 0)
        {
            model.meshCount = model.materialCount = static_cast<i32>(m3d->nummaterial);
            LogInfo("MODEL: model has {} material meshes", model.materialCount);
        }
        else
        {
            model.meshCount = 1; model.materialCount = 0;
            LogInfo("MODEL: No materials, putting all meshes in a default material");
        }

        // A default material is always required, so adding +1
        model.materialCount++;

        // Faces must be in non-decreasing materialid order. Verify that quickly, sorting them otherwise
        // WARNING: Sorting is not needed, valid M3D model files should already be sorted
        // Keeping the sorting function for reference (Check PR #3363 #3385)
        /*
        for (a = 1; a < m3d->numface; a++)
        {
            if (m3d->face[a-1].materialid <= m3d->face[a].materialid) continue;

            // face[a-1] > face[a].  slide face[a] lower
            m3df_t slider = m3d->face[a];
            j = a-1;

            do
            {   // face[j] > slider, face[j+1] is svailable vacant gap
                m3d->face[j+1] = m3d->face[j];
                j = j-1;
            }
            while (j >= 0 && m3d->face[j].materialid > slider.materialid);

            m3d->face[j+1] = slider;
        }
        */

        model.meshes = static_cast<Mesh *>(calloc((model.meshCount > 0) ? model.meshCount : 1, sizeof(Mesh)));
        model.meshMaterial = static_cast<i32 *>(calloc((model.meshCount > 0) ? model.meshCount : 1, sizeof(i32)));
        model.materials = static_cast<Material *>(calloc(model.materialCount + 1, sizeof(Material)));

        // Map no material to index 0 with default shader, everything else materialid + 1
        model.materials[0] = LoadMaterialDefault();

        for (i = l = 0, k = -1; i < static_cast<i32>(m3d->numface); i++, l++)
        {
            // Materials are grouped together
            if (mi != m3d->face[i].materialid)
            {
                // There should be only one material switch per material kind,
                // but be bulletproof for non-optimal model files
                if (k + 1 >= model.meshCount)
                {
                    model.meshCount++;

                    // Create a second buffer for mesh re-allocation
                    auto tempMeshes = static_cast<Mesh *>(calloc(model.meshCount, sizeof(Mesh)));
                    memcpy(tempMeshes, model.meshes, (model.meshCount - 1)*sizeof(Mesh));
                    free(model.meshes);
                    model.meshes = tempMeshes;

                    // Create a second buffer for material re-allocation
                    auto tempMeshMaterial = static_cast<i32 *>(calloc(model.meshCount, sizeof(i32)));
                    memcpy(tempMeshMaterial, model.meshMaterial, (model.meshCount - 1)*sizeof(i32));
                    free(model.meshMaterial);
                    model.meshMaterial = tempMeshMaterial;
                }

                k++;
                mi = static_cast<i32>(m3d->face[i].materialid);

                // Only allocate colors VertexBuffer if there's a color vertex in the model for this material batch
                // if all colors are fully transparent black for all vertices of this material, then assuming no vertex colors
                for (j = i, l = vcolor = 0; (j < static_cast<i32>(m3d->numface)) && (mi == m3d->face[j].materialid); j++, l++)
                {
                    if (!m3d->vertex[m3d->face[j].vertex[0]].color ||
                        !m3d->vertex[m3d->face[j].vertex[1]].color ||
                        !m3d->vertex[m3d->face[j].vertex[2]].color) vcolor = 1;
                }

                model.meshes[k].vertexCount = l*3;
                model.meshes[k].triangleCount = l;
                model.meshes[k].vertices = static_cast<f32 *>(calloc(model.meshes[k].vertexCount*3, sizeof(f32)));
                model.meshes[k].texcoords = static_cast<f32 *>(calloc(model.meshes[k].vertexCount*2, sizeof(f32)));
                model.meshes[k].normals = static_cast<f32 *>(calloc(model.meshes[k].vertexCount*3, sizeof(f32)));

                // If no map is provided, or colors are defined, allocate storage for vertex colors
                // M3D specs only consider vertex colors if no material is provided, however raylib uses both and mixes the colors
                if ((mi == M3D_UNDEF) || vcolor) model.meshes[k].colors = static_cast<u8 *>(calloc(model.meshes[k].vertexCount*4, sizeof(u8)));

                // If no map is provided and vertex colors are allocated, set them to white
                if ((mi == M3D_UNDEF) && (model.meshes[k].colors != nullptr))
                {
                    for (i32 c = 0; c < model.meshes[k].vertexCount*4; c++) model.meshes[k].colors[c] = 255;
                }

                if (m3d->numbone && m3d->numskin)
                {
                    model.meshes[k].boneIndices = static_cast<u8 *>(calloc(model.meshes[k].vertexCount*4, sizeof(u8)));
                    model.meshes[k].boneWeights = static_cast<f32 *>(calloc(model.meshes[k].vertexCount*4, sizeof(f32)));
                }

                model.meshMaterial[k] = mi + 1;
                l = 0;
            }

            // Process meshes per material, add triangles
            // NOTE: Buffer data is checked for null: allocation may fail on huge models
            if (model.meshes[k].vertices != nullptr)
            {
                model.meshes[k].vertices[l*9 + 0] = m3d->vertex[m3d->face[i].vertex[0]].x*m3d->scale;
                model.meshes[k].vertices[l*9 + 1] = m3d->vertex[m3d->face[i].vertex[0]].y*m3d->scale;
                model.meshes[k].vertices[l*9 + 2] = m3d->vertex[m3d->face[i].vertex[0]].z*m3d->scale;
                model.meshes[k].vertices[l*9 + 3] = m3d->vertex[m3d->face[i].vertex[1]].x*m3d->scale;
                model.meshes[k].vertices[l*9 + 4] = m3d->vertex[m3d->face[i].vertex[1]].y*m3d->scale;
                model.meshes[k].vertices[l*9 + 5] = m3d->vertex[m3d->face[i].vertex[1]].z*m3d->scale;
                model.meshes[k].vertices[l*9 + 6] = m3d->vertex[m3d->face[i].vertex[2]].x*m3d->scale;
                model.meshes[k].vertices[l*9 + 7] = m3d->vertex[m3d->face[i].vertex[2]].y*m3d->scale;
                model.meshes[k].vertices[l*9 + 8] = m3d->vertex[m3d->face[i].vertex[2]].z*m3d->scale;
            }

            // Without vertex color (full transparency), using the default color
            if (model.meshes[k].colors != nullptr)
            {
                if (m3d->vertex[m3d->face[i].vertex[0]].color & 0xff000000)
                    memcpy(&model.meshes[k].colors[l*12 + 0], &m3d->vertex[m3d->face[i].vertex[0]].color, 4);
                if (m3d->vertex[m3d->face[i].vertex[1]].color & 0xff000000)
                    memcpy(&model.meshes[k].colors[l*12 + 4], &m3d->vertex[m3d->face[i].vertex[1]].color, 4);
                if (m3d->vertex[m3d->face[i].vertex[2]].color & 0xff000000)
                    memcpy(&model.meshes[k].colors[l*12 + 8], &m3d->vertex[m3d->face[i].vertex[2]].color, 4);
            }

            if ((model.meshes[k].texcoords != nullptr) && (m3d->face[i].texcoord[0] != M3D_UNDEF))
            {
                model.meshes[k].texcoords[l*6 + 0] = m3d->tmap[m3d->face[i].texcoord[0]].u;
                model.meshes[k].texcoords[l*6 + 1] = 1.0f - m3d->tmap[m3d->face[i].texcoord[0]].v;
                model.meshes[k].texcoords[l*6 + 2] = m3d->tmap[m3d->face[i].texcoord[1]].u;
                model.meshes[k].texcoords[l*6 + 3] = 1.0f - m3d->tmap[m3d->face[i].texcoord[1]].v;
                model.meshes[k].texcoords[l*6 + 4] = m3d->tmap[m3d->face[i].texcoord[2]].u;
                model.meshes[k].texcoords[l*6 + 5] = 1.0f - m3d->tmap[m3d->face[i].texcoord[2]].v;
            }

            if ((model.meshes[k].normals != nullptr) && (m3d->face[i].normal[0] != M3D_UNDEF))
            {
                model.meshes[k].normals[l*9 + 0] = m3d->vertex[m3d->face[i].normal[0]].x;
                model.meshes[k].normals[l*9 + 1] = m3d->vertex[m3d->face[i].normal[0]].y;
                model.meshes[k].normals[l*9 + 2] = m3d->vertex[m3d->face[i].normal[0]].z;
                model.meshes[k].normals[l*9 + 3] = m3d->vertex[m3d->face[i].normal[1]].x;
                model.meshes[k].normals[l*9 + 4] = m3d->vertex[m3d->face[i].normal[1]].y;
                model.meshes[k].normals[l*9 + 5] = m3d->vertex[m3d->face[i].normal[1]].z;
                model.meshes[k].normals[l*9 + 6] = m3d->vertex[m3d->face[i].normal[2]].x;
                model.meshes[k].normals[l*9 + 7] = m3d->vertex[m3d->face[i].normal[2]].y;
                model.meshes[k].normals[l*9 + 8] = m3d->vertex[m3d->face[i].normal[2]].z;
            }

            // Add skin (vertex / bone weight pairs)
            if ((m3d->numbone && m3d->numskin) &&
                (model.meshes[k].boneIndices != nullptr) && (model.meshes[k].boneWeights != nullptr))
            {
                for (n = 0; n < 3; n++)
                {
                    const i32 skinid = static_cast<i32>(m3d->vertex[m3d->face[i].vertex[n]].skinid);

                    // Check if there is a skin for this mesh
                    if ((skinid != M3D_UNDEF) && (skinid < static_cast<i32>(m3d->numskin)))
                    {
                        for (j = 0; j < 4; j++)
                        {
                            model.meshes[k].boneIndices[l*12 + n*4 + j] = m3d->skin[skinid].boneid[j];
                            model.meshes[k].boneWeights[l*12 + n*4 + j] = m3d->skin[skinid].weight[j];
                        }
                    }
                    else
                    {
                        // Boneless meshes with skeletal animations are not supported, so
                        // putting all vertices without a bone into a special "no bone" bone
                        model.meshes[k].boneIndices[l*12 + n*4] = m3d->numbone;
                        model.meshes[k].boneWeights[l*12 + n*4] = 1.0f;
                    }
                }
            }
        }

        // Load materials
        for (i = 0; i < static_cast<i32>(m3d->nummaterial); i++)
        {
            model.materials[i + 1] = LoadMaterialDefault();

            for (j = 0; j < m3d->material[i].numprop; j++)
            {
                prop = &m3d->material[i].prop[j];

                switch (prop->type)
                {
                    case m3dp_Kd:
                    {
                        memcpy(&model.materials[i + 1].maps[MATERIAL_MAP_DIFFUSE].color, &prop->value.color, 4);
                        model.materials[i + 1].maps[MATERIAL_MAP_DIFFUSE].value = 0.0f;
                    } break;
                    case m3dp_Ks:
                    {
                        memcpy(&model.materials[i + 1].maps[MATERIAL_MAP_SPECULAR].color, &prop->value.color, 4);
                    } break;
                    case m3dp_Ns:
                    {
                        model.materials[i + 1].maps[MATERIAL_MAP_SPECULAR].value = prop->value.fnum;
                    } break;
                    case m3dp_Ke:
                    {
                        memcpy(&model.materials[i + 1].maps[MATERIAL_MAP_EMISSION].color, &prop->value.color, 4);
                        model.materials[i + 1].maps[MATERIAL_MAP_EMISSION].value = 0.0f;
                    } break;
                    case m3dp_Pm:
                    {
                        model.materials[i + 1].maps[MATERIAL_MAP_METALNESS].value = prop->value.fnum;
                    } break;
                    case m3dp_Pr:
                    {
                        model.materials[i + 1].maps[MATERIAL_MAP_ROUGHNESS].value = prop->value.fnum;
                    } break;
                    case m3dp_Ps:
                    {
                        model.materials[i + 1].maps[MATERIAL_MAP_NORMAL].color = WHITE;
                        model.materials[i + 1].maps[MATERIAL_MAP_NORMAL].value = prop->value.fnum;
                    } break;
                    default:
                    {
                        if (prop->type >= 128)
                        {
                            auto image = Image{ nullptr };
                            image.data = m3d->texture[prop->value.textureid].d;
                            image.width = m3d->texture[prop->value.textureid].w;
                            image.height = m3d->texture[prop->value.textureid].h;
                            image.mipmaps = 1;
                            image.format = (m3d->texture[prop->value.textureid].f == 4)? PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 :
                                           ((m3d->texture[prop->value.textureid].f == 3)? PIXELFORMAT_UNCOMPRESSED_R8G8B8 :
                                           ((m3d->texture[prop->value.textureid].f == 2)? PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA : PIXELFORMAT_UNCOMPRESSED_GRAYSCALE));

                            switch (prop->type)
                            {
                                case m3dp_map_Kd: model.materials[i + 1].maps[MATERIAL_MAP_DIFFUSE].texture = LoadTextureFromImage(image); break;
                                case m3dp_map_Ks: model.materials[i + 1].maps[MATERIAL_MAP_SPECULAR].texture = LoadTextureFromImage(image); break;
                                case m3dp_map_Ke: model.materials[i + 1].maps[MATERIAL_MAP_EMISSION].texture = LoadTextureFromImage(image); break;
                                case m3dp_map_Km: model.materials[i + 1].maps[MATERIAL_MAP_NORMAL].texture = LoadTextureFromImage(image); break;
                                case m3dp_map_Ka: model.materials[i + 1].maps[MATERIAL_MAP_OCCLUSION].texture = LoadTextureFromImage(image); break;
                                case m3dp_map_Pm: model.materials[i + 1].maps[MATERIAL_MAP_ROUGHNESS].texture = LoadTextureFromImage(image); break;
                                default: break;
                            }
                        }
                    } break;
                }
            }
        }

        // Load bones
        if (m3d->numbone)
        {
            model.skeleton.boneCount = m3d->numbone + 1;
            model.skeleton.bones = static_cast<BoneInfo *>(calloc(model.skeleton.boneCount, sizeof(BoneInfo)));
            model.skeleton.bindPose = static_cast<Transform *>(calloc(model.skeleton.boneCount, sizeof(Transform)));

            for (i = 0; i < static_cast<i32>(m3d->numbone); i++)
            {
                model.skeleton.bones[i].parent = static_cast<i32>(m3d->bone[i].parent);
                snprintf(model.skeleton.bones[i].name, sizeof(model.skeleton.bones[i].name), "%s", m3d->bone[i].name);
                model.skeleton.bindPose[i].translation.x = m3d->vertex[m3d->bone[i].pos].x*m3d->scale;
                model.skeleton.bindPose[i].translation.y = m3d->vertex[m3d->bone[i].pos].y*m3d->scale;
                model.skeleton.bindPose[i].translation.z = m3d->vertex[m3d->bone[i].pos].z*m3d->scale;
                model.skeleton.bindPose[i].rotation.x = m3d->vertex[m3d->bone[i].ori].x;
                model.skeleton.bindPose[i].rotation.y = m3d->vertex[m3d->bone[i].ori].y;
                model.skeleton.bindPose[i].rotation.z = m3d->vertex[m3d->bone[i].ori].z;
                model.skeleton.bindPose[i].rotation.w = m3d->vertex[m3d->bone[i].ori].w;

                // NOTE: If the orientation quaternion is not normalized, then that's encoding scaling
                model.skeleton.bindPose[i].rotation = glm::normalize(model.skeleton.bindPose[i].rotation);
                model.skeleton.bindPose[i].scale.x = model.skeleton.bindPose[i].scale.y = model.skeleton.bindPose[i].scale.z = 1.0f;

                // Child bones are stored in parent bone relative space, convert that into model space
                if (model.skeleton.bones[i].parent >= 0)
                {
                    model.skeleton.bindPose[i].rotation = ((model.skeleton.bindPose[model.skeleton.bones[i].parent].rotation) * (model.skeleton.bindPose[i].rotation));
                    model.skeleton.bindPose[i].translation = ((model.skeleton.bindPose[model.skeleton.bones[i].parent].rotation)*(model.skeleton.bindPose[i].translation));
                    model.skeleton.bindPose[i].translation = ((model.skeleton.bindPose[i].translation) + (model.skeleton.bindPose[model.skeleton.bones[i].parent].translation));
                    model.skeleton.bindPose[i].scale = ((model.skeleton.bindPose[i].scale) * (model.skeleton.bindPose[model.skeleton.bones[i].parent].scale));
                }
            }

            // Add a special "no bone" bone
            model.skeleton.bones[i].parent = -1;
            memcpy(model.skeleton.bones[i].name, "NO BONE", 7);
            model.skeleton.bindPose[i].translation.x = 0.0f;
            model.skeleton.bindPose[i].translation.y = 0.0f;
            model.skeleton.bindPose[i].translation.z = 0.0f;
            model.skeleton.bindPose[i].rotation.x = 0.0f;
            model.skeleton.bindPose[i].rotation.y = 0.0f;
            model.skeleton.bindPose[i].rotation.z = 0.0f;
            model.skeleton.bindPose[i].rotation.w = 1.0f;
            model.skeleton.bindPose[i].scale.x = model.skeleton.bindPose[i].scale.y = model.skeleton.bindPose[i].scale.z = 1.0f;
        }

        // Load bone-pose default mesh into animation vertices. These will be updated when UpdateModelAnimation gets
        // called, but not before, however DrawMesh uses these if they exist (so not good if they are left empty)
        if (m3d->numbone && m3d->numskin)
        {
            for (i = 0; i < model.meshCount; i++)
            {
                model.meshes[i].boneCount = static_cast<i32>(model.skeleton.boneCount);
            }

            // Initialize runtime animation data: current pose and bone matrices
            model.currentPose = static_cast<Transform *>(calloc((model.skeleton.boneCount > 0) ? model.skeleton.boneCount : 1, sizeof(Transform)));
            model.boneMatrices = static_cast<mat4 *>(calloc((model.skeleton.boneCount > 0) ? model.skeleton.boneCount : 1, sizeof(mat4)));
            for (u32 j = 0; j < model.skeleton.boneCount; j++) model.boneMatrices[j] = mat4(1.0f);
        }

        m3d_free(m3d);
        UnloadFileData(fileData);
    }

    return model;
}

#define M3D_ANIMDELAY 17    // Animation frames delay, (~1000 ms/60 FPS = 16.666666 ms)

// Load M3D animation data
static ModelAnimation *LoadModelAnimationsM3D(const char *fileName, i32 *animCount)
{
    ModelAnimation *animations = nullptr;

    m3d_t *m3d = nullptr;
    i32 i = 0, j = 0;
    *animCount = 0;

    auto dataSize = 0;
    auto fileData = LoadFileData(fileName, &dataSize);

    if (fileData != nullptr)
    {
        m3d = m3d_load(fileData, m3d_loaderhook, m3d_freehook, nullptr);

        if (!m3d || M3D_ERR_ISFATAL(m3d->errcode))
        {
            LogWarning("MODEL: [{}] Failed to load M3D data, error code {}", fileName, m3d? m3d->errcode : -2);
            UnloadFileData(fileData);
            return nullptr;
        }
        else LogInfo("MODEL: [{}] M3D data loaded successfully: {} animations, {} bones, {} skins", fileName, m3d->numaction, m3d->numbone, m3d->numskin);

        // No animation or bones, exit out. skins are not required because some people use one animation for N models
        if (!m3d->numaction || !m3d->numbone)
        {
            m3d_free(m3d);
            UnloadFileData(fileData);
            return nullptr;
        }

        animations = static_cast<ModelAnimation *>(calloc(m3d->numaction, sizeof(ModelAnimation)));
        *animCount = static_cast<i32>(m3d->numaction);

        for (u32 a = 0; a < m3d->numaction; a++)
        {
            animations[a].boneCount = m3d->numbone + 1;
            auto bones = static_cast<BoneInfo *>(calloc((m3d->numbone + 1), sizeof(BoneInfo)));

            animations[a].keyframeCount = static_cast<i32>(m3d->action[a].durationmsec)/M3D_ANIMDELAY;
            animations[a].keyframePoses = static_cast<Transform * *>(calloc(animations[a].keyframeCount, sizeof(Transform *)));
            snprintf(animations[a].name, sizeof(animations[a].name), "%s", m3d->action[a].name);

            LogInfo("MODEL: [{}] Loaded animation: {} | Frames: {} | Duration: {} ms", fileName, animations[a].name, animations[a].keyframeCount, m3d->action[a].durationmsec);

            for (i = 0; i < static_cast<i32>(m3d->numbone); i++)
            {
                bones[i].parent = static_cast<i32>(m3d->bone[i].parent);
                snprintf(bones[i].name, sizeof(bones[i].name), "%s", m3d->bone[i].name);
            }

            // A special, never transformed "no bone" bone, used for boneless vertices
            bones[i].parent = -1;
            memcpy(bones[i].name, "NO BONE", 7);

            // M3D stores frames at arbitrary intervals with sparse skeletons; Full skeletons is required at
            // regular intervals, so let the M3D SDK do the heavy lifting and calculate interpolated bones
            for (i = 0; i < animations[a].keyframeCount; i++)
            {
                animations[a].keyframePoses[i] = static_cast<Transform *>(calloc((m3d->numbone + 1), sizeof(Transform)));

                m3db_t *pose = m3d_pose(m3d, a, i*M3D_ANIMDELAY);

                if (pose != nullptr)
                {
                    for (j = 0; j < static_cast<i32>(m3d->numbone); j++)
                    {
                        animations[a].keyframePoses[i][j].translation.x = m3d->vertex[pose[j].pos].x*m3d->scale;
                        animations[a].keyframePoses[i][j].translation.y = m3d->vertex[pose[j].pos].y*m3d->scale;
                        animations[a].keyframePoses[i][j].translation.z = m3d->vertex[pose[j].pos].z*m3d->scale;
                        animations[a].keyframePoses[i][j].rotation.x = m3d->vertex[pose[j].ori].x;
                        animations[a].keyframePoses[i][j].rotation.y = m3d->vertex[pose[j].ori].y;
                        animations[a].keyframePoses[i][j].rotation.z = m3d->vertex[pose[j].ori].z;
                        animations[a].keyframePoses[i][j].rotation.w = m3d->vertex[pose[j].ori].w;
                        animations[a].keyframePoses[i][j].rotation = glm::normalize(animations[a].keyframePoses[i][j].rotation);
                        animations[a].keyframePoses[i][j].scale.x = animations[a].keyframePoses[i][j].scale.y = animations[a].keyframePoses[i][j].scale.z = 1.0f;

                        // Child bones are stored in parent bone relative space, convert that into model space
                        if (bones[j].parent >= 0)
                        {
                            animations[a].keyframePoses[i][j].rotation = ((animations[a].keyframePoses[i][bones[j].parent].rotation) * (animations[a].keyframePoses[i][j].rotation));
                            animations[a].keyframePoses[i][j].translation = ((animations[a].keyframePoses[i][bones[j].parent].rotation)*(animations[a].keyframePoses[i][j].translation));
                            animations[a].keyframePoses[i][j].translation = ((animations[a].keyframePoses[i][j].translation) + (animations[a].keyframePoses[i][bones[j].parent].translation));
                            animations[a].keyframePoses[i][j].scale = ((animations[a].keyframePoses[i][j].scale) * (animations[a].keyframePoses[i][bones[j].parent].scale));
                        }
                    }

                    // Default transform for the "no bone" bone
                    animations[a].keyframePoses[i][j].translation.x = 0.0f;
                    animations[a].keyframePoses[i][j].translation.y = 0.0f;
                    animations[a].keyframePoses[i][j].translation.z = 0.0f;
                    animations[a].keyframePoses[i][j].rotation.x = 0.0f;
                    animations[a].keyframePoses[i][j].rotation.y = 0.0f;
                    animations[a].keyframePoses[i][j].rotation.z = 0.0f;
                    animations[a].keyframePoses[i][j].rotation.w = 1.0f;
                    animations[a].keyframePoses[i][j].scale.x = 1.0f;
                    animations[a].keyframePoses[i][j].scale.y = 1.0f;
                    animations[a].keyframePoses[i][j].scale.z = 1.0f;

                    free(pose);
                }
            }

            free(bones);
        }

        m3d_free(m3d);
        UnloadFileData(fileData);
    }

    return animations;
}
#endif

#endif // SUPPORT_MODULE_RMODELS
