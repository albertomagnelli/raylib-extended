/*******************************************************************************************
*
*   raylib-extended [terrain] - example 04: 4096x4096 heightmap terrain from GPU simplex noise
*
*   Uses the Noise class (extensions/noise.h) to render a 4096x4096 simplex noise field and turns
*   it into a flyable heightmap terrain. Everything that touches 16.7 million texels runs on the GPU:
*
*       1. Noise::Create2D() renders five octaves of GPU simplex noise
*          (4096, 2048, 1024, 512, 256 texels, each with the doubled frequency)
*       2. A compute shader combines them into one 4096x4096 R32F heightmap with the requested
*          layout: a flat basin of seven terrace levels in the middle (fractal sum, quantized into
*          steps), ringed by ridged mountains that rise towards the border of the map
*       3. Thermal erosion: a few ping-pong compute passes move material from steep cells
*          into their lower neighbours (talus angle), which carves the ridges. The basin is
*          masked out, so the terraces keep their crisp edges
*       4. The terrain itself is a 512x512 quad grid that is displaced in the vertex shader by
*          sampling the heightmap, its normals come from central differences of the heightmap
*       5. The fragment shader picks sand/grass/rock/snow from height and slope, adds sun light,
*          sky ambient and distance fog
*
*   The heightmap is 4096x4096 texels = 67 MB as R32F and is only read on the CPU for the
*   camera collision and the statistics in the HUD.
*
*   DEPENDENCIES:
*       raylib 6.5+   - Window/GL context, Image for the CPU copy of the heightmap
*       rlgl + glad   - raw OpenGL 4.6 (compute shaders, DSA, texture/image storage)
*       noise.h       - the Noise class that renders the octaves
*
*   WARNING: Requires OpenGL 4.6 support:
*       cmake -B build -DOPENGL_VERSION=4.6
*
*   Controls:
*       W A S D       : Fly forward/left/back/right (camera follows the view direction)
*       E / Q         : Fly up/down
*       MOUSE (RMB)   : Look around
*       MOUSE WHEEL   : Flight speed
*       T             : Cycle view mode (shaded / heightmap / normals)
*       F             : Wireframe
*       R             : Reset the camera to the start position
*       G             : Regenerate the heightmap with a new random offset
*       F12 / ESC     : Screenshot / quit
*
*   Command line:
*       ./terrain_example --auto     Generates, checks the heightmap, flies a path, screenshots, quits
*
*   License: zlib/libpng
**********************************************************************************************/

#include "raylib.h"
#include "raymath.h"        // Required for: Clamp(), Vector3 helpers
#include "rlgl.h"           // Required for: rlDrawRenderBatchActive(), rlGetVersion()
#include "external/glad.h"  // Required for: OpenGL 4.6 functionality

#include "noise.h"          // Required for: Noise (GPU simplex noise octaves)

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>   // Required for: glm::lookAt(), glm::perspective()

#include <cmath>            // Required for: sqrtf(), fabsf(), sinf(), cosf(), fmaxf()
#include <cstring>          // Required for: strcmp()
#include <cstdarg>          // Required for: va_list, va_start(), vsnprintf()
#include <cstdio>           // Required for: printf()
#include <cstdlib>          // Required for: EXIT_SUCCESS, EXIT_FAILURE
#include <random>           // Required for: std::mt19937
#include <vector>           // Required for: std::vector (grid vertices/indices)

//------------------------------------------------------------------------------------
// Defines
//------------------------------------------------------------------------------------
#define HEIGHTMAP_SIZE      4096        // Heightmap resolution in texels (4096x4096 = 16.7 M texels)
#define OCTAVE_COUNT        4           // fBm octaves (a 5th would alias: under ~4 texels per noise period)
#define EROSION_ITERATIONS  6           // Thermal erosion ping pong passes
#define TERRAIN_SIZE        4000.0f     // World size of the terrain square in units
#define TERRAIN_HEIGHT      340.0f      // Height scale applied to the normalized heightmap
#define GRID_QUADS          512         // Terrain grid, GRID_QUADS x GRID_QUADS quads
#define COMBINE_GROUP_SIZE  8           // Must match local_size_x/y in the compute shaders
#define NOISE_BASE_SCALE    0.0015f     // Base frequency of the lowest octave (noise units per texel)

//------------------------------------------------------------------------------------
// Shader sources (GLSL 430 core, compute needs 4.3+)
//------------------------------------------------------------------------------------
// Compute pass 1: fractal sum of the noise octaves into one R32F heightmap
// Compute pass 2: thermal erosion, moves material from a cell into its lower neighbours
static const char *combineShaderCode = R"(
#version 430 core

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0, r32f) readonly uniform image2D imgHeightIn;
layout(binding = 1, r32f) writeonly uniform image2D imgHeightOut;

uniform sampler2D u_octaves[4];
uniform int u_octaveCount;
uniform float u_warpStrength;

uniform float u_basinRadius;        // Radius (in uv units, 0.5 = edge) where the flat terraced basin ends
uniform float u_rimRadius;          // Radius where the mountain rim is fully developed
uniform float u_terraceCount;       // Number of terrace levels in the basin
uniform float u_basinHeight;        // Height of the lowest terrace
uniform float u_terraceRise;        // Height gained per terrace level
uniform float u_mountainHeight;     // Height of the ridged mountains on the rim
uniform float u_rimRise;            // Extra rise towards the border of the map
uniform float u_heightCurve;

uniform int u_size;
uniform int u_pass;                 // 0: height from the octaves, 1: thermal erosion
uniform float u_erosionStrength;
uniform float u_talus;              // Maximum height difference between two neighboured cells
uniform bool u_erodeBasin;          // Erode the flat basin too (keeps the terraces crisp when false)

// Domain warp, the second and third octave displace the sample position
vec2 Warp(vec2 uv)
{
    return vec2(texture(u_octaves[1], uv*0.75).r, texture(u_octaves[2], uv*0.75 + 0.37).r) - 0.5;
}

// Smooth fractal sum (fBm), 0..1
float Fractal(vec2 uv)
{
    float sum = 0.0;
    float amplitude = 1.0;
    float amplitudeSum = 0.0;

    for (int i = 0; i < u_octaveCount; i++)
    {
        sum += texture(u_octaves[i], uv).r*amplitude;
        amplitudeSum += amplitude;
        amplitude *= 0.5;
    }

    return clamp(sum/amplitudeSum, 0.0, 1.0);
}

// Ridged fractal sum, the sharp crests of |2n-1| give the mountain ridges, 0..1
float Ridged(vec2 uv)
{
    float sum = 0.0;
    float amplitude = 1.0;
    float amplitudeSum = 0.0;

    for (int i = 0; i < u_octaveCount; i++)
    {
        float ridge = 1.0 - abs(texture(u_octaves[i], uv).r*2.0 - 1.0);
        sum += ridge*ridge*amplitude;
        amplitudeSum += amplitude;
        amplitude *= 0.5;
    }

    return clamp(sum/amplitudeSum, 0.0, 1.0);
}

// 0 in the flat basin, 1 on the mountain rim
float RimMask(float radius)
{
    return smoothstep(u_basinRadius, u_rimRadius, radius);
}

float HeightAt(vec2 uv)
{
    vec2 warped = uv + Warp(uv)*u_warpStrength;
    float radius = length(uv*2.0 - 1.0);        // 0 at the center, 1 at the border

    // --- Center: flat basin made of terraces ---
    // floor() gives the level, the smoothstep() over the riser keeps the step edges from aliasing
    float level = Fractal(warped)*u_terraceCount;
    float riser = fract(level);
    float tier = (floor(level) + smoothstep(0.0, 0.12, riser))/u_terraceCount;

    float basin = u_basinHeight + tier*u_terraceRise;

    // --- Rim: ridged mountains ---
    float mountain = u_basinHeight + u_terraceRise + Ridged(warped*1.1)*u_mountainHeight;

    float height = mix(basin, mountain, RimMask(radius));

    // Everything rises towards the border, the outermost texels fall off again
    height += u_rimRise*smoothstep(u_basinRadius, 1.0, radius);
    height *= 1.0 - 0.5*smoothstep(1.02, 1.30, radius);

    return clamp(pow(clamp(height, 0.0, 1.0), u_heightCurve), 0.0, 1.0);
}

void main()
{
    ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
    if ((texel.x >= u_size) || (texel.y >= u_size)) return;

    vec2 uv = (vec2(texel) + 0.5)/float(u_size);

    if (u_pass == 0)
    {
        imageStore(imgHeightOut, texel, vec4(HeightAt(uv), 0.0, 0.0, 1.0));
        return;
    }

    // Thermal erosion: a cell that is much higher than a neighbour gives it material
    ivec2 xmax = ivec2(u_size - 1);
    float center = imageLoad(imgHeightIn, texel).r;

    float transfer = 0.0;

    const ivec2 offsets[4] = { ivec2(-1, 0), ivec2(1, 0), ivec2(0, -1), ivec2(0, 1) };

    for (int i = 0; i < 4; i++)
    {
        ivec2 neighbour = clamp(texel + offsets[i], ivec2(0), xmax);
        float difference = center - imageLoad(imgHeightIn, neighbour).r;

        if (difference > u_talus) transfer += (difference - u_talus)*0.25;
    }

    // In the basin the terraces stay crisp, erosion only shapes the mountain rim
    float radius = length(uv*2.0 - 1.0);
    float erosionMask = u_erodeBasin? 1.0 : RimMask(radius);

    imageStore(imgHeightOut, texel, vec4(center - transfer*u_erosionStrength*erosionMask, 0.0, 0.0, 1.0));
}
)";

// Terrain vertex shader: displaces the grid with the heightmap, normals from central differences
static const char *terrainVertexShaderCode = R"(
#version 430 core

layout(location = 0) in vec3 a_position;    // xz in world space, y unused
layout(location = 1) in vec2 a_texcoord;    // Heightmap uv in [0, 1]

uniform sampler2D u_heightMap;
uniform mat4 u_viewProj;
uniform float u_heightScale;                // World height of a normalized heightmap value
uniform float u_texelSize;                  // 1.0/HEIGHTMAP_SIZE
uniform float u_normalScale;                // heightScale/(2*texelSizeWorld)
uniform float u_normalStep;                 // Normal stencil width in texels (matches the grid resolution)

out vec3 v_worldPosition;
out vec3 v_normal;
out vec2 v_texcoord;
out float v_height;

void main()
{
    float height = texture(u_heightMap, a_texcoord).r;

    vec2 step = vec2(u_texelSize*u_normalStep);

    float left = texture(u_heightMap, a_texcoord - vec2(step.x, 0.0)).r;
    float right = texture(u_heightMap, a_texcoord + vec2(step.x, 0.0)).r;
    float bottom = texture(u_heightMap, a_texcoord - vec2(0.0, step.y)).r;
    float top = texture(u_heightMap, a_texcoord + vec2(0.0, step.y)).r;

    // The stencil is wider than one texel, so the normals follow the grid instead of the finest texel detail
    v_normal = normalize(vec3(-(right - left)*u_normalScale/u_normalStep, 1.0, -(top - bottom)*u_normalScale/u_normalStep));
    v_height = height;
    v_worldPosition = vec3(a_position.x, height*u_heightScale, a_position.z);
    v_texcoord = a_texcoord;

    gl_Position = u_viewProj*vec4(v_worldPosition, 1.0);
}
)";

// Terrain fragment shader: sand/grass/rock/snow by height and slope, sun light, sky ambient, fog
static const char *terrainFragmentShaderCode = R"(
#version 430 core

in vec3 v_worldPosition;
in vec3 v_normal;
in vec2 v_texcoord;
in float v_height;

out vec4 finalColor;

uniform vec3 u_sunDirection;
uniform vec3 u_cameraPosition;
uniform vec3 u_skyColor;
uniform vec3 u_fogColor;
uniform float u_fogStart;
uniform float u_fogEnd;
uniform float u_maxHeight;
uniform int u_debugView;            // 0: shaded, 1: heightmap, 2: normals

void main()
{
    if (u_debugView == 1)
    {
        float h = clamp(v_height, 0.0, 1.0);
        finalColor = vec4(mix(vec3(0.02, 0.04, 0.10), vec3(1.0, 0.98, 0.92), h), 1.0);
        return;
    }

    if (u_debugView == 2)
    {
        finalColor = vec4(normalize(v_normal)*0.5 + 0.5, 1.0);
        return;
    }

    vec3 normal = normalize(v_normal);
    float slope = 1.0 - clamp(normal.y, 0.0, 1.0);
    float height01 = clamp(v_height, 0.0, 1.0);

    vec3 sand = vec3(0.78, 0.71, 0.53);
    vec3 grass = vec3(0.22, 0.34, 0.16);
    vec3 rock = vec3(0.33, 0.29, 0.27);
    vec3 snow = vec3(0.93, 0.95, 0.98);

    vec3 albedo = mix(sand, grass, smoothstep(0.02, 0.11, height01));
    albedo = mix(albedo, rock, smoothstep(0.22, 0.52, slope)*0.85);
    albedo = mix(albedo, snow, smoothstep(0.62, 0.80, height01 + 0.18*(1.0 - slope)));

    vec3 sun = normalize(u_sunDirection);
    float diffuse = max(dot(normal, sun), 0.0);
    float sky = 0.5 + 0.5*normal.y;                         // Hemispheric sky light from above

    vec3 ambientColor = mix(vec3(0.30, 0.28, 0.24), vec3(0.42, 0.50, 0.62), sky);
    vec3 sunColor = vec3(1.06, 1.00, 0.90);

    vec3 color = albedo*(ambientColor*0.70 + sunColor*diffuse*1.15);

    // Distance fog into the sky color
    float distance = length(u_cameraPosition - v_worldPosition);
    float fog = clamp((distance - u_fogStart)/(u_fogEnd - u_fogStart), 0.0, 1.0);
    color = mix(color, u_fogColor, fog);

    // Slight horizon haze for the far ridges
    color = mix(color, u_skyColor, clamp((distance - u_fogEnd)/(u_fogEnd*1.5), 0.0, 0.35));

    finalColor = vec4(color, 1.0);
}
)";

//------------------------------------------------------------------------------------
// Types
//------------------------------------------------------------------------------------
// Heightmap on the GPU plus the CPU copy used for the camera collision and the statistics
typedef struct HeightmapData {
    GLuint texture = 0;             // Final R32F heightmap, 4096x4096
    std::vector<f32> cpuHeights;    // CPU copy, heightmapSize*heightmapSize floats
    i32 size = 0;                   // Texture size in texels
    f32 minimum = 0.0f;
    f32 maximum = 0.0f;
    f32 mean = 0.0f;
    f32 deviation = 0.0f;
    f32 averageSlope = 0.0f;
    f64 generationMilliseconds = 0.0;
} HeightmapData;

// Terrain grid: one index buffer over the whole map, displaced in the vertex shader
typedef struct TerrainGrid {
    GLuint vertexArray = 0;
    GLuint vertexBuffer = 0;
    GLuint indexBuffer = 0;
    u32 indexCount = 0;
} TerrainGrid;

//------------------------------------------------------------------------------------
// Module Functions Declaration
//------------------------------------------------------------------------------------
static GLuint CompileShader(GLenum type, const char *code);
static GLuint LinkProgram(GLuint vertexShader, GLuint fragmentShader);
static void GenerateHeightmap(Noise &noise, HeightmapData &heightmap, f32 offsetX, f32 offsetY);
static void ReleaseHeightmap(HeightmapData &heightmap);
static void BuildTerrainGrid(TerrainGrid &grid);
static f32 SampleHeightCpu(const HeightmapData &heightmap, f32 u, f32 v);
static void PrintCheck(bool ok, const char *format, ...);

//------------------------------------------------------------------------------------
// Module Functions Definition
//------------------------------------------------------------------------------------
// Compile a shader object, returns 0 and prints the info log on failure
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

        printf("FAIL  shader compilation (%s)\n%s\n", (type == GL_VERTEX_SHADER)? "vertex" : ((type == GL_FRAGMENT_SHADER)? "fragment" : "compute"), log);
        glDeleteShader(shader);

        return 0;
    }

    return shader;
}

// Link a program, a fragment shader of 0 links a compute program
static GLuint LinkProgram(const GLuint vertexShader, const GLuint fragmentShader)
{
    const GLuint program = glCreateProgram();

    glAttachShader(program, vertexShader);
    if (fragmentShader != 0) glAttachShader(program, fragmentShader);
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

// Render the noise octaves with the Noise class, combine them into the heightmap and erode it
static void GenerateHeightmap(Noise &noise, HeightmapData &heightmap, const f32 offsetX, const f32 offsetY)
{
    const f64 startTime = GetTime();

    // 1) Noise octaves: half the texels and double the frequency per octave
    u32 octaveTextures[OCTAVE_COUNT] = { };

    i32 octaveSize = HEIGHTMAP_SIZE;
    f32 octaveScale = NOISE_BASE_SCALE;

    for (i32 octave = 0; octave < OCTAVE_COUNT; octave++)
    {
        const Texture2D octaveTexture = noise.Create2D(octaveSize, octaveSize, octaveScale, vec2{ offsetX, offsetY });
        octaveTextures[octave] = octaveTexture.id;

        octaveSize /= 2;
        octaveScale *= 4.0f;        // Half the texels needs four times the per texel scale to double the frequency
    }

    // 2) Two R32F heightmaps to ping pong between the combine and the erosion passes
    GLuint heights[2] = { 0 };

    glCreateTextures(GL_TEXTURE_2D, 2, heights);

    for (i32 i = 0; i < 2; i++)
    {
        glTextureStorage2D(heights[i], 1, GL_R32F, HEIGHTMAP_SIZE, HEIGHTMAP_SIZE);
        glTextureParameteri(heights[i], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(heights[i], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(heights[i], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(heights[i], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    // 3) Combine program
    const GLuint combineShader = CompileShader(GL_COMPUTE_SHADER, combineShaderCode);
    const GLuint combineProgram = LinkProgram(combineShader, 0);
    glDeleteShader(combineShader);

    if (combineProgram == 0)
    {
        printf("FAIL  heightmap compute program could not be linked\n");
        return;
    }

    // Bind the octave textures to the units the sampler array reads from
    GLint units[OCTAVE_COUNT] = { 0, 1, 2, 3 };
    for (i32 i = 0; i < OCTAVE_COUNT; i++) glBindTextureUnit(static_cast<GLuint>(i), octaveTextures[i]);

    glUseProgram(combineProgram);
    glUniform1iv(glGetUniformLocation(combineProgram, "u_octaves[0]"), OCTAVE_COUNT, units);
    glUniform1i(glGetUniformLocation(combineProgram, "u_octaveCount"), OCTAVE_COUNT);
    glUniform1f(glGetUniformLocation(combineProgram, "u_warpStrength"), 0.18f);

    // Shape of the terrain: flat terraced basin in the middle, ridged mountains around it
    glUniform1f(glGetUniformLocation(combineProgram, "u_basinRadius"), 0.42f);      // Flat basin up to ~42% of the half width
    glUniform1f(glGetUniformLocation(combineProgram, "u_rimRadius"), 0.82f);        // Mountains fully developed from here
    glUniform1f(glGetUniformLocation(combineProgram, "u_terraceCount"), 7.0f);      // Seven terrace levels in the basin
    glUniform1f(glGetUniformLocation(combineProgram, "u_basinHeight"), 0.10f);      // Lowest terrace
    glUniform1f(glGetUniformLocation(combineProgram, "u_terraceRise"), 0.15f);      // Step height between two terraces
    glUniform1f(glGetUniformLocation(combineProgram, "u_mountainHeight"), 0.70f);   // Ridge height on the rim
    glUniform1f(glGetUniformLocation(combineProgram, "u_rimRise"), 0.10f);          // Extra rise towards the border
    glUniform1f(glGetUniformLocation(combineProgram, "u_heightCurve"), 1.10f);

    glUniform1i(glGetUniformLocation(combineProgram, "u_size"), HEIGHTMAP_SIZE);
    glUniform1f(glGetUniformLocation(combineProgram, "u_erosionStrength"), 0.65f);
    glUniform1f(glGetUniformLocation(combineProgram, "u_talus"), 0.0012f);
    glUniform1i(glGetUniformLocation(combineProgram, "u_erodeBasin"), 0);           // Keep the terraces crisp

    const GLint passLoc = glGetUniformLocation(combineProgram, "u_pass");
    const GLuint groups = HEIGHTMAP_SIZE/COMBINE_GROUP_SIZE;

    // 4) Pass 0: fractal sum into heights[0]
    glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);
    glBindImageTexture(1, heights[0], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

    glUniform1i(passLoc, 0);
    glDispatchCompute(groups, groups, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // 5) Thermal erosion, ping pong between the two heightmaps
    for (i32 iteration = 0; iteration < EROSION_ITERATIONS; iteration++)
    {
        const i32 source = iteration%2;
        const i32 target = (iteration + 1)%2;

        glBindImageTexture(0, heights[source], 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);
        glBindImageTexture(1, heights[target], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

        glUniform1i(passLoc, 1);
        glDispatchCompute(groups, groups, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    glUseProgram(0);
    glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);
    glBindImageTexture(1, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
    glDeleteProgram(combineProgram);

    // The eroded result lives in the last written heightmap
    const i32 resultIndex = EROSION_ITERATIONS%2;
    const i32 discardedIndex = (EROSION_ITERATIONS + 1)%2;

    glDeleteTextures(1, &heights[discardedIndex]);

    // The octaves are no longer needed, the heightmap was built from them
    glDeleteTextures(OCTAVE_COUNT, octaveTextures);

    ReleaseHeightmap(heightmap);

    heightmap.texture = heights[resultIndex];
    heightmap.size = HEIGHTMAP_SIZE;
    heightmap.cpuHeights.resize(static_cast<size_t>(HEIGHTMAP_SIZE)*HEIGHTMAP_SIZE);

    glGetTextureImage(heightmap.texture, 0, GL_RED, GL_FLOAT,
                      static_cast<GLsizei>(heightmap.cpuHeights.size()*sizeof(f32)), heightmap.cpuHeights.data());

    // 6) Statistics and a slope check for the self test
    f64 sum = 0.0;
    f64 sumSquares = 0.0;
    f64 slopeSum = 0.0;

    heightmap.minimum = 1.0f;
    heightmap.maximum = 0.0f;

    for (i32 y = 0; y < HEIGHTMAP_SIZE; y++)
    {
        for (i32 x = 0; x < HEIGHTMAP_SIZE; x++)
        {
            const f32 height = heightmap.cpuHeights[static_cast<size_t>(y)*HEIGHTMAP_SIZE + x];

            heightmap.minimum = fminf(heightmap.minimum, height);
            heightmap.maximum = fmaxf(heightmap.maximum, height);
            sum += height;
            sumSquares += static_cast<f64>(height)*height;

            const f32 right = heightmap.cpuHeights[static_cast<size_t>(y)*HEIGHTMAP_SIZE + ((x + 1)%HEIGHTMAP_SIZE)];
            const f32 top = heightmap.cpuHeights[static_cast<size_t>((y + 1)%HEIGHTMAP_SIZE)*HEIGHTMAP_SIZE + x];
            slopeSum += fabs(static_cast<double>(right) - height) + fabs(static_cast<double>(top) - height);
        }
    }

    const f64 texelCount = static_cast<f64>(HEIGHTMAP_SIZE)*HEIGHTMAP_SIZE;

    heightmap.mean = static_cast<f32>(sum/texelCount);
    const f64 variance = (sumSquares/texelCount) - (static_cast<f64>(heightmap.mean)*heightmap.mean);
    heightmap.deviation = static_cast<f32>(sqrt(fmax(variance, 0.0)));
    heightmap.averageSlope = static_cast<f32>(slopeSum/(2.0*texelCount));
    heightmap.generationMilliseconds = (GetTime() - startTime)*1000.0;
}

// Delete the heightmap texture and the CPU copy
static void ReleaseHeightmap(HeightmapData &heightmap)
{
    if (heightmap.texture != 0) glDeleteTextures(1, &heightmap.texture);

    heightmap.texture = 0;
    heightmap.cpuHeights.clear();
}

// Build the terrain grid: xz positions in world space plus the heightmap uv, u32 indexed
static void BuildTerrainGrid(TerrainGrid &grid)
{
    const i32 vertexLine = GRID_QUADS + 1;

    std::vector<f32> vertices;
    vertices.reserve(static_cast<size_t>(vertexLine)*vertexLine*5);

    for (i32 z = 0; z < vertexLine; z++)
    {
        for (i32 x = 0; x < vertexLine; x++)
        {
            const f32 u = static_cast<f32>(x)/GRID_QUADS;
            const f32 v = static_cast<f32>(z)/GRID_QUADS;

            vertices.push_back((u - 0.5f)*TERRAIN_SIZE);        // x
            vertices.push_back(0.0f);                           // y, displaced in the vertex shader
            vertices.push_back((v - 0.5f)*TERRAIN_SIZE);        // z
            vertices.push_back(u);
            vertices.push_back(v);
        }
    }

    std::vector<u32> indices;
    indices.reserve(static_cast<size_t>(GRID_QUADS)*GRID_QUADS*6);

    for (i32 z = 0; z < GRID_QUADS; z++)
    {
        for (i32 x = 0; x < GRID_QUADS; x++)
        {
            const u32 topLeft = static_cast<u32>(z*vertexLine + x);
            const u32 topRight = topLeft + 1;
            const u32 bottomLeft = static_cast<u32>((z + 1)*vertexLine + x);
            const u32 bottomRight = bottomLeft + 1;

            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }

    grid.indexCount = static_cast<u32>(indices.size());

    glCreateVertexArrays(1, &grid.vertexArray);
    glCreateBuffers(1, &grid.vertexBuffer);
    glCreateBuffers(1, &grid.indexBuffer);

    glNamedBufferData(grid.vertexBuffer, static_cast<GLsizeiptr>(vertices.size()*sizeof(f32)), vertices.data(), GL_STATIC_DRAW);
    glNamedBufferData(grid.indexBuffer, static_cast<GLsizeiptr>(indices.size()*sizeof(u32)), indices.data(), GL_STATIC_DRAW);

    // Binding 0: position (3 floats) + heightmap uv (2 floats), 20 bytes stride
    glVertexArrayVertexBuffer(grid.vertexArray, 0, grid.vertexBuffer, 0, 5*sizeof(f32));
    glEnableVertexArrayAttrib(grid.vertexArray, 0);
    glVertexArrayAttribFormat(grid.vertexArray, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(grid.vertexArray, 0, 0);
    glEnableVertexArrayAttrib(grid.vertexArray, 1);
    glVertexArrayAttribFormat(grid.vertexArray, 1, 2, GL_FLOAT, GL_FALSE, 3*sizeof(f32));
    glVertexArrayAttribBinding(grid.vertexArray, 1, 0);

    glVertexArrayElementBuffer(grid.vertexArray, grid.indexBuffer);
}

// Bilinear height lookup on the CPU copy, u/v in [0, 1]
static f32 SampleHeightCpu(const HeightmapData &heightmap, const f32 u, const f32 v)
{
    if (heightmap.cpuHeights.empty()) return 0.0f;

    const f32 x = Clamp(u, 0.0f, 0.999999f)*static_cast<f32>(heightmap.size);
    const f32 y = Clamp(v, 0.0f, 0.999999f)*static_cast<f32>(heightmap.size);

    const auto x0 = static_cast<i32>(x);
    const auto y0 = static_cast<i32>(y);
    const i32 x1 = (x0 + 1)%heightmap.size;
    const i32 y1 = (y0 + 1)%heightmap.size;

    const f32 fx = x - static_cast<f32>(x0);
    const f32 fy = y - static_cast<f32>(y0);

    const auto at = [&heightmap](const i32 px, const i32 py) {
        return heightmap.cpuHeights[static_cast<size_t>(py)*heightmap.size + px];
    };

    const f32 bottom = at(x0, y0)*(1.0f - fx) + at(x1, y0)*fx;
    const f32 top = at(x0, y1)*(1.0f - fx) + at(x1, y1)*fx;

    return bottom*(1.0f - fy) + top*fy;
}

// Export the 4096x4096 heightmap as a grayscale PNG (one pixel per heightmap texel)
static void ExportHeightmapPng(const HeightmapData &heightmap, const char *fileName)
{
    if (heightmap.cpuHeights.empty()) return;

    Image image = { };
    image.width = heightmap.size;
    image.height = heightmap.size;
    image.mipmaps = 1;
    image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    image.data = malloc(static_cast<size_t>(heightmap.size)*heightmap.size*4);

    if (image.data == nullptr) return;

    auto *pixels = static_cast<u8 *>(image.data);

    for (size_t i = 0; i < heightmap.cpuHeights.size(); i++)
    {
        const auto value = static_cast<u8>(Clamp(heightmap.cpuHeights[i], 0.0f, 1.0f)*255.0f + 0.5f);

        pixels[i*4 + 0] = value;
        pixels[i*4 + 1] = value;
        pixels[i*4 + 2] = value;
        pixels[i*4 + 3] = 255;
    }

    ExportImage(image, fileName);
    UnloadImage(image);

    printf("INFO  heightmap exported: %s (%ix%i px)\n", fileName, heightmap.size, heightmap.size);
}

// Print a self check line and count the failures
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

    constexpr i32 screenWidth = 1280;
    constexpr i32 screenHeight = 720;

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "raylib-extended [terrain] - 4096x4096 heightmap from GPU simplex noise");

    if (rlGetVersion() != RL_OPENGL_46)
    {
        printf("FAIL  raylib was not compiled with OpenGL 4.6 support (cmake -DOPENGL_VERSION=4.6)\n");

        CloseWindow();
        return EXIT_FAILURE;
    }

    printf("INFO  GL version:  %s\n", reinterpret_cast<const char *>(glGetString(GL_VERSION)));
    printf("INFO  GL renderer: %s\n", reinterpret_cast<const char *>(glGetString(GL_RENDERER)));

    //----------------------------------------------------------------------------------
    // Heightmap: five noise octaves rendered by the Noise class, combined and eroded on the GPU
    //----------------------------------------------------------------------------------
    Noise noise;

    if (!noise.IsReady())
    {
        printf("FAIL  compute shaders are not available\n");

        CloseWindow();
        return EXIT_FAILURE;
    }

    std::mt19937 random(4242);
    std::uniform_real_distribution<f32> uniformOffset(-500.0f, 500.0f);

    HeightmapData heightmap;
    GenerateHeightmap(noise, heightmap, uniformOffset(random), uniformOffset(random));

    if (heightmap.texture == 0)
    {
        printf("FAIL  heightmap generation failed\n");

        noise.UnloadPrograms();
        CloseWindow();
        return EXIT_FAILURE;
    }

    printf("\n--- heightmap %ix%i (%i octaves, %i erosion passes) ---\n",
           heightmap.size, heightmap.size, OCTAVE_COUNT, EROSION_ITERATIONS);
    printf("INFO  generated in %.1f ms (GPU)\n", static_cast<double>(heightmap.generationMilliseconds));
    printf("INFO  height: min %.4f  max %.4f  mean %.4f  deviation %.4f  average slope %.5f/texel\n",
           heightmap.minimum, heightmap.maximum, heightmap.mean, heightmap.deviation, heightmap.averageSlope);

    PrintCheck(heightmap.size == HEIGHTMAP_SIZE, "heightmap is %ix%i texels", heightmap.size, heightmap.size);
    PrintCheck((heightmap.maximum - heightmap.minimum) > 0.35f, "heightmap has relief (min %.3f, max %.3f)", heightmap.minimum, heightmap.maximum);
    PrintCheck((heightmap.mean > 0.15f) && (heightmap.mean < 0.85f), "heightmap mean is inside the expected band (%.3f)", heightmap.mean);
    PrintCheck(heightmap.deviation > 0.05f, "heightmap deviation shows variation (%.3f)", heightmap.deviation);
    PrintCheck(heightmap.averageSlope > 0.0002f, "eroded heightmap keeps a slope (%.5f per texel)", heightmap.averageSlope);

    //----------------------------------------------------------------------------------
    // Terrain mesh and shaders
    //----------------------------------------------------------------------------------
    TerrainGrid grid;
    BuildTerrainGrid(grid);

    const GLuint terrainVertexShader = CompileShader(GL_VERTEX_SHADER, terrainVertexShaderCode);
    const GLuint terrainFragmentShader = CompileShader(GL_FRAGMENT_SHADER, terrainFragmentShaderCode);
    const GLuint terrainProgram = LinkProgram(terrainVertexShader, terrainFragmentShader);

    glDeleteShader(terrainVertexShader);
    glDeleteShader(terrainFragmentShader);

    if (terrainProgram == 0)
    {
        printf("FAIL  terrain program could not be linked\n");

        noise.UnloadPrograms();
        CloseWindow();
        return EXIT_FAILURE;
    }

    const GLint heightMapLoc = glGetUniformLocation(terrainProgram, "u_heightMap");
    const GLint viewProjLoc = glGetUniformLocation(terrainProgram, "u_viewProj");
    const GLint heightScaleLoc = glGetUniformLocation(terrainProgram, "u_heightScale");
    const GLint texelSizeLoc = glGetUniformLocation(terrainProgram, "u_texelSize");
    const GLint normalScaleLoc = glGetUniformLocation(terrainProgram, "u_normalScale");
    const GLint normalStepLoc = glGetUniformLocation(terrainProgram, "u_normalStep");
    const GLint sunDirectionLoc = glGetUniformLocation(terrainProgram, "u_sunDirection");
    const GLint cameraPositionLoc = glGetUniformLocation(terrainProgram, "u_cameraPosition");
    const GLint skyColorLoc = glGetUniformLocation(terrainProgram, "u_skyColor");
    const GLint fogColorLoc = glGetUniformLocation(terrainProgram, "u_fogColor");
    const GLint fogStartLoc = glGetUniformLocation(terrainProgram, "u_fogStart");
    const GLint fogEndLoc = glGetUniformLocation(terrainProgram, "u_fogEnd");
    const GLint maxHeightLoc = glGetUniformLocation(terrainProgram, "u_maxHeight");
    const GLint debugViewLoc = glGetUniformLocation(terrainProgram, "u_debugView");

    const f32 texelWorldSize = TERRAIN_SIZE/static_cast<f32>(HEIGHTMAP_SIZE);

    glBindTextureUnit(0, heightmap.texture);
    glUseProgram(terrainProgram);
    glUniform1i(heightMapLoc, 0);
    glUniform1f(heightScaleLoc, TERRAIN_HEIGHT);
    glUniform1f(texelSizeLoc, 1.0f/static_cast<f32>(HEIGHTMAP_SIZE));
    glUniform1f(normalScaleLoc, TERRAIN_HEIGHT/(2.0f*texelWorldSize));
    glUniform1f(normalStepLoc, static_cast<f32>(HEIGHTMAP_SIZE)/GRID_QUADS);   // One grid quad in texels
    glUniform3f(skyColorLoc, 0.58f, 0.70f, 0.86f);
    glUniform3f(fogColorLoc, 0.58f, 0.70f, 0.86f);       // Same as the clear color, so the ridges fade into the sky
    glUniform1f(fogStartLoc, 1800.0f);
    glUniform1f(fogEndLoc, 6500.0f);
    glUniform1f(maxHeightLoc, 1.0f);
    glUseProgram(0);

    //----------------------------------------------------------------------------------
    // Camera and render state
    //----------------------------------------------------------------------------------
    vec3 cameraPosition = { 0.0f, 0.0f, -1750.0f };
    auto cameraYaw = 0.0f;                      // Radians, 0 looks along +z
    auto cameraPitch = -0.12f;
    auto flightSpeed = 220.0f;
    auto debugView = 0;                         // 0: shaded, 1: heightmap, 2: normals
    auto wireframe = false;
    auto frame = 0;
    auto frameTimeMs = 0.0f;                    // Smoothed frame time, DrawFPS() needs a full second to settle

    const vec3 sunDirection = normalize(vec3{ 0.35f, 0.62f, 0.42f });   // Points towards the sun, so flat ground is lit

    // Point the camera at a world position
    auto LookAtTarget = [&](const vec3 target)
    {
        const vec3 direction = normalize(target - cameraPosition);

        cameraYaw = atan2f(direction.x, direction.z);
        cameraPitch = asinf(Clamp(direction.y, -1.0f, 1.0f));
    };

    // Auto mode flies a fixed path so the screenshots are reproducible
    auto AutoCamera = [&](const f32 time, const i32 frameIndex)
    {
        if (frameIndex < 30)
        {
            // Low orbit over the terraced basin, looking out to the mountain rim
            const f32 angle = time*0.9f;

            cameraPosition = vec3{
                sinf(angle)*760.0f,
                TERRAIN_HEIGHT*0.30f + 22.0f*sinf(time*1.3f),
                -cosf(angle)*760.0f
            };
            LookAtTarget(vec3{ sinf(angle + 1.2f)*1800.0f, TERRAIN_HEIGHT*0.5f, -cosf(angle + 1.2f)*1800.0f });
        }
        else
        {
            // Overview from above: the terraced basin fills the frame, the mountain rim closes it
            cameraPosition = vec3{ 640.0f, TERRAIN_HEIGHT*1.75f, -660.0f };
            LookAtTarget(vec3{ 0.0f, TERRAIN_HEIGHT*0.25f, 40.0f });
        }
    };

    //----------------------------------------------------------------------------------
    // Main loop
    //----------------------------------------------------------------------------------
    while (!WindowShouldClose())
    {
        // Input: free flight, RMB looks around
        if (!autoMode)
        {
            if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
            {
                const vec2 delta = GetMouseDelta();
                cameraYaw += delta.x*0.004f;
                cameraPitch = Clamp(cameraPitch - delta.y*0.004f, -1.35f, 1.35f);
            }

            const f32 wheel = GetMouseWheelMove();
            if (wheel != 0.0f) flightSpeed = Clamp(flightSpeed*((wheel > 0.0f)? 1.25f : 0.8f), 25.0f, 2000.0f);
            if (IsKeyPressed(KEY_T)) debugView = (debugView + 1)%3;
            if (IsKeyPressed(KEY_F)) wireframe = !wireframe;
            if (IsKeyPressed(KEY_K)) ExportHeightmapPng(heightmap, "terrain_heightmap_4096.png");

            if (IsKeyPressed(KEY_R))
            {
                cameraPosition = vec3{ 0.0f, TERRAIN_HEIGHT*1.2f, -1750.0f };
                cameraYaw = 0.0f;
                cameraPitch = -0.12f;
            }

            if (IsKeyPressed(KEY_G))
            {
                GenerateHeightmap(noise, heightmap, uniformOffset(random), uniformOffset(random));
                glBindTextureUnit(0, heightmap.texture);
                printf("INFO  regenerated in %.1f ms  min %.3f  max %.3f  mean %.3f\n",
                       static_cast<double>(heightmap.generationMilliseconds), heightmap.minimum, heightmap.maximum, heightmap.mean);
            }

            // Movement follows the view direction, the pitch only affects the vertical part
            const vec3 forward = normalize(vec3{ sinf(cameraYaw)*cosf(cameraPitch), sinf(cameraPitch), cosf(cameraYaw)*cosf(cameraPitch) });
            const vec3 right = normalize(cross(forward, vec3{ 0.0f, 1.0f, 0.0f }));
            const f32 step = flightSpeed*GetFrameTime();

            if (IsKeyDown(KEY_W)) cameraPosition = cameraPosition + forward*step;
            if (IsKeyDown(KEY_S)) cameraPosition = cameraPosition - forward*step;
            if (IsKeyDown(KEY_D)) cameraPosition = cameraPosition + right*step;
            if (IsKeyDown(KEY_A)) cameraPosition = cameraPosition - right*step;
            if (IsKeyDown(KEY_E)) cameraPosition.y += step;
            if (IsKeyDown(KEY_Q)) cameraPosition.y -= step;
        }
        else
        {
            AutoCamera(static_cast<f32>(frame)/60.0f, frame);
        }

        // Smoothed frame time for the HUD (DrawFPS() would need a full second of samples)
        frameTimeMs = (frame == 0)? (GetFrameTime()*1000.0f) : (frameTimeMs*0.9f + GetFrameTime()*1000.0f*0.1f);

        // Camera collision: stay above the terrain surface
        const f32 terrainHeight = SampleHeightCpu(heightmap, cameraPosition.x/TERRAIN_SIZE + 0.5f, cameraPosition.z/TERRAIN_SIZE + 0.5f)*TERRAIN_HEIGHT;
        if (cameraPosition.y < terrainHeight + 12.0f) cameraPosition.y = terrainHeight + 12.0f;

        // Matrices
        const vec3 lookDirection = {
            sinf(cameraYaw)*cosf(cameraPitch),
            sinf(cameraPitch),
            cosf(cameraYaw)*cosf(cameraPitch)
        };

        const glm::vec3 eye(cameraPosition.x, cameraPosition.y, cameraPosition.z);
        const glm::vec3 center = eye + glm::vec3(lookDirection.x, lookDirection.y, lookDirection.z);
        const glm::mat4 view = glm::lookAt(eye, center, glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::mat4 projection = glm::perspective(DEG2RAD*55.0f, static_cast<f32>(screenWidth)/static_cast<f32>(screenHeight), 0.5f, 6000.0f);
        const glm::mat4 viewProj = projection*view;

        // Render
        //----------------------------------------------------------------------------------
        BeginDrawing();
            ClearBackground(Color{ 148, 179, 219, 255 });        // Same as u_fogColor (0.58, 0.70, 0.86)

            // Terrain: one indexed draw of the displaced grid
            rlEnableDepthTest();

            if (wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

            // NOTE: raylib binds its own textures (font atlas, default texture) to unit 0 for the
            // HUD, so the heightmap has to be bound again right before the terrain draw
            glBindTextureUnit(0, heightmap.texture);

            glUseProgram(terrainProgram);
            glUniformMatrix4fv(viewProjLoc, 1, GL_FALSE, &viewProj[0][0]);
            glUniform3f(sunDirectionLoc, sunDirection.x, sunDirection.y, sunDirection.z);
            glUniform3f(cameraPositionLoc, cameraPosition.x, cameraPosition.y, cameraPosition.z);
            glUniform1i(debugViewLoc, debugView);

            glBindVertexArray(grid.vertexArray);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(grid.indexCount), GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);
            glUseProgram(0);

            if (wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            rlDisableDepthTest();

            // HUD
            DrawText(TextFormat("%.2f ms/frame (%.0f fps)", frameTimeMs, (frameTimeMs > 0.0f)? 1000.0f/frameTimeMs : 0.0f), 20, 20, 20, RAYWHITE);
            DrawText(TextFormat("heightmap %ix%i   octaves %i   erosion %i passes   generated in %.0f ms",
                                heightmap.size, heightmap.size, OCTAVE_COUNT, EROSION_ITERATIONS, static_cast<double>(heightmap.generationMilliseconds)),
                     20, 52, 20, RAYWHITE);
            DrawText(TextFormat("height min %.3f  max %.3f  mean %.3f  deviation %.3f", heightmap.minimum, heightmap.maximum, heightmap.mean, heightmap.deviation), 20, 78, 20, LIGHTGRAY);
            DrawText(TextFormat("grid %ix%i quads = %u triangles   height scale %.0f   terrace basin + %i levels, mountain rim",
                                GRID_QUADS, GRID_QUADS, grid.indexCount/3, TERRAIN_HEIGHT, 7), 20, 104, 20, LIGHTGRAY);
            DrawText(TextFormat("view: %s", (debugView == 0)? "shaded" : ((debugView == 1)? "heightmap" : "normals")), 20, 130, 20, LIGHTGRAY);
            DrawText(TextFormat("camera %.0f / %.0f / %.0f   altitude above terrain %.0f   speed %.0f",
                                cameraPosition.x, cameraPosition.y, cameraPosition.z, cameraPosition.y - terrainHeight, flightSpeed), 20, 156, 20, LIGHTGRAY);
            DrawText("WASD: fly, E/Q: up/down, RMB: look, wheel: speed, T: view, F: wireframe, K: save heightmap PNG, G: regenerate, R: reset", 20, screenHeight - 40, 20, DARKGRAY);

            if (autoMode && ((frame%15) == 0))
            {
                printf("INFO  frame %3i: %.2f ms/frame, camera (%.0f, %.0f, %.0f), altitude %.0f, view %i\n",
                       frame, frameTimeMs, cameraPosition.x, cameraPosition.y, cameraPosition.z, cameraPosition.y - terrainHeight, debugView);
            }
        EndDrawing();

        // Auto mode: screenshots, a second view mode and the final checks
        if (autoMode)
        {
            if (frame == 18)
            {
                TakeScreenshot("terrain_example.png");
                printf("INFO  screenshot saved: terrain_example.png\n");

                ExportHeightmapPng(heightmap, "terrain_heightmap_4096.png");
            }

            // Overview from above: shows the terraced basin and the surrounding mountain rim
            if (frame == 38)
            {
                TakeScreenshot("terrain_overview.png");
                printf("INFO  screenshot saved: terrain_overview.png\n");
            }

            if (frame == 44) debugView = 1;             // heightmap view
            if (frame == 48)
            {
                TakeScreenshot("terrain_heightmap.png");
                printf("INFO  screenshot saved: terrain_heightmap.png\n");
            }

            if (frame == 50) debugView = 2;             // normal view

            if (frame == 54)
            {
                TakeScreenshot("terrain_normals.png");
                printf("INFO  screenshot saved: terrain_normals.png\n");
            }

            if (frame >= 60) break;
        }

        frame++;
    }

    PrintCheck(glGetError() == GL_NO_ERROR, "no OpenGL error during the frame loop");

    // Deinitialization
    //--------------------------------------------------------------------------------------
    glDeleteVertexArrays(1, &grid.vertexArray);
    glDeleteBuffers(1, &grid.vertexBuffer);
    glDeleteBuffers(1, &grid.indexBuffer);
    glDeleteProgram(terrainProgram);
    ReleaseHeightmap(heightmap);

    noise.UnloadPrograms();         // Programs belong to the GL context, they have to go before CloseWindow()

    CloseWindow();

    printf("\n%i failure(s)\n", checkFailures);

    return (checkFailures == 0)? EXIT_SUCCESS : EXIT_FAILURE;
}


