/**********************************************************************************************
*
*   raymath - raylib math glue on top of glm
*
*   Vector, matrix and quaternion arithmetic is provided by glm: Vector2, Vector3, Vector4,
*   Matrix and Quaternion are glm types (see raylib.h). This header keeps only the helpers
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
*       glm::mat4 and raylib's former Matrix describe the same transform.
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

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

// NOTE: raymath can be used standalone, without including raylib.h:
// the math types are just glm aliases, so provide them here in that case.
#if !defined(RL_VECTOR2_TYPE)
    using Vector2 = glm::vec2;
    using Vector3 = glm::vec3;
    using Vector4 = glm::vec4;
    using Quaternion = glm::quat;
    using Matrix = glm::mat4;

    #define RL_VECTOR2_TYPE
    #define RL_VECTOR3_TYPE
    #define RL_VECTOR4_TYPE
    #define RL_QUATERNION_TYPE
    #define RL_MATRIX_TYPE
#endif

#ifndef EPSILON
    #define EPSILON 0.000001f
#endif

//----------------------------------------------------------------------------------
// Types
//----------------------------------------------------------------------------------
// float3, 3 components, used to upload a Vector3 to a shader uniform
typedef struct float3 {
    float v[3];
} float3;

// float16, 16 components, used to upload a Matrix to a shader uniform
typedef struct float16 {
    float v[16];
} float16;

//----------------------------------------------------------------------------------
// Module Functions Definition - Scalar helpers
//----------------------------------------------------------------------------------
inline float Clamp(float value, float min, float max)
{
    return (value < min)? min : ((value > max)? max : value);
}

inline float Lerp(float start, float end, float amount)
{
    return start + amount*(end - start);
}

inline float Normalize(float value, float start, float end)
{
    return (value - start)/(end - start);
}

inline float Remap(float value, float inputStart, float inputEnd, float outputStart, float outputEnd)
{
    return outputStart + ((outputEnd - outputStart)/(inputEnd - inputStart))*(value - inputStart);
}

inline float Wrap(float value, float min, float max)
{
    return value - (max - min)*std::floor((value - min)/(max - min));
}

inline bool FloatEquals(float x, float y)
{
    // Relative epsilon comparison, keeps the previous raymath behaviour
    return (std::fabs(x - y) <= EPSILON*std::fmax(1.0f, std::fmax(std::fabs(x), std::fabs(y))));
}

//----------------------------------------------------------------------------------
// Module Functions Definition - Vector2 helpers
//----------------------------------------------------------------------------------
// Calculate the 2d cross product of two vectors (scalar)
inline float Vector2CrossProduct(Vector2 v1, Vector2 v2)
{
    return (v1.x*v2.y - v1.y*v2.x);
}

// Calculate angle defined by a two vectors line (radians)
inline float Vector2Angle(Vector2 v1, Vector2 v2)
{
    const float dot = v1.x*v2.x + v1.y*v2.y;
    const float det = v1.x*v2.y - v1.y*v2.x;

    return std::atan2(det, dot);
}

// Calculate angle between two vectors, in radians, clockwise from the X axis
inline float Vector2LineAngle(Vector2 start, Vector2 end)
{
    return -std::atan2(end.y - start.y, end.x - start.x);
}

// Move Vector towards target
// Rotate vector by angle
// NOTE: raylib-specific 2D helper, glm::rotate() works on 3D vectors only
inline Vector2 Vector2Rotate(Vector2 v, float angle)
{
    const float cosres = std::cos(angle);
    const float sinres = std::sin(angle);

    return Vector2{v.x*cosres - v.y*sinres, v.x*sinres + v.y*cosres};
}

inline Vector2 Vector2MoveTowards(Vector2 v, Vector2 target, float maxDistance)
{
    const float dx = target.x - v.x;
    const float dy = target.y - v.y;
    const float value = (dx*dx) + (dy*dy);

    if ((value == 0.0f) || ((maxDistance >= 0.0f) && (value <= maxDistance*maxDistance))) return target;

    const float dist = std::sqrt(value);

    return Vector2{v.x + dx/dist*maxDistance, v.y + dy/dist*maxDistance};
}

//----------------------------------------------------------------------------------
// Module Functions Definition - Vector3 helpers
//----------------------------------------------------------------------------------
// Calculate the perpendicular vector to another
inline Vector3 Vector3Perpendicular(Vector3 v)
{
    float min = std::fabs(v.x);
    Vector3 cardinalAxis{1.0f, 0.0f, 0.0f};

    if (std::fabs(v.y) < min)
    {
        min = std::fabs(v.y);
        cardinalAxis = Vector3{0.0f, 1.0f, 0.0f};
    }

    if (std::fabs(v.z) < min) cardinalAxis = Vector3{0.0f, 0.0f, 1.0f};

    return Vector3{v.y*cardinalAxis.z - v.z*cardinalAxis.y,
                   v.z*cardinalAxis.x - v.x*cardinalAxis.z,
                   v.x*cardinalAxis.y - v.y*cardinalAxis.x};
}

// Calculate angle between two vectors, returns degrees
inline float Vector3Angle(Vector3 v1, Vector3 v2)
{
    const Vector3 cross = glm::cross(v1, v2);
    return std::atan2(std::sqrt(glm::dot(cross, cross)), glm::dot(v1, v2));
}

// Calculate the projection of the vector v1 on to v2
inline Vector3 Vector3Project(Vector3 v1, Vector3 v2)
{
    const float v2Length = glm::dot(v2, v2);

    if (v2Length > 0.0f) return v2*(glm::dot(v1, v2)/v2Length);

    return Vector3{};
}

// Calculate the rejection of the vector v1 on to v2
inline Vector3 Vector3Reject(Vector3 v1, Vector3 v2)
{
    const float v2Length = glm::dot(v2, v2);

    if (v2Length > 0.0f) return v1 - v2*(glm::dot(v1, v2)/v2Length);

    return Vector3{};
}

// Orthonormalize provided vectors, makes vectors normalized and orthogonal to each other
inline void Vector3OrthoNormalize(Vector3 *v1, Vector3 *v2)
{
    *v1 = glm::normalize(*v1);

    Vector3 vn1 = glm::cross(*v1, *v2);
    vn1 = glm::normalize(vn1);

    *v2 = glm::cross(vn1, *v1);
}

// Rotate vector by axis and angle using Euler-Rodrigues formula, angle in radians
inline Vector3 Vector3RotateByAxisAngle(Vector3 v, Vector3 axis, float angle)
{
    axis = glm::normalize(axis);

    angle *= 0.5f;
    float a = std::sin(angle);
    const Vector3 w = axis*a;
    a = std::cos(angle);

    const Vector3 wv = glm::cross(w, v);
    const Vector3 wwv = glm::cross(w, wv);

    return v + wv*(2.0f*a) + wwv*2.0f;
}

// Move Vector towards target
inline Vector3 Vector3MoveTowards(Vector3 v, Vector3 target, float maxDistance)
{
    const Vector3 dir = target - v;
    const float value = glm::dot(dir, dir);

    if ((value == 0.0f) || ((maxDistance >= 0.0f) && (value <= maxDistance*maxDistance))) return target;

    return v + dir*(1.0f/std::sqrt(value))*maxDistance;
}

// Calculate the cubic Hermite interpolation between two values
inline Vector3 Vector3CubicHermite(Vector3 v1, Vector3 tangent1, Vector3 v2, Vector3 tangent2, float amount)
{
    const float t2 = amount*amount;
    const float t3 = t2*amount;
    const float h00 = 2.0f*t3 - 3.0f*t2 + 1.0f;
    const float h10 = t3 - 2.0f*t2 + amount;
    const float h01 = -2.0f*t3 + 3.0f*t2;
    const float h11 = t3 - t2;

    return v1*h00 + tangent1*h10 + v2*h01 + tangent2*h11;
}

// Calculate barycenter of a triangle, given 4 points: p point in the triangle, a, b, c triangle vertices
inline Vector3 Vector3Barycenter(Vector3 p, Vector3 a, Vector3 b, Vector3 c)
{
    const Vector3 v0 = b - a;
    const Vector3 v1 = c - a;
    const Vector3 v2 = p - a;
    const float d00 = glm::dot(v0, v0);
    const float d01 = glm::dot(v0, v1);
    const float d11 = glm::dot(v1, v1);
    const float d20 = glm::dot(v2, v0);
    const float d21 = glm::dot(v2, v1);

    const float denom = d00*d11 - d01*d01;

    Vector3 result{};
    result.y = (d11*d20 - d01*d21)/denom;
    result.z = (d00*d21 - d01*d20)/denom;
    result.x = 1.0f - (result.z + result.y);

    return result;
}

// Project point from world coordinate system to the normalized device coordinate (NDC) space
inline Vector3 Vector3Unproject(const Vector3 source, const Matrix& projection, const Matrix& view)
{
    const Matrix matViewProjInv = glm::inverse(projection*view);

    const Vector4 qtransformed = matViewProjInv*Vector4{source, 1.0f};

    return Vector3{qtransformed.x/qtransformed.w,
                   qtransformed.y/qtransformed.w,
                   qtransformed.z/qtransformed.w};
}

// Get Vector3 as float3, useful for shader uniforms
inline float3 Vector3ToFloatV(Vector3 v)
{
    float3 buffer{};
    buffer.v[0] = v.x;
    buffer.v[1] = v.y;
    buffer.v[2] = v.z;

    return buffer;
}

//----------------------------------------------------------------------------------
// Module Functions Definition - Vector4 helpers
//----------------------------------------------------------------------------------
// Move Vector towards target
inline Vector4 Vector4MoveTowards(Vector4 v, Vector4 target, float maxDistance)
{
    const Vector4 dir = target - v;
    const float value = glm::dot(dir, dir);

    if ((value == 0.0f) || ((maxDistance >= 0.0f) && (value <= maxDistance*maxDistance))) return target;

    return v + dir*(1.0f/std::sqrt(value))*maxDistance;
}

//----------------------------------------------------------------------------------
// Module Functions Definition - Quaternion helpers
//----------------------------------------------------------------------------------
// NOTE: glm::quat has no component-wise operators, these helpers are kept because
// raymath's spline and interpolation code uses them as plain 4-component values.
inline Quaternion QuaternionAdd(Quaternion q1, Quaternion q2)
{
    return Quaternion{q1.w + q2.w, q1.x + q2.x, q1.y + q2.y, q1.z + q2.z};
}

inline Quaternion QuaternionAddValue(Quaternion q, float add)
{
    return Quaternion{q.w + add, q.x + add, q.y + add, q.z + add};
}

inline Quaternion QuaternionSubtract(Quaternion q1, Quaternion q2)
{
    return Quaternion{q1.w - q2.w, q1.x - q2.x, q1.y - q2.y, q1.z - q2.z};
}

inline Quaternion QuaternionSubtractValue(Quaternion q, float sub)
{
    return Quaternion{q.w - sub, q.x - sub, q.y - sub, q.z - sub};
}

inline Quaternion QuaternionScale(Quaternion q, float mul)
{
    return Quaternion{q.w*mul, q.x*mul, q.y*mul, q.z*mul};
}

inline Quaternion QuaternionDivide(Quaternion q, float div)
{
    return Quaternion{q.w/div, q.x/div, q.y/div, q.z/div};
}

// Calculate quaternion based on the rotation from one vector to another
inline Quaternion QuaternionFromVector3ToVector3(Vector3 from, Vector3 to)
{
    const float cos2Theta = glm::dot(from, to);
    const Vector3 cross = glm::cross(from, to);

    const float lengths = std::sqrt(glm::dot(cross, cross) + cos2Theta*cos2Theta); // |from|*|to|

    float x = cross.x;
    float y = cross.y;
    float z = cross.z;
    float w = lengths + cos2Theta;

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

    return glm::normalize(Quaternion{w, x, y, z});
}

// Calculate slerp-optimized interpolation between two quaternions
// NOTE: Takes the shorter path (negates q2 when the dot product is negative), which
// glm does not expose as a single call.
inline Quaternion QuaternionNlerp(Quaternion q1, Quaternion q2, float amount)
{
    if (glm::dot(q1, q2) < 0.0f) q2 = -q2;

    return glm::normalize(glm::lerp(q1, q2, amount));
}

// Calculate quaternion cubic spline interpolation using Cubic Hermite Spline algorithm
// as described in the GLTF 2.0 specification: https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#interpolation-cubic
inline Quaternion QuaternionCubicHermiteSpline(Quaternion q1, Quaternion outTangent1, Quaternion q2, Quaternion inTangent2, float t)
{
    const float t2 = t*t;
    const float t3 = t2*t;
    const float h00 = 2.0f*t3 - 3.0f*t2 + 1.0f;
    const float h10 = t3 - 2.0f*t2 + t;
    const float h01 = -2.0f*t3 + 3.0f*t2;
    const float h11 = t3 - t2;

    return glm::normalize(QuaternionAdd(QuaternionAdd(QuaternionScale(q1, h00), QuaternionScale(outTangent1, h10)),
                                       QuaternionAdd(QuaternionScale(q2, h01), QuaternionScale(inTangent2, h11))));
}

// Calculate quaternion from Euler angles (pitch, yaw, roll), in radians
inline Quaternion QuaternionFromEuler(float pitch, float yaw, float roll)
{
    const float x0 = std::cos(pitch*0.5f);
    const float x1 = std::sin(pitch*0.5f);
    const float y0 = std::cos(yaw*0.5f);
    const float y1 = std::sin(yaw*0.5f);
    const float z0 = std::cos(roll*0.5f);
    const float z1 = std::sin(roll*0.5f);

    return Quaternion{x0*y0*z0 + x1*y1*z1,   // w
                      x1*y0*z0 - x0*y1*z1,   // x
                      x0*y1*z0 + x1*y0*z1,   // y
                      x0*y0*z1 - x1*y1*z0};  // z
}

// Calculate Euler angles (pitch, yaw, roll) from a quaternion, in radians
inline Vector3 QuaternionToEuler(Quaternion q)
{
    Vector3 result{};

    // Pitch (y-axis rotation)
    float y0 = 2.0f*(q.w*q.y - q.z*q.x);
    y0 = y0 > 1.0f? 1.0f : y0;
    y0 = y0 < -1.0f? -1.0f : y0;
    result.y = std::asin(y0);

    if (std::fabs(y0) >= 0.9999995f)
    {
        // Gimbal lock: roll and yaw turn about the same axis, keep the whole turn in roll
        const float sign = (q.w < 0.0f)? -1.0f : 1.0f;
        result.x = 2.0f*std::atan2(sign*q.x, sign*q.w);
    }
    else
    {
        // Roll (x-axis rotation)
        const float x0 = 2.0f*(q.w*q.x + q.y*q.z);
        const float x1 = 1.0f - 2.0f*(q.x*q.x + q.y*q.y);
        result.x = std::atan2(x0, x1);

        // Yaw (z-axis rotation)
        const float z0 = 2.0f*(q.w*q.z + q.x*q.y);
        const float z1 = 1.0f - 2.0f*(q.y*q.y + q.z*q.z);
        result.z = std::atan2(z0, z1);
    }

    return result;
}

// Calculate quaternion from axis and angle, angle in radians
inline Quaternion QuaternionFromAxisAngle(Vector3 axis, float angle)
{
    return glm::angleAxis(angle, glm::normalize(axis));
}

// Calculate quaternion axis and angle, returns angle in radians
inline void QuaternionToAxisAngle(Quaternion q, Vector3 *outAxis, float *outAngle)
{
    if (std::fabs(q.w) > 1.0f) q = glm::normalize(q);

    Vector3 resAxis{0.0f, 0.0f, 0.0f};
    const float resAngle = 2.0f*std::acos(q.w);
    const float den = std::sqrt(1.0f - q.w*q.w);

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
// Module Functions Definition - Matrix helpers
//----------------------------------------------------------------------------------
// Create a rotation matrix from Euler angles (x, y, z), in radians
inline Matrix MatrixRotateXYZ(const Vector3 angle)
{
    auto result = Matrix(1.0f);

    const float cosz = std::cos(-angle.z);
    const float sinz = std::sin(-angle.z);
    const float cosy = std::cos(-angle.y);
    const float siny = std::sin(-angle.y);
    const float cosx = std::cos(-angle.x);
    const float sinx = std::sin(-angle.x);

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
inline Matrix MatrixRotateZYX(const Vector3 angle)
{
    auto result = Matrix(1.0f);

    const float cz = std::cos(angle.z);
    const float sz = std::sin(angle.z);
    const float cy = std::cos(angle.y);
    const float sy = std::sin(angle.y);
    const float cx = std::cos(angle.x);
    const float sx = std::sin(angle.x);

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
inline Matrix MatrixCompose(Vector3 translation, Quaternion rotation, Vector3 scale)
{
    Matrix result = glm::mat4_cast(rotation);

    result[0] = Vector4{Vector3{result[0]}*scale.x, 0.0f};
    result[1] = Vector4{Vector3{result[1]}*scale.y, 0.0f};
    result[2] = Vector4{Vector3{result[2]}*scale.z, 0.0f};
    result[3] = Vector4{translation, 1.0f};

    return result;
}

// Decompose a transformation matrix into its rotational, translational and scaling components and remove shear
inline void MatrixDecompose(Matrix mat, Vector3 *translation, Quaternion *rotation, Vector3 *scale)
{
    constexpr auto eps = static_cast<float>(1e-9);

    // Extract Translation
    *translation = Vector3{mat[3][0], mat[3][1], mat[3][2]};

    // Matrix Columns - Rotation will be extracted into here
    Vector3 matColumns[3] = {Vector3{mat[0][0], mat[0][1], mat[0][2]},
                             Vector3{mat[1][0], mat[1][1], mat[1][2]},
                             Vector3{mat[2][0], mat[2][1], mat[2][2]}};

    // Shear Parameters XY, XZ, and YZ (extracted and ignored)
    float shear[3] = {0};

    // Max-Normalizing helps numerical stability
    float stabilizer = eps;
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
    Vector3 scl;
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
    *rotation = glm::quat_cast(glm::mat3{matColumns[0], matColumns[1], matColumns[2]});
}

// Get Matrix as float16, useful for shader uniforms
inline float16 MatrixToFloatV(Matrix mat)
{
    float16 result{};
    for (int i = 0; i < 16; i++) result.v[i] = mat[i/4][i%4];

    return result;
}

#ifndef MatrixToFloat
    #define MatrixToFloat(mat) (MatrixToFloatV(mat).v)
#endif

#endif // RAYMATH_H
