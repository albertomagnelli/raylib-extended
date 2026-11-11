/**********************************************************************************************
*
*   raymath - raylib math glue on top of glm
*
*   Vector, matrix and quaternion arithmetic is provided by glm: vec2, vec3, vec4,
*   mat4 and quat are glm types (see raylib.h). This header keeps only the helpers
*   glm does not provide:
*
*     - float3/float16 upload buffers (Vector3ToFloatV, MatrixToFloatV, MatrixToFloat)
*     - game-oriented vector helpers (MoveTowards, OrthoNormalize, Project/Reject,
*       Barycenter, Perpendicular, Unproject, CubicHermite, Angle)
*     - raylib's Euler convention (pitch/yaw/roll, quaternion order ZYX) and axis-angle helpers
*     - MatrixRotateXYZ/MatrixRotateZYX, MatrixCompose, MatrixDecompose
*     - scalar helpers (Clamp, Lerp, Normalize, Remap, Wrap, FloatEquals)
*
*   CONVENTIONS:
*     - Matrices are column-major, OpenGL style, right-handed, and multiply column vectors:
*       mat4 and raylib's former mat4 describe the same transform.
*     - All math is float only, angles are in radians unless stated otherwise.
*     - raylib's geometric helpers keep their previous units: MatrixRotateXYZ/ZYX and
*       QuaternionFromEuler take radians, Vector3RotateByAxisAngle takes radians.
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2013-2026 Ramon Santamaria (@raysan5)
*
**********************************************************************************************/

#ifndef RAYMATH_H
#define RAYMATH_H

#if !defined(__cplusplus)
    #error "raymath.h requires C++: the math types are glm aliases"
#endif

#include <cmath>

#include "types.h"

#include <glm/gtc/type_ptr.hpp>

// NOTE: raymath can be used standalone, without including raylib.h:
// types.h provides vec2/vec3/vec4, mat4 and quat in that case.
#if !defined(RL_MATRIX_TYPE)
    #define RL_MATRIX_TYPE
#endif

#if !defined(RL_QUATERNION_TYPE)
    #define RL_QUATERNION_TYPE
#endif

#ifndef EPSILON
    #define EPSILON 0.000001f
#endif

//----------------------------------------------------------------------------------
// Types
//----------------------------------------------------------------------------------
// float3, 3 components, used to upload a vec3 to a shader uniform
typedef struct float3 {
    f32 v[3];
} float3;

// float16, 16 components, used to upload a mat4 to a shader uniform
typedef struct float16 {
    f32 v[16];
} float16;

//----------------------------------------------------------------------------------
// Module Functions Definition - Scalar helpers
//----------------------------------------------------------------------------------
inline f32 Clamp(const f32 value, const f32 min, const f32 max)
{
    return (value < min)? min : ((value > max)? max : value);
}

inline f32 Lerp(const f32 start, const f32 end, const f32 amount)
{
    return start + amount*(end - start);
}

inline f32 Normalize(const f32 value, const f32 start, const f32 end)
{
    return (value - start)/(end - start);
}

inline f32 Remap(const f32 value, const f32 inputStart, const f32 inputEnd, const f32 outputStart, const f32 outputEnd)
{
    return outputStart + ((outputEnd - outputStart)/(inputEnd - inputStart))*(value - inputStart);
}

inline f32 Wrap(const f32 value, const f32 min, const f32 max)
{
    return value - (max - min)*std::floor((value - min)/(max - min));
}

inline bool FloatEquals(const f32 x, const f32 y)
{
    // Relative epsilon comparison, keeps the previous raymath behaviour
    return (std::fabs(x - y) <= EPSILON*std::fmax(1.0f, std::fmax(std::fabs(x), std::fabs(y))));
}

//----------------------------------------------------------------------------------
// Module Functions Definition - vec2 helpers
//----------------------------------------------------------------------------------
// Calculate the 2d cross product of two vectors (scalar)
inline f32 Vector2CrossProduct(const vec2 v1, const vec2 v2)
{
    return (v1.x*v2.y - v1.y*v2.x);
}

// Calculate angle defined by a two vectors line (radians)
inline f32 Vector2Angle(const vec2 v1, const vec2 v2)
{
    const f32 dot = v1.x*v2.x + v1.y*v2.y;
    const f32 det = v1.x*v2.y - v1.y*v2.x;

    return std::atan2(det, dot);
}

// Calculate angle between two vectors, in radians, clockwise from the X axis
inline f32 Vector2LineAngle(const vec2 start, const vec2 end)
{
    return -std::atan2(end.y - start.y, end.x - start.x);
}

// Move Vector towards target
// Rotate vector by angle
// NOTE: raylib-specific 2D helper, glm::rotate() works on 3D vectors only
inline vec2 Vector2Rotate(const vec2 v, const f32 angle)
{
    const f32 cosres = std::cos(angle);
    const f32 sinres = std::sin(angle);

    return vec2{v.x*cosres - v.y*sinres, v.x*sinres + v.y*cosres};
}

inline vec2 Vector2MoveTowards(const vec2 v, const vec2 target, const f32 maxDistance)
{
    const f32 dx = target.x - v.x;
    const f32 dy = target.y - v.y;
    const f32 value = (dx*dx) + (dy*dy);

    if ((value == 0.0f) || ((maxDistance >= 0.0f) && (value <= maxDistance*maxDistance))) return target;

    const f32 dist = std::sqrt(value);

    return vec2{v.x + dx/dist*maxDistance, v.y + dy/dist*maxDistance};
}

//----------------------------------------------------------------------------------
// Module Functions Definition - vec3 helpers
//----------------------------------------------------------------------------------
// Calculate the perpendicular vector to another
inline vec3 Vector3Perpendicular(const vec3 v)
{
    f32 min = std::fabs(v.x);
    vec3 cardinalAxis{1.0f, 0.0f, 0.0f};

    if (std::fabs(v.y) < min)
    {
        min = std::fabs(v.y);
        cardinalAxis = vec3{0.0f, 1.0f, 0.0f};
    }

    if (std::fabs(v.z) < min) cardinalAxis = vec3{0.0f, 0.0f, 1.0f};

    return vec3{v.y*cardinalAxis.z - v.z*cardinalAxis.y,
                   v.z*cardinalAxis.x - v.x*cardinalAxis.z,
                   v.x*cardinalAxis.y - v.y*cardinalAxis.x};
}

// Calculate angle between two vectors, returns degrees
inline f32 Vector3Angle(const vec3 v1, const vec3 v2)
{
    const vec3 cross = glm::cross(v1, v2);
    return std::atan2(std::sqrt(glm::dot(cross, cross)), glm::dot(v1, v2));
}

// Calculate the projection of the vector v1 on to v2
inline vec3 Vector3Project(const vec3 v1, const vec3 v2)
{
    const f32 v2Length = glm::dot(v2, v2);

    if (v2Length > 0.0f) return v2*(glm::dot(v1, v2)/v2Length);

    return vec3{};
}

// Calculate the rejection of the vector v1 on to v2
inline vec3 Vector3Reject(const vec3 v1, const vec3 v2)
{
    const f32 v2Length = glm::dot(v2, v2);

    if (v2Length > 0.0f) return v1 - v2*(glm::dot(v1, v2)/v2Length);

    return vec3{};
}

// Orthonormalize provided vectors, makes vectors normalized and orthogonal to each other
inline void Vector3OrthoNormalize(vec3 *v1, vec3 *v2)
{
    *v1 = glm::normalize(*v1);

    vec3 vn1 = glm::cross(*v1, *v2);
    vn1 = glm::normalize(vn1);

    *v2 = glm::cross(vn1, *v1);
}

// Rotate vector by axis and angle using Euler-Rodrigues formula, angle in radians
inline vec3 Vector3RotateByAxisAngle(const vec3 v, vec3 axis, f32 angle)
{
    axis = glm::normalize(axis);

    angle *= 0.5f;
    f32 a = std::sin(angle);
    const vec3 w = axis*a;
    a = std::cos(angle);

    const vec3 wv = glm::cross(w, v);
    const vec3 wwv = glm::cross(w, wv);

    return v + wv*(2.0f*a) + wwv*2.0f;
}

// Move Vector towards target
inline vec3 Vector3MoveTowards(const vec3 v, const vec3 target, const f32 maxDistance)
{
    const vec3 dir = target - v;
    const f32 value = glm::dot(dir, dir);

    if ((value == 0.0f) || ((maxDistance >= 0.0f) && (value <= maxDistance*maxDistance))) return target;

    return v + dir*(1.0f/std::sqrt(value))*maxDistance;
}

// Calculate the cubic Hermite interpolation between two values
inline vec3 Vector3CubicHermite(const vec3 v1, const vec3 tangent1, const vec3 v2, const vec3 tangent2, const f32 amount)
{
    const f32 t2 = amount*amount;
    const f32 t3 = t2*amount;
    const f32 h00 = 2.0f*t3 - 3.0f*t2 + 1.0f;
    const f32 h10 = t3 - 2.0f*t2 + amount;
    const f32 h01 = -2.0f*t3 + 3.0f*t2;
    const f32 h11 = t3 - t2;

    return v1*h00 + tangent1*h10 + v2*h01 + tangent2*h11;
}

// Calculate barycenter of a triangle, given 4 points: p point in the triangle, a, b, c triangle vertices
inline vec3 Vector3Barycenter(const vec3 p, const vec3 a, const vec3 b, const vec3 c)
{
    const vec3 v0 = b - a;
    const vec3 v1 = c - a;
    const vec3 v2 = p - a;
    const f32 d00 = glm::dot(v0, v0);
    const f32 d01 = glm::dot(v0, v1);
    const f32 d11 = glm::dot(v1, v1);
    const f32 d20 = glm::dot(v2, v0);
    const f32 d21 = glm::dot(v2, v1);

    const f32 denom = d00*d11 - d01*d01;

    vec3 result{};
    result.y = (d11*d20 - d01*d21)/denom;
    result.z = (d00*d21 - d01*d20)/denom;
    result.x = 1.0f - (result.z + result.y);

    return result;
}

// Project point from world coordinate system to the normalized device coordinate (NDC) space
inline vec3 Vector3Unproject(const vec3 source, const mat4& projection, const mat4& view)
{
    const mat4 matViewProjInv = glm::inverse(projection*view);

    const vec4 qtransformed = matViewProjInv*vec4{source, 1.0f};

    return vec3{qtransformed.x/qtransformed.w,
                   qtransformed.y/qtransformed.w,
                   qtransformed.z/qtransformed.w};
}

// Get vec3 as float3, useful for shader uniforms
inline float3 Vector3ToFloatV(vec3 v)
{
    float3 buffer{};
    buffer.v[0] = v.x;
    buffer.v[1] = v.y;
    buffer.v[2] = v.z;

    return buffer;
}

//----------------------------------------------------------------------------------
// Module Functions Definition - vec4 helpers
//----------------------------------------------------------------------------------
// Move Vector towards target
inline vec4 Vector4MoveTowards(const vec4 v, const vec4 target, const f32 maxDistance)
{
    const vec4 dir = target - v;
    const f32 value = glm::dot(dir, dir);

    if ((value == 0.0f) || ((maxDistance >= 0.0f) && (value <= maxDistance*maxDistance))) return target;

    return v + dir*(1.0f/std::sqrt(value))*maxDistance;
}

//----------------------------------------------------------------------------------
// Module Functions Definition - quat helpers
//----------------------------------------------------------------------------------
// NOTE: quat has no component-wise operators, these helpers are kept because
// raymath's spline and interpolation code uses them as plain 4-component values.
inline quat QuaternionAdd(const quat q1, const quat q2)
{
    return quat{q1.w + q2.w, q1.x + q2.x, q1.y + q2.y, q1.z + q2.z};
}

inline quat QuaternionAddValue(const quat q, const f32 add)
{
    return quat{q.w + add, q.x + add, q.y + add, q.z + add};
}

inline quat QuaternionSubtract(const quat q1, const quat q2)
{
    return quat{q1.w - q2.w, q1.x - q2.x, q1.y - q2.y, q1.z - q2.z};
}

inline quat QuaternionSubtractValue(const quat q, const f32 sub)
{
    return quat{q.w - sub, q.x - sub, q.y - sub, q.z - sub};
}

inline quat QuaternionScale(const quat q, const f32 mul)
{
    return quat{q.w*mul, q.x*mul, q.y*mul, q.z*mul};
}

inline quat QuaternionDivide(const quat q, const f32 div)
{
    return quat{q.w/div, q.x/div, q.y/div, q.z/div};
}

// Calculate quaternion based on the rotation from one vector to another
inline quat QuaternionFromVector3ToVector3(const vec3 from, const vec3 to)
{
    const f32 cos2Theta = glm::dot(from, to);
    const vec3 cross = glm::cross(from, to);

    const f32 lengths = std::sqrt(glm::dot(cross, cross) + cos2Theta*cos2Theta); // |from|*|to|

    f32 x = cross.x;
    f32 y = cross.y;
    f32 z = cross.z;
    f32 w = lengths + cos2Theta;

    // Opposite vectors: cross product is zero, rotate 180 degrees around any axis perpendicular to 'from'
    if ((lengths > 0.0f) && (w <= lengths*EPSILON))
    {
        if (std::fabs(from.x) > std::fabs(from.z))
        {
            x = -from.y;
            y = from.x;
            z = 0.0f;
        }
        else
        {
            x = 0.0f;
            y = -from.z;
            z = from.y;
        }

        w = 0.0f;
    }

    return glm::normalize(quat{w, x, y, z});
}

// Calculate slerp-optimized interpolation between two quaternions
// NOTE: Takes the shorter path (negates q2 when the dot product is negative), which
// glm does not expose as a single call.
inline quat QuaternionNlerp(const quat q1, quat q2, const f32 amount)
{
    if (glm::dot(q1, q2) < 0.0f) q2 = -q2;

    return glm::normalize(glm::lerp(q1, q2, amount));
}

// Calculate quaternion cubic spline interpolation using Cubic Hermite Spline algorithm
// as described in the GLTF 2.0 specification: https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#interpolation-cubic
inline quat QuaternionCubicHermiteSpline(const quat q1, const quat outTangent1, const quat q2, const quat inTangent2, const f32 t)
{
    const f32 t2 = t*t;
    const f32 t3 = t2*t;
    const f32 h00 = 2.0f*t3 - 3.0f*t2 + 1.0f;
    const f32 h10 = t3 - 2.0f*t2 + t;
    const f32 h01 = -2.0f*t3 + 3.0f*t2;
    const f32 h11 = t3 - t2;

    return glm::normalize(QuaternionAdd(QuaternionAdd(QuaternionScale(q1, h00), QuaternionScale(outTangent1, h10)),
                                       QuaternionAdd(QuaternionScale(q2, h01), QuaternionScale(inTangent2, h11))));
}

// Calculate quaternion from Euler angles (pitch, yaw, roll), in radians
inline quat QuaternionFromEuler(const f32 pitch, const f32 yaw, const f32 roll)
{
    const f32 x0 = std::cos(pitch*0.5f);
    const f32 x1 = std::sin(pitch*0.5f);
    const f32 y0 = std::cos(yaw*0.5f);
    const f32 y1 = std::sin(yaw*0.5f);
    const f32 z0 = std::cos(roll*0.5f);
    const f32 z1 = std::sin(roll*0.5f);

    return quat{x0*y0*z0 + x1*y1*z1,         // w
                      x1*y0*z0 - x0*y1*z1,   // x
                      x0*y1*z0 + x1*y0*z1,   // y
                      x0*y0*z1 - x1*y1*z0};  // z
}

// Calculate Euler angles (pitch, yaw, roll) from a quaternion, in radians
inline vec3 QuaternionToEuler(const quat q)
{
    vec3 result{};

    // Pitch (y-axis rotation)
    f32 y0 = 2.0f*(q.w*q.y - q.z*q.x);
    y0 = y0 > 1.0f? 1.0f : y0;
    y0 = y0 < -1.0f? -1.0f : y0;
    result.y = std::asin(y0);

    if (std::fabs(y0) >= 0.9999995f)
    {
        // Gimbal lock: roll and yaw turn about the same axis, keep the whole turn in roll
        const f32 sign = (q.w < 0.0f)? -1.0f : 1.0f;
        result.x = 2.0f*std::atan2(sign*q.x, sign*q.w);
    }
    else
    {
        // Roll (x-axis rotation)
        const f32 x0 = 2.0f*(q.w*q.x + q.y*q.z);
        const f32 x1 = 1.0f - 2.0f*(q.x*q.x + q.y*q.y);
        result.x = std::atan2(x0, x1);

        // Yaw (z-axis rotation)
        const f32 z0 = 2.0f*(q.w*q.z + q.x*q.y);
        const f32 z1 = 1.0f - 2.0f*(q.y*q.y + q.z*q.z);
        result.z = std::atan2(z0, z1);
    }

    return result;
}

// Calculate quaternion from axis and angle, angle in radians
inline quat QuaternionFromAxisAngle(const vec3 axis, const f32 angle)
{
    return glm::angleAxis(angle, glm::normalize(axis));
}

// Calculate quaternion axis and angle, returns angle in radians
inline void QuaternionToAxisAngle(quat q, vec3 *outAxis, f32 *outAngle)
{
    if (std::fabs(q.w) > 1.0f) q = glm::normalize(q);

    vec3 resAxis{0.0f, 0.0f, 0.0f};
    const f32 resAngle = 2.0f*std::acos(q.w);
    const f32 den = std::sqrt(1.0f - q.w*q.w);

    if (den > EPSILON)
    {
        resAxis.x = q.x/den;
        resAxis.y = q.y/den;
        resAxis.z = q.z/den;
    }
    else
    {
        // This occurs when the angle is zero
        // Not a problem, set an arbitrary normalized axis
        resAxis.x = 1.0f;
    }

    *outAxis = resAxis;
    *outAngle = resAngle;
}

//----------------------------------------------------------------------------------
// Module Functions Definition - mat4 helpers
//----------------------------------------------------------------------------------
// Create a rotation matrix from Euler angles (x, y, z), in radians
inline mat4 MatrixRotateXYZ(const vec3 angle)
{
    auto result = mat4(1.0f);

    const f32 cosz = std::cos(-angle.z);
    const f32 sinz = std::sin(-angle.z);
    const f32 cosy = std::cos(-angle.y);
    const f32 siny = std::sin(-angle.y);
    const f32 cosx = std::cos(-angle.x);
    const f32 sinx = std::sin(-angle.x);

    result[0][0] = cosz*cosy;
    result[0][1] = (cosz*siny*sinx) - (sinz*cosx);
    result[0][2] = (cosz*siny*cosx) + (sinz*sinx);

    result[1][0] = sinz*cosy;
    result[1][1] = (sinz*siny*sinx) + (cosz*cosx);
    result[1][2] = (sinz*siny*cosx) - (cosz*sinx);

    result[2][0] = -siny;
    result[2][1] = cosy*sinx;
    result[2][2] = cosy*cosx;

    return result;
}

// Create a rotation matrix from Euler angles (x, y, z), applied in ZYX order, in radians
inline mat4 MatrixRotateZYX(const vec3 angle)
{
    auto result = mat4(1.0f);

    const f32 cz = std::cos(angle.z);
    const f32 sz = std::sin(angle.z);
    const f32 cy = std::cos(angle.y);
    const f32 sy = std::sin(angle.y);
    const f32 cx = std::cos(angle.x);
    const f32 sx = std::sin(angle.x);

    result[0][0] = cz*cy;
    result[1][0] = cz*sy*sx - cx*sz;
    result[2][0] = sz*sx + cz*cx*sy;

    result[0][1] = cy*sz;
    result[1][1] = cz*cx + sz*sy*sx;
    result[2][1] = cx*sz*sy - cz*sx;

    result[0][2] = -sy;
    result[1][2] = cy*sx;
    result[2][2] = cy*cx;

    return result;
}

// Compose a transformation matrix from translation, rotation and scale
inline mat4 MatrixCompose(const vec3 translation, const quat rotation, const vec3 scale)
{
    mat4 result = glm::mat4_cast(rotation);

    result[0] = vec4{vec3{result[0]}*scale.x, 0.0f};
    result[1] = vec4{vec3{result[1]}*scale.y, 0.0f};
    result[2] = vec4{vec3{result[2]}*scale.z, 0.0f};
    result[3] = vec4{translation, 1.0f};

    return result;
}

// Decompose a transformation matrix into its rotational, translational and scaling components and remove shear
inline void MatrixDecompose(const mat4 mat, vec3 *translation, quat *rotation, vec3 *scale)
{
    constexpr auto eps = static_cast<f32>(1e-9);

    // Extract Translation
    *translation = vec3{mat[3][0], mat[3][1], mat[3][2]};

    // Matrix columns - rotation will be extracted into here
    vec3 matColumns[3] = {vec3{mat[0][0], mat[0][1], mat[0][2]},
                             vec3{mat[1][0], mat[1][1], mat[1][2]},
                             vec3{mat[2][0], mat[2][1], mat[2][2]}};

    // Shear Parameters XY, XZ, and YZ (extracted and ignored)
    f32 shear[3] = {0};

    // Max-Normalizing helps numerical stability
    f32 stabilizer = eps;
    for (const auto & matColumn : matColumns)
    {
        stabilizer = std::fmax(stabilizer, std::fabs(matColumn.x));
        stabilizer = std::fmax(stabilizer, std::fabs(matColumn.y));
        stabilizer = std::fmax(stabilizer, std::fabs(matColumn.z));
    }
    matColumns[0] *= 1.0f/stabilizer;
    matColumns[1] *= 1.0f/stabilizer;
    matColumns[2] *= 1.0f/stabilizer;

    // X Scale
    vec3 scl;
    scl.x = glm::length(matColumns[0]);
    if (scl.x > eps) matColumns[0] *= 1.0f/scl.x;

    // Compute XY shear and make col2 orthogonal
    shear[0] = glm::dot(matColumns[0], matColumns[1]);
    matColumns[1] -= matColumns[0]*shear[0];

    // Y Scale
    scl.y = glm::length(matColumns[1]);
    if (scl.y > eps)
    {
        matColumns[1] *= 1.0f/scl.y;
        shear[0] /= scl.y; // Correct XY shear
    }

    // Compute XZ and YZ shears and make col3 orthogonal
    shear[1] = glm::dot(matColumns[0], matColumns[2]);
    matColumns[2] -= matColumns[0]*shear[1];
    shear[2] = glm::dot(matColumns[1], matColumns[2]);
    matColumns[2] -= matColumns[1]*shear[2];

    // Z Scale
    scl.z = glm::length(matColumns[2]);
    if (scl.z > eps)
    {
        matColumns[2] *= 1.0f/scl.z;
        shear[1] /= scl.z; // Correct XZ shear
        shear[2] /= scl.z; // Correct YZ shear
    }

    // matColumns are now orthonormal in O(3). Now ensure its in SO(3) by enforcing det = 1
    if (glm::dot(matColumns[0], glm::cross(matColumns[1], matColumns[2])) < 0.0f)
    {
        scl = -scl;
        matColumns[0] = -matColumns[0];
        matColumns[1] = -matColumns[1];
        matColumns[2] = -matColumns[2];
    }

    // Set Scale
    *scale = scl*stabilizer;

    // Extract Rotation
    *rotation = glm::quat_cast(mat3{matColumns[0], matColumns[1], matColumns[2]});
}

// Get mat4 as float16, useful for shader uniforms
inline float16 MatrixToFloatV(const mat4 mat)
{
    float16 result{};
    for (i32 i = 0; i < 16; i++) result.v[i] = mat[i/4][i%4];

    return result;
}

#ifndef MatrixToFloat
    #define MatrixToFloat(mat) (MatrixToFloatV(mat).v)
#endif

#endif // RAYMATH_H
