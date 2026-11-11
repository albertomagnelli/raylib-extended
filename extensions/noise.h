/**********************************************************************************************
*
*   noise - GPU Simplex Noise texture generator for raylib (C++ class)
*
*   Noise renders 1D, 2D and 3D simplex noise into GPU textures with compute shaders:
*       - Create1D() : GL_TEXTURE_1D, one row of noise, work group 64x1x1
*       - Create2D() : raylib Texture2D (GL_TEXTURE_2D), work group 8x8x1
*       - Create3D() : GL_TEXTURE_3D, work group 8x8x8 (volumetric noise for raymarching, clouds, ...)
*       - Read()     : copies the result back to the CPU as an Image (export, tests, thumbnails)
*
*   The three GLSL compute sources are kept verbatim in this header, the class only handles the
*   texture allocation, the image unit binding, the uniforms, the dispatch and the barriers.
*
*   DEPENDENCIES:
*       raylib 6.5+   - Window/GL context, Image/Texture types, PIXELFORMAT_UNCOMPRESSED_R32G32B32A32
*       rlgl          - rlGetVersion(), rlLoadShader(), rlLoadShaderProgramCompute(), rlComputeShaderDispatch()
*                       and the OpenGL entry points (glad)
*
*   USAGE:
*       #include "noise.h"
*
*       InitWindow(800, 600, "noise");                          // Creates the GL context
*       Noise noise;                                            // Programs are built on first use
*
*       Texture2D noise2D = noise.Create2D(512, 512, 0.01f);    // Draw it like any other texture
*       Noise::Texture3D noise3D = noise.Create3D(128, 128, 128, 0.05f);
*
*       Image slice = noise.Read(noise3D, 64);                  // CPU copy of one layer
*       UnloadImage(slice);
*
*       UnloadTexture(noise2D);
*       noise3D.Unload();
*
*   NOTES:
*       - Requires OpenGL 4.3+ (compute shaders), check Noise::IsSupported() before use.
*         The raylib-extended build has to set OPENGL_VERSION to 4.3 or 4.6.
*       - Call it after InitWindow(), the GL context must exist.
*       - Dimensions are rounded up to a multiple of the work group size (64 for 1D, 8 for 2D/3D)
*         so every shader invocation writes inside the texture; the returned struct reports the real
*         size and a TRACELOG warning is emitted when a size had to be rounded up.
*       - Textures are GL_RGBA32F with GL_LINEAR filtering, the noise is written to rgb and normalized
*         to [0.0, 1.0], alpha stays 1.0. Wrap mode is GL_CLAMP_TO_EDGE, change it with
*         glTextureParameteri() on the returned id if you need GL_REPEAT.
*       - The dispatch saves and restores the shader program bound before, so it can be called
*         between raylib draw calls.
*       - Header-only, all member functions are inline, just include it from a .cpp file.
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2026 raylib-extended contributors
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

#ifndef RAYLIB_EXTENDED_NOISE_H
#define RAYLIB_EXTENDED_NOISE_H

#include "raylib.h"         // Required for: u32, i32, f32, vec2, vec3, Image, Texture2D, PIXELFORMAT_UNCOMPRESSED_R32G32B32A32
#include "rlgl.h"           // Required for: rlGetVersion(), RL_OPENGL_43, rlLoadShader(), rlLoadShaderProgramCompute(),
                            //               rlComputeShaderDispatch() and the OpenGL entry points (glad)
#include "external/glad.h"  // Required for: OpenGL 4.3+ functionality (glCreateTextures, glTextureStorage*, glBindImageTexture, ...)

#include <cstddef>          // Required for: std::size_t
#include <string_view>      // Required for: std::string_view (shader sources)
#include <stdlib.h>         // Required for: malloc()

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
#define NOISE_TEXTURE_FORMAT        GL_RGBA32F    // Internal format, must match layout(rgba32f) in the shaders
#define NOISE_GROUP_SIZE_1D         64            // local_size_x of the 1D shader
#define NOISE_GROUP_SIZE_2D         8             // local_size_x/y of the 2D shader
#define NOISE_GROUP_SIZE_3D         8             // local_size_x/y/z of the 3D shader

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// Simplex noise texture generator, one compute program per dimension
class Noise
{
public:
    // 1D noise texture (GL_TEXTURE_1D, rgb = noise, a = 1.0)
    struct Texture1D
    {
        u32 id = 0;                             // OpenGL texture id
        i32 width = 0;                          // Texture width

        [[nodiscard]] bool IsValid() const { return (id != 0) && (width > 0); }
        void Unload() const { if (id != 0) glDeleteTextures(1, &id); }
    };

    // 3D noise texture (GL_TEXTURE_3D, rgb = noise, a = 1.0)
    struct Texture3D
    {
        u32 id = 0;                             // OpenGL texture id
        i32 width = 0;                          // Texture width
        i32 height = 0;                         // Texture height
        i32 depth = 0;                          // Texture depth (number of layers)

        [[nodiscard]] bool IsValid() const { return (id != 0) && (width > 0) && (height > 0) && (depth > 0); }
        void Unload() const { if (id != 0) glDeleteTextures(1, &id); }
    };

    Noise() = default;
    ~Noise() { UnloadPrograms(); }

    // A Noise instance owns GL programs, copying would delete them twice
    Noise(const Noise &) = delete;
    Noise &operator=(const Noise &) = delete;

    // Check if the current GL context provides compute shaders (OpenGL 4.3+)
    [[nodiscard]] static bool IsSupported() { return rlGetVersion() >= RL_OPENGL_43; }

    // Build the compute programs now instead of on the first Create*() call
    [[nodiscard]] bool IsReady() { return EnsurePrograms(); }

    // Render a 1D noise texture, 'scale' is the noise frequency per texel, 'offset' shifts the sample position
    [[nodiscard]] Texture1D Create1D(const i32 width = 512, const f32 scale = 0.05f, const f32 offset = 0.0f)
    {
        Texture1D texture = { };

        if (!EnsurePrograms()) return texture;

        const i32 textureWidth = RoundUpToGroup(width, NOISE_GROUP_SIZE_1D);
        if (textureWidth != width) TRACELOG(LOG_WARNING, "NOISE: 1D width %i rounded up to %i (work group size: %i)", width, textureWidth, NOISE_GROUP_SIZE_1D);
        if (textureWidth <= 0) return texture;

        texture.id = CreateTextureStorage(GL_TEXTURE_1D, textureWidth, 0, 0);
        if (texture.id == 0) return texture;

        BindImage(texture.id, false);
        Dispatch(program1D, scale, vec3{ offset, 0.0f, 0.0f }, 1,
                 static_cast<u32>(textureWidth/NOISE_GROUP_SIZE_1D), 1, 1);
        UnbindImage();

        texture.width = textureWidth;

        return texture;
    }

    // Render a 2D noise texture as raylib Texture2D (format PIXELFORMAT_UNCOMPRESSED_R32G32B32A32)
    [[nodiscard]] Texture2D Create2D(const i32 width = 256, const i32 height = 256, const f32 scale = 0.05f, const vec2 offset = { 0.0f, 0.0f })
    {
        Texture2D texture = { };

        if (!EnsurePrograms()) return texture;

        const i32 textureWidth = RoundUpToGroup(width, NOISE_GROUP_SIZE_2D);
        const i32 textureHeight = RoundUpToGroup(height, NOISE_GROUP_SIZE_2D);
        if ((textureWidth != width) || (textureHeight != height)) TRACELOG(LOG_WARNING, "NOISE: 2D size %ix%i rounded up to %ix%i (work group size: %i)", width, height, textureWidth, textureHeight, NOISE_GROUP_SIZE_2D);
        if ((textureWidth <= 0) || (textureHeight <= 0)) return texture;

        texture.id = CreateTextureStorage(GL_TEXTURE_2D, textureWidth, textureHeight, 0);
        if (texture.id == 0) return texture;

        BindImage(texture.id, false);
        Dispatch(program2D, scale, vec3{ offset.x, offset.y, 0.0f }, 2,
                 static_cast<u32>(textureWidth/NOISE_GROUP_SIZE_2D), static_cast<u32>(textureHeight/NOISE_GROUP_SIZE_2D), 1);
        UnbindImage();

        texture.width = textureWidth;
        texture.height = textureHeight;
        texture.mipmaps = 1;
        texture.format = PIXELFORMAT_UNCOMPRESSED_R32G32B32A32;

        return texture;
    }

    // Render a 3D noise texture, all layers are written by a single dispatch
    [[nodiscard]] Texture3D Create3D(const i32 width = 256, const i32 height = 256, const i32 depth = 256, const f32 scale = 0.05f, const vec3 offset = { 0.0f, 0.0f, 0.0f })
    {
        Texture3D texture = { };

        if (!EnsurePrograms()) return texture;

        const i32 textureWidth = RoundUpToGroup(width, NOISE_GROUP_SIZE_3D);
        const i32 textureHeight = RoundUpToGroup(height, NOISE_GROUP_SIZE_3D);
        const i32 textureDepth = RoundUpToGroup(depth, NOISE_GROUP_SIZE_3D);
        if ((textureWidth != width) || (textureHeight != height) || (textureDepth != depth)) TRACELOG(LOG_WARNING, "NOISE: 3D size %ix%ix%i rounded up to %ix%ix%i (work group size: %i)", width, height, depth, textureWidth, textureHeight, textureDepth, NOISE_GROUP_SIZE_3D);
        if ((textureWidth <= 0) || (textureHeight <= 0) || (textureDepth <= 0)) return texture;

        texture.id = CreateTextureStorage(GL_TEXTURE_3D, textureWidth, textureHeight, textureDepth);
        if (texture.id == 0) return texture;

        BindImage(texture.id, true);        // Layered: the whole 3D texture is written, not a single layer
        Dispatch(program3D, scale, offset, 3,
                 static_cast<u32>(textureWidth/NOISE_GROUP_SIZE_3D), static_cast<u32>(textureHeight/NOISE_GROUP_SIZE_3D), static_cast<u32>(textureDepth/NOISE_GROUP_SIZE_3D));
        UnbindImage();

        texture.width = textureWidth;
        texture.height = textureHeight;
        texture.depth = textureDepth;

        return texture;
    }

    // Copy a 1D noise texture to the CPU (format PIXELFORMAT_UNCOMPRESSED_R32G32B32A32)
    [[nodiscard]] Image Read(const Texture1D &texture) const
    {
        return ReadTexture(texture.id, texture.width, 1, 1, -1);
    }

    // Copy a 2D noise texture to the CPU (format PIXELFORMAT_UNCOMPRESSED_R32G32B32A32)
    [[nodiscard]] Image Read(const Texture2D &texture) const
    {
        return ReadTexture(texture.id, texture.width, texture.height, 1, -1);
    }

    // Copy one layer of a 3D noise texture to the CPU (format PIXELFORMAT_UNCOMPRESSED_R32G32B32A32)
    [[nodiscard]] Image Read(const Texture3D &texture, const i32 layer = 0) const
    {
        return ReadTexture(texture.id, texture.width, texture.height, 1, layer);
    }

    // Delete the compute programs, they are rebuilt on the next Create*() call
    void UnloadPrograms()
    {
        if (program1D != 0) { rlUnloadShaderProgram(program1D); program1D = 0; }
        if (program2D != 0) { rlUnloadShaderProgram(program2D); program2D = 0; }
        if (program3D != 0) { rlUnloadShaderProgram(program3D); program3D = 0; }
    }

private:
    u32 program1D = 0;                      // Compute programs, 0 while not built
    u32 program2D = 0;
    u32 program3D = 0;

    // Build the three compute programs, returns false if compute shaders are unavailable
    [[nodiscard]] bool EnsurePrograms()
    {
        if ((program1D != 0) && (program2D != 0) && (program3D != 0)) return true;

        if (!IsSupported())
        {
            TRACELOG(LOG_WARNING, "NOISE: compute shaders require OpenGL 4.3+, current version: %i (build raylib with OPENGL_VERSION 4.3 or 4.6)", rlGetVersion());
            return false;
        }

        if (program1D == 0) program1D = BuildProgram(sn1D);
        if (program2D == 0) program2D = BuildProgram(sn2D);
        if (program3D == 0) program3D = BuildProgram(sn3D);

        return (program1D != 0) && (program2D != 0) && (program3D != 0);
    }

    // Compile a compute shader source and link it into a program
    [[nodiscard]] static u32 BuildProgram(const std::string_view source)
    {
        // NOTE: #version has to be the first token, the sources start with a newline after R"(
        const std::size_t start = source.find_first_not_of(" \t\r\n");
        const std::string_view code = (start == std::string_view::npos)? source : source.substr(start);

        const u32 shaderId = rlLoadShader(code, RL_COMPUTE_SHADER);
        if (shaderId == 0) return 0;

        const u32 programId = rlLoadShaderProgramCompute(shaderId);

        glDeleteShader(shaderId);           // The linked program keeps its own reference

        return programId;
    }

    // Create an immutable storage texture with linear filtering and clamp-to-edge wrap mode
    [[nodiscard]] static u32 CreateTextureStorage(const u32 target, const i32 width, const i32 height, const i32 depth)
    {
        u32 id = 0;

        glCreateTextures(target, 1, &id);
        if (id == 0) return 0;

        if (target == GL_TEXTURE_1D) glTextureStorage1D(id, 1, NOISE_TEXTURE_FORMAT, width);
        else if (target == GL_TEXTURE_2D) glTextureStorage2D(id, 1, NOISE_TEXTURE_FORMAT, width, height);
        else glTextureStorage3D(id, 1, NOISE_TEXTURE_FORMAT, width, height, depth);

        glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if (target == GL_TEXTURE_3D) glTextureParameteri(id, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        return id;
    }

    // Bind a texture to the image unit 0 the shaders write to
    static void BindImage(const u32 textureId, const bool layered)
    {
        glBindImageTexture(0, textureId, 0, layered? GL_TRUE : GL_FALSE, 0, GL_WRITE_ONLY, NOISE_TEXTURE_FORMAT);
    }

    static void UnbindImage()
    {
        glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, NOISE_TEXTURE_FORMAT);
    }

    // Set the uniforms, run the compute grid and restore the previously bound program
    static void Dispatch(const u32 programId, const f32 scale, const vec3 offset, const i32 dimensions,
                         const u32 groupX, const u32 groupY, const u32 groupZ)
    {
        GLint previousProgram = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);

        glUseProgram(programId);

        // Uniforms use explicit locations, see layout(location = ...) in the shader sources
        glUniform1f(0, scale);                                          // location 0: u_Scale
        if (dimensions == 1) glUniform1f(1, offset.x);                  // location 1: u_Offset (float)
        else if (dimensions == 2) glUniform2f(1, offset.x, offset.y);   // location 1: u_Offset (vec2)
        else glUniform3f(1, offset.x, offset.y, offset.z);              // location 1: u_Offset (vec3)

        rlComputeShaderDispatch(groupX, groupY, groupZ);

        // Make the written texels visible to texture reads (sampling and glGetTextureImage)
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT);

        glUseProgram(previousProgram);
    }

    // Round a dimension up to the next multiple of the work group size, keeps every write inside the texture
    [[nodiscard]] static i32 RoundUpToGroup(const i32 value, const i32 groupSize)
    {
        if (value <= 0) return 0;
        return ((value + groupSize - 1)/groupSize)*groupSize;
    }

    // Pull a texture back to RAM as RGBA32F image, 'layer' < 0 reads the whole texture,
    // otherwise only that single layer of a 3D texture is copied
    [[nodiscard]] static Image ReadTexture(const u32 textureId, const i32 width, const i32 height, const i32 depth, const i32 layer)
    {
        Image image = { };

        if ((textureId == 0) || (width <= 0) || (height <= 0) || (depth <= 0)) return image;

        const i32 readDepth = (layer >= 0)? 1 : depth;
        const std::size_t dataSize = static_cast<std::size_t>(width)*static_cast<std::size_t>(height)*static_cast<std::size_t>(readDepth)*4*sizeof(f32);

        image.data = malloc(dataSize);
        if (image.data == nullptr) return Image{ };

        if (layer >= 0) glGetTextureSubImage(textureId, 0, 0, 0, layer, width, height, 1, GL_RGBA, GL_FLOAT, static_cast<GLsizei>(dataSize), image.data);
        else glGetTextureImage(textureId, 0, GL_RGBA, GL_FLOAT, static_cast<GLsizei>(dataSize), image.data);

        image.width = width;
        image.height = height;
        image.mipmaps = 1;
        image.format = PIXELFORMAT_UNCOMPRESSED_R32G32B32A32;

        return image;
    }

    // --- Shader sources, one compute program per dimension ---

    // 1D simplex noise, 64 threads per work group
    static constexpr std::string_view sn1D = R"(
#version 460 core

// Optimized for 1D arrays matching a 64 thread execution block
layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

// Output texture binding (1D Floating point texture)
layout(rgba32f, binding = 0) writeonly uniform image1D imgOutput1D;

layout(location = 0) uniform float u_Scale = 0.05f;
layout(location = 1) uniform float u_Offset = 0.0f;

vec3 mod289(vec3 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec2 mod289(vec2 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec2 permute(vec2 x) { return mod289(((x * 34.0) + 1.0) * x); }

// --- 1D Simplex Noise Function ---
float snoise(float v) {
    // 1D simplex is just a single step interval line segment
    float i = floor(v);
    float x0 = v - i;
    float x1 = x0 - 1.0;

    // Permutations & Wrap
    vec2 p = permute(mod289(vec3(i, i + 1.0, 0.0)).xy);

    // Map Gradients between [-1, 1]
    vec2 gx = fract(p * 0.0243902439) * 2.0 - 1.0; // 1/41
    vec2 g = gx * (1.79284291400159 - 0.85373472095314 * 0.5);

    // Mix contributions using matching kernel bounds
    vec2 m = max(0.5 - vec2(x0*x0, x1*x1), 0.0);
    m = m * m;
    m = m * m;

    // NOTE: The kernel maxes out at about 0.05, this factor normalizes the result to [-1.0, 1.0]
    return 20.0 * 4.0 * dot(m, vec2(g.x * x0, g.y * x1));
}

void main() {
    int texelCoord = int(gl_GlobalInvocationID.x);

    float samplePos = float(texelCoord) * u_Scale + u_Offset;
    float noiseVal = snoise(samplePos);
    float normalizedNoise = noiseVal * 0.5f + 0.5f;

    imageStore(imgOutput1D, texelCoord, vec4(vec3(normalizedNoise), 1.0f));
}
)";

    // 2D simplex noise, 8x8 threads per work group
    static constexpr std::string_view sn2D = R"(
#version 460 core

// Balanced 2D grid processing layout
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// Output texture binding (2D Flat texture)
layout(rgba32f, binding = 0) writeonly uniform image2D imgOutput2D;

layout(location = 0) uniform float u_Scale = 0.05f;
layout(location = 1) uniform vec2 u_Offset = vec2(0.0f);

vec3 mod289(vec3 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec3 permute(vec3 x) { return mod289(((x * 34.0) + 1.0) * x); }

// --- 2D Simplex Noise Function ---
float snoise(vec2 v) {
    const vec4 C = vec4(0.211324865405187,  // (3.0-sqrt(3.0))/6.0
                        0.366025403784439,  // 0.5*(sqrt(3.0)-1.0)
                       -0.577350269189626,  // -1.0 + 2.0 * C.x
                        0.024390243902439); // 1.0 / 41.0
    // First corner
    vec2 i  = floor(v + dot(v, C.yy));
    vec2 x0 = v -   i + dot(i, C.xx);

    // Other corners
    vec2 i1;
    i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    vec4 x12 = x0.xyxy + C.xxzz;
    x12.xy -= i1;

    // Permutations
    i = mod289(vec3(i, 0.0)).xy;
    vec3 p = permute(permute(i.y + vec3(0.0, i1.y, 1.0)) + i.x + vec3(0.0, i1.x, 1.0));

    vec3 m = max(0.5 - vec3(dot(x0, x0), dot(x12.xy, x12.xy), dot(x12.zw, x12.zw)), 0.0);
    m = m * m;
    m = m * m;

    // Gradients: Calculate 41 points on a line mapped onto a circle
    vec3 x = 2.0 * fract(p * C.www) - 1.0;      // C.www (vec3), C.wwww would be a vec4 and does not match p
    vec3 h = abs(x) - 0.5;
    vec3 ox = floor(x + 0.5);
    vec3 a0 = x - ox;

    // Normalise gradients implicitly
    m *= 1.79284291400159 - 0.85373472095314 * (a0 * a0 + h * h);

    // Compute final noise contribution value
    vec3 g;
    g.x  = a0.x  * x0.x  + h.x  * x0.y;
    g.yz = a0.yz * x12.xz + h.yz * x12.yw;
    return 130.0 * dot(m, g);
}

void main() {
    ivec2 texelCoord = ivec2(gl_GlobalInvocationID.xy);

    vec2 samplePos = vec2(texelCoord) * u_Scale + u_Offset;
    float noiseVal = snoise(samplePos);
    float normalizedNoise = noiseVal * 0.5f + 0.5f;

    imageStore(imgOutput2D, texelCoord, vec4(vec3(normalizedNoise), 1.0f));
}
)";

    // 3D simplex noise, 8x8x8 threads per work group
    static constexpr std::string_view sn3D = R"(
#version 460 core

// Process data in 8x8x8 localized 3D blocks
layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

// Output texture binding (3D Floating point texture)
layout(rgba32f, binding = 0) writeonly uniform image3D imgOutput;

// Uniform variables to scale and animate the noise
layout(location = 0) uniform float u_Scale = 0.05f;
layout(location = 1) uniform vec3 u_Offset = vec3(0.0f);

// Modulo 289 without using expensive division operations
vec3 mod289(vec3 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec4 mod289(vec4 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec4 permute(vec4 x) { return mod289(((x * 34.0) + 1.0) * x); }
vec4 taylorInvSqrt(vec4 r) { return 1.79284291400159 - 0.85373472095314 * r; }

// --- 3D Simplex Noise Function ---
float snoise(vec3 v) {
    const vec2 C = vec2(1.0 / 6.0, 1.0 / 3.0);
    const vec4 D = vec4(0.0, 0.5, 1.0, 2.0);

    // First corner
    vec3 i  = floor(v + dot(v, C.yyy));
    vec3 x0 = v - i + dot(i, C.xxx);

    // Other corners
    vec3 g = step(x0.yzx, x0.xyz);
    vec3 l = 1.0 - g;
    vec3 i1 = min(g.xyz, l.zxy);
    vec3 i2 = max(g.xyz, l.zxy);

    // x0 = x0 - 0.0 + 0.0 * C.xxx;
    // x1 = x0 - i1 + 1.0 * C.xxx;
    // x2 = x0 - i2 + 2.0 * C.xxx;
    // x3 = x0 - 1.0 + 3.0 * C.xxx;
    vec3 x1 = x0 - i1 + C.xxx;
    vec3 x2 = x0 - i2 + C.yyy; // 2.0 * 1.0/6.0 = 1.0/3.0
    vec3 x3 = x0 - D.yyy;      // -1.0 + 3.0 * 1.0/6.0 = -0.5

    // Permutations
    i = mod289(i);
    vec4 p = permute(permute(permute(
                i.z + vec4(0.0, i1.z, i2.z, 1.0))
              + i.y + vec4(0.0, i1.y, i2.y, 1.0))
              + i.x + vec4(0.0, i1.x, i2.x, 1.0));

    // Gradients: 7x7 points over a square, mapped onto an octahedron.
    // The ring size 17*17 = 289 is close to a multiple of 49 (49*6 = 294)
    float n_ = 0.142857142857; // 1.0/7.0
    vec3  ns = n_ * D.wyz - D.xzx;

    vec4 j = p - 49.0 * floor(p * ns.z * ns.z);  // mod(p,7*7)

    vec4 x_ = floor(j * ns.z);
    vec4 y_ = floor(j - 7.0 * x_);    // mod(j,N)

    vec4 x = x_ * ns.x + ns.yyyy;
    vec4 y = y_ * ns.x + ns.yyyy;
    vec4 h = 1.0 - abs(x) - abs(y);

    vec4 b0 = vec4(x.xy, y.xy);
    vec4 b1 = vec4(x.zw, y.zw);

    vec4 s0 = floor(b0) * 2.0 + 1.0;
    vec4 s1 = floor(b1) * 2.0 + 1.0;
    vec4 sh = -step(h, vec4(0.0));

    vec4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
    vec4 a1 = b1.xzyw + s1.xzyw * sh.zzww;

    vec3 p0 = vec3(a0.xy, h.x);
    vec3 p1 = vec3(a0.zw, h.y);
    vec3 p2 = vec3(a1.xy, h.z);
    vec3 p3 = vec3(a1.zw, h.w);

    // Normalise gradients
    vec4 norm = taylorInvSqrt(vec4(dot(p0, p0), dot(p1, p1), dot(p2, p2), dot(p3, p3)));
    p0 *= norm.x;
    p1 *= norm.y;
    p2 *= norm.z;
    p3 *= norm.w;

    // Mix contributions
    vec4 m = max(0.6 - vec4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0);
    m = m * m;
    return 42.0 * dot(m * m, vec4(dot(p0, x0), dot(p1, x1), dot(p2, x2), dot(p3, x3)));
}

void main() {
    // Current coordinate matching the voxel position
    ivec3 texelCoord = ivec3(gl_GlobalInvocationID.xyz);

    // Sample position scaled and shifted
    vec3 samplePos = vec3(texelCoord) * u_Scale + u_Offset;

    // Compute simplex noise (returns value between -1.0 and 1.0)
    float noiseVal = snoise(samplePos);

    // Map from [-1.0, 1.0] to [0.0, 1.0] for visibility
    float normalizedNoise = noiseVal * 0.5f + 0.5f;

    // Write out grayscale value to the 3D texture mapping
    imageStore(imgOutput, texelCoord, vec4(vec3(normalizedNoise), 1.0f));
}
)";
};

#endif // RAYLIB_EXTENDED_NOISE_H
