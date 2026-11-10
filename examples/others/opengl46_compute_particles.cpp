/*******************************************************************************************
*
*   raylib [others] example - OpenGL 4.6 compute particles
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
*   It mixes raylib with plain OpenGL 4.6 core code to simulate and render particles
*   entirely on the GPU:
*
*       - A COMPUTE SHADER (GLSL 460) integrates the particle positions stored in a
*         Shader Storage Buffer Object (SSBO)
*       - The same SSBO is read directly by the vertex shader, indexed with gl_VertexID
*       - glMultiDrawArraysIndirectCount() (OpenGL 4.6 core) renders all particle
*         batches with a single draw call, the number of draws comes from a GPU buffer
*       - gl_DrawID (GLSL 460) selects a palette color per indirect draw
*       - Direct State Access (DSA, OpenGL 4.5+) creates and fills buffers/vertex arrays
*
*   rlgl batches draw operations internally, so the current batch must be flushed
*   (rlDrawRenderBatchActive()) before issuing plain OpenGL work.
*
********************************************************************************************/

// NOTE: The example only runs if raylib was compiled with OpenGL 4.6 support,
// check the raylib build instructions in the header of this file
#include "raylib.h"
#include "rlgl.h"           // Required for: rlDrawRenderBatchActive(), rlGetVersion()
#include "external/glad.h"  // Required for: OpenGL 4.6 functionality

#include <math.h>           // Required for: fminf(), fabsf()

//------------------------------------------------------------------------------------
// Defines and types
//------------------------------------------------------------------------------------
#define MAX_PARTICLES       20000       // Total number of particles (stored in one SSBO)
#define PARTICLE_BATCHES    4           // Number of indirect draws (one per palette color)
#define COMPUTE_GROUP_SIZE  64          // Must match local_size_x in the compute shader

// Particle data layout, it must match the vec4-based struct in the shaders (std430)
typedef struct Particle {
    float positionSize[4];  // xy: position (NDC), z: unused, w: point size in pixels
    float velocity[2];      // xy: velocity in normalized device coordinates per second
    float padding[2];       // Unused, keeps the struct aligned to 32 bytes
} Particle;

// Indirect draw command, it must match GL's DrawArraysIndirectCommand layout
typedef struct DrawArraysIndirectCommand {
    unsigned int count;         // Number of vertices (particles) to draw
    unsigned int instanceCount; // Number of instances
    unsigned int first;         // First particle index in the SSBO
    unsigned int baseInstance;  // Unused
} DrawArraysIndirectCommand;

//------------------------------------------------------------------------------------
// Module Functions Declaration
//------------------------------------------------------------------------------------
static GLuint CompileShader(GLenum type, const char *code);
static GLuint LinkProgram(GLuint vertexShader, GLuint fragmentShader);
static void InitParticles(Particle *particles);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 800;
    const int screenHeight = 450;

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "raylib [others] example - OpenGL 4.6 compute particles");

    // This example relies on OpenGL 4.6 features (compute shaders, DSA, multi-draw indirect count)
    if (rlGetVersion() != RL_OPENGL_46)
    {
        TraceLog(LOG_ERROR, "EXAMPLE: raylib was not compiled with OpenGL 4.6 support");
        TraceLog(LOG_ERROR, "EXAMPLE: Rebuild raylib defining GRAPHICS_API_OPENGL_46");
        TraceLog(LOG_ERROR, "EXAMPLE: For CMake: cmake -B build -DOPENGL_VERSION=4.6");

        CloseWindow();
        return 1;
    }

    TraceLog(LOG_INFO, "GL: OpenGL version: %s", reinterpret_cast<const char *>(glGetString(GL_VERSION)));
    TraceLog(LOG_INFO, "GL: GLSL version: %s", reinterpret_cast<const char *>(glGetString(GL_SHADING_LANGUAGE_VERSION)));
    TraceLog(LOG_INFO, "GL: Renderer: %s", reinterpret_cast<const char *>(glGetString(GL_RENDERER)));

    //----------------------------------------------------------------------------------
    // Shaders
    //----------------------------------------------------------------------------------
    // Compute shader: integrates every particle position inside the SSBO
    const char *computeShaderCode =
        "#version 460 core\n"
        "\n"
        "layout(local_size_x = 64) in;\n"
        "\n"
        "struct Particle {\n"
        "    vec4 positionSize;      // xy: position (NDC), w: point size\n"
        "    vec4 velocity;\n"
        "};\n"
        "\n"
        "layout(std430, binding = 0) buffer ParticleBuffer { Particle particles[]; };\n"
        "\n"
        "uniform uint particleCount;\n"
        "uniform float deltaTime;\n"
        "\n"
        "void main()\n"
        "{\n"
        "    uint id = gl_GlobalInvocationID.x;\n"
        "    if (id >= particleCount) return;\n"
        "\n"
        "    Particle particle = particles[id];\n"
        "    particle.positionSize.xy += particle.velocity.xy*deltaTime;\n"
        "\n"
        "    // Bounce particles inside the [-1, 1] normalized device coordinates box\n"
        "    if (particle.positionSize.x > 1.0) { particle.positionSize.x = 1.0; particle.velocity.x = -abs(particle.velocity.x); }\n"
        "    if (particle.positionSize.x < -1.0) { particle.positionSize.x = -1.0; particle.velocity.x = abs(particle.velocity.x); }\n"
        "    if (particle.positionSize.y > 1.0) { particle.positionSize.y = 1.0; particle.velocity.y = -abs(particle.velocity.y); }\n"
        "    if (particle.positionSize.y < -1.0) { particle.positionSize.y = -1.0; particle.velocity.y = abs(particle.velocity.y); }\n"
        "\n"
        "    particles[id] = particle;\n"
        "}\n";

    // Vertex shader: reads the SSBO directly, gl_VertexID is the particle index
    const char *vertexShaderCode =
        "#version 460 core\n"
        "\n"
        "struct Particle {\n"
        "    vec4 positionSize;      // xy: position (NDC), w: point size\n"
        "    vec4 velocity;\n"
        "};\n"
        "\n"
        "layout(std430, binding = 0) readonly buffer ParticleBuffer { Particle particles[]; };\n"
        "\n"
        "uniform vec3 palette[4];\n"
        "\n"
        "out vec4 fragColor;\n"
        "\n"
        "void main()\n"
        "{\n"
        "    Particle particle = particles[gl_VertexID];\n"
        "\n"
        "    gl_Position = vec4(particle.positionSize.xy, 0.0, 1.0);\n"
        "    gl_PointSize = max(particle.positionSize.w, 1.0);\n"
        "\n"
        "    // gl_DrawID (GLSL 460) identifies the draw inside glMultiDrawArraysIndirectCount()\n"
        "    fragColor = vec4(palette[gl_DrawID], 1.0);\n"
        "}\n";

    // Fragment shader: draws soft round points
    const char *fragmentShaderCode =
        "#version 460 core\n"
        "\n"
        "in vec4 fragColor;\n"
        "out vec4 finalColor;\n"
        "\n"
        "void main()\n"
        "{\n"
        "    vec2 point = gl_PointCoord*2.0 - 1.0;\n"
        "    float radius = dot(point, point);\n"
        "    if (radius > 1.0) discard;\n"
        "\n"
        "    finalColor = vec4(fragColor.rgb, smoothstep(1.0, 0.6, radius));\n"
        "}\n";

    GLuint computeShader = CompileShader(GL_COMPUTE_SHADER, computeShaderCode);
    GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vertexShaderCode);
    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentShaderCode);

    if ((computeShader == 0) || (vertexShader == 0) || (fragmentShader == 0))
    {
        TraceLog(LOG_ERROR, "EXAMPLE: Shader compilation failed, OpenGL 4.6 support required");

        CloseWindow();
        return 1;
    }

    // Link compute and graphics programs
    GLuint computeProgram = glCreateProgram();
    glAttachShader(computeProgram, computeShader);
    glLinkProgram(computeProgram);

    GLint computeLinked = 0;
    glGetProgramiv(computeProgram, GL_LINK_STATUS, &computeLinked);

    if (computeLinked == GL_FALSE)
    {
        char log[1024] = {};
        glGetProgramInfoLog(computeProgram, sizeof(log), nullptr, log);

        TraceLog(LOG_ERROR, "EXAMPLE: Compute program link failed");
        TraceLog(LOG_ERROR, "SHADER: %s", log);

        CloseWindow();
        return 1;
    }

    GLuint renderProgram = LinkProgram(vertexShader, fragmentShader);

    // Shaders are no longer required once the programs are linked
    glDeleteShader(computeShader);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    if (renderProgram == 0)
    {
        TraceLog(LOG_ERROR, "EXAMPLE: Shader program link failed");

        CloseWindow();
        return 1;
    }

    GLint particleCountLoc = glGetUniformLocation(computeProgram, "particleCount");
    GLint deltaTimeLoc = glGetUniformLocation(computeProgram, "deltaTime");
    GLint paletteLoc = glGetUniformLocation(renderProgram, "palette[0]");

    //----------------------------------------------------------------------------------
    // Particle data
    //----------------------------------------------------------------------------------
    // Keep the particle array out of the stack (20000 * 32 bytes), it can be
    // re-uploaded to the SSBO at any time (for example when resetting)
    static Particle particles[MAX_PARTICLES] = {};
    InitParticles(particles);

    // Create the SSBO that holds all particles (Direct State Access, OpenGL 4.5+)
    GLuint particleBuffer = 0;
    glCreateBuffers(1, &particleBuffer);
    glNamedBufferData(particleBuffer, sizeof(particles), particles, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleBuffer);

    // Indirect draw commands: PARTICLE_BATCHES draws, one per palette color
    // NOTE: All batches read from the same SSBO, each one starting at a different particle
    DrawArraysIndirectCommand drawCommands[PARTICLE_BATCHES] = {};
    unsigned int particlesPerBatch = MAX_PARTICLES/PARTICLE_BATCHES;

    for (int i = 0; i < PARTICLE_BATCHES; i++)
    {
        drawCommands[i].count = particlesPerBatch;
        drawCommands[i].instanceCount = 1;
        drawCommands[i].first = i*particlesPerBatch;
    }

    GLuint drawIndirectBuffer = 0;
    glCreateBuffers(1, &drawIndirectBuffer);
    glNamedBufferData(drawIndirectBuffer, sizeof(drawCommands), drawCommands, GL_STATIC_DRAW);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, drawIndirectBuffer);

    // Draw count is also stored in a GPU buffer (GL_PARAMETER_BUFFER)
    // NOTE: OpenGL 4.6 multi-draw count reads the number of draws from this buffer
    GLuint drawCountBuffer = 0;
    unsigned int drawCount = PARTICLE_BATCHES;
    glCreateBuffers(1, &drawCountBuffer);
    glNamedBufferData(drawCountBuffer, sizeof(drawCount), &drawCount, GL_STATIC_DRAW);
    glBindBuffer(GL_PARAMETER_BUFFER, drawCountBuffer);

    // Particles are drawn from the SSBO, no vertex attributes are required
    // NOTE: A Vertex Array Object is still mandatory in OpenGL core profile
    GLuint vertexArray = 0;
    glCreateVertexArrays(1, &vertexArray);

    // Allows the vertex shader to define the point size of each particle
    glEnable(GL_PROGRAM_POINT_SIZE);

    // Palette used by the vertex shader, one color per indirect draw
    float palette[PARTICLE_BATCHES][3] = {
        { 1.00f, 0.62f, 0.16f },    // Orange
        { 0.35f, 0.85f, 1.00f },    // Sky blue
        { 1.00f, 0.45f, 0.75f },    // Pink
        { 0.55f, 1.00f, 0.65f }     // Mint
    };

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        if (IsKeyPressed(KEY_SPACE))
        {
            // Re-initialize the particle data and upload it to the SSBO again
            InitParticles(particles);
            glNamedBufferSubData(particleBuffer, 0, sizeof(particles), particles);
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();
            ClearBackground(Color{ 10, 12, 24, 255 });

            DrawText("OpenGL 4.6 compute shader particles", 20, 20, 20, RAYWHITE);
            DrawText(TextFormat("%i particles, %i indirect draws", MAX_PARTICLES, PARTICLE_BATCHES), 20, 46, 10, GRAY);
            DrawText("[SPACE] Reset particles", 20, 64, 10, DARKGRAY);
            DrawFPS(screenWidth - 100, 20);

            // Flush raylib internal batch before issuing plain OpenGL calls
            rlDrawRenderBatchActive();

            // 1) Simulate all particles with a compute shader (GPU only)
            float deltaTime = fminf(GetFrameTime(), 1.0f/30.0f);

            glUseProgram(computeProgram);
                glUniform1ui(particleCountLoc, MAX_PARTICLES);
                glUniform1f(deltaTimeLoc, deltaTime);
                glDispatchCompute((MAX_PARTICLES + COMPUTE_GROUP_SIZE - 1)/COMPUTE_GROUP_SIZE, 1, 1);
            glUseProgram(0);

            // Make the SSBO writes visible to the vertex shader
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

            // 2) Draw all particle batches with a single OpenGL 4.6 multi-draw call
            glUseProgram(renderProgram);
                glUniform3fv(paletteLoc, PARTICLE_BATCHES, reinterpret_cast<float *>(palette));

                glBindVertexArray(vertexArray);
                    glMultiDrawArraysIndirectCount(GL_POINTS, nullptr, 0, PARTICLE_BATCHES, 0);
                glBindVertexArray(0);
            glUseProgram(0);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    glDeleteProgram(renderProgram);
    glDeleteProgram(computeProgram);

    glDeleteVertexArrays(1, &vertexArray);
    glDeleteBuffers(1, &drawCountBuffer);
    glDeleteBuffers(1, &drawIndirectBuffer);
    glDeleteBuffers(1, &particleBuffer);

    CloseWindow();          // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//------------------------------------------------------------------------------------
// Module Functions Definition
//------------------------------------------------------------------------------------
// Compile a shader and report any compilation error
static GLuint CompileShader(GLenum type, const char *code)
{
    GLuint shaderId = glCreateShader(type);

    glShaderSource(shaderId, 1, &code, nullptr);
    glCompileShader(shaderId);

    GLint success = 0;
    glGetShaderiv(shaderId, GL_COMPILE_STATUS, &success);

    if (success == GL_FALSE)
    {
        char log[1024] = {};
        glGetShaderInfoLog(shaderId, sizeof(log), nullptr, log);

        TraceLog(LOG_ERROR, "SHADER: Failed to compile shader (type: 0x%04X)", type);
        TraceLog(LOG_ERROR, "SHADER: %s", log);

        glDeleteShader(shaderId);
        return 0;
    }

    return shaderId;
}

// Link a vertex and a fragment shader into a program
static GLuint LinkProgram(GLuint vertexShader, GLuint fragmentShader)
{
    GLuint programId = glCreateProgram();

    glAttachShader(programId, vertexShader);
    glAttachShader(programId, fragmentShader);
    glLinkProgram(programId);

    GLint success = 0;
    glGetProgramiv(programId, GL_LINK_STATUS, &success);

    if (success == GL_FALSE)
    {
        char log[1024] = {};
        glGetProgramInfoLog(programId, sizeof(log), nullptr, log);

        TraceLog(LOG_ERROR, "SHADER: Failed to link program");
        TraceLog(LOG_ERROR, "SHADER: %s", log);

        glDeleteProgram(programId);
        return 0;
    }

    return programId;
}

// Initialize particle positions and velocities with random values
static void InitParticles(Particle *particles)
{
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        particles[i].positionSize[0] = static_cast<float>(GetRandomValue(-1000, 1000))/1000.0f;
        particles[i].positionSize[1] = static_cast<float>(GetRandomValue(-1000, 1000))/1000.0f;

        // Random velocity in the [-0.15, 0.15] range (normalized device coordinates per second)
        particles[i].velocity[0] = static_cast<float>(GetRandomValue(-150, 150))/1000.0f;
        particles[i].velocity[1] = static_cast<float>(GetRandomValue(-150, 150))/1000.0f;

        // Keep particles above a minimum speed so they do not stay frozen
        if ((fabsf(particles[i].velocity[0]) < 0.03f) && (fabsf(particles[i].velocity[1]) < 0.03f))
        {
            particles[i].velocity[0] = 0.05f;
            particles[i].velocity[1] = 0.05f;
        }

        // Point size in pixels (stored in positionSize.w, as expected by the vertex shader)
        particles[i].positionSize[3] = static_cast<float>(GetRandomValue(15, 45))/10.0f;
    }
}
