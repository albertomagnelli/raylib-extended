//
// Created by ingsto on 10.11.26.
//

#ifndef RAYLIB_EXTENDED_NOISE_H
#define RAYLIB_EXTENDED_NOISE_H
#include <string>

// 1. Generate & Allocate a 3D target Texture for our compute shader outputs
GLuint texture3D;
glCreateTextures(GL_TEXTURE_3D, 1, &texture3D);
glTextureStorage3D(texture3D, 1, GL_RGBA32F, 256, 256, 256);

// 2. Bind the Texture layer to image unit matching layout(binding = 0)
glBindImageTexture(0, texture3D, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

// 3. Compile Compute Shader and use the Shader Program Pipeline
glUseProgram(computeShaderProgramID);

// Set uniform values
glUniform1f(0, 0.045f);                  // location 0: u_Scale
glUniform3f(1, 12.3f, 45.6f, 78.9f);     // location 1: u_Offset

// 4. Dispatch the compute grid.
// Since our local work groups are 8x8x8, we divide structural size 256 by 8 (= 32)
glDispatchCompute(32, 32, 32);

// 5. Block CPU execution execution flow until GPU finishes calculation routines
glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);


1D	GL_TEXTURE_1D	glTextureStorage1D(tex, 1, GL_RGBA32F, Width);	glDispatchCompute(Width / 64, 1, 1);
2D	GL_TEXTURE_2D	glTextureStorage2D(tex, 1, GL_RGBA32F, Width, Height);	glDispatchCompute(Width / 8, Height / 8, 1);


std::string sn1d = R"(
#version 460 core

// Optimized for 1D arrays matching a 64 thread execution block
layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

// Output texture binding (1D Floating point texture)
layout(rgba32f, binding = 0) writeonly uniform image1D imgOutput1D;

layout(location = 0) uniform float u_Scale = 0.05f;
layout(location = 1) uniform float u_Offset = 0.0f;

vec3 mod289(vec3 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec2 permute(vec2 x) { return mod289(((x * 34.0) + 1.0) * x).xy; }

// --- 1D Simplex Noise Function ---
float snoise(float v) {
    // 1D simplex is just a single step interval line segment
    float i = floor(v);
    float x0 = v - i;
    float x1 = x0 - 1.0;

    // Permutations & Wrap
    vec2 p = permute(mod289(vec3(i, i + 1.0, 0.0)).xy);

    // Map Gradients between [-1, 1]
    vec2 gx = frac(p * 0.0243902439) * 2.0 - 1.0; // 1/41
    vec2 g = gx * (1.79284291400159 - 0.85373472095314 * 0.5);

    // Mix contributions using matching kernel bounds
    vec2 m = max(0.5 - vec2(x0*x0, x1*x1), 0.0);
    m = m * m;
    m = m * m;

    return 4.0 * dot(m, vec2(g.x * x0, g.y * x1));
}

void main() {
    int texelCoord = int(gl_GlobalInvocationID.x);

    float samplePos = float(texelCoord) * u_Scale + u_Offset;
    float noiseVal = snoise(samplePos);
    float normalizedNoise = noiseVal * 0.5f + 0.5f;

    imageStore(imgOutput1D, texelCoord, vec4(vec3(normalizedNoise), 1.0f));
}
)";
std::string sn2d = R"(
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
    vec3 x = 2.0 * frac(p * C.wwww) - 1.0;
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
std::string sn3d = R"(
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

    vec4 j = p - 49.0 * floor(p * ns.z);  // mod(p,7*7)

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

#endif //RAYLIB_EXTENDED_NOISE_H
