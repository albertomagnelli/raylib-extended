/*******************************************************************************************
*
*   raylib [others] example - OpenGL 4.6 GPU-driven instancing with glMultiDrawElementsIndirect
*
*   Example complexity rating: [★★★★] 4/4
*
*   Example originally created with raylib 6.5
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2026 raylib contributors
*
********************************************************************************************
*
*   WARNING: This example requires raylib to be compiled with OpenGL 4.6 support:
*       cmake -B build -DOPENGL_VERSION=4.6
*   or define GRAPHICS_API_OPENGL_46 when compiling rlgl/raylib.
*
*   A GPU-driven asteroid field, everything except the camera runs on the GPU:
*
*       - 40000 instances live in one SSBO (model matrix, color, bounding sphere)
*       - Three sphere LODs (high/mid/low poly) are packed into ONE vertex buffer and
*         ONE index buffer, the LODs only differ in firstIndex/baseVertex
*       - A COMPUTE SHADER culls every instance against the view frustum, picks the LOD
*         from the distance to the camera and compacts the survivors per LOD group with
*         atomicAdd() into a second SSBO
*       - A second compute step writes the instanceCount of every indirect command from
*         those atomics, so the CPU never touches a count
*       - glMultiDrawElementsIndirect() (OpenGL 4.6 core) renders all three LOD groups
*         with a single indexed draw call, the per-instance attributes are fetched starting
*         at the command's baseInstance (that is what makes the compaction work)
*       - gl_DrawID (GLSL 460) selects a per-LOD tint inside the vertex shader
*       - Direct State Access (DSA, OpenGL 4.5+) creates and fills buffers/vertex arrays
*
*   Controls:
*       MOUSE            : Orbit the camera, wheel zooms
*       C                : Toggle frustum culling (watch the visible count in the HUD)
*       L                : Toggle LOD selection (all instances use the high poly mesh)
*       F                : Toggle wireframe
*       SPACE            : Pause/resume the orbit
*       F12 / ESC        : Screenshot / quit
*
*   Command line:
*       ./opengl46_indirect_elements --auto     Runs 60 frames, self checks and screenshots, then quits
*
*   rlgl batches draw operations internally, so the current batch must be flushed
*   (rlDrawRenderBatchActive()) before issuing plain OpenGL work, and the GL state the
*   raylib batch relies on has to be restored afterwards.
*
********************************************************************************************/

// NOTE: The example only runs if raylib was compiled with OpenGL 4.6 support,
// check the raylib build instructions in the header of this file
#include "raylib.h"
#include "raymath.h"        // Required for: Clamp(), vec2/vec3/mat4 helpers
#include "rlgl.h"           // Required for: rlDrawRenderBatchActive(), rlGetVersion()
#include "external/glad.h"  // Required for: OpenGL 4.6 functionality

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>            // Required for: sqrtf(), fmaxf(), cosf(), sinf()
#include <cstdarg>          // Required for: va_list, va_start(), vsnprintf()
#include <cstdio>           // Required for: printf()
#include <cstring>          // Required for: memcpy()
#include <random>           // Required for: std::mt19937, std::uniform_real_distribution
#include <vector>           // Required for: std::vector (packed vertex/index data)

//------------------------------------------------------------------------------------
// Defines and types
//------------------------------------------------------------------------------------
#define MAX_INSTANCES       40000       // Total number of instances in the field
#define LOD_COUNT           3           // Three sphere meshes, most detailed first
#define CULL_GROUP_SIZE     64          // Must match local_size_x in the compute shader
#define MAX_CULL_TRIANGLES  120000000   // Safety limit for the HUD/self check (visible*count/3)

// Per-instance data uploaded once, it must match the Instance struct in the compute shader (std430)
typedef struct Instance {
    f32 model[16];          // Model matrix, column-major (64 bytes)
    f32 color[4];           // Base color (16 bytes)
    f32 sphere[4];          // xyz: world space center, w: world space radius (16 bytes)
} Instance;                 // 96 bytes

// Compacted instance written by the culling compute shader, read by the instanced vertex attributes
typedef struct VisibleInstance {
    f32 model[16];          // Model matrix, column-major (64 bytes)
    f32 color[4];           // Base color (16 bytes)
} VisibleInstance;          // 80 bytes

// Indirect draw command, it must match GL's DrawElementsIndirectCommand layout (and the shader)
typedef struct DrawElementsIndirectCommand {
    u32 count;              // Index count of the LOD mesh
    u32 instanceCount;      // Written by the compute shader every frame
    u32 firstIndex;         // First index of the LOD mesh inside the shared index buffer
    i32 baseVertex;         // First vertex of the LOD mesh inside the shared vertex buffer
    u32 baseInstance;       // First instance of the LOD group inside the visible instance buffer
} DrawElementsIndirectCommand;

// One packed level of detail (sphere mesh inside the shared vertex/index buffers)
typedef struct LodMesh {
    u32 indexCount;         // Number of indices
    u32 firstIndex;         // Offset into the shared index buffer
    i32 baseVertex;         // Offset into the shared vertex buffer
    i32 vertexCount;        // Number of vertices
} LodMesh;

//------------------------------------------------------------------------------------
// Shader sources (GLSL 460)
//------------------------------------------------------------------------------------
// Compute shader, three modes inside one program:
//   u_mode 0: reset the per group counters
//   u_mode 1: cull, pick the LOD and compact the instance into the visible buffer
//   u_mode 2: copy the counters into the indirect commands
static const char *cullShaderCode = R"(
#version 460 core

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

struct Instance {
    mat4 model;
    vec4 color;
    vec4 sphere;            // xyz: world center, w: world radius
};

struct VisibleInstance {
    mat4 model;
    vec4 color;
};

struct DrawElementsIndirectCommand {
    uint count;
    uint instanceCount;
    uint firstIndex;
    int  baseVertex;
    uint baseInstance;
};

layout(std430, binding = 0) readonly buffer InstanceBuffer { Instance instances[]; };
layout(std430, binding = 1) writeonly buffer VisibleBuffer { VisibleInstance visibleInstances[]; };
layout(std430, binding = 2) buffer CommandBuffer { DrawElementsIndirectCommand commands[]; };
layout(std430, binding = 3) buffer CounterBuffer { uint counters[4]; };  // [0..2] per LOD, [3] total visible

uniform mat4 u_viewProj;
uniform vec4 u_frustumPlanes[6];        // xyz: plane normal, w: plane distance
uniform vec3 u_cameraPosition;
uniform vec2 u_lodDistances;            // group 0 below x, group 1 below y, group 2 above
uniform uint u_instanceCount;
uniform uint u_slotsPerGroup;           // Capacity of one LOD group inside the visible buffer
uniform int u_mode;                     // 0: reset, 1: cull, 2: finalize
uniform bool u_cullingEnabled;
uniform bool u_lodEnabled;

// Sphere against the six clip space planes of the view frustum
bool SphereInFrustum(vec4 sphere)
{
    for (int i = 0; i < 6; i++)
    {
        if (dot(u_frustumPlanes[i].xyz, sphere.xyz) + u_frustumPlanes[i].w < -sphere.w) return false;
    }

    return true;
}

void main()
{
    uint id = gl_GlobalInvocationID.x;

    if (u_mode == 0)
    {
        if (id < 4) counters[id] = 0;
        return;
    }

    if (u_mode == 2)
    {
        if (id == 0)
        {
            for (int group = 0; group < 3; group++) commands[group].instanceCount = counters[group];
            counters[3] = counters[0] + counters[1] + counters[2];
        }
        return;
    }

    // u_mode 1: cull, choose the LOD, compact
    if (id >= u_instanceCount) return;

    Instance instance = instances[id];

    if (u_cullingEnabled && !SphereInFrustum(instance.sphere)) return;

    uint group = 0;

    if (u_lodEnabled)
    {
        float distance = length(instance.sphere.xyz - u_cameraPosition);
        group = (distance < u_lodDistances.x)? 0 : ((distance < u_lodDistances.y)? 1 : 2);
    }

    uint slot = atomicAdd(counters[group], 1);

    // Safety net, can only happen if a group holds more instances than it has slots
    if (slot >= u_slotsPerGroup) return;

    visibleInstances[group*u_slotsPerGroup + slot].model = instance.model;
    visibleInstances[group*u_slotsPerGroup + slot].color = instance.color;
}
)";

// Vertex shader, the per-instance model matrix and color come from instanced attributes
// that start at the current command's baseInstance
static const char *instanceVertexShaderCode = R"(
#version 460 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;

layout(location = 2) in vec4 a_model0;
layout(location = 3) in vec4 a_model1;
layout(location = 4) in vec4 a_model2;
layout(location = 5) in vec4 a_model3;
layout(location = 6) in vec4 a_color;

uniform mat4 u_viewProj;
uniform vec3 u_lightDirection;
uniform vec4 u_lodTint[3];      // Per draw tint, indexed with gl_DrawID

out vec3 v_worldPosition;
out vec3 v_normal;
out vec3 v_color;

void main()
{
    mat4 model = mat4(a_model0, a_model1, a_model2, a_model3);
    vec4 worldPosition = model*vec4(a_position, 1.0);

    v_worldPosition = worldPosition.xyz;
    v_normal = mat3(model)*a_normal;            // Uniform scale per instance
    v_color = a_color.rgb*u_lodTint[gl_DrawID].rgb;

    gl_Position = u_viewProj*worldPosition;
}
)";

// Fragment shader, Lambert + rim light + distance fog
static const char *instanceFragmentShaderCode = R"(
#version 460 core

in vec3 v_worldPosition;
in vec3 v_normal;
in vec3 v_color;

out vec4 finalColor;

uniform vec3 u_lightDirection;
uniform vec3 u_cameraPosition;

void main()
{
    vec3 normal = normalize(v_normal);
    vec3 viewDirection = normalize(u_cameraPosition - v_worldPosition);

    float diffuse = max(dot(normal, -u_lightDirection), 0.0);
    float rim = pow(1.0 - max(dot(normal, viewDirection), 0.0), 3.0);
    float fog = clamp((length(u_cameraPosition - v_worldPosition) - 70.0)/160.0, 0.0, 1.0);

    vec3 color = v_color*(0.12 + 0.88*diffuse) + rim*0.30*v_color;
    color = mix(color, vec3(0.02, 0.03, 0.06), fog);

    finalColor = vec4(color, 1.0);
}
)";

//------------------------------------------------------------------------------------
// Module Functions Declaration
//------------------------------------------------------------------------------------
static GLuint CompileShader(GLenum type, const char *code);
static GLuint LinkProgram(GLuint vertexShader, GLuint fragmentShader);
static void ExtractFrustumPlanes(const glm::mat4 &viewProj, glm::vec4 planes[6]);
static void InitInstances(Instance *instances);
static void PrintCheck(const bool ok, const char *format, ...);

// Packed geometry of all LOD meshes
typedef struct GeometryData {
    std::vector<f32> vertices;      // Interleaved position(3) + normal(3) per vertex
    std::vector<u32> indices;       // u32 indices, each LOD mesh keeps its local index range
    LodMesh lods[LOD_COUNT];        // Index/vertex ranges of the three LOD meshes
} GeometryData;

static void BuildGeometry(GeometryData &geometry);

// Orbit camera of the example
typedef struct OrbitCamera {
    f32 yaw = 0.65f;                // Horizontal angle in radians
    f32 pitch = 0.35f;              // Vertical angle in radians
    f32 distance = 58.0f;           // Distance to the pivot
    vec3 pivot = { 0.0f, 0.0f, 0.0f };
    bool orbit = true;              // Auto rotation on/off
} OrbitCamera;


//------------------------------------------------------------------------------------
// Module Functions Definition
//------------------------------------------------------------------------------------
// Compile a shader object, returns 0 and logs the info log on failure
static GLuint CompileShader(const GLenum type, const char *code)
{
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &code, nullptr);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if (compiled == GL_FALSE)
    {
        char log[2048] = { 0 };
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);

        printf("FAIL  shader compilation (%s)\n%s\n", (type == GL_COMPUTE_SHADER)? "compute" : ((type == GL_VERTEX_SHADER)? "vertex" : "fragment"), log);
        glDeleteShader(shader);

        return 0;
    }

    return shader;
}

// Link a vertex/fragment program
static GLuint LinkProgram(const GLuint vertexShader, const GLuint fragmentShader)
{
    const GLuint program = glCreateProgram();

    glAttachShader(program, vertexShader);
    if (fragmentShader != 0) glAttachShader(program, fragmentShader);   // Compute programs have none
    glLinkProgram(program);

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);

    if (linked == GL_FALSE)
    {
        char log[2048] = { 0 };
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);

        printf("FAIL  program link\n%s\n", log);
        glDeleteProgram(program);

        return 0;
    }

    return program;
}

// Build the three sphere LODs and pack them into one interleaved vertex buffer and one index buffer
static void BuildGeometry(GeometryData &geometry)
{
    // (rings, slices) per LOD, high poly first, the meshes only differ in their index range
    const i32 lodRings[LOD_COUNT] = { 40, 16, 6 };
    const i32 lodSlices[LOD_COUNT] = { 60, 24, 9 };

    for (i32 lod = 0; lod < LOD_COUNT; lod++)
    {
        const Mesh mesh = GenMeshSphere(1.0f, lodRings[lod], lodSlices[lod]);

        // NOTE: GenMeshSphere() returns a non indexed mesh (indices == NULL), the index buffer is
        // built from the triangle list below, so all LODs share the same indexed draw path
        const auto indexCount = (mesh.indices != nullptr)? static_cast<u32>(mesh.triangleCount*3) : static_cast<u32>(mesh.vertexCount);

        LodMesh &lodMesh = geometry.lods[lod];
        lodMesh.vertexCount = mesh.vertexCount;
        lodMesh.baseVertex = static_cast<i32>(geometry.vertices.size()/6);
        lodMesh.firstIndex = static_cast<u32>(geometry.indices.size());
        lodMesh.indexCount = indexCount;

        // Interleave position and normal, raymath stores them in two separate arrays
        for (i32 i = 0; i < mesh.vertexCount; i++)
        {
            geometry.vertices.push_back(mesh.vertices[i*3 + 0]);
            geometry.vertices.push_back(mesh.vertices[i*3 + 1]);
            geometry.vertices.push_back(mesh.vertices[i*3 + 2]);
            geometry.vertices.push_back(mesh.normals[i*3 + 0]);
            geometry.vertices.push_back(mesh.normals[i*3 + 1]);
            geometry.vertices.push_back(mesh.normals[i*3 + 2]);
        }

        // raylib meshes are u16 indexed, the indirect draw uses GL_UNSIGNED_INT,
        // non indexed meshes get the trivial 0..n sequence
        for (u32 i = 0; i < lodMesh.indexCount; i++)
        {
            geometry.indices.push_back((mesh.indices != nullptr)? static_cast<u32>(mesh.indices[i]) : i);
        }

        UnloadMesh(mesh);
    }
}

// Extract the six view frustum planes from a view-projection matrix (Gribb/Hartmann)
static void ExtractFrustumPlanes(const glm::mat4 &viewProj, glm::vec4 planes[6])
{
    // glm stores the matrix column-major, the plane equations are built from its rows
    const glm::vec4 row0(viewProj[0][0], viewProj[1][0], viewProj[2][0], viewProj[3][0]);
    const glm::vec4 row1(viewProj[0][1], viewProj[1][1], viewProj[2][1], viewProj[3][1]);
    const glm::vec4 row2(viewProj[0][2], viewProj[1][2], viewProj[2][2], viewProj[3][2]);
    const glm::vec4 row3(viewProj[0][3], viewProj[1][3], viewProj[2][3], viewProj[3][3]);

    planes[0] = row3 + row0;        // Left
    planes[1] = row3 - row0;        // Right
    planes[2] = row3 + row1;        // Bottom
    planes[3] = row3 - row1;        // Top
    planes[4] = row3 + row2;        // Near
    planes[5] = row3 - row2;        // Far

    for (i32 i = 0; i < 6; i++)
    {
        const f32 length = sqrtf(planes[i].x*planes[i].x + planes[i].y*planes[i].y + planes[i].z*planes[i].z);
        if (length > 0.0f) planes[i] /= length;
    }
}

// Fill the instance array with a randomly distributed field of spheres
static void InitInstances(Instance *instances)
{
    // Deterministic layout, so the screenshots and the self check are comparable
    std::mt19937 random(2026);
    std::uniform_real_distribution<f32> uniformAngle(0.0f, 2.0f*PI);
    std::uniform_real_distribution<f32> uniformHeight(-14.0f, 14.0f);
    std::uniform_real_distribution<f32> uniformRadius(24.0f, 118.0f);
    std::uniform_real_distribution<f32> uniformScale(0.35f, 2.4f);

    const Color palette[6] = {
        Color{ 126, 158, 214, 255 }, Color{ 196, 160, 118, 255 }, Color{ 132, 186, 148, 255 },
        Color{ 200, 128, 132, 255 }, Color{ 152, 136, 196, 255 }, Color{ 190, 190, 176, 255 }
    };

    for (i32 i = 0; i < MAX_INSTANCES; i++)
    {
        // Spread the field in a ring around the pivot so the frustum culling has something to do
        const f32 angle = uniformAngle(random);
        const f32 radius = uniformRadius(random);
        const f32 scale = uniformScale(random);

        const vec3 position = { cosf(angle)*radius, uniformHeight(random), sinf(angle)*radius };
        const vec3 axis = normalize(vec3{ uniformAngle(random), uniformAngle(random), uniformAngle(random) });

        const mat4 model = glm::translate(mat4(1.0f), position)*glm::rotate(mat4(1.0f), angle*3.0f, axis)*glm::scale(mat4(1.0f), vec3{ scale, scale, scale });
        const Color color = palette[i%6];

        memcpy(instances[i].model, &model[0][0], sizeof(instances[i].model));

        instances[i].color[0] = static_cast<f32>(color.r)/255.0f;
        instances[i].color[1] = static_cast<f32>(color.g)/255.0f;
        instances[i].color[2] = static_cast<f32>(color.b)/255.0f;
        instances[i].color[3] = 1.0f;

        instances[i].sphere[0] = position.x;
        instances[i].sphere[1] = position.y;
        instances[i].sphere[2] = position.z;
        instances[i].sphere[3] = scale;         // Unit sphere scaled by 'scale'
    }
}

// Print a self check line and keep the failure count
static i32 checkFailures = 0;
static void PrintCheck(const bool ok, const char *format, ...)
{
    char buffer[512] = { 0 };

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (!ok) checkFailures++;

    printf("%s  %s\n", ok? "PASS" : "FAIL", buffer);
}

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main(const i32 argc, char **argv)
{
    const bool autoMode = (argc > 1) && (strcmp(argv[1], "--auto") == 0);

    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr auto screenWidth = 1280;
    constexpr auto screenHeight = 720;

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "raylib [others] example - glMultiDrawElementsIndirect (GPU-driven instancing)");

    // This example relies on OpenGL 4.6 features (compute shaders, DSA, multi draw elements indirect)
    if (rlGetVersion() != RL_OPENGL_46)
    {
        TraceLog(LOG_ERROR, "EXAMPLE: raylib was not compiled with OpenGL 4.6 support");
        TraceLog(LOG_ERROR, "EXAMPLE: Rebuild raylib defining GRAPHICS_API_OPENGL_46");
        TraceLog(LOG_ERROR, "EXAMPLE: For CMake: cmake -B build -DOPENGL_VERSION=4.6");

        CloseWindow();
        return 1;
    }

    printf("INFO  GL version:  %s\n", reinterpret_cast<const char *>(glGetString(GL_VERSION)));
    printf("INFO  GL renderer: %s\n", reinterpret_cast<const char *>(glGetString(GL_RENDERER)));

    //----------------------------------------------------------------------------------
    // Geometry: three sphere LODs inside one vertex buffer and one index buffer
    //----------------------------------------------------------------------------------
    GeometryData geometry;
    BuildGeometry(geometry);

    printf("INFO  LOD meshes:\n");
    for (i32 lod = 0; lod < LOD_COUNT; lod++)
    {
        printf("INFO    lod %i: %i vertices, %i triangles, firstIndex %u, baseVertex %i\n",
               lod, geometry.lods[lod].vertexCount, geometry.lods[lod].indexCount/3,
               geometry.lods[lod].firstIndex, geometry.lods[lod].baseVertex);
    }

    //----------------------------------------------------------------------------------
    // Shaders and programs
    //----------------------------------------------------------------------------------
    const GLuint cullShader = CompileShader(GL_COMPUTE_SHADER, cullShaderCode);
    const GLuint instanceVertexShader = CompileShader(GL_VERTEX_SHADER, instanceVertexShaderCode);
    const GLuint instanceFragmentShader = CompileShader(GL_FRAGMENT_SHADER, instanceFragmentShaderCode);

    if ((cullShader == 0) || (instanceVertexShader == 0) || (instanceFragmentShader == 0))
    {
        TraceLog(LOG_ERROR, "EXAMPLE: Shader compilation failed");

        CloseWindow();
        return 1;
    }

    const GLuint cullProgram = LinkProgram(cullShader, 0);
    const GLuint instanceProgram = LinkProgram(instanceVertexShader, instanceFragmentShader);

    glDeleteShader(cullShader);
    glDeleteShader(instanceVertexShader);
    glDeleteShader(instanceFragmentShader);

    if ((cullProgram == 0) || (instanceProgram == 0))
    {
        TraceLog(LOG_ERROR, "EXAMPLE: Shader program link failed");

        CloseWindow();
        return 1;
    }

    // Compute program uniforms
    const GLint cullViewProjLoc = glGetUniformLocation(cullProgram, "u_viewProj");
    const GLint cullFrustumLoc = glGetUniformLocation(cullProgram, "u_frustumPlanes[0]");
    const GLint cullCameraPositionLoc = glGetUniformLocation(cullProgram, "u_cameraPosition");
    const GLint cullLodDistancesLoc = glGetUniformLocation(cullProgram, "u_lodDistances");
    const GLint cullInstanceCountLoc = glGetUniformLocation(cullProgram, "u_instanceCount");
    const GLint cullSlotsLoc = glGetUniformLocation(cullProgram, "u_slotsPerGroup");
    const GLint cullModeLoc = glGetUniformLocation(cullProgram, "u_mode");
    const GLint cullEnabledLoc = glGetUniformLocation(cullProgram, "u_cullingEnabled");
    const GLint cullLodEnabledLoc = glGetUniformLocation(cullProgram, "u_lodEnabled");

    // Render program uniforms
    const GLint instanceViewProjLoc = glGetUniformLocation(instanceProgram, "u_viewProj");
    const GLint instanceLightDirLoc = glGetUniformLocation(instanceProgram, "u_lightDirection");
    const GLint instanceCameraPositionLoc = glGetUniformLocation(instanceProgram, "u_cameraPosition");
    const GLint instanceLodTintLoc = glGetUniformLocation(instanceProgram, "u_lodTint[0]");

    //----------------------------------------------------------------------------------
    // Buffers (Direct State Access)
    //----------------------------------------------------------------------------------
    // Static vertex/index data of all LOD meshes
    GLuint vertexBuffer = 0;
    glCreateBuffers(1, &vertexBuffer);
    glNamedBufferData(vertexBuffer, static_cast<GLsizeiptr>(geometry.vertices.size()*sizeof(f32)), geometry.vertices.data(), GL_STATIC_DRAW);

    GLuint indexBuffer = 0;
    glCreateBuffers(1, &indexBuffer);
    glNamedBufferData(indexBuffer, static_cast<GLsizeiptr>(geometry.indices.size()*sizeof(u32)), geometry.indices.data(), GL_STATIC_DRAW);

    // All instances, uploaded once
    static Instance instances[MAX_INSTANCES] = { };
    InitInstances(instances);

    GLuint instanceBuffer = 0;
    glCreateBuffers(1, &instanceBuffer);
    glNamedBufferData(instanceBuffer, sizeof(instances), instances, GL_STATIC_DRAW);

    // Compacted visible instances, the compute shader writes it, the vertex attributes read it
    GLuint visibleBuffer = 0;
    glCreateBuffers(1, &visibleBuffer);
    glNamedBufferData(visibleBuffer, static_cast<GLsizeiptr>(LOD_COUNT*MAX_INSTANCES*sizeof(VisibleInstance)), nullptr, GL_DYNAMIC_DRAW);

    // Indirect commands, count/firstIndex/baseVertex are static, instanceCount is written on the GPU
    DrawElementsIndirectCommand commands[LOD_COUNT] = { };
    for (i32 lod = 0; lod < LOD_COUNT; lod++)
    {
        commands[lod].count = geometry.lods[lod].indexCount;
        commands[lod].instanceCount = 0;
        commands[lod].firstIndex = geometry.lods[lod].firstIndex;
        commands[lod].baseVertex = geometry.lods[lod].baseVertex;
        commands[lod].baseInstance = static_cast<u32>(lod)*MAX_INSTANCES;
    }

    GLuint commandBuffer = 0;
    glCreateBuffers(1, &commandBuffer);
    glNamedBufferData(commandBuffer, sizeof(commands), commands, GL_DYNAMIC_DRAW);

    // Per group counters [0..2] plus the total visible count [3], cleared by the compute shader
    GLuint counterBuffer = 0;
    glCreateBuffers(1, &counterBuffer);
    glNamedBufferData(counterBuffer, 4*sizeof(u32), nullptr, GL_DYNAMIC_DRAW);

    // Shader storage buffer bindings, they stay the same for the whole program
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, instanceBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, visibleBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, commandBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, counterBuffer);

    //----------------------------------------------------------------------------------
    // Vertex array: mesh attributes from the static buffers, instance attributes from the visible buffer
    //----------------------------------------------------------------------------------
    GLuint vertexArray = 0;
    glCreateVertexArrays(1, &vertexArray);

    // Binding 0: per vertex data (position + normal, interleaved, 24 bytes stride)
    glVertexArrayVertexBuffer(vertexArray, 0, vertexBuffer, 0, 6*sizeof(f32));
    glEnableVertexArrayAttrib(vertexArray, 0);
    glVertexArrayAttribFormat(vertexArray, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(vertexArray, 0, 0);
    glEnableVertexArrayAttrib(vertexArray, 1);
    glVertexArrayAttribFormat(vertexArray, 1, 3, GL_FLOAT, GL_FALSE, 3*sizeof(f32));
    glVertexArrayAttribBinding(vertexArray, 1, 0);

    glVertexArrayElementBuffer(vertexArray, indexBuffer);

    // Binding 1: per instance data (mat4 + color, 80 bytes stride, divisor 1)
    glVertexArrayVertexBuffer(vertexArray, 1, visibleBuffer, 0, sizeof(VisibleInstance));

    for (i32 column = 0; column < 4; column++)
    {
        glEnableVertexArrayAttrib(vertexArray, 2 + column);
        glVertexArrayAttribFormat(vertexArray, 2 + column, 4, GL_FLOAT, GL_FALSE, static_cast<GLuint>(column*4*sizeof(f32)));
        glVertexArrayAttribBinding(vertexArray, 2 + column, 1);
    }

    glEnableVertexArrayAttrib(vertexArray, 6);
    glVertexArrayAttribFormat(vertexArray, 6, 4, GL_FLOAT, GL_FALSE, 16*sizeof(f32));
    glVertexArrayAttribBinding(vertexArray, 6, 1);

    // The instance attributes advance once per instance, not per vertex
    glVertexArrayBindingDivisor(vertexArray, 1, 1);

    //----------------------------------------------------------------------------------
    // Camera, light and render state
    //----------------------------------------------------------------------------------
    OrbitCamera orbit;
    Camera3D camera = { };
    camera.up = vec3{ 0.0f, 1.0f, 0.0f };
    camera.fovy = 55.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    const vec3 lightDirection = normalize(vec3{ -0.45f, -0.75f, -0.4f });
    const vec4 lodTint[LOD_COUNT] = {
        vec4{ 1.00f, 1.00f, 1.00f, 1.0f },      // High poly: unfiltered
        vec4{ 0.92f, 0.96f, 1.05f, 1.0f },      // Mid poly: slightly cool
        vec4{ 1.08f, 0.94f, 0.90f, 1.0f }       // Low poly: slightly warm
    };

    const vec2 lodDistances = { 46.0f, 96.0f };     // Group 0 below 46, group 1 below 96, group 2 above

    auto cullingEnabled = true;
    auto lodEnabled = true;
    auto wireframe = false;

    u32 counters[4] = { 0 };                        // [0..2] per group, [3] total visible
    auto frame = 0;

    //----------------------------------------------------------------------------------
    // Main loop
    //----------------------------------------------------------------------------------
    while (!WindowShouldClose())
    {
        // Input, the camera orbits on its own, the mouse takes over while dragging
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
        {
            const vec2 delta = GetMouseDelta();
            orbit.yaw += delta.x*0.005f;
            orbit.pitch = Clamp(orbit.pitch + delta.y*0.005f, -1.45f, 1.45f);
            orbit.orbit = false;
        }

        const f32 wheel = GetMouseWheelMove();
        if (wheel != 0.0f) orbit.distance = Clamp(orbit.distance - wheel*4.0f, 12.0f, 240.0f);

        if (IsKeyPressed(KEY_SPACE)) orbit.orbit = !orbit.orbit;
        if (IsKeyPressed(KEY_C)) cullingEnabled = !cullingEnabled;
        if (IsKeyPressed(KEY_L)) lodEnabled = !lodEnabled;
        if (IsKeyPressed(KEY_F)) wireframe = !wireframe;

        if (orbit.orbit) orbit.yaw += GetFrameTime()*0.12f;

        // Camera position from the orbit angles
        const vec3 eye = {
            orbit.pivot.x + orbit.distance*cosf(orbit.pitch)*cosf(orbit.yaw),
            orbit.pivot.y + orbit.distance*sinf(orbit.pitch),
            orbit.pivot.z + orbit.distance*cosf(orbit.pitch)*sinf(orbit.yaw)
        };

        camera.position = eye;
        camera.target = orbit.pivot;

        // glm matrices, used for the compute culling and for the custom draw
        const glm::vec3 cameraEye(eye.x, eye.y, eye.z);
        const glm::mat4 view = glm::lookAt(cameraEye, glm::vec3(orbit.pivot.x, orbit.pivot.y, orbit.pivot.z), glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::mat4 projection = glm::perspective(DEG2RAD*camera.fovy, static_cast<f32>(screenWidth)/static_cast<f32>(screenHeight), 0.05f, 400.0f);
        const glm::mat4 viewProj = projection*view;

        glm::vec4 frustumPlanes[6];
        ExtractFrustumPlanes(viewProj, frustumPlanes);

        // Render
        //----------------------------------------------------------------------------------
        BeginDrawing();
            ClearBackground(Color{ 6, 7, 12, 255 });

            BeginMode3D(camera);
                DrawGrid(60, 4.0f);

                // Flush the raylib batch, the following draw is plain OpenGL
                rlDrawRenderBatchActive();

                // Compute pass: reset, cull and finalize
                glUseProgram(cullProgram);
                glUniformMatrix4fv(cullViewProjLoc, 1, GL_FALSE, &viewProj[0][0]);
                glUniform4fv(cullFrustumLoc, 6, &frustumPlanes[0][0]);
                glUniform3f(cullCameraPositionLoc, cameraEye.x, cameraEye.y, cameraEye.z);
                glUniform2f(cullLodDistancesLoc, lodDistances.x, lodDistances.y);
                glUniform1ui(cullInstanceCountLoc, MAX_INSTANCES);
                glUniform1ui(cullSlotsLoc, MAX_INSTANCES);
                glUniform1i(cullEnabledLoc, cullingEnabled? 1 : 0);
                glUniform1i(cullLodEnabledLoc, lodEnabled? 1 : 0);

                glUniform1i(cullModeLoc, 0);
                glDispatchCompute(1, 1, 1);

                // Each dispatch has to see the writes of the previous one, dispatches may overlap otherwise
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

                glUniform1i(cullModeLoc, 1);
                glDispatchCompute((MAX_INSTANCES + CULL_GROUP_SIZE - 1)/CULL_GROUP_SIZE, 1, 1);

                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

                glUniform1i(cullModeLoc, 2);
                glDispatchCompute(1, 1, 1);

                // Make the visible instances available to the vertex attribute fetch and the commands to the draw
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
                glUseProgram(0);

                // Draw pass: one indexed indirect multi draw for all LOD groups
                if (wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

                glUseProgram(instanceProgram);
                glUniformMatrix4fv(instanceViewProjLoc, 1, GL_FALSE, &viewProj[0][0]);
                glUniform3f(instanceLightDirLoc, lightDirection.x, lightDirection.y, lightDirection.z);
                glUniform3f(instanceCameraPositionLoc, cameraEye.x, cameraEye.y, cameraEye.z);
                glUniform4fv(instanceLodTintLoc, LOD_COUNT, &lodTint[0][0]);

                glBindVertexArray(vertexArray);
                glBindBuffer(GL_DRAW_INDIRECT_BUFFER, commandBuffer);
                glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr, LOD_COUNT, 0);
                glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
                glBindVertexArray(0);
                glUseProgram(0);

                if (wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            EndMode3D();

            // Read the counters back, a few frames apart, they are written by the GPU
            if ((frame%8) == 0) glGetNamedBufferSubData(counterBuffer, 0, sizeof(counters), counters);

            const u32 visible = counters[3];
            const f32 visibleRatio = 100.0f*static_cast<f32>(visible)/static_cast<f32>(MAX_INSTANCES);

            // HUD
            DrawFPS(20, 20);
            DrawText(TextFormat("instances: %i   visible: %u (%.1f%%)", MAX_INSTANCES, visible, visibleRatio), 20, 50, 20, RAYWHITE);
            DrawText(TextFormat("LOD groups: high %u   mid %u   low %u", counters[0], counters[1], counters[2]), 20, 76, 20, LIGHTGRAY);
            DrawText(TextFormat("culled: %u   draws: 1 x glMultiDrawElementsIndirect (%i indirect commands)", MAX_INSTANCES - visible, LOD_COUNT), 20, 102, 20, LIGHTGRAY);
            DrawText(TextFormat("culling [C]: %s   LOD [L]: %s   wireframe [F]: %s   orbit [SPACE]: %s",
                                cullingEnabled? "on" : "off", lodEnabled? "on" : "off", wireframe? "on" : "off", orbit.orbit? "on" : "off"),
                     20, screenHeight - 60, 20, DARKGRAY);
            DrawText("MOUSE: orbit, wheel: zoom, ESC: quit", 20, screenHeight - 32, 20, DARKGRAY);

            // Self check output, only in the auto mode
            if (autoMode && ((frame%15) == 0))
            {
                printf("INFO  frame %3i: visible %5u (%u/%u/%u), culling %s, lod %s\n",
                       frame, visible, counters[0], counters[1], counters[2], cullingEnabled? "on" : "off", lodEnabled? "on" : "off");
            }
        EndDrawing();

        // Auto mode: verify the GPU results, take a screenshot and quit
        if (autoMode)
        {
            if (frame == 20)
            {
                PrintCheck(visible > 0, "culling yields visible instances (%u of %i)", visible, MAX_INSTANCES);
                PrintCheck(visible < MAX_INSTANCES, "frustum culling removed instances (%u visible)", visible);
                PrintCheck((counters[0] + counters[1] + counters[2]) == visible, "group counters sum to the visible total (%u + %u + %u vs %u)", counters[0], counters[1], counters[2], visible);

                TakeScreenshot("opengl46_indirect_elements.png");
                printf("INFO  screenshot saved: opengl46_indirect_elements.png\n");
            }

            // Second phase: culling off, every instance has to survive
            if (frame == 34) cullingEnabled = false;
            if (frame == 42)
            {
                PrintCheck(visible == MAX_INSTANCES, "culling off draws all %i instances (%u visible)", MAX_INSTANCES, visible);
                PrintCheck((counters[0] + counters[1] + counters[2]) == MAX_INSTANCES, "all groups together hold every instance");
                PrintCheck((lodEnabled)? ((counters[0] > 0) && (counters[2] > 0)) : true, "LOD selection spreads instances over the groups");
            }

            if (frame >= 60) break;
        }

        frame++;
    }

    // Self check of the GL error state
    PrintCheck(glGetError() == GL_NO_ERROR, "no OpenGL error during the frame loop");

    // Deinitialization
    //--------------------------------------------------------------------------------------
    glDeleteVertexArrays(1, &vertexArray);
    glDeleteBuffers(1, &vertexBuffer);
    glDeleteBuffers(1, &indexBuffer);
    glDeleteBuffers(1, &instanceBuffer);
    glDeleteBuffers(1, &visibleBuffer);
    glDeleteBuffers(1, &commandBuffer);
    glDeleteBuffers(1, &counterBuffer);
    glDeleteProgram(cullProgram);
    glDeleteProgram(instanceProgram);

    CloseWindow();

    printf("\n%i failure(s)\n", checkFailures);

    return (checkFailures == 0)? 0 : 1;
}


